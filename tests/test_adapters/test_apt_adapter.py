import pytest
from upm.adapters.apt_adapter import AptAdapter


def test_apt_adapter_creation():
    """Test creating APT adapter"""
    adapter = AptAdapter()
    assert adapter is not None


def test_apt_availability():
    """Test checking if APT is available"""
    adapter = AptAdapter()
    # On Ubuntu/Debian systems, APT should be available
    # On other systems, this might be False
    result = adapter.is_available()
    assert isinstance(result, bool)


def test_apt_search():
    """Test searching for packages in APT"""
    adapter = AptAdapter()

    if not adapter.is_available():
        pytest.skip("APT not available on this system")

    # Search for a common package
    results = adapter.search("python3")

    # Should find at least one result
    assert len(results) > 0

    # Check result structure
    result = results[0]
    assert hasattr(result, 'package_id')
    assert hasattr(result, 'name')
    assert hasattr(result, 'description')
    assert hasattr(result, 'version')
    assert result.package_manager == 'apt'


def test_apt_list_installed():
    """Test listing installed APT packages"""
    adapter = AptAdapter()

    if not adapter.is_available():
        pytest.skip("APT not available on this system")

    installed = adapter.list_installed()

    # Should have some installed packages on any Debian/Ubuntu system
    assert len(installed) > 0

    # Check structure of first package
    pkg = installed[0]
    assert hasattr(pkg, 'package_id')
    assert hasattr(pkg, 'name')
    assert hasattr(pkg, 'version')
    assert pkg.package_manager == 'apt'
