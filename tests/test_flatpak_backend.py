"""
Comprehensive test suite for flatpak_backend.py
Tests package validation and core functionality
"""

import pytest
import subprocess
from unittest.mock import Mock, patch
from flatpak_backend import FlatpakBackend, validate_flatpak_id


class TestP0SecurityFixes:
    """Tests for P0 security vulnerabilities in Flatpak backend"""

    @pytest.mark.security
    def test_p0_03_flatpak_id_validation(self):
        """P0-3: Test Flatpak ID validation prevents command injection"""
        # Valid Flatpak IDs (reverse DNS notation)
        assert validate_flatpak_id("org.mozilla.firefox") == True
        assert validate_flatpak_id("com.google.Chrome") == True
        assert validate_flatpak_id("org.gnome.Builder") == True
        assert validate_flatpak_id("io.github.project") == True
        assert validate_flatpak_id("com.example.App-Name") == True
        assert validate_flatpak_id("org.freedesktop.Platform") == True

        # Invalid Flatpak IDs (command injection attempts)
        assert validate_flatpak_id("test; rm -rf /") == False
        assert validate_flatpak_id("org.test && whoami") == False
        assert validate_flatpak_id("com.test | nc") == False
        assert validate_flatpak_id("$(id).example.App") == False
        assert validate_flatpak_id("`whoami`.test.app") == False
        assert validate_flatpak_id("org.test\nrm -rf") == False

        # Invalid: doesn't follow reverse DNS pattern
        assert validate_flatpak_id("firefox") == False  # Must have at least 2 parts
        assert validate_flatpak_id("firefox.app") == False  # Still needs domain
        assert validate_flatpak_id("app") == False

        # Invalid: special characters
        assert validate_flatpak_id("org/mozilla/firefox") == False
        assert validate_flatpak_id("org mozilla firefox") == False
        assert validate_flatpak_id("org@mozilla.firefox") == False
        assert validate_flatpak_id("org;mozilla;firefox") == False

        # Invalid: empty or too long
        assert validate_flatpak_id("") == False
        assert validate_flatpak_id("a." + "b" * 255) == False

    @pytest.mark.security
    @patch('flatpak_backend.subprocess.run')
    def test_p0_03_install_blocks_invalid_ids(self, mock_run):
        """P0-3: Test install_package blocks invalid Flatpak IDs"""
        backend = FlatpakBackend()

        # Valid ID should proceed
        mock_run.return_value = Mock(returncode=0, stdout="", stderr="")
        success, msg = backend.install_package("org.mozilla.firefox")
        mock_run.assert_called()

        # Invalid ID should be rejected before subprocess
        mock_run.reset_mock()
        success, msg = backend.install_package("evil; rm -rf /")

        assert success == False
        assert "Invalid Flatpak ID" in msg
        mock_run.assert_not_called()

    @pytest.mark.security
    @patch('flatpak_backend.subprocess.run')
    def test_p0_03_uninstall_blocks_invalid_ids(self, mock_run):
        """P0-3: Test uninstall_package blocks invalid Flatpak IDs"""
        backend = FlatpakBackend()

        success, msg = backend.uninstall_package("evil`id`app")

        assert success == False
        assert "Invalid Flatpak ID" in msg
        mock_run.assert_not_called()


class TestFlatpakBackendCore:
    """Tests for core Flatpak backend functionality"""

    @pytest.mark.unit
    @patch('flatpak_backend.subprocess.run')
    def test_is_available(self, mock_run):
        """Test Flatpak availability check"""
        backend = FlatpakBackend()

        # Flatpak is available
        mock_run.return_value = Mock(returncode=0)
        assert backend.is_available() == True

        # Flatpak is not available
        mock_run.return_value = Mock(returncode=1)
        assert backend.is_available() == False

        # Exception handling
        mock_run.side_effect = Exception("Error")
        assert backend.is_available() == False

    @pytest.mark.unit
    @patch('flatpak_backend.subprocess.run')
    def test_is_flathub_configured(self, mock_run):
        """Test Flathub repository configuration check"""
        backend = FlatpakBackend()

        # Flathub is configured
        mock_run.return_value = Mock(
            returncode=0,
            stdout="flathub\tsystem\tflathub\thttps://dl.flathub.org/repo/",
            stderr=""
        )
        assert backend.is_flathub_configured() == True

        # Flathub is not configured
        mock_run.return_value = Mock(returncode=0, stdout="", stderr="")
        assert backend.is_flathub_configured() == False

    @pytest.mark.unit
    @patch('flatpak_backend.subprocess.run')
    def test_setup_flathub(self, mock_run):
        """Test Flathub repository setup"""
        backend = FlatpakBackend()

        mock_run.return_value = Mock(returncode=0, stdout="", stderr="")

        success, msg = backend.setup_flathub()

        assert success == True or "already added" in msg.lower()
        mock_run.assert_called()

    @pytest.mark.unit
    @patch('flatpak_backend.requests.get')
    def test_search_via_api(self, mock_get):
        """Test Flathub API search"""
        backend = FlatpakBackend()

        # Mock API response
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = [
            {
                'name': 'Firefox',
                'id': 'org.mozilla.firefox',
                'summary': 'Web browser',
                'version': '110.0'
            }
        ]
        mock_get.return_value = mock_response

        results = backend.search_packages("firefox", limit=10)

        assert isinstance(results, list)
        mock_get.assert_called()

    @pytest.mark.unit
    @patch('flatpak_backend.subprocess.run')
    def test_install_user_mode(self, mock_run):
        """Test Flatpak installation in user mode"""
        backend = FlatpakBackend()

        mock_run.return_value = Mock(returncode=0, stdout="", stderr="")

        success, msg = backend.install_package("org.mozilla.firefox", system_wide=False)

        assert success == True

        # Verify --user flag is used
        call_args = mock_run.call_args[0][0]
        assert '--user' in call_args
        assert 'org.mozilla.firefox' in call_args

    @pytest.mark.unit
    @patch('flatpak_backend.subprocess.run')
    def test_install_system_wide(self, mock_run):
        """Test Flatpak installation system-wide"""
        backend = FlatpakBackend()

        mock_run.return_value = Mock(returncode=0, stdout="", stderr="")

        success, msg = backend.install_package("org.mozilla.firefox", system_wide=True)

        # Should use pkexec for system-wide installation
        call_args = mock_run.call_args[0][0]
        assert 'pkexec' in call_args

    @pytest.mark.unit
    @patch('flatpak_backend.subprocess.run')
    def test_uninstall_success(self, mock_run):
        """Test successful Flatpak uninstallation"""
        backend = FlatpakBackend()

        mock_run.return_value = Mock(returncode=0, stdout="", stderr="")

        success, msg = backend.uninstall_package("org.mozilla.firefox")

        assert success == True
        assert "Successfully uninstalled" in msg

    @pytest.mark.unit
    @patch('flatpak_backend.subprocess.run')
    def test_get_installed_packages(self, mock_run):
        """Test getting list of installed Flatpaks"""
        backend = FlatpakBackend()

        # Mock flatpak list output
        mock_run.return_value = Mock(
            returncode=0,
            stdout="Firefox\torg.mozilla.firefox\t110.0\tstable\tsystem",
            stderr=""
        )

        packages = backend.get_installed_packages()

        assert isinstance(packages, list)
        mock_run.assert_called()

    @pytest.mark.unit
    @patch('flatpak_backend.subprocess.run')
    def test_timeout_handling(self, mock_run):
        """Test timeout handling in Flatpak operations"""
        backend = FlatpakBackend()

        mock_run.side_effect = subprocess.TimeoutExpired(['flatpak'], 300)

        success, msg = backend.install_package("org.test.App")

        assert success == False
        assert "timed out" in msg.lower()


class TestFlathubAPI:
    """Tests for Flathub API integration"""

    @pytest.mark.unit
    @patch('flatpak_backend.requests.get')
    def test_api_error_handling(self, mock_get):
        """Test handling of API errors"""
        backend = FlatpakBackend()

        # API returns error
        mock_response = Mock()
        mock_response.status_code = 500
        mock_get.return_value = mock_response

        results = backend.search_packages("firefox", limit=10)

        # Should fall back to CLI search or return empty
        assert isinstance(results, list)

    @pytest.mark.unit
    @patch('flatpak_backend.requests.get')
    def test_api_timeout(self, mock_get):
        """Test API timeout handling"""
        backend = FlatpakBackend()

        mock_get.side_effect = Exception("Timeout")

        results = backend.search_packages("firefox", limit=10)

        # Should handle gracefully
        assert isinstance(results, list)


class TestEdgeCases:
    """Tests for edge cases and error handling"""

    @pytest.mark.unit
    def test_search_empty_query(self):
        """Test search with empty query"""
        backend = FlatpakBackend()

        results = backend.search_packages("", limit=10)
        assert results == []

    @pytest.mark.unit
    def test_backend_type_attributes(self):
        """Test backend has correct type attributes"""
        backend = FlatpakBackend()

        assert backend.package_type == "flatpak"
        assert backend.source_repo == "Flathub"
        assert "flathub.org" in backend.flathub_api

    @pytest.mark.unit
    @patch('flatpak_backend.subprocess.run')
    def test_network_error_handling(self, mock_run):
        """Test handling of network errors"""
        backend = FlatpakBackend()

        mock_run.side_effect = Exception("Network error")

        success, msg = backend.install_package("org.test.App")

        assert success == False
        assert "failed" in msg.lower()


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
