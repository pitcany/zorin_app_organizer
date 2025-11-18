import pytest
import tempfile
import os
from pathlib import Path
from datetime import datetime
from upm.database import Database
from upm.models import ActionLog


@pytest.fixture
def temp_db():
    """Create a temporary database for testing"""
    fd, path = tempfile.mkstemp(suffix=".db")
    os.close(fd)
    db = Database(path)
    yield db
    db.close()
    os.unlink(path)


def test_database_initialization(temp_db):
    """Test that database tables are created"""
    # Check that we can query tables without error
    cursor = temp_db.conn.cursor()

    # Check action_log table exists
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='action_log'")
    assert cursor.fetchone() is not None

    # Check package_snapshots table exists
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='package_snapshots'")
    assert cursor.fetchone() is not None

    # Check user_prefs table exists
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='user_prefs'")
    assert cursor.fetchone() is not None

    # Check api_cache table exists
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='api_cache'")
    assert cursor.fetchone() is not None


def test_log_action(temp_db):
    """Test logging an action"""
    log = ActionLog(
        timestamp=datetime.now(),
        action_type="install",
        package_name="firefox",
        package_manager="apt",
        version="120.0",
        success=True
    )

    temp_db.log_action(log)

    # Verify it was saved
    cursor = temp_db.conn.cursor()
    cursor.execute("SELECT * FROM action_log WHERE package_name='firefox'")
    row = cursor.fetchone()

    assert row is not None
    assert row[2] == "install"  # action_type
    assert row[3] == "firefox"  # package_name
    assert row[4] == "apt"  # package_manager
