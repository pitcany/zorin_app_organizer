# Unified Package Manager (UPM)

A unified interface for managing packages across APT, Flatpak, and Snap on Zorin OS.

## Status

**Phase 1 Complete:** Core infrastructure with CLI interface

- ✅ Database layer with SQLite
- ✅ Abstract adapter interface
- ✅ APT adapter (using python-apt)
- ✅ Flatpak adapter (CLI-based)
- ✅ Snap adapter (CLI-based)
- ✅ Command-line interface
- 🚧 GUI (coming in Phase 2)
- 🚧 API integration for rich metadata (coming in Phase 3)

## Quick Start

### Installation

```bash
# Install dependencies
pip install -r requirements.txt

# Install in development mode
pip install -e .
```

### Usage

```bash
# Search for packages across all package managers
upm search firefox

# List installed packages
upm list

# Install a package (requires sudo/pkexec)
upm install apt python3-pip
upm install flatpak org.mozilla.Firefox
upm install snap firefox

# Remove a package
upm remove apt python3-pip
```

## Architecture

See `docs/plans/2025-11-18-unified-package-manager-design.md` for full design documentation.

### Components

- `upm/models.py` - Data models for packages and logs
- `upm/database.py` - SQLite database layer
- `upm/adapters/base.py` - Abstract adapter interface
- `upm/adapters/apt_adapter.py` - APT integration via python-apt
- `upm/adapters/flatpak_adapter.py` - Flatpak integration via CLI
- `upm/adapters/snap_adapter.py` - Snap integration via CLI
- `upm/cli.py` - Command-line interface

## Development

### Running Tests

```bash
# Run all tests
pytest tests/ -v

# Run with coverage
pytest tests/ -v --cov=upm --cov-report=term-missing

# Run specific test file
pytest tests/test_adapters/test_apt_adapter.py -v
```

### Project Structure

```
upm/
├── __init__.py
├── models.py           # Data models
├── database.py         # Database layer
├── cli.py              # CLI interface
└── adapters/
    ├── __init__.py
    ├── base.py         # Abstract interface
    ├── apt_adapter.py  # APT implementation
    ├── flatpak_adapter.py
    └── snap_adapter.py

tests/
├── test_models.py
├── test_database.py
├── test_cli.py
├── test_integration.py
└── test_adapters/
    ├── test_base.py
    ├── test_apt_adapter.py
    ├── test_flatpak_adapter.py
    └── test_snap_adapter.py
```

## Next Steps (Phase 2)

- GTK4 GUI implementation
- Search, Installed, Logs, Settings tabs
- Progress indicators for operations
- Notifications

## License

TBD
