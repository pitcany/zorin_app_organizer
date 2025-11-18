import pytest
from upm.adapters.base import PackageManagerAdapter


def test_adapter_cannot_be_instantiated():
    """Test that abstract base class cannot be instantiated"""
    with pytest.raises(TypeError):
        PackageManagerAdapter()


def test_adapter_has_required_methods():
    """Test that adapter defines required abstract methods"""
    required_methods = ['search', 'install', 'remove', 'list_installed', 'get_details', 'is_available']

    for method in required_methods:
        assert hasattr(PackageManagerAdapter, method)
