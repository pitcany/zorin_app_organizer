"""
Database management module for Unified Package Manager
Handles SQLite database operations for tracking apps, preferences, and logs
"""

import sqlite3
import os
from datetime import datetime
from typing import List, Dict, Optional, Tuple


class DatabaseManager:
    """Manages SQLite database for UPM"""

    def __init__(self, db_path: str = "upm.db"):
        """Initialize database connection and create tables if they don't exist"""
        self.db_path = db_path
        self.conn = None
        self.cursor = None
        self.connect()
        self.create_tables()

    def connect(self):
        """Establish database connection"""
        self.conn = sqlite3.connect(self.db_path)
        self.cursor = self.conn.cursor()

    def create_tables(self):
        """Create database tables if they don't exist"""
        # Installed apps table
        self.cursor.execute("""
            CREATE TABLE IF NOT EXISTS installed_apps (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                package_name TEXT NOT NULL UNIQUE,
                package_type TEXT NOT NULL,
                source_repo TEXT NOT NULL,
                version TEXT,
                description TEXT,
                install_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        """)

        # User preferences table
        self.cursor.execute("""
            CREATE TABLE IF NOT EXISTS user_prefs (
                id INTEGER PRIMARY KEY,
                repo_flathub_enabled BOOLEAN DEFAULT TRUE,
                repo_snapd_enabled BOOLEAN DEFAULT TRUE,
                repo_apt_enabled BOOLEAN DEFAULT TRUE,
                notifications_enabled BOOLEAN DEFAULT TRUE,
                log_retention_days INTEGER DEFAULT 30,
                auto_save_metadata BOOLEAN DEFAULT TRUE
            )
        """)

        # Initialize default preferences if not exist
        self.cursor.execute("SELECT COUNT(*) FROM user_prefs")
        if self.cursor.fetchone()[0] == 0:
            self.cursor.execute("""
                INSERT INTO user_prefs (id) VALUES (1)
            """)

        # Logs table
        self.cursor.execute("""
            CREATE TABLE IF NOT EXISTS logs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                action_type TEXT NOT NULL,
                package_name TEXT,
                package_type TEXT,
                status TEXT NOT NULL,
                message TEXT,
                error_details TEXT
            )
        """)

        # Custom APT repositories table
        self.cursor.execute("""
            CREATE TABLE IF NOT EXISTS custom_repositories (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                repo_name TEXT NOT NULL,
                repo_url TEXT NOT NULL UNIQUE,
                repo_type TEXT NOT NULL,
                enabled BOOLEAN DEFAULT TRUE,
                added_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        """)

        self.conn.commit()

    # ==================== Installed Apps Methods ====================

    def add_installed_app(self, name: str, package_name: str, package_type: str,
                         source_repo: str, version: str = "", description: str = ""):
        """Add a newly installed app to the database"""
        try:
            self.cursor.execute("""
                INSERT INTO installed_apps (name, package_name, package_type, source_repo, version, description)
                VALUES (?, ?, ?, ?, ?, ?)
            """, (name, package_name, package_type, source_repo, version, description))
            self.conn.commit()
            return True
        except sqlite3.IntegrityError:
            # App already exists
            return False

    def remove_installed_app(self, package_name: str):
        """Remove an app from the installed apps list"""
        self.cursor.execute("DELETE FROM installed_apps WHERE package_name = ?", (package_name,))
        self.conn.commit()

    def get_installed_apps(self, package_type: Optional[str] = None) -> List[Dict]:
        """Get list of installed apps, optionally filtered by package type"""
        if package_type:
            self.cursor.execute("""
                SELECT name, package_name, package_type, source_repo, version, description, install_date
                FROM installed_apps
                WHERE package_type = ?
                ORDER BY name
            """, (package_type,))
        else:
            self.cursor.execute("""
                SELECT name, package_name, package_type, source_repo, version, description, install_date
                FROM installed_apps
                ORDER BY name
            """)

        apps = []
        for row in self.cursor.fetchall():
            apps.append({
                'name': row[0],
                'package_name': row[1],
                'package_type': row[2],
                'source_repo': row[3],
                'version': row[4],
                'description': row[5],
                'install_date': row[6]
            })
        return apps

    def is_app_installed(self, package_name: str) -> bool:
        """Check if an app is already installed"""
        self.cursor.execute("SELECT COUNT(*) FROM installed_apps WHERE package_name = ?", (package_name,))
        return self.cursor.fetchone()[0] > 0

    # ==================== User Preferences Methods ====================

    def get_preferences(self) -> Dict:
        """Get user preferences"""
        self.cursor.execute("""
            SELECT repo_flathub_enabled, repo_snapd_enabled, repo_apt_enabled,
                   notifications_enabled, log_retention_days, auto_save_metadata
            FROM user_prefs WHERE id = 1
        """)
        row = self.cursor.fetchone()
        return {
            'repo_flathub_enabled': bool(row[0]),
            'repo_snapd_enabled': bool(row[1]),
            'repo_apt_enabled': bool(row[2]),
            'notifications_enabled': bool(row[3]),
            'log_retention_days': row[4],
            'auto_save_metadata': bool(row[5])
        }

    def update_preference(self, key: str, value):
        """Update a specific preference"""
        query = f"UPDATE user_prefs SET {key} = ? WHERE id = 1"
        self.cursor.execute(query, (value,))
        self.conn.commit()

    def update_preferences(self, prefs: Dict):
        """Update multiple preferences at once"""
        for key, value in prefs.items():
            self.update_preference(key, value)

    # ==================== Logging Methods ====================

    def add_log(self, action_type: str, status: str, package_name: str = "",
                package_type: str = "", message: str = "", error_details: str = ""):
        """Add a log entry"""
        self.cursor.execute("""
            INSERT INTO logs (action_type, package_name, package_type, status, message, error_details)
            VALUES (?, ?, ?, ?, ?, ?)
        """, (action_type, package_name, package_type, status, message, error_details))
        self.conn.commit()

    def get_logs(self, limit: int = 100, action_type: Optional[str] = None) -> List[Dict]:
        """Get log entries, optionally filtered by action type"""
        if action_type:
            self.cursor.execute("""
                SELECT timestamp, action_type, package_name, package_type, status, message, error_details
                FROM logs
                WHERE action_type = ?
                ORDER BY timestamp DESC
                LIMIT ?
            """, (action_type, limit))
        else:
            self.cursor.execute("""
                SELECT timestamp, action_type, package_name, package_type, status, message, error_details
                FROM logs
                ORDER BY timestamp DESC
                LIMIT ?
            """, (limit,))

        logs = []
        for row in self.cursor.fetchall():
            logs.append({
                'timestamp': row[0],
                'action_type': row[1],
                'package_name': row[2],
                'package_type': row[3],
                'status': row[4],
                'message': row[5],
                'error_details': row[6]
            })
        return logs

    def export_logs(self, filepath: str, days: int = 7):
        """Export logs to a text file"""
        from datetime import timedelta

        cutoff_date = datetime.now() - timedelta(days=days)
        self.cursor.execute("""
            SELECT timestamp, action_type, package_name, package_type, status, message, error_details
            FROM logs
            WHERE timestamp >= ?
            ORDER BY timestamp DESC
        """, (cutoff_date,))

        with open(filepath, 'w') as f:
            f.write(f"Unified Package Manager - Logs Export\n")
            f.write(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"Period: Last {days} days\n")
            f.write("=" * 80 + "\n\n")

            for row in self.cursor.fetchall():
                timestamp, action_type, package_name, package_type, status, message, error_details = row
                f.write(f"[{timestamp}] {action_type.upper()}\n")
                f.write(f"  Package: {package_name} ({package_type})\n")
                f.write(f"  Status: {status}\n")
                if message:
                    f.write(f"  Message: {message}\n")
                if error_details:
                    f.write(f"  Error: {error_details}\n")
                f.write("-" * 80 + "\n\n")

    def cleanup_old_logs(self):
        """Remove logs older than the retention period"""
        prefs = self.get_preferences()
        retention_days = prefs['log_retention_days']

        from datetime import timedelta
        cutoff_date = datetime.now() - timedelta(days=retention_days)

        self.cursor.execute("DELETE FROM logs WHERE timestamp < ?", (cutoff_date,))
        deleted = self.cursor.rowcount
        self.conn.commit()
        return deleted

    # ==================== Custom Repositories Methods ====================

    def add_custom_repository(self, repo_name: str, repo_url: str, repo_type: str):
        """Add a custom repository (e.g., PPA)"""
        try:
            self.cursor.execute("""
                INSERT INTO custom_repositories (repo_name, repo_url, repo_type)
                VALUES (?, ?, ?)
            """, (repo_name, repo_url, repo_type))
            self.conn.commit()
            return True
        except sqlite3.IntegrityError:
            return False

    def remove_custom_repository(self, repo_id: int):
        """Remove a custom repository"""
        self.cursor.execute("DELETE FROM custom_repositories WHERE id = ?", (repo_id,))
        self.conn.commit()

    def get_custom_repositories(self, repo_type: Optional[str] = None) -> List[Dict]:
        """Get list of custom repositories"""
        if repo_type:
            self.cursor.execute("""
                SELECT id, repo_name, repo_url, repo_type, enabled, added_date
                FROM custom_repositories
                WHERE repo_type = ?
                ORDER BY repo_name
            """, (repo_type,))
        else:
            self.cursor.execute("""
                SELECT id, repo_name, repo_url, repo_type, enabled, added_date
                FROM custom_repositories
                ORDER BY repo_name
            """)

        repos = []
        for row in self.cursor.fetchall():
            repos.append({
                'id': row[0],
                'name': row[1],
                'url': row[2],
                'type': row[3],
                'enabled': bool(row[4]),
                'added_date': row[5]
            })
        return repos

    def toggle_repository(self, repo_id: int, enabled: bool):
        """Enable or disable a custom repository"""
        self.cursor.execute("UPDATE custom_repositories SET enabled = ? WHERE id = ?", (enabled, repo_id))
        self.conn.commit()

    # ==================== Utility Methods ====================

    def close(self):
        """Close database connection"""
        if self.conn:
            self.conn.close()

    def __enter__(self):
        """Context manager entry"""
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit"""
        self.close()
