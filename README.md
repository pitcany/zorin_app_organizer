# PolySynaptic

A modern multi-backend package manager GUI for Ubuntu/Zorin-like systems, supporting APT, Snap, and Flatpak packages with a unified interface.

PolySynaptic is a fork of [Synaptic Package Manager](https://github.com/mvo5/synaptic) extended with first-class support for multiple package formats.

## Features

- **Unified Interface**: Single search bar and package list for APT, Snap, and Flatpak
- **Source Badges**: Visual indicators showing package source (deb/snap/flatpak)
- **Trust & Confinement**: Security badges showing trust level and sandboxing status
- **Smart Recommendations**: Package ranking system suggests the best source for each app
- **Duplicate Detection**: Identifies same app across providers
- **Structured Logging**: JSON-formatted debug logs for troubleshooting
- **Extensible Architecture**: Provider registry allows adding new backends

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    PolySynaptic GTK UI                       │
│  ┌──────────────┬──────────────┬──────────────────────────┐ │
│  │ Sources Pane │ Package List │      Detail View         │ │
│  │   [✓] APT    │              │  Name: Firefox           │ │
│  │   [✓] Snap   │  firefox     │  Source: [flatpak]       │ │
│  │   [✓] Flatpak│  vlc         │  Trust: [Verified ✓]     │ │
│  │              │  gimp        │  Confinement: [Sandbox]  │ │
│  └──────────────┴──────────────┴──────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Provider Registry                         │
│   ┌─────────────┬─────────────┬─────────────────────────┐   │
│   │ APTProvider │ SnapProvider│   FlatpakProvider       │   │
│   └──────┬──────┴──────┬──────┴───────────┬─────────────┘   │
└──────────┼─────────────┼──────────────────┼─────────────────┘
           ▼             ▼                  ▼
    ┌──────────┐  ┌───────────┐     ┌─────────────┐
    │ apt-pkg  │  │  snapd    │     │  flatpak    │
    │ library  │  │  daemon   │     │   daemon    │
    └──────────┘  └───────────┘     └─────────────┘
```

## Building

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt install build-essential autoconf automake libtool pkg-config \
    libgtk-3-dev libvte-2.91-dev libapt-pkg-dev libxapian-dev intltool
```

### Build from Source

```bash
cd polysynaptic
./autogen.sh
./configure --prefix=/usr/local
make
sudo make install
```

### Docker Development Environment

```bash
# Build development container
docker build -f polysynaptic/Dockerfile.dev -t polysynaptic-dev .

# Interactive shell
docker run -it --rm -v $(pwd):/workspace polysynaptic-dev

# Build in container
docker run --rm -v $(pwd):/workspace polysynaptic-dev \
    bash -c "cd /workspace/polysynaptic && ./autogen.sh && ./configure && make"
```

## Development

### Development Makefile

```bash
cd polysynaptic
make -f Makefile.dev setup    # First-time setup
make -f Makefile.dev build    # Build
make -f Makefile.dev test     # Run tests
make -f Makefile.dev lint     # Static analysis
```

### Running Tests

```bash
cd polysynaptic
make check
# Or specific tests:
./tests/test_backends
./tests/test_providers
```

## Project Structure

```
polysynaptic/
├── common/                 # Core library
│   ├── packagesourceprovider.h  # Provider interface
│   ├── aptprovider.*       # APT backend
│   ├── snapprovider.*      # Snap backend
│   ├── flatpakprovider.*   # Flatpak backend
│   ├── packageranking.*    # Scoring system
│   └── structuredlog.h     # JSON logging
├── gtk/                    # GTK3 UI
│   ├── rgunifiedview.*     # Multi-backend list
│   ├── rgsourcespane.*     # Source filter pane
│   └── rgdebugpanel.*      # Debug panel
├── tests/                  # Unit tests
├── Dockerfile.dev          # Dev container
└── Makefile.dev           # Dev workflow
```

## Adding New Providers

PolySynaptic uses dependency injection for extensibility:

```cpp
class MyProvider : public PackageSourceProvider {
    std::string getId() const override { return "my-backend"; }
    // ... implement interface methods
};

// Register at startup
ProviderRegistry::instance().registerProvider(
    std::make_shared<MyProvider>());
```

## License

GNU General Public License v2.0 - see [LICENSE](LICENSE)

Based on Synaptic Package Manager by Michael Vogt and others.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests: `make -f Makefile.dev test`
5. Submit a pull request
