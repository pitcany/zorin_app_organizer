"""
Comprehensive test suite for snap_backend.py
Tests package validation and core functionality
"""

import pytest
import subprocess
from unittest.mock import Mock, patch
from snap_backend import SnapBackend, validate_snap_name


class TestP0SecurityFixes:
    """Tests for P0 security vulnerabilities in Snap backend"""

    @pytest.mark.security
    def test_p0_03_snap_name_validation(self):
        """P0-3: Test snap name validation prevents command injection"""
        # Valid snap names
        assert validate_snap_name("firefox") == True
        assert validate_snap_name("chromium") == True
        assert validate_snap_name("vlc") == True
        assert validate_snap_name("test-snap") == True
        assert validate_snap_name("test-snap-123") == True
        assert validate_snap_name("123snap") == True

        # Invalid snap names (command injection attempts)
        assert validate_snap_name("test; rm -rf /") == False
        assert validate_snap_name("test && whoami") == False
        assert validate_snap_name("test | nc") == False
        assert validate_snap_name("$(id)") == False
        assert validate_snap_name("`whoami`") == False
        assert validate_snap_name("test\nrm -rf") == False

        # Invalid: uppercase letters (snaps are lowercase)
        assert validate_snap_name("Firefox") == False
        assert validate_snap_name("TEST") == False

        # Invalid: special characters not allowed in snaps
        assert validate_snap_name("test_snap") == False  # Underscore not allowed
        assert validate_snap_name("test.snap") == False  # Dot not allowed
        assert validate_snap_name("test/snap") == False
        assert validate_snap_name("test snap") == False  # Space
        assert validate_snap_name("test@snap") == False

        # Invalid: empty or too long
        assert validate_snap_name("") == False
        assert validate_snap_name("a" * 201) == False

        # Invalid: cannot start with hyphen
        assert validate_snap_name("-test") == False

    @pytest.mark.security
    @patch('snap_backend.subprocess.run')
    def test_p0_03_install_blocks_invalid_names(self, mock_run):
        """P0-3: Test install_package blocks invalid snap names"""
        backend = SnapBackend()

        # Valid snap should proceed
        mock_run.return_value = Mock(returncode=0, stdout="", stderr="")
        success, msg = backend.install_package("valid-snap")
        mock_run.assert_called()

        # Invalid snap should be rejected before subprocess
        mock_run.reset_mock()
        success, msg = backend.install_package("evil; rm -rf /")

        assert success == False
        assert "Invalid snap name" in msg
        mock_run.assert_not_called()

    @pytest.mark.security
    @patch('snap_backend.subprocess.run')
    def test_p0_03_uninstall_blocks_invalid_names(self, mock_run):
        """P0-3: Test uninstall_package blocks invalid snap names"""
        backend = SnapBackend()

        success, msg = backend.uninstall_package("evil`id`snap")

        assert success == False
        assert "Invalid snap name" in msg
        mock_run.assert_not_called()


class TestSnapBackendCore:
    """Tests for core Snap backend functionality"""

    @pytest.mark.unit
    @patch('snap_backend.subprocess.run')
    def test_is_available(self, mock_run):
        """Test Snap availability check"""
        backend = SnapBackend()

        # Snap is available
        mock_run.return_value = Mock(returncode=0)
        assert backend.is_available() == True

        # Snap is not available
        mock_run.return_value = Mock(returncode=1)
        assert backend.is_available() == False

        # Exception handling
        mock_run.side_effect = Exception("Error")
        assert backend.is_available() == False

    @pytest.mark.unit
    @patch('snap_backend.subprocess.run')
    def test_is_snapd_running(self, mock_run):
        """Test snapd service status check"""
        backend = SnapBackend()

        # Service is active
        mock_run.return_value = Mock(returncode=0, stdout="active\n", stderr="")
        assert backend.is_snapd_running() == True

        # Service is inactive
        mock_run.return_value = Mock(returncode=3, stdout="inactive\n", stderr="")
        assert backend.is_snapd_running() == False

    @pytest.mark.unit
    @patch('snap_backend.subprocess.run')
    def test_search_packages(self, mock_run):
        """Test snap package search"""
        backend = SnapBackend()

        # Mock snap find output
        mock_run.return_value = Mock(
            returncode=0,
            stdout="Name    Version    Publisher    Notes    Summary\nfirefox    110.0    mozilla✓    -    Mozilla Firefox",
            stderr=""
        )

        results = backend.search_packages("firefox", limit=10)

        assert isinstance(results, list)
        mock_run.assert_called()

    @pytest.mark.unit
    @patch('snap_backend.subprocess.run')
    def test_install_success(self, mock_run):
        """Test successful snap installation"""
        backend = SnapBackend()

        mock_run.return_value = Mock(returncode=0, stdout="Success", stderr="")

        success, msg = backend.install_package("test-snap")

        assert success == True
        assert "Successfully installed" in msg

        # Verify command structure
        call_args = mock_run.call_args[0][0]
        assert 'snap' in call_args
        assert 'install' in call_args
        assert 'test-snap' in call_args

    @pytest.mark.unit
    @patch('snap_backend.subprocess.run')
    def test_install_classic_mode(self, mock_run):
        """Test snap installation with classic confinement"""
        backend = SnapBackend()

        mock_run.return_value = Mock(returncode=0, stdout="", stderr="")

        success, msg = backend.install_package("test-snap", classic=True)

        # Verify --classic flag is included
        call_args = mock_run.call_args[0][0]
        assert '--classic' in call_args

    @pytest.mark.unit
    @patch('snap_backend.subprocess.run')
    def test_install_classic_required_message(self, mock_run):
        """Test handling of snaps that require classic confinement"""
        backend = SnapBackend()

        mock_run.return_value = Mock(
            returncode=1,
            stdout="",
            stderr="error: This snap requires classic confinement"
        )

        success, msg = backend.install_package("test-snap", classic=False)

        assert success == False
        assert "classic" in msg.lower()

    @pytest.mark.unit
    @patch('snap_backend.subprocess.run')
    def test_uninstall_success(self, mock_run):
        """Test successful snap uninstallation"""
        backend = SnapBackend()

        mock_run.return_value = Mock(returncode=0, stdout="", stderr="")

        success, msg = backend.uninstall_package("test-snap")

        assert success == True
        assert "Successfully uninstalled" in msg

    @pytest.mark.unit
    @patch('snap_backend.subprocess.run')
    def test_get_installed_packages(self, mock_run):
        """Test getting list of installed snaps"""
        backend = SnapBackend()

        # Mock snap list output
        mock_run.return_value = Mock(
            returncode=0,
            stdout="Name    Version    Rev    Tracking    Publisher    Notes\nfirefox    110.0    1234    latest/stable    mozilla✓    -",
            stderr=""
        )

        packages = backend.get_installed_packages()

        assert isinstance(packages, list)
        mock_run.assert_called()

    @pytest.mark.unit
    @patch('snap_backend.subprocess.run')
    def test_timeout_handling(self, mock_run):
        """Test timeout handling in snap operations"""
        backend = SnapBackend()

        mock_run.side_effect = subprocess.TimeoutExpired(['snap'], 300)

        success, msg = backend.install_package("test-snap")

        assert success == False
        assert "timed out" in msg.lower()


class TestEdgeCases:
    """Tests for edge cases and error handling"""

    @pytest.mark.unit
    def test_search_empty_query(self):
        """Test search with empty query"""
        backend = SnapBackend()

        results = backend.search_packages("", limit=10)
        assert results == []

    @pytest.mark.unit
    def test_backend_type_attributes(self):
        """Test backend has correct type attributes"""
        backend = SnapBackend()

        assert backend.package_type == "snap"
        assert backend.source_repo == "Snap Store"

    @pytest.mark.unit
    @patch('snap_backend.subprocess.run')
    def test_network_error_handling(self, mock_run):
        """Test handling of network errors"""
        backend = SnapBackend()

        mock_run.side_effect = Exception("Network error")

        success, msg = backend.install_package("test-snap")

        assert success == False
        assert "failed" in msg.lower()


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
