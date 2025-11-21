"""
Database backup module for Unified Package Manager
Provides automated backup functionality with rotation and compression
"""

import os
import shutil
import sqlite3
import gzip
import json
from datetime import datetime, timezone, timedelta
from typing import List, Dict, Optional
from pathlib import Path


class DatabaseBackup:
    """Manages automated database backups with rotation"""

    def __init__(self, db_path: str = "upm.db", backup_dir: str = None):
        """
        Initialize backup manager

        Args:
            db_path: Path to the SQLite database
            backup_dir: Directory for storing backups (default: ~/.local/share/upm/backups)
        """
        self.db_path = db_path

        # Default backup directory in user's home
        if backup_dir is None:
            home = Path.home()
            self.backup_dir = home / ".local" / "share" / "upm" / "backups"
        else:
            self.backup_dir = Path(backup_dir)

        # Create backup directory if it doesn't exist
        self.backup_dir.mkdir(parents=True, exist_ok=True)

        # Backup configuration
        self.max_daily_backups = 7      # Keep 7 daily backups
        self.max_weekly_backups = 4     # Keep 4 weekly backups
        self.max_monthly_backups = 6    # Keep 6 monthly backups
        self.compress_backups = True    # Compress backups by default

    def create_backup(self, backup_type: str = "daily", compress: bool = None) -> Optional[str]:
        """
        Create a database backup

        Args:
            backup_type: Type of backup (daily, weekly, monthly, manual)
            compress: Whether to compress the backup (default: self.compress_backups)

        Returns:
            Path to the created backup file, or None if failed
        """
        if compress is None:
            compress = self.compress_backups

        try:
            # Generate backup filename with timestamp
            timestamp = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S")
            backup_name = f"upm_backup_{backup_type}_{timestamp}"

            # Backup path
            if compress:
                backup_file = self.backup_dir / f"{backup_name}.db.gz"
            else:
                backup_file = self.backup_dir / f"{backup_name}.db"

            # Perform SQLite online backup (works even if database is in use)
            source_conn = sqlite3.connect(self.db_path)
            backup_conn = sqlite3.connect(str(backup_file).replace('.gz', '') if compress else str(backup_file))

            # Copy database using backup API
            source_conn.backup(backup_conn)

            source_conn.close()
            backup_conn.close()

            # Compress if requested
            if compress:
                uncompressed_path = str(backup_file).replace('.gz', '')
                with open(uncompressed_path, 'rb') as f_in:
                    with gzip.open(str(backup_file), 'wb') as f_out:
                        shutil.copyfileobj(f_in, f_out)
                # Remove uncompressed file
                os.remove(uncompressed_path)

            # Create metadata file
            metadata = {
                'backup_type': backup_type,
                'created_at': datetime.now(timezone.utc).isoformat(),
                'original_db': self.db_path,
                'compressed': compress,
                'size_bytes': os.path.getsize(backup_file)
            }

            metadata_file = backup_file.with_suffix('.json')
            with open(metadata_file, 'w') as f:
                json.dump(metadata, f, indent=2)

            print(f"✅ Backup created: {backup_file}")
            print(f"   Size: {os.path.getsize(backup_file) / 1024:.2f} KB")

            return str(backup_file)

        except Exception as e:
            print(f"❌ Backup failed: {e}")
            return None

    def restore_backup(self, backup_file: str, target_db: str = None) -> bool:
        """
        Restore database from a backup

        Args:
            backup_file: Path to the backup file
            target_db: Target database path (default: self.db_path)

        Returns:
            True if successful, False otherwise
        """
        if target_db is None:
            target_db = self.db_path

        try:
            backup_path = Path(backup_file)

            if not backup_path.exists():
                print(f"❌ Backup file not found: {backup_file}")
                return False

            # Create backup of current database before restoring
            current_backup = f"{target_db}.pre-restore-{datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S')}"
            if os.path.exists(target_db):
                shutil.copy2(target_db, current_backup)
                print(f"📦 Current database backed up to: {current_backup}")

            # Decompress if needed
            if backup_file.endswith('.gz'):
                with gzip.open(backup_file, 'rb') as f_in:
                    with open(target_db, 'wb') as f_out:
                        shutil.copyfileobj(f_in, f_out)
            else:
                shutil.copy2(backup_file, target_db)

            print(f"✅ Database restored from: {backup_file}")
            return True

        except Exception as e:
            print(f"❌ Restore failed: {e}")
            # Restore the pre-restore backup if it exists
            if 'current_backup' in locals() and os.path.exists(current_backup):
                shutil.copy2(current_backup, target_db)
                print(f"⚠️  Rolled back to pre-restore state")
            return False

    def rotate_backups(self):
        """
        Rotate backups according to retention policy
        Keeps: 7 daily, 4 weekly, 6 monthly backups
        """
        try:
            # Get all backup files
            backups = self._get_all_backups()

            # Categorize by type
            daily_backups = [b for b in backups if b['type'] == 'daily']
            weekly_backups = [b for b in backups if b['type'] == 'weekly']
            monthly_backups = [b for b in backups if b['type'] == 'monthly']

            # Rotate each type
            self._rotate_backup_type(daily_backups, self.max_daily_backups)
            self._rotate_backup_type(weekly_backups, self.max_weekly_backups)
            self._rotate_backup_type(monthly_backups, self.max_monthly_backups)

        except Exception as e:
            print(f"❌ Backup rotation failed: {e}")

    def _get_all_backups(self) -> List[Dict]:
        """Get list of all backups with metadata"""
        backups = []

        for backup_file in self.backup_dir.glob("upm_backup_*.db*"):
            # Skip metadata files
            if backup_file.suffix == '.json':
                continue

            # Get metadata if available
            metadata_file = backup_file.with_suffix('.json')
            if metadata_file.exists():
                with open(metadata_file) as f:
                    metadata = json.load(f)
            else:
                # Parse from filename
                parts = backup_file.stem.replace('.db', '').split('_')
                metadata = {
                    'backup_type': parts[2] if len(parts) > 2 else 'unknown',
                    'created_at': datetime.fromtimestamp(backup_file.stat().st_mtime, tz=timezone.utc).isoformat()
                }

            backups.append({
                'path': str(backup_file),
                'type': metadata['backup_type'],
                'created_at': datetime.fromisoformat(metadata['created_at']),
                'size': backup_file.stat().st_size
            })

        # Sort by creation date, newest first
        backups.sort(key=lambda x: x['created_at'], reverse=True)
        return backups

    def _rotate_backup_type(self, backups: List[Dict], max_keep: int):
        """Remove old backups of a specific type"""
        if len(backups) <= max_keep:
            return

        # Keep only the newest max_keep backups
        to_delete = backups[max_keep:]

        for backup in to_delete:
            try:
                # Delete backup file
                os.remove(backup['path'])

                # Delete metadata file if exists
                metadata_file = Path(backup['path']).with_suffix('.json')
                if metadata_file.exists():
                    os.remove(metadata_file)

                print(f"🗑️  Deleted old backup: {backup['path']}")
            except Exception as e:
                print(f"⚠️  Failed to delete {backup['path']}: {e}")

    def list_backups(self) -> List[Dict]:
        """
        List all available backups

        Returns:
            List of backup information dictionaries
        """
        backups = self._get_all_backups()

        print("\n📋 Available Backups:")
        print("=" * 80)

        if not backups:
            print("No backups found.")
            return []

        for idx, backup in enumerate(backups, 1):
            created = backup['created_at'].strftime("%Y-%m-%d %H:%M:%S UTC")
            size_kb = backup['size'] / 1024
            print(f"{idx}. [{backup['type']:8s}] {created} - {size_kb:8.2f} KB")
            print(f"   Path: {backup['path']}")

        print("=" * 80)
        return backups

    def verify_backup(self, backup_file: str) -> bool:
        """
        Verify that a backup file is a valid SQLite database

        Args:
            backup_file: Path to the backup file

        Returns:
            True if valid, False otherwise
        """
        try:
            # Decompress if needed
            if backup_file.endswith('.gz'):
                import tempfile
                with tempfile.NamedTemporaryFile(delete=False) as tmp:
                    with gzip.open(backup_file, 'rb') as f_in:
                        shutil.copyfileobj(f_in, tmp)
                    tmp_path = tmp.name

                test_file = tmp_path
            else:
                test_file = backup_file

            # Try to open as SQLite database
            conn = sqlite3.connect(test_file)
            cursor = conn.cursor()

            # Try a simple query
            cursor.execute("SELECT name FROM sqlite_master WHERE type='table'")
            tables = cursor.fetchall()

            conn.close()

            # Clean up temp file if created
            if backup_file.endswith('.gz'):
                os.remove(test_file)

            print(f"✅ Backup verified: {len(tables)} tables found")
            return True

        except Exception as e:
            print(f"❌ Backup verification failed: {e}")
            return False

    def auto_backup(self) -> Optional[str]:
        """
        Automatically determine backup type and create backup

        Returns:
            Path to created backup, or None if failed
        """
        now = datetime.now(timezone.utc)

        # Determine backup type based on day
        if now.day == 1:
            # First day of month - monthly backup
            backup_type = "monthly"
        elif now.weekday() == 0:  # Monday
            # Monday - weekly backup
            backup_type = "weekly"
        else:
            # Default - daily backup
            backup_type = "daily"

        # Create backup
        backup_path = self.create_backup(backup_type=backup_type)

        # Rotate old backups
        if backup_path:
            self.rotate_backups()

        return backup_path


def main():
    """Command-line interface for backup management"""
    import argparse

    parser = argparse.ArgumentParser(description="UPM Database Backup Manager")
    parser.add_argument('--db', default='upm.db', help='Database file path')
    parser.add_argument('--backup-dir', help='Backup directory')

    subparsers = parser.add_subparsers(dest='command', help='Commands')

    # Backup command
    backup_parser = subparsers.add_parser('backup', help='Create a backup')
    backup_parser.add_argument('--type', choices=['daily', 'weekly', 'monthly', 'manual'],
                               default='manual', help='Backup type')
    backup_parser.add_argument('--no-compress', action='store_true', help='Do not compress backup')

    # Restore command
    restore_parser = subparsers.add_parser('restore', help='Restore from backup')
    restore_parser.add_argument('backup_file', help='Backup file to restore')
    restore_parser.add_argument('--target', help='Target database file')

    # List command
    subparsers.add_parser('list', help='List all backups')

    # Verify command
    verify_parser = subparsers.add_parser('verify', help='Verify a backup')
    verify_parser.add_argument('backup_file', help='Backup file to verify')

    # Auto backup command
    subparsers.add_parser('auto', help='Automatic backup with rotation')

    args = parser.parse_args()

    # Create backup manager
    backup_mgr = DatabaseBackup(db_path=args.db, backup_dir=args.backup_dir)

    # Execute command
    if args.command == 'backup':
        backup_mgr.create_backup(
            backup_type=args.type,
            compress=not args.no_compress
        )

    elif args.command == 'restore':
        backup_mgr.restore_backup(
            backup_file=args.backup_file,
            target_db=args.target
        )

    elif args.command == 'list':
        backup_mgr.list_backups()

    elif args.command == 'verify':
        backup_mgr.verify_backup(args.backup_file)

    elif args.command == 'auto':
        backup_mgr.auto_backup()

    else:
        parser.print_help()


if __name__ == '__main__':
    main()
