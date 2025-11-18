import pytest
from upm.adapters.flatpak_adapter import FlatpakAdapter


def test_flatpak_adapter_creation():
    """Test creating Flatpak adapter"""
    adapter = FlatpakAdapter()
    assert adapter is not None


def test_flatpak_availability():
    """Test checking if Flatpak is available"""
    adapter = FlatpakAdapter()
    result = adapter.is_available()
    assert isinstance(result, bool)


def test_flatpak_search():
    """Test searching for packages in Flatpak"""
    adapter = FlatpakAdapter()

    if not adapter.is_available():
        pytest.skip("Flatpak not available on this system")

    # Search for a common app
    results = adapter.search("firefox")

    # Structure should be correct even if no results
    assert isinstance(results, list)

    if len(results) > 0:
        result = results[0]
        assert hasattr(result, 'package_id')
        assert result.package_manager == 'flatpak'
