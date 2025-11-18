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
