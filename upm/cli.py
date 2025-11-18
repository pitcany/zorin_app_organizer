#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path
from upm.adapters.apt_adapter import AptAdapter
from upm.adapters.flatpak_adapter import FlatpakAdapter
from upm.adapters.snap_adapter import SnapAdapter
from upm.database import Database


def parse_args(argv=None):
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description='Unified Package Manager for Zorin OS',
        prog='upm'
    )

    subparsers = parser.add_subparsers(dest='command', help='Commands')

    # Search command
    search_parser = subparsers.add_parser('search', help='Search for packages')
    search_parser.add_argument('query', help='Search query')

    # List command
    list_parser = subparsers.add_parser('list', help='List installed packages')

    # Install command
    install_parser = subparsers.add_parser('install', help='Install a package')
    install_parser.add_argument('package_manager', choices=['apt', 'flatpak', 'snap'],
                                help='Package manager to use')
    install_parser.add_argument('package_id', help='Package ID to install')

    # Remove command
    remove_parser = subparsers.add_parser('remove', help='Remove a package')
    remove_parser.add_argument('package_manager', choices=['apt', 'flatpak', 'snap'],
                               help='Package manager to use')
    remove_parser.add_argument('package_id', help='Package ID to remove')

    return parser.parse_args(argv)


def get_adapters():
    """Get all available package manager adapters"""
    adapters = {
        'apt': AptAdapter(),
        'flatpak': FlatpakAdapter(),
        'snap': SnapAdapter()
    }
    return adapters


def cmd_search(args, adapters):
    """Execute search command"""
    print(f"Searching for '{args.query}'...\n")

    for name, adapter in adapters.items():
        if not adapter.is_available():
            print(f"[{name.upper()}] Not available")
            continue

        print(f"[{name.upper()}] Searching...")
        results = adapter.search(args.query)

        if not results:
            print(f"  No results found")
        else:
            for result in results[:5]:  # Limit to 5 results per source
                print(f"  {result.package_id} - {result.name}")
                print(f"    Version: {result.version}")
                print(f"    {result.description[:80]}...")
                print()
        print()


def cmd_list(args, adapters):
    """Execute list command"""
    print("Installed packages:\n")

    for name, adapter in adapters.items():
        if not adapter.is_available():
            continue

        print(f"[{name.upper()}]")
        installed = adapter.list_installed()

        if not installed:
            print("  No packages installed")
        else:
            for pkg in installed[:10]:  # Limit to 10 for readability
                print(f"  {pkg.package_id} - {pkg.version}")

        print()


def cmd_install(args, adapters):
    """Execute install command"""
    adapter = adapters.get(args.package_manager)

    if not adapter or not adapter.is_available():
        print(f"Error: {args.package_manager} is not available")
        return 1

    print(f"Installing {args.package_id} via {args.package_manager}...")
    success = adapter.install(args.package_id)

    if success:
        print("Installation successful!")
        return 0
    else:
        print("Installation failed!")
        return 1


def cmd_remove(args, adapters):
    """Execute remove command"""
    adapter = adapters.get(args.package_manager)

    if not adapter or not adapter.is_available():
        print(f"Error: {args.package_manager} is not available")
        return 1

    print(f"Removing {args.package_id} via {args.package_manager}...")
    success = adapter.remove(args.package_id)

    if success:
        print("Removal successful!")
        return 0
    else:
        print("Removal failed!")
        return 1


def main():
    """Main entry point"""
    args = parse_args()

    if not args.command:
        print("Error: No command specified. Use --help for usage.")
        return 1

    adapters = get_adapters()

    if args.command == 'search':
        cmd_search(args, adapters)
    elif args.command == 'list':
        cmd_list(args, adapters)
    elif args.command == 'install':
        return cmd_install(args, adapters)
    elif args.command == 'remove':
        return cmd_remove(args, adapters)

    return 0


if __name__ == '__main__':
    sys.exit(main())
