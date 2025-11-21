"""
Tests for database backup functionality
"""

import pytest
import os
import tempfile
import sqlite3
from pathlib import Path
from backup import DatabaseBackup


@pytest.fixture
def temp_db():
    """Create a temporary test database"""
    fd, db_path = tempfile.mkstemp(suffix='.db')
    os.close(fd)

    # Create a simple test database
    conn = sqlite3.connect(db_path)
    conn.execute("CREATE TABLE test_table (id INTEGER PRIMARY KEY, data TEXT)")
    conn.execute("INSERT INTO test_table (data) VALUES ('test data 1')")
    conn.execute("INSERT INTO test_table (data) VALUES ('test data 2')")
    conn.commit()
    conn.close()

    yield db_path

    # Cleanup
    try:
        os.unlink(db_path)
    except:
        pass


@pytest.fixture
def temp_backup_dir():
    """Create a temporary backup directory"""
    backup_dir = tempfile.mkdtemp()
    yield backup_dir

    # Cleanup
    import shutil
    try:
        shutil.rmtree(backup_dir)
    except:
        pass


class TestDatabaseBackup:
    """Tests for DatabaseBackup class"""

    @pytest.mark.unit
    def test_create_backup(self, temp_db, temp_backup_dir):
        """Test creating a database backup"""
        backup_mgr = DatabaseBackup(db_path=temp_db, backup_dir=temp_backup_dir)

        backup_file = backup_mgr.create_backup(backup_type='manual', compress=False)

        assert backup_file is not None
        assert os.path.exists(backup_file)
        assert backup_file.endswith('.db')

    @pytest.mark.unit
    def test_create_compressed_backup(self, temp_db, temp_backup_dir):
        """Test creating a compressed backup"""
        backup_mgr = DatabaseBackup(db_path=temp_db, backup_dir=temp_backup_dir)

        backup_file = backup_mgr.create_backup(backup_type='manual', compress=True)

        assert backup_file is not None
        assert os.path.exists(backup_file)
        assert backup_file.endswith('.gz')

    @pytest.mark.unit
    def test_backup_restore(self, temp_db, temp_backup_dir):
        """Test backup and restore cycle"""
        backup_mgr = DatabaseBackup(db_path=temp_db, backup_dir=temp_backup_dir)

        # Create backup
        backup_file = backup_mgr.create_backup(backup_type='manual', compress=False)
        assert backup_file is not None

        # Modify original database
        conn = sqlite3.connect(temp_db)
        conn.execute("INSERT INTO test_table (data) VALUES ('modified data')")
        conn.commit()
        conn.close()

        # Restore from backup
        result = backup_mgr.restore_backup(backup_file)
        assert result == True

        # Verify data was restored
        conn = sqlite3.connect(temp_db)
        cursor = conn.cursor()
        cursor.execute("SELECT COUNT(*) FROM test_table")
        count = cursor.fetchone()[0]
        conn.close()

        assert count == 2  # Original 2 rows, not 3

    @pytest.mark.unit
    def test_verify_backup(self, temp_db, temp_backup_dir):
        """Test backup verification"""
        backup_mgr = DatabaseBackup(db_path=temp_db, backup_dir=temp_backup_dir)

        backup_file = backup_mgr.create_backup(backup_type='manual', compress=False)
        assert backup_file is not None

        # Verify backup
        is_valid = backup_mgr.verify_backup(backup_file)
        assert is_valid == True

    @pytest.mark.unit
    def test_list_backups(self, temp_db, temp_backup_dir):
        """Test listing backups"""
        backup_mgr = DatabaseBackup(db_path=temp_db, backup_dir=temp_backup_dir)

        # Create multiple backups
        backup_mgr.create_backup(backup_type='daily', compress=False)
        backup_mgr.create_backup(backup_type='weekly', compress=False)

        # List backups
        backups = backup_mgr.list_backups()

        assert len(backups) >= 2
        assert any(b['type'] == 'daily' for b in backups)
        assert any(b['type'] == 'weekly' for b in backups)

    @pytest.mark.unit
    def test_backup_rotation(self, temp_db, temp_backup_dir):
        """Test backup rotation"""
        backup_mgr = DatabaseBackup(db_path=temp_db, backup_dir=temp_backup_dir)
        backup_mgr.max_daily_backups = 3  # Keep only 3 daily backups

        # Create 5 backups
        for i in range(5):
            backup_mgr.create_backup(backup_type='daily', compress=False)

        # Rotate backups
        backup_mgr.rotate_backups()

        # List backups
        backups = backup_mgr.list_backups()
        daily_backups = [b for b in backups if b['type'] == 'daily']

        # Should only have 3 daily backups
        assert len(daily_backups) <= 3


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
