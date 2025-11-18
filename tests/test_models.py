import pytest
from datetime import datetime
from upm.models import PackageSearchResult, PackageInfo, PackageDetails, ActionLog


def test_package_search_result_creation():
    """Test creating a PackageSearchResult"""
    result = PackageSearchResult(
        package_id="firefox",
        name="Firefox",
        description="Web browser",
        version="120.0",
        package_manager="apt"
    )

    assert result.package_id == "firefox"
    assert result.name == "Firefox"
    assert result.description == "Web browser"
    assert result.version == "120.0"
    assert result.package_manager == "apt"


def test_package_info_creation():
    """Test creating a PackageInfo for installed package"""
    info = PackageInfo(
        package_id="gimp",
        name="GIMP",
        version="2.10.34",
        package_manager="flatpak",
        install_date=datetime(2025, 11, 1)
    )

    assert info.package_id == "gimp"
    assert info.name == "GIMP"
    assert info.version == "2.10.34"
    assert info.package_manager == "flatpak"
    assert info.install_date.year == 2025


def test_package_details_creation():
    """Test creating PackageDetails with required and optional fields"""
    # Test with only required fields
    details_minimal = PackageDetails(
        package_id="vim",
        name="Vim",
        description="Vi IMproved - enhanced vi editor",
        version="9.0.1000",
        package_manager="apt"
    )

    assert details_minimal.package_id == "vim"
    assert details_minimal.name == "Vim"
    assert details_minimal.description == "Vi IMproved - enhanced vi editor"
    assert details_minimal.version == "9.0.1000"
    assert details_minimal.package_manager == "apt"
    assert details_minimal.homepage is None
    assert details_minimal.license is None
    assert details_minimal.size is None
    assert details_minimal.dependencies is None

    # Test with all fields including optional
    details_full = PackageDetails(
        package_id="blender",
        name="Blender",
        description="3D creation suite",
        version="3.6.0",
        package_manager="snap",
        homepage="https://www.blender.org",
        license="GPL-3.0",
        size=245760000,  # bytes
        dependencies=["python3", "libgl1"]
    )

    assert details_full.package_id == "blender"
    assert details_full.name == "Blender"
    assert details_full.homepage == "https://www.blender.org"
    assert details_full.license == "GPL-3.0"
    assert details_full.size == 245760000
    assert len(details_full.dependencies) == 2
    assert "python3" in details_full.dependencies


def test_action_log_creation():
    """Test creating ActionLog with success and failure cases"""
    # Test successful installation
    log_success = ActionLog(
        timestamp=datetime(2025, 11, 18, 14, 30, 0),
        action_type="install",
        package_name="vlc",
        package_manager="apt",
        version="3.0.18",
        success=True
    )

    assert log_success.timestamp == datetime(2025, 11, 18, 14, 30, 0)
    assert log_success.action_type == "install"
    assert log_success.package_name == "vlc"
    assert log_success.package_manager == "apt"
    assert log_success.version == "3.0.18"
    assert log_success.success is True
    assert log_success.error_message is None
    assert log_success.user_note is None

    # Test failed removal with error message
    log_failure = ActionLog(
        timestamp=datetime(2025, 11, 18, 15, 0, 0),
        action_type="remove",
        package_name="broken-package",
        package_manager="flatpak",
        success=False,
        error_message="Package not found in system",
        user_note="Attempted cleanup of non-existent package"
    )

    assert log_failure.action_type == "remove"
    assert log_failure.package_name == "broken-package"
    assert log_failure.success is False
    assert log_failure.error_message == "Package not found in system"
    assert log_failure.user_note == "Attempted cleanup of non-existent package"
    assert log_failure.version is None

    # Test sync action
    log_sync = ActionLog(
        timestamp=datetime(2025, 11, 18, 16, 0, 0),
        action_type="sync",
        package_name="system-packages",
        package_manager="apt"
    )

    assert log_sync.action_type == "sync"
    assert log_sync.success is True  # default value
