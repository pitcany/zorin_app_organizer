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
