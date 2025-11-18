import pytest
from upm.adapters.snap_adapter import SnapAdapter


def test_snap_adapter_creation():
    """Test creating Snap adapter"""
    adapter = SnapAdapter()
    assert adapter is not None


def test_snap_availability():
    """Test checking if Snap is available"""
    adapter = SnapAdapter()
    result = adapter.is_available()
    assert isinstance(result, bool)


def test_snap_search():
    """Test searching for packages in Snap"""
    adapter = SnapAdapter()

    if not adapter.is_available():
        pytest.skip("Snap not available on this system")

    # Search for a common snap
    results = adapter.search("firefox")

    # Structure should be correct even if no results
    assert isinstance(results, list)

    if len(results) > 0:
        result = results[0]
        assert hasattr(result, 'package_id')
        assert result.package_manager == 'snap'
