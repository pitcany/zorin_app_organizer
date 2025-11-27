# PolySynaptic - Multi-Backend Package Manager

PolySynaptic is a fork of Synaptic Package Manager that extends the classic APT interface with first-class support for Snap and Flatpak packages. It provides a unified GUI for managing packages from all three ecosystems on Ubuntu/Zorin and similar Debian-based distributions.

## Features

- **Unified Search**: Single search bar returning results from APT, Snap, and Flatpak simultaneously
- **Backend Filters**: Toggle visibility of each package source
- **Clear Labeling**: Visual badges showing package source (deb/snap/flatpak)
- **Multi-Backend Transactions**: Queue operations across all backends
- **Graceful Degradation**: Works as regular Synaptic if Snap/Flatpak are unavailable
- **Modular Architecture**: Clean backend abstraction for easy extensibility

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     GTK UI Layer                             │
│  ┌──────────────┬────────────────┬────────────────────────┐ │
│  │RGMainWindow  │RGUnifiedView   │RGBackendSettingsWindow │ │
│  │(existing)    │(new)           │(new)                   │ │
│  └──────────────┴────────────────┴────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    BackendManager                            │
│  - Coordinates all backends                                  │
│  - Unified search (parallel execution)                       │
│  - Transaction management                                    │
│  - Configuration persistence                                 │
└─────────────────────────────────────────────────────────────┘
                              │
          ┌───────────────────┼───────────────────┐
          ▼                   ▼                   ▼
┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
│   AptBackend    │ │   SnapBackend   │ │ FlatpakBackend  │
│                 │ │                 │ │                 │
│ Wraps existing  │ │ Uses snap CLI:  │ │ Uses flatpak:   │
│ Synaptic logic  │ │ - snap find     │ │ - flatpak search│
│ via RPackage-   │ │ - snap info     │ │ - flatpak info  │
│ Lister          │ │ - snap install  │ │ - flatpak install│
│                 │ │ - snap remove   │ │ - flatpak remove│
└─────────────────┘ └─────────────────┘ └─────────────────┘
          │                   │                   │
          ▼                   ▼                   ▼
┌─────────────────────────────────────────────────────────────┐
│                   IPackageBackend Interface                  │
│  - listPackages(query)                                       │
│  - getPackageDetails(id)                                     │
│  - installPackage(id)                                        │
│  - removePackage(id)                                         │
│  - updatePackage(id)                                         │
│  - getInstalledState(id)                                     │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    PackageInfo (Unified Model)               │
│  - id, name, summary, description                            │
│  - version, installedVersion                                 │
│  - sourceBackend (APT/SNAP/FLATPAK)                          │
│  - installStatus (INSTALLED/AVAILABLE/UPDATE_AVAILABLE)      │
│  - Backend-specific fields (channel, remote, etc.)           │
└─────────────────────────────────────────────────────────────┘
```

## Directory Structure

```
polysynaptic/
├── common/                 # Core package management logic
│   ├── ipackagebackend.h   # Backend interface definition
│   ├── aptbackend.h/cc     # APT backend (wraps Synaptic)
│   ├── snapbackend.h/cc    # Snap backend (CLI-based)
│   ├── flatpakbackend.h/cc # Flatpak backend (CLI-based)
│   ├── backendmanager.h/cc # Backend coordinator
│   └── [original synaptic files...]
├── gtk/                    # GTK UI components
│   ├── rgunifiedview.h/cc  # Unified package list widget
│   ├── rgbackendsettings.h/cc # Backend settings dialog
│   └── [original synaptic files...]
├── tests/                  # Test suite
│   └── test_backends.cc    # Backend unit tests
└── [build files, docs, etc.]
```

## New Files Summary

### Backend Abstraction Layer (common/)

| File | Description |
|------|-------------|
| `ipackagebackend.h` | Abstract interface defining methods all backends must implement |
| `aptbackend.h/cc` | APT backend wrapping RPackageLister and RPackage |
| `snapbackend.h/cc` | Snap backend using snap CLI commands |
| `flatpakbackend.h/cc` | Flatpak backend using flatpak CLI commands |
| `backendmanager.h/cc` | Coordinates backends, manages transactions |

### UI Components (gtk/)

| File | Description |
|------|-------------|
| `rgunifiedview.h/cc` | GtkTreeModel for displaying packages from all backends |
| `rgbackendsettings.h/cc` | Settings dialog for backend configuration |

### Tests (tests/)

| File | Description |
|------|-------------|
| `test_backends.cc` | Unit tests for backend abstraction layer |

## Building

### Dependencies

On Ubuntu/Zorin, install build dependencies:

```bash
# Essential build tools
sudo apt install build-essential autoconf automake libtool

# APT development libraries
sudo apt install libapt-pkg-dev

# GTK3 development
sudo apt install libgtk-3-dev libvte-2.91-dev

# Optional but recommended
sudo apt install libxapian-dev  # Full-text search
```

### Compilation

```bash
cd polysynaptic

# Generate configure script
./autogen.sh

# Configure
./configure

# Build
make

# Run (requires root for package operations)
sudo ./gtk/polysynaptic
```

### Building a .deb Package

```bash
# Install packaging tools
sudo apt install devscripts debhelper

# Build package
dpkg-buildpackage -us -uc -b

# Install
sudo dpkg -i ../polysynaptic_1.0.0_amd64.deb
```

## Adding a New Backend

To add support for another package manager (e.g., AppImage, Nix):

1. **Create backend files** in `common/`:
   ```cpp
   // common/appimagebackend.h
   class AppImageBackend : public IPackageBackend {
       // Implement all virtual methods
   };
   ```

2. **Implement the interface**:
   - `getType()` / `getName()` - Return backend info
   - `isAvailable()` - Check if backend is usable
   - `searchPackages()` - Search for packages
   - `installPackage()` / `removePackage()` - Operations

3. **Register in BackendManager**:
   ```cpp
   // In backendmanager.cc
   _appimageBackend = make_unique<AppImageBackend>();
   ```

4. **Add UI toggle** in `rgbackendsettings.cc`

5. **Update build files**:
   - Add sources to `common/Makefile.am`

## Configuration

Configuration is stored in `~/.synaptic/polysynaptic.conf`:

```ini
# Backend enable/disable flags
apt_enabled=true
snap_enabled=true
flatpak_enabled=true
```

Flatpak-specific settings are managed through the backend directly.

## Testing

Run the test suite:

```bash
cd tests
make test_backends
./test_backends
```

Or with the full build:

```bash
make check
```

## Error Handling

- **Backend Availability**: If snapd is not running or flatpak is not installed, those backends are automatically disabled with informative tooltips
- **CLI Timeouts**: All CLI operations have configurable timeouts (default 120s)
- **Input Validation**: Package names are validated to prevent command injection
- **Graceful Degradation**: Failures in one backend don't affect others

## Security Considerations

1. **No shell=True**: All subprocess calls use explicit argument lists
2. **Input Validation**: Package names validated with strict regex patterns
3. **Privilege Escalation**: Uses `pkexec` (PolicyKit) for privileged operations
4. **Timeout Enforcement**: All CLI calls have timeouts to prevent hangs

## License

PolySynaptic is licensed under the GNU General Public License v2.0, same as the original Synaptic Package Manager.

## Credits

- Original Synaptic Package Manager by Michael Vogt and the Debian/Ubuntu team
- PolySynaptic extensions by the community

## See Also

- [Synaptic Package Manager](https://github.com/mvo5/synaptic)
- [Snap Documentation](https://snapcraft.io/docs)
- [Flatpak Documentation](https://flatpak.org/documentation/)
