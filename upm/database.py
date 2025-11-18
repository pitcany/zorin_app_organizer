import sqlite3
from pathlib import Path
from datetime import datetime
from typing import Optional
from upm.models import ActionLog


class Database:
    """SQLite database manager for UPM"""

    def __init__(self, db_path: str):
        """Initialize database connection and create tables"""
        self.db_path = db_path
        Path(db_path).parent.mkdir(parents=True, exist_ok=True)
        self.conn = sqlite3.connect(db_path)
        self.conn.row_factory = sqlite3.Row
        self._create_tables()
        self._create_indexes()
        self._init_preferences()

    def _create_tables(self):
        """Create database tables"""
        cursor = self.conn.cursor()

        # action_log table
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS action_log (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp DATETIME NOT NULL,
                action_type TEXT NOT NULL,
                package_name TEXT NOT NULL,
                package_manager TEXT NOT NULL,
                version TEXT,
                success BOOLEAN NOT NULL,
                error_message TEXT,
                user_note TEXT
            )
        """)

        # package_snapshots table
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS package_snapshots (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                snapshot_timestamp DATETIME NOT NULL,
                package_name TEXT NOT NULL,
                package_manager TEXT NOT NULL,
                version TEXT NOT NULL,
                install_date DATETIME
            )
        """)

        # user_prefs table
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS user_prefs (
                key TEXT PRIMARY KEY,
                value TEXT NOT NULL
            )
        """)

        # api_cache table
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS api_cache (
                cache_key TEXT PRIMARY KEY,
                response_data TEXT NOT NULL,
                cached_at DATETIME NOT NULL,
                expires_at DATETIME NOT NULL
            )
        """)

        self.conn.commit()

    def _create_indexes(self):
        """Create database indexes for performance"""
        cursor = self.conn.cursor()

        cursor.execute("""
            CREATE INDEX IF NOT EXISTS idx_action_log_timestamp
            ON action_log(timestamp)
        """)

        cursor.execute("""
            CREATE INDEX IF NOT EXISTS idx_action_log_package
            ON action_log(package_name)
        """)

        cursor.execute("""
            CREATE INDEX IF NOT EXISTS idx_snapshots_timestamp
            ON package_snapshots(snapshot_timestamp)
        """)

        cursor.execute("""
            CREATE INDEX IF NOT EXISTS idx_cache_expires
            ON api_cache(expires_at)
        """)

        self.conn.commit()

    def _init_preferences(self):
        """Initialize default preferences if not exists"""
        defaults = {
            "flathub_enabled": "true",
            "snap_enabled": "true",
            "ppa_enabled": "true",
            "show_success_notifications": "true",
            "show_error_notifications": "true",
            "log_retention_days": "30"
        }

        cursor = self.conn.cursor()
        for key, value in defaults.items():
            cursor.execute("""
                INSERT OR IGNORE INTO user_prefs (key, value)
                VALUES (?, ?)
            """, (key, value))

        self.conn.commit()

    def log_action(self, action: ActionLog):
        """Log a package operation"""
        cursor = self.conn.cursor()
        cursor.execute("""
            INSERT INTO action_log
            (timestamp, action_type, package_name, package_manager, version, success, error_message, user_note)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            action.timestamp,
            action.action_type,
            action.package_name,
            action.package_manager,
            action.version,
            action.success,
            action.error_message,
            action.user_note
        ))
        self.conn.commit()

    def close(self):
        """Close database connection"""
        self.conn.close()
