"""
Logging and notification system for Unified Package Manager
Handles both file-based logging and in-app notifications
"""

import logging
import os
from datetime import datetime
from typing import Optional
from PyQt5.QtCore import QObject, pyqtSignal


class NotificationSignals(QObject):
    """Qt signals for notifications"""
    notification_signal = pyqtSignal(str, str, str)  # title, message, type


class LoggerManager:
    """Manages logging and notifications for UPM"""

    def __init__(self, log_dir: str = "logs", log_file: str = "upm.log"):
        """Initialize logger with file and console handlers"""
        self.log_dir = log_dir
        self.log_file = log_file
        self.log_path = os.path.join(log_dir, log_file)

        # Create logs directory if it doesn't exist
        os.makedirs(log_dir, exist_ok=True)

        # Setup file logger
        self.logger = logging.getLogger("UPM")
        self.logger.setLevel(logging.DEBUG)

        # File handler
        file_handler = logging.FileHandler(self.log_path)
        file_handler.setLevel(logging.DEBUG)

        # Console handler
        console_handler = logging.StreamHandler()
        console_handler.setLevel(logging.INFO)

        # Formatter
        formatter = logging.Formatter(
            '%(asctime)s - %(name)s - %(levelname)s - %(message)s',
            datefmt='%Y-%m-%d %H:%M:%S'
        )
        file_handler.setFormatter(formatter)
        console_handler.setFormatter(formatter)

        # Add handlers
        self.logger.addHandler(file_handler)
        self.logger.addHandler(console_handler)

        # Notification signals
        self.notification_signals = NotificationSignals()

    def log_info(self, message: str):
        """Log info message"""
        self.logger.info(message)

    def log_warning(self, message: str):
        """Log warning message"""
        self.logger.warning(message)

    def log_error(self, message: str, error: Optional[Exception] = None):
        """Log error message with optional exception details"""
        if error:
            self.logger.error(f"{message}: {str(error)}", exc_info=True)
        else:
            self.logger.error(message)

    def log_debug(self, message: str):
        """Log debug message"""
        self.logger.debug(message)

    def log_success(self, message: str):
        """Log success message"""
        self.logger.info(f"SUCCESS: {message}")

    # ==================== Installation/Uninstallation Logging ====================

    def log_install_attempt(self, package_name: str, package_type: str):
        """Log installation attempt"""
        self.log_info(f"Attempting to install {package_type} package: {package_name}")

    def log_install_success(self, package_name: str, package_type: str):
        """Log successful installation"""
        message = f"Successfully installed {package_type} package: {package_name}"
        self.log_success(message)
        self.emit_notification("Installation Successful", message, "success")

    def log_install_failure(self, package_name: str, package_type: str, error: str):
        """Log installation failure"""
        message = f"Failed to install {package_type} package: {package_name}"
        self.log_error(f"{message} - Error: {error}")
        self.emit_notification("Installation Failed", f"{message}\n{error}", "error")

    def log_uninstall_attempt(self, package_name: str, package_type: str):
        """Log uninstallation attempt"""
        self.log_info(f"Attempting to uninstall {package_type} package: {package_name}")

    def log_uninstall_success(self, package_name: str, package_type: str):
        """Log successful uninstallation"""
        message = f"Successfully uninstalled {package_type} package: {package_name}"
        self.log_success(message)
        self.emit_notification("Uninstallation Successful", message, "success")

    def log_uninstall_failure(self, package_name: str, package_type: str, error: str):
        """Log uninstallation failure"""
        message = f"Failed to uninstall {package_type} package: {package_name}"
        self.log_error(f"{message} - Error: {error}")
        self.emit_notification("Uninstallation Failed", f"{message}\n{error}", "error")

    # ==================== Search Logging ====================

    def log_search(self, query: str, repositories: list):
        """Log search operation"""
        repos_str = ", ".join(repositories)
        self.log_info(f"Searching for '{query}' in repositories: {repos_str}")

    def log_search_results(self, query: str, results_count: int):
        """Log search results"""
        self.log_info(f"Search for '{query}' returned {results_count} results")

    def log_search_error(self, repository: str, error: str):
        """Log search error for a specific repository"""
        message = f"Error searching {repository}"
        self.log_error(f"{message}: {error}")
        self.emit_notification("Search Error", f"{message}\n{error}", "warning")

    # ==================== System Status Logging ====================

    def log_system_warning(self, message: str):
        """Log system warning"""
        self.log_warning(message)
        self.emit_notification("System Warning", message, "warning")

    def log_system_error(self, message: str):
        """Log system error"""
        self.log_error(message)
        self.emit_notification("System Error", message, "error")

    def log_repository_status(self, repo_name: str, enabled: bool):
        """Log repository status change"""
        status = "enabled" if enabled else "disabled"
        self.log_info(f"Repository {repo_name} {status}")

    def log_settings_change(self, setting: str, old_value, new_value):
        """Log settings change"""
        self.log_info(f"Settings changed: {setting} from {old_value} to {new_value}")

    # ==================== Notification Methods ====================

    def emit_notification(self, title: str, message: str, notification_type: str = "info"):
        """Emit notification signal for GUI display

        Args:
            title: Notification title
            message: Notification message
            notification_type: Type of notification (success, error, warning, info)
        """
        self.notification_signals.notification_signal.emit(title, message, notification_type)

    # ==================== Log File Management ====================

    def get_log_file_path(self) -> str:
        """Get the path to the log file"""
        return self.log_path

    def read_log_file(self, lines: int = 100) -> str:
        """Read the last N lines from the log file"""
        try:
            with open(self.log_path, 'r') as f:
                all_lines = f.readlines()
                return ''.join(all_lines[-lines:])
        except FileNotFoundError:
            return "Log file not found"
        except Exception as e:
            return f"Error reading log file: {str(e)}"

    def export_logs_to_file(self, export_path: str, days: int = 7):
        """Export logs from the last N days to a file"""
        try:
            from datetime import timedelta
            cutoff_date = datetime.now() - timedelta(days=days)

            with open(self.log_path, 'r') as source:
                with open(export_path, 'w') as dest:
                    dest.write(f"Unified Package Manager - Logs Export\n")
                    dest.write(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                    dest.write(f"Period: Last {days} days\n")
                    dest.write("=" * 80 + "\n\n")

                    for line in source:
                        # Simple date filtering (logs are formatted with dates)
                        try:
                            # Extract date from log line
                            date_str = line.split(' - ')[0]
                            log_date = datetime.strptime(date_str, '%Y-%m-%d %H:%M:%S')
                            if log_date >= cutoff_date:
                                dest.write(line)
                        except (ValueError, IndexError):
                            # If we can't parse the date, include the line anyway
                            dest.write(line)

            self.log_success(f"Logs exported to {export_path}")
            return True
        except Exception as e:
            self.log_error(f"Failed to export logs: {str(e)}")
            return False

    def clear_log_file(self):
        """Clear the log file"""
        try:
            with open(self.log_path, 'w') as f:
                f.write("")
            self.log_info("Log file cleared")
            return True
        except Exception as e:
            self.log_error(f"Failed to clear log file: {str(e)}")
            return False


# Global logger instance
_logger_instance = None


def get_logger() -> LoggerManager:
    """Get or create the global logger instance"""
    global _logger_instance
    if _logger_instance is None:
        _logger_instance = LoggerManager()
    return _logger_instance
