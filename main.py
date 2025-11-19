#!/usr/bin/env python3
"""
Unified Package Manager (UPM) for Zorin OS
Main application entry point
"""

import sys
import os
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout,
                             QHBoxLayout, QTabWidget, QLabel, QLineEdit, QPushButton,
                             QTableWidget, QTableWidgetItem, QMessageBox, QComboBox,
                             QCheckBox, QSpinBox, QTextEdit, QHeaderView, QProgressBar,
                             QFileDialog, QGroupBox, QFormLayout)
from PyQt5.QtCore import Qt, QThread, pyqtSignal, QTimer
from PyQt5.QtGui import QIcon

from database import DatabaseManager
from logger import get_logger
from apt_backend import AptBackend
from snap_backend import SnapBackend
from flatpak_backend import FlatpakBackend


class SearchWorker(QThread):
    """Worker thread for package search operations"""
    finished = pyqtSignal(list)
    error = pyqtSignal(str)

    def __init__(self, backends, query):
        super().__init__()
        self.backends = backends
        self.query = query

    def run(self):
        """Execute search across enabled backends"""
        try:
            results = []
            for backend in self.backends:
                try:
                    packages = backend.search_packages(self.query, limit=50)
                    results.extend(packages)
                except Exception as e:
                    print(f"Error searching {backend.package_type}: {str(e)}")
            self.finished.emit(results)
        except Exception as e:
            self.error.emit(str(e))


class InstallWorker(QThread):
    """Worker thread for package installation"""
    finished = pyqtSignal(bool, str)

    def __init__(self, backend, package_name):
        super().__init__()
        self.backend = backend
        self.package_name = package_name

    def run(self):
        """Execute package installation"""
        success, message = self.backend.install_package(self.package_name)
        self.finished.emit(success, message)


class UninstallWorker(QThread):
    """Worker thread for package uninstallation"""
    finished = pyqtSignal(bool, str)

    def __init__(self, backend, package_name):
        super().__init__()
        self.backend = backend
        self.package_name = package_name

    def run(self):
        """Execute package uninstallation"""
        success, message = self.backend.uninstall_package(self.package_name)
        self.finished.emit(success, message)


class UnifiedPackageManager(QMainWindow):
    """Main application window"""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("Unified Package Manager - Zorin OS")
        self.setMinimumSize(1000, 700)

        # Initialize components
        self.db = DatabaseManager()
        self.logger = get_logger()

        # Initialize backends
        self.apt_backend = AptBackend()
        self.snap_backend = SnapBackend()
        self.flatpak_backend = FlatpakBackend()

        # Connect notification signals
        self.logger.notification_signals.notification_signal.connect(self.show_notification)

        # Initialize UI
        self.init_ui()

        # Load preferences
        self.load_preferences()

        # Check system status
        self.check_system_status()

    def init_ui(self):
        """Initialize the user interface"""
        # Central widget
        central_widget = QWidget()
        self.setCentralWidget(central_widget)

        # Main layout
        layout = QVBoxLayout()
        central_widget.setLayout(layout)

        # Tab widget
        self.tabs = QTabWidget()
        layout.addWidget(self.tabs)

        # Create tabs
        self.create_search_tab()
        self.create_installed_tab()
        self.create_settings_tab()
        self.create_logs_tab()

        # Status bar
        self.statusBar().showMessage("Ready")

    def create_search_tab(self):
        """Create the search and install tab"""
        tab = QWidget()
        layout = QVBoxLayout()
        tab.setLayout(layout)

        # Search bar
        search_layout = QHBoxLayout()
        layout.addLayout(search_layout)

        search_layout.addWidget(QLabel("Search:"))
        self.search_input = QLineEdit()
        self.search_input.setPlaceholderText("Enter package name...")
        self.search_input.returnPressed.connect(self.search_packages)
        search_layout.addWidget(self.search_input)

        self.search_button = QPushButton("Search")
        self.search_button.clicked.connect(self.search_packages)
        search_layout.addWidget(self.search_button)

        # Repository filter
        filter_layout = QHBoxLayout()
        layout.addLayout(filter_layout)

        filter_layout.addWidget(QLabel("Filter by:"))
        self.repo_filter = QComboBox()
        self.repo_filter.addItems(["All Repositories", "APT", "Snap", "Flatpak"])
        self.repo_filter.currentTextChanged.connect(self.filter_results)
        filter_layout.addWidget(self.repo_filter)
        filter_layout.addStretch()

        # Results table
        self.search_table = QTableWidget()
        self.search_table.setColumnCount(6)
        self.search_table.setHorizontalHeaderLabels([
            "Name", "Description", "Version", "Type", "Source", "Action"
        ])
        self.search_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.Stretch)
        self.search_table.setSelectionBehavior(QTableWidget.SelectRows)
        self.search_table.setEditTriggers(QTableWidget.NoEditTriggers)
        layout.addWidget(self.search_table)

        # Progress bar
        self.search_progress = QProgressBar()
        self.search_progress.setVisible(False)
        layout.addWidget(self.search_progress)

        self.tabs.addTab(tab, "Search && Install")

    def create_installed_tab(self):
        """Create the installed applications tab"""
        tab = QWidget()
        layout = QVBoxLayout()
        tab.setLayout(layout)

        # Controls
        controls_layout = QHBoxLayout()
        layout.addLayout(controls_layout)

        refresh_button = QPushButton("Refresh")
        refresh_button.clicked.connect(self.load_installed_apps)
        controls_layout.addWidget(refresh_button)

        controls_layout.addWidget(QLabel("Filter by:"))
        self.installed_filter = QComboBox()
        self.installed_filter.addItems(["All", "APT", "Snap", "Flatpak"])
        self.installed_filter.currentTextChanged.connect(self.filter_installed)
        controls_layout.addWidget(self.installed_filter)
        controls_layout.addStretch()

        # Installed apps table
        self.installed_table = QTableWidget()
        self.installed_table.setColumnCount(6)
        self.installed_table.setHorizontalHeaderLabels([
            "Name", "Package Name", "Version", "Type", "Source", "Action"
        ])
        self.installed_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self.installed_table.setSelectionBehavior(QTableWidget.SelectRows)
        self.installed_table.setEditTriggers(QTableWidget.NoEditTriggers)
        layout.addWidget(self.installed_table)

        self.tabs.addTab(tab, "Installed Applications")

    def create_settings_tab(self):
        """Create the settings tab"""
        tab = QWidget()
        layout = QVBoxLayout()
        tab.setLayout(layout)

        # Repository settings group
        repo_group = QGroupBox("Third-Party Repositories")
        repo_layout = QFormLayout()
        repo_group.setLayout(repo_layout)

        self.apt_checkbox = QCheckBox("Enable APT repositories")
        repo_layout.addRow("APT:", self.apt_checkbox)

        self.snap_checkbox = QCheckBox("Enable Snap Store")
        repo_layout.addRow("Snap:", self.snap_checkbox)

        self.flatpak_checkbox = QCheckBox("Enable Flathub (Flatpak)")
        repo_layout.addRow("Flatpak:", self.flatpak_checkbox)

        layout.addWidget(repo_group)

        # Notification settings group
        notif_group = QGroupBox("Notifications")
        notif_layout = QFormLayout()
        notif_group.setLayout(notif_layout)

        self.notifications_checkbox = QCheckBox("Enable notifications")
        notif_layout.addRow("Notifications:", self.notifications_checkbox)

        layout.addWidget(notif_group)

        # Database settings group
        db_group = QGroupBox("Database Settings")
        db_layout = QFormLayout()
        db_group.setLayout(db_layout)

        self.auto_save_checkbox = QCheckBox("Auto-save application metadata")
        db_layout.addRow("Auto-save:", self.auto_save_checkbox)

        self.log_retention = QSpinBox()
        self.log_retention.setRange(1, 365)
        self.log_retention.setSuffix(" days")
        db_layout.addRow("Log retention:", self.log_retention)

        layout.addWidget(db_group)

        # Buttons
        buttons_layout = QHBoxLayout()
        layout.addLayout(buttons_layout)

        save_button = QPushButton("Save Settings")
        save_button.clicked.connect(self.save_settings)
        buttons_layout.addWidget(save_button)

        reset_button = QPushButton("Reset to Defaults")
        reset_button.clicked.connect(self.reset_settings)
        buttons_layout.addWidget(reset_button)
        buttons_layout.addStretch()

        layout.addStretch()

        self.tabs.addTab(tab, "Settings")

    def create_logs_tab(self):
        """Create the logs viewer tab"""
        tab = QWidget()
        layout = QVBoxLayout()
        tab.setLayout(layout)

        # Controls
        controls_layout = QHBoxLayout()
        layout.addLayout(controls_layout)

        refresh_button = QPushButton("Refresh")
        refresh_button.clicked.connect(self.load_logs)
        controls_layout.addWidget(refresh_button)

        export_button = QPushButton("Export Logs")
        export_button.clicked.connect(self.export_logs)
        controls_layout.addWidget(export_button)

        clear_button = QPushButton("Clear Logs")
        clear_button.clicked.connect(self.clear_logs)
        controls_layout.addWidget(clear_button)

        controls_layout.addStretch()

        # Logs text area
        self.logs_text = QTextEdit()
        self.logs_text.setReadOnly(True)
        self.logs_text.setStyleSheet("font-family: monospace;")
        layout.addWidget(self.logs_text)

        self.tabs.addTab(tab, "Logs")

    # ==================== Search Functions ====================

    def search_packages(self):
        """Search for packages across enabled repositories"""
        query = self.search_input.text().strip()
        if not query:
            QMessageBox.warning(self, "Search", "Please enter a search term")
            return

        # Get enabled backends
        prefs = self.db.get_preferences()
        backends = []

        if prefs['repo_apt_enabled'] and self.apt_backend.is_available():
            backends.append(self.apt_backend)
        if prefs['repo_snapd_enabled'] and self.snap_backend.is_available():
            backends.append(self.snap_backend)
        if prefs['repo_flathub_enabled'] and self.flatpak_backend.is_available():
            backends.append(self.flatpak_backend)

        if not backends:
            QMessageBox.warning(self, "Search", "No package repositories are enabled")
            return

        # Log search
        repo_names = [b.source_repo for b in backends]
        self.logger.log_search(query, repo_names)

        # Show progress
        self.search_progress.setVisible(True)
        self.search_progress.setRange(0, 0)  # Indeterminate
        self.search_button.setEnabled(False)
        self.statusBar().showMessage(f"Searching for '{query}'...")

        # Start search worker
        self.search_worker = SearchWorker(backends, query)
        self.search_worker.finished.connect(self.display_search_results)
        self.search_worker.error.connect(self.search_error)
        self.search_worker.start()

    def display_search_results(self, results):
        """Display search results in the table"""
        self.search_progress.setVisible(False)
        self.search_button.setEnabled(True)
        self.statusBar().showMessage(f"Found {len(results)} packages")

        self.search_results = results
        self.filter_results()

    def filter_results(self):
        """Filter search results by repository type"""
        filter_text = self.repo_filter.currentText()

        # Clear table
        self.search_table.setRowCount(0)

        # Filter results
        filtered = self.search_results if hasattr(self, 'search_results') else []

        if filter_text != "All Repositories":
            filter_type = filter_text.lower()
            filtered = [r for r in filtered if r['package_type'] == filter_type]

        # Populate table
        for package in filtered:
            row = self.search_table.rowCount()
            self.search_table.insertRow(row)

            self.search_table.setItem(row, 0, QTableWidgetItem(package['name']))
            self.search_table.setItem(row, 1, QTableWidgetItem(package.get('description', '')[:100]))
            self.search_table.setItem(row, 2, QTableWidgetItem(package.get('version', '')))
            self.search_table.setItem(row, 3, QTableWidgetItem(package['package_type'].upper()))
            self.search_table.setItem(row, 4, QTableWidgetItem(package['source_repo']))

            # Install button
            install_button = QPushButton("Install" if not package.get('installed') else "Installed")
            install_button.setEnabled(not package.get('installed'))
            install_button.clicked.connect(lambda checked, p=package: self.install_package(p))
            self.search_table.setCellWidget(row, 5, install_button)

    def search_error(self, error):
        """Handle search error"""
        self.search_progress.setVisible(False)
        self.search_button.setEnabled(True)
        self.statusBar().showMessage("Search failed")
        QMessageBox.critical(self, "Search Error", f"Search failed: {error}")

    # ==================== Install/Uninstall Functions ====================

    def install_package(self, package):
        """Install a package"""
        confirm = QMessageBox.question(
            self, "Install Package",
            f"Install {package['name']} from {package['source_repo']}?",
            QMessageBox.Yes | QMessageBox.No
        )

        if confirm != QMessageBox.Yes:
            return

        # Get appropriate backend
        backend = self.get_backend(package['package_type'])
        if not backend:
            QMessageBox.critical(self, "Error", "Package manager not available")
            return

        # Log installation attempt
        self.logger.log_install_attempt(package['package_name'], package['package_type'])

        # Show progress
        self.statusBar().showMessage(f"Installing {package['name']}...")

        # Start install worker
        self.install_worker = InstallWorker(backend, package['package_name'])
        self.install_worker.finished.connect(
            lambda success, msg, p=package: self.install_finished(success, msg, p)
        )
        self.install_worker.start()

    def install_finished(self, success, message, package):
        """Handle installation completion"""
        if success:
            self.logger.log_install_success(package['package_name'], package['package_type'])

            # Add to database
            self.db.add_installed_app(
                package['name'],
                package['package_name'],
                package['package_type'],
                package['source_repo'],
                package.get('version', ''),
                package.get('description', '')
            )

            # Add to database logs
            self.db.add_log(
                'install',
                'success',
                package['package_name'],
                package['package_type'],
                f"Installed {package['name']}"
            )

            self.statusBar().showMessage(f"Successfully installed {package['name']}")

            # Refresh search results
            if hasattr(self, 'search_results'):
                self.search_packages()
        else:
            self.logger.log_install_failure(package['package_name'], package['package_type'], message)

            self.db.add_log(
                'install',
                'failed',
                package['package_name'],
                package['package_type'],
                message
            )

            self.statusBar().showMessage("Installation failed")

    def uninstall_package(self, package):
        """Uninstall a package"""
        confirm = QMessageBox.question(
            self, "Uninstall Package",
            f"Are you sure you want to uninstall {package['name']} ({package['package_type'].upper()})?",
            QMessageBox.Yes | QMessageBox.No
        )

        if confirm != QMessageBox.Yes:
            return

        # Get appropriate backend
        backend = self.get_backend(package['package_type'])
        if not backend:
            QMessageBox.critical(self, "Error", "Package manager not available")
            return

        # Log uninstallation attempt
        self.logger.log_uninstall_attempt(package['package_name'], package['package_type'])

        # Show progress
        self.statusBar().showMessage(f"Uninstalling {package['name']}...")

        # Start uninstall worker
        self.uninstall_worker = UninstallWorker(backend, package['package_name'])
        self.uninstall_worker.finished.connect(
            lambda success, msg, p=package: self.uninstall_finished(success, msg, p)
        )
        self.uninstall_worker.start()

    def uninstall_finished(self, success, message, package):
        """Handle uninstallation completion"""
        if success:
            self.logger.log_uninstall_success(package['package_name'], package['package_type'])

            # Remove from database
            self.db.remove_installed_app(package['package_name'])

            # Add to database logs
            self.db.add_log(
                'uninstall',
                'success',
                package['package_name'],
                package['package_type'],
                f"Uninstalled {package['name']}"
            )

            self.statusBar().showMessage(f"Successfully uninstalled {package['name']}")

            # Refresh installed apps
            self.load_installed_apps()
        else:
            self.logger.log_uninstall_failure(package['package_name'], package['package_type'], message)

            self.db.add_log(
                'uninstall',
                'failed',
                package['package_name'],
                package['package_type'],
                message
            )

            self.statusBar().showMessage("Uninstallation failed")

    # ==================== Installed Apps Functions ====================

    def load_installed_apps(self):
        """Load installed applications"""
        self.statusBar().showMessage("Loading installed applications...")

        try:
            # Get installed apps from database and package managers
            db_apps = self.db.get_installed_apps()

            # Also query package managers directly
            all_apps = list(db_apps)

            prefs = self.db.get_preferences()

            if prefs['repo_apt_enabled'] and self.apt_backend.is_available():
                try:
                    apt_apps = self.apt_backend.get_installed_packages()
                    # Add apps not in database
                    for app in apt_apps:
                        if not any(a['package_name'] == app['package_name'] for a in all_apps):
                            all_apps.append(app)
                except Exception as e:
                    self.logger.log_error(f"Error loading APT packages: {str(e)}")

            if prefs['repo_snapd_enabled'] and self.snap_backend.is_available():
                try:
                    snap_apps = self.snap_backend.get_installed_packages()
                    for app in snap_apps:
                        if not any(a['package_name'] == app['package_name'] for a in all_apps):
                            all_apps.append(app)
                except Exception as e:
                    self.logger.log_error(f"Error loading Snap packages: {str(e)}")

            if prefs['repo_flathub_enabled'] and self.flatpak_backend.is_available():
                try:
                    flatpak_apps = self.flatpak_backend.get_installed_packages()
                    for app in flatpak_apps:
                        if not any(a['package_name'] == app['package_name'] for a in all_apps):
                            all_apps.append(app)
                except Exception as e:
                    self.logger.log_error(f"Error loading Flatpak packages: {str(e)}")

            self.installed_apps = all_apps
            self.filter_installed()

            self.statusBar().showMessage(f"Loaded {len(all_apps)} installed applications")

        except Exception as e:
            self.logger.log_error(f"Error loading installed apps: {str(e)}")
            QMessageBox.critical(self, "Error", f"Failed to load installed apps: {str(e)}")
            self.statusBar().showMessage("Failed to load installed applications")

    def filter_installed(self):
        """Filter installed apps by type"""
        filter_text = self.installed_filter.currentText()

        # Clear table
        self.installed_table.setRowCount(0)

        # Filter apps
        filtered = self.installed_apps if hasattr(self, 'installed_apps') else []

        if filter_text != "All":
            filter_type = filter_text.lower()
            filtered = [a for a in filtered if a['package_type'] == filter_type]

        # Populate table
        for app in filtered:
            row = self.installed_table.rowCount()
            self.installed_table.insertRow(row)

            self.installed_table.setItem(row, 0, QTableWidgetItem(app['name']))
            self.installed_table.setItem(row, 1, QTableWidgetItem(app['package_name']))
            self.installed_table.setItem(row, 2, QTableWidgetItem(app.get('version', '')))
            self.installed_table.setItem(row, 3, QTableWidgetItem(app['package_type'].upper()))
            self.installed_table.setItem(row, 4, QTableWidgetItem(app['source_repo']))

            # Uninstall button
            uninstall_button = QPushButton("Uninstall")
            uninstall_button.clicked.connect(lambda checked, a=app: self.uninstall_package(a))
            self.installed_table.setCellWidget(row, 5, uninstall_button)

    # ==================== Settings Functions ====================

    def load_preferences(self):
        """Load preferences from database"""
        prefs = self.db.get_preferences()

        self.apt_checkbox.setChecked(prefs['repo_apt_enabled'])
        self.snap_checkbox.setChecked(prefs['repo_snapd_enabled'])
        self.flatpak_checkbox.setChecked(prefs['repo_flathub_enabled'])
        self.notifications_checkbox.setChecked(prefs['notifications_enabled'])
        self.auto_save_checkbox.setChecked(prefs['auto_save_metadata'])
        self.log_retention.setValue(prefs['log_retention_days'])

    def save_settings(self):
        """Save settings to database"""
        prefs = {
            'repo_apt_enabled': self.apt_checkbox.isChecked(),
            'repo_snapd_enabled': self.snap_checkbox.isChecked(),
            'repo_flathub_enabled': self.flatpak_checkbox.isChecked(),
            'notifications_enabled': self.notifications_checkbox.isChecked(),
            'auto_save_metadata': self.auto_save_checkbox.isChecked(),
            'log_retention_days': self.log_retention.value()
        }

        self.db.update_preferences(prefs)
        self.logger.log_info("Settings saved")

        QMessageBox.information(self, "Settings", "Settings saved successfully")

    def reset_settings(self):
        """Reset settings to defaults"""
        confirm = QMessageBox.question(
            self, "Reset Settings",
            "Reset all settings to defaults?",
            QMessageBox.Yes | QMessageBox.No
        )

        if confirm == QMessageBox.Yes:
            self.apt_checkbox.setChecked(True)
            self.snap_checkbox.setChecked(True)
            self.flatpak_checkbox.setChecked(True)
            self.notifications_checkbox.setChecked(True)
            self.auto_save_checkbox.setChecked(True)
            self.log_retention.setValue(30)

            self.save_settings()

    # ==================== Logs Functions ====================

    def load_logs(self):
        """Load and display logs"""
        logs = self.db.get_logs(limit=100)

        log_text = ""
        for log in logs:
            log_text += f"[{log['timestamp']}] {log['action_type'].upper()}\n"
            log_text += f"  Package: {log['package_name']} ({log['package_type']})\n"
            log_text += f"  Status: {log['status']}\n"
            if log['message']:
                log_text += f"  Message: {log['message']}\n"
            if log['error_details']:
                log_text += f"  Error: {log['error_details']}\n"
            log_text += "-" * 80 + "\n\n"

        self.logs_text.setPlainText(log_text)

    def export_logs(self):
        """Export logs to file"""
        filepath, _ = QFileDialog.getSaveFileName(
            self, "Export Logs",
            "upm_logs.txt",
            "Text Files (*.txt)"
        )

        if filepath:
            try:
                self.db.export_logs(filepath, days=self.log_retention.value())
                QMessageBox.information(self, "Export Logs", f"Logs exported to {filepath}")
            except Exception as e:
                QMessageBox.critical(self, "Export Error", f"Failed to export logs: {str(e)}")

    def clear_logs(self):
        """Clear logs"""
        confirm = QMessageBox.question(
            self, "Clear Logs",
            "Clear all logs from the database?",
            QMessageBox.Yes | QMessageBox.No
        )

        if confirm == QMessageBox.Yes:
            deleted = self.db.cleanup_old_logs()
            self.logger.log_info(f"Cleared {deleted} log entries")
            self.load_logs()
            QMessageBox.information(self, "Clear Logs", f"Cleared {deleted} log entries")

    # ==================== Helper Functions ====================

    def get_backend(self, package_type):
        """Get the appropriate backend for a package type"""
        if package_type == 'apt':
            return self.apt_backend
        elif package_type == 'snap':
            return self.snap_backend
        elif package_type == 'flatpak':
            return self.flatpak_backend
        return None

    def check_system_status(self):
        """Check system status and warn about missing components"""
        warnings = []

        if not self.apt_backend.is_available():
            warnings.append("APT is not available")

        if not self.snap_backend.is_available():
            warnings.append("Snap is not installed")
        elif not self.snap_backend.is_snapd_running():
            warnings.append("Snapd service is not running")

        if not self.flatpak_backend.is_available():
            warnings.append("Flatpak is not installed")
        elif not self.flatpak_backend.is_flathub_configured():
            warnings.append("Flathub repository is not configured")

        if warnings:
            self.logger.log_system_warning("System check: " + "; ".join(warnings))

    def show_notification(self, title, message, notification_type):
        """Show notification to user"""
        prefs = self.db.get_preferences()
        if not prefs['notifications_enabled']:
            return

        if notification_type == "success":
            QMessageBox.information(self, title, message)
        elif notification_type == "error":
            QMessageBox.critical(self, title, message)
        elif notification_type == "warning":
            QMessageBox.warning(self, title, message)
        else:
            QMessageBox.information(self, title, message)

    def closeEvent(self, event):
        """Handle application close"""
        self.db.close()
        event.accept()


def main():
    """Main entry point"""
    app = QApplication(sys.argv)
    app.setApplicationName("Unified Package Manager")
    app.setOrganizationName("Zorin OS")

    window = UnifiedPackageManager()
    window.show()

    # Load installed apps on startup (deferred to allow UI to render first)
    QTimer.singleShot(0, window.load_installed_apps)

    sys.exit(app.exec_())


if __name__ == '__main__':
    main()
