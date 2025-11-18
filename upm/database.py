import sqlite3
from pathlib import Path
from datetime import datetime
from typing import Optional
from upm.models import ActionLog


class Database:
    """SQLite database manager for UPM

    Thread Safety:
        Database instances are NOT thread-safe. Each thread should create
        its own Database instance to avoid race conditions and connection issues.
    """

    def __init__(self, db_path: str):
        """Initialize database connection and create tables

        Args:
            db_path: Path to the SQLite database file

        Raises:
            RuntimeError: If database initialization fails due to SQLite or filesystem errors
        """
        try:
            self.db_path = db_path
            Path(db_path).parent.mkdir(parents=True, exist_ok=True)
            self.conn = sqlite3.connect(db_path)
            self.conn.row_factory = sqlite3.Row
            self._create_tables()
            self._create_indexes()
            self._init_preferences()
        except sqlite3.Error as e:
            raise RuntimeError(f"Failed to initialize database at {db_path}: {e}")
        except OSError as e:
            raise RuntimeError(f"Failed to create database directory for {db_path}: {e}")

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
        """Log a package operation

        Args:
            action: ActionLog instance containing operation details

        Raises:
            ValueError: If action_type is invalid or package_name is empty
            RuntimeError: If database operation fails
        """
        # Validate action_type
        valid_action_types = {'install', 'remove', 'sync'}
        if action.action_type not in valid_action_types:
            raise ValueError(
                f"Invalid action_type '{action.action_type}'. "
                f"Must be one of: {', '.join(sorted(valid_action_types))}"
            )

        # Validate package_name
        if not action.package_name or not action.package_name.strip():
            raise ValueError("package_name cannot be empty")

        try:
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
        except sqlite3.Error as e:
            self.conn.rollback()
            raise RuntimeError(f"Failed to log action for package '{action.package_name}': {e}")

    def close(self):
        """Close database connection"""
        self.conn.close()
