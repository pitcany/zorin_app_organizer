import pytest
from upm.cli import main, parse_args


def test_parse_args_search():
    """Test parsing search command"""
    args = parse_args(['search', 'firefox'])
    assert args.command == 'search'
    assert args.query == 'firefox'


def test_parse_args_list():
    """Test parsing list command"""
    args = parse_args(['list'])
    assert args.command == 'list'


def test_parse_args_install():
    """Test parsing install command"""
    args = parse_args(['install', 'apt', 'firefox'])
    assert args.command == 'install'
    assert args.package_manager == 'apt'
    assert args.package_id == 'firefox'
