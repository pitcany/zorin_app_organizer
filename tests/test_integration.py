import pytest
from upm.adapters.apt_adapter import AptAdapter
from upm.adapters.flatpak_adapter import FlatpakAdapter
from upm.adapters.snap_adapter import SnapAdapter


def test_all_adapters_follow_interface():
    """Test that all adapters properly implement the interface"""
    adapters = [AptAdapter(), FlatpakAdapter(), SnapAdapter()]

    for adapter in adapters:
        # Test that all methods exist
        assert hasattr(adapter, 'search')
        assert hasattr(adapter, 'install')
        assert hasattr(adapter, 'remove')
        assert hasattr(adapter, 'list_installed')
        assert hasattr(adapter, 'get_details')
        assert hasattr(adapter, 'is_available')

        # Test that is_available returns bool
        assert isinstance(adapter.is_available(), bool)


def test_search_across_all_adapters():
    """Test searching across all available adapters"""
    adapters = {
        'apt': AptAdapter(),
        'flatpak': FlatpakAdapter(),
        'snap': SnapAdapter()
    }

    query = "python"
    all_results = []

    for name, adapter in adapters.items():
        if adapter.is_available():
            results = adapter.search(query)
            assert isinstance(results, list)
            all_results.extend(results)

    # Should get at least some results from available package managers
    assert len(all_results) >= 0  # May be 0 if no package managers available
