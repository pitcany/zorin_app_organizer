"""
Comprehensive test suite for database.py
Tests all P0 security fixes and core functionality
"""

import pytest
import sqlite3
import os
import tempfile
import threading
import time
from datetime import datetime, timezone, timedelta
from database import DatabaseManager


@pytest.fixture
def temp_db():
    """Create a temporary database for testing"""
    fd, path = tempfile.mkstemp(suffix='.db')
    os.close(fd)
    yield path
    # Cleanup
    try:
        os.unlink(path)
    except:
        pass


@pytest.fixture
def db(temp_db):
    """Create a DatabaseManager instance with temp database"""
    database = DatabaseManager(temp_db)
    yield database
    database.close()


class TestP0SecurityFixes:
    """Tests for all P0 security vulnerabilities"""

    @pytest.mark.security
    def test_p0_01_sql_injection_prevention(self, db):
        """P0-1: Test SQL injection prevention in update_preference"""
        # Valid preference keys should work
        db.update_preference('repo_flathub_enabled', 1)
        prefs = db.get_preferences()
        assert prefs['repo_flathub_enabled'] == True

        # Invalid/malicious keys should raise ValueError
        with pytest.raises(ValueError, match="Invalid preference key"):
            db.update_preference('malicious_key', 1)

        # SQL injection attempt should be blocked
        with pytest.raises(ValueError):
            db.update_preference("repo_flathub_enabled = 1; DROP TABLE user_prefs; --", 1)

        # Table should still exist
        db.cursor.execute("SELECT COUNT(*) FROM user_prefs")
        assert db.cursor.fetchone()[0] >= 0

    @pytest.mark.security
    def test_p0_02_thread_safety(self, temp_db):
        """P0-2: Test thread-local storage prevents race conditions"""
        results = []
        errors = []

        def worker(worker_id):
            try:
                # Each thread gets its own connection
                db = DatabaseManager(temp_db)

                # Add an app with thread-specific data
                package_name = f"test-package-{worker_id}"
                db.add_installed_app(
                    f"Test App {worker_id}",
                    package_name,
                    "apt",
                    "Test Repo",
                    "1.0",
                    f"Description {worker_id}"
                )

                # Verify it was added
                apps = db.get_installed_apps()
                found = any(app['package_name'] == package_name for app in apps)
                results.append((worker_id, found))

                db.close()
            except Exception as e:
                errors.append((worker_id, str(e)))

        # Create 10 threads accessing database concurrently
        threads = []
        for i in range(10):
            t = threading.Thread(target=worker, args=(i,))
            threads.append(t)
            t.start()

        for t in threads:
            t.join()

        # All threads should succeed
        assert len(errors) == 0, f"Thread errors: {errors}"
        assert len(results) == 10
        assert all(found for _, found in results)

    @pytest.mark.security
    def test_p0_04_null_pointer_prevention(self, temp_db):
        """P0-4: Test NULL pointer crash prevention in get_preferences"""
        # Create database without preferences row
        db = DatabaseManager(temp_db)
        db.cursor.execute("DELETE FROM user_prefs")
        db.conn.commit()

        # get_preferences should auto-recover
        prefs = db.get_preferences()
        assert prefs is not None
        assert 'repo_flathub_enabled' in prefs
        assert 'repo_snapd_enabled' in prefs

        db.close()

    @pytest.mark.security
    def test_p0_05_resource_leak_prevention(self):
        """P0-5: Test resource leak prevention in __init__"""
        # Test with invalid database path that will fail
        with pytest.raises(RuntimeError, match="Failed to initialize database"):
            # Try to create database in read-only directory
            db = DatabaseManager("/proc/test_invalid.db")

    @pytest.mark.security
    def test_p0_06_path_traversal_prevention(self, db):
        """P0-6: Test path traversal prevention in export_logs"""
        # Add some test logs
        db.add_log("install", "test-package", "apt", "success", "Test message")

        # Valid path should work
        with tempfile.TemporaryDirectory() as tmpdir:
            valid_path = os.path.join(tmpdir, "export.txt")
            db.export_logs(valid_path, days=7)
            assert os.path.exists(valid_path)

        # Path traversal attempts should be blocked
        with pytest.raises(ValueError, match="Invalid file path"):
            db.export_logs("../../../etc/passwd", days=7)

        with pytest.raises(ValueError, match="Invalid file path"):
            db.export_logs("/etc/shadow", days=7)

        with pytest.raises(ValueError, match="Invalid file path"):
            db.export_logs("/sys/kernel/debug/test", days=7)

        # Non-existent directory should be blocked
        with pytest.raises(ValueError, match="Directory does not exist"):
            db.export_logs("/nonexistent/path/export.txt", days=7)

    @pytest.mark.security
    def test_p0_10_timezone_aware_timestamps(self, db):
        """P0-10: Test timezone-aware datetime usage"""
        # Add a log entry
        db.add_log("install", "test-package", "apt", "success", "Test")

        # Export logs with timezone-aware cutoff
        with tempfile.TemporaryDirectory() as tmpdir:
            export_path = os.path.join(tmpdir, "logs.txt")
            db.export_logs(export_path, days=7)

            # File should be created
            assert os.path.exists(export_path)

            # Content should contain the log
            with open(export_path) as f:
                content = f.read()
                assert "test-package" in content

        # Test cleanup_old_logs with timezone-aware dates
        # First, set retention to 0 days
        db.update_preference('log_retention_days', 0)

        # Cleanup should work without errors
        db.cleanup_old_logs()

    @pytest.mark.security
    def test_p0_13_soft_delete_prevents_data_loss(self, db):
        """P0-13: Test soft delete prevents permanent data loss"""
        # Add an app
        db.add_installed_app("Test App", "test-pkg", "apt", "Test Repo", "1.0", "Description")

        # Verify it's installed
        assert db.is_app_installed("test-pkg")
        apps = db.get_installed_apps()
        assert len(apps) == 1

        # Remove the app (soft delete)
        db.remove_installed_app("test-pkg")

        # App should not appear in normal queries
        assert not db.is_app_installed("test-pkg")
        apps = db.get_installed_apps()
        assert len(apps) == 0

        # But data should still exist in database with deleted_at timestamp
        db.cursor.execute("SELECT deleted_at FROM installed_apps WHERE package_name = ?", ("test-pkg",))
        row = db.cursor.fetchone()
        assert row is not None
        assert row[0] is not None  # deleted_at should be set


class TestDatabaseCore:
    """Tests for core database functionality"""

    @pytest.mark.unit
    def test_create_tables(self, db):
        """Test that all tables are created correctly"""
        tables = ['installed_apps', 'user_prefs', 'logs', 'custom_repositories']

        for table in tables:
            db.cursor.execute(f"SELECT name FROM sqlite_master WHERE type='table' AND name=?", (table,))
            assert db.cursor.fetchone() is not None

    @pytest.mark.unit
    def test_add_and_get_installed_apps(self, db):
        """Test adding and retrieving installed apps"""
        # Add an app
        result = db.add_installed_app(
            "Firefox",
            "firefox",
            "apt",
            "Ubuntu Main",
            "110.0",
            "Web browser"
        )
        assert result == True

        # Adding duplicate should fail
        result = db.add_installed_app(
            "Firefox",
            "firefox",
            "apt",
            "Ubuntu Main",
            "110.0",
            "Web browser"
        )
        assert result == False

        # Retrieve apps
        apps = db.get_installed_apps()
        assert len(apps) == 1
        assert apps[0]['name'] == "Firefox"
        assert apps[0]['package_name'] == "firefox"
        assert apps[0]['package_type'] == "apt"

    @pytest.mark.unit
    def test_filter_apps_by_type(self, db):
        """Test filtering installed apps by package type"""
        # Add apps of different types
        db.add_installed_app("App1", "app1", "apt", "Repo", "1.0", "Desc")
        db.add_installed_app("App2", "app2", "snap", "Repo", "1.0", "Desc")
        db.add_installed_app("App3", "app3", "flatpak", "Repo", "1.0", "Desc")

        # Filter by type
        apt_apps = db.get_installed_apps(package_type="apt")
        assert len(apt_apps) == 1
        assert apt_apps[0]['package_type'] == "apt"

        snap_apps = db.get_installed_apps(package_type="snap")
        assert len(snap_apps) == 1

        flatpak_apps = db.get_installed_apps(package_type="flatpak")
        assert len(flatpak_apps) == 1

    @pytest.mark.unit
    def test_preferences(self, db):
        """Test preference management"""
        # Get default preferences
        prefs = db.get_preferences()
        assert prefs['repo_flathub_enabled'] == True
        assert prefs['notifications_enabled'] == True
        assert prefs['log_retention_days'] == 30

        # Update preference
        db.update_preference('log_retention_days', 60)
        prefs = db.get_preferences()
        assert prefs['log_retention_days'] == 60

        # Update multiple preferences
        db.update_preferences({
            'repo_flathub_enabled': False,
            'notifications_enabled': False
        })
        prefs = db.get_preferences()
        assert prefs['repo_flathub_enabled'] == False
        assert prefs['notifications_enabled'] == False

    @pytest.mark.unit
    def test_logging(self, db):
        """Test log management"""
        # Add logs
        db.add_log("install", "package1", "apt", "success", "Installed successfully")
        db.add_log("uninstall", "package2", "snap", "failed", "Failed to uninstall", "Error details")
        db.add_log("search", "query", "all", "success", "Search completed")

        # Get all logs
        logs = db.get_logs()
        assert len(logs) >= 3

        # Get logs by action type
        install_logs = db.get_logs(action_type="install")
        assert len(install_logs) >= 1
        assert install_logs[0]['action_type'] == "install"

        # Get logs by date range
        recent_logs = db.get_logs(days=1)
        assert len(recent_logs) >= 3

    @pytest.mark.unit
    def test_custom_repositories(self, db):
        """Test custom repository management"""
        # Add repository
        repo_id = db.add_custom_repository(
            "Test PPA",
            "ppa:test/ppa",
            "apt",
            enabled=True
        )
        assert repo_id is not None

        # Get repositories
        repos = db.get_custom_repositories()
        assert len(repos) == 1
        assert repos[0]['name'] == "Test PPA"
        assert repos[0]['enabled'] == True

        # Toggle repository
        db.toggle_custom_repository(repo_id, False)
        repos = db.get_custom_repositories()
        assert repos[0]['enabled'] == False

        # Delete repository
        db.delete_custom_repository(repo_id)
        repos = db.get_custom_repositories()
        assert len(repos) == 0

    @pytest.mark.unit
    def test_context_manager(self, temp_db):
        """Test database can be used as context manager"""
        with DatabaseManager(temp_db) as db:
            db.add_installed_app("Test", "test", "apt", "repo", "1.0", "desc")
            apps = db.get_installed_apps()
            assert len(apps) == 1


class TestDatabaseEdgeCases:
    """Tests for edge cases and error handling"""

    @pytest.mark.unit
    def test_empty_database(self, db):
        """Test operations on empty database"""
        apps = db.get_installed_apps()
        assert len(apps) == 0

        logs = db.get_logs()
        assert len(logs) == 0

        repos = db.get_custom_repositories()
        assert len(repos) == 0

    @pytest.mark.unit
    def test_unicode_handling(self, db):
        """Test Unicode characters in data"""
        db.add_installed_app(
            "Tëst Äpp 测试",
            "test-unicode",
            "apt",
            "Rëpö",
            "1.0",
            "Dëscription with émojis 🎉🚀"
        )

        apps = db.get_installed_apps()
        assert apps[0]['name'] == "Tëst Äpp 测试"
        assert "🎉" in apps[0]['description']

    @pytest.mark.unit
    def test_large_data_handling(self, db):
        """Test handling of large datasets"""
        # Add many apps
        for i in range(100):
            db.add_installed_app(
                f"App {i}",
                f"app-{i}",
                "apt",
                "repo",
                "1.0",
                f"Description {i}"
            )

        apps = db.get_installed_apps()
        assert len(apps) == 100

    @pytest.mark.unit
    def test_special_characters_in_package_names(self, db):
        """Test handling of special characters"""
        # Valid characters
        db.add_installed_app("Test", "lib-test+dev-1.0", "apt", "repo", "1.0", "desc")
        assert db.is_app_installed("lib-test+dev-1.0")

    @pytest.mark.unit
    def test_whitelist_all_valid_preferences(self, db):
        """Test that all expected preference keys are in whitelist"""
        valid_keys = [
            'repo_flathub_enabled',
            'repo_snapd_enabled',
            'repo_apt_enabled',
            'notifications_enabled',
            'log_retention_days',
            'auto_save_metadata'
        ]

        for key in valid_keys:
            # Should not raise exception
            if 'enabled' in key or 'metadata' in key:
                db.update_preference(key, 1)
            else:
                db.update_preference(key, 30)

        prefs = db.get_preferences()
        assert all(key in prefs for key in valid_keys)


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
