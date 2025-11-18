# UPM CLI Usage Guide

## Commands

### Search

Search for packages across all enabled package managers:

```bash
upm search <query>
```

Example:
```bash
upm search firefox
```

Output shows results grouped by package manager (APT, Flatpak, Snap).

### List Installed

List all installed packages from all package managers:

```bash
upm list
```

Output shows package name and version, grouped by source.

### Install

Install a package from a specific package manager:

```bash
upm install <package-manager> <package-id>
```

Package managers: `apt`, `flatpak`, `snap`

Examples:
```bash
upm install apt python3-pip
upm install flatpak org.mozilla.Firefox
upm install snap firefox
```

**Note:** Installation requires elevated privileges (sudo/pkexec).

### Remove

Remove an installed package:

```bash
upm remove <package-manager> <package-id>
```

Examples:
```bash
upm remove apt python3-pip
upm remove flatpak org.mozilla.Firefox
upm remove snap firefox
```

**Note:** Removal requires elevated privileges.

## Package Manager Availability

UPM automatically detects which package managers are available on your system:

- **APT**: Always available on Ubuntu/Debian-based systems
- **Flatpak**: Available if `flatpak` command is installed
- **Snap**: Available if `snap` command is installed and snapd is running

If a package manager is not available, it will be skipped during search and an error shown for install/remove operations.

## Tips

- Use specific search terms for better results
- Package IDs differ between package managers:
  - APT: Usually lowercase (e.g., `firefox`, `python3`)
  - Flatpak: Reverse domain notation (e.g., `org.mozilla.Firefox`)
  - Snap: Usually lowercase (e.g., `firefox`, `spotify`)
- Check search results to find the correct package ID before installing

## Troubleshooting

### "Package manager not available"

Make sure the package manager is installed:

```bash
# Check Flatpak
which flatpak

# Check Snap
which snap
snap version
```

### "Installation failed"

Common causes:
- Insufficient permissions (run with sudo)
- Package not found (check package ID)
- Network connectivity issues
- Disk space constraints

### Tests fail for certain package managers

Tests will automatically skip if a package manager is not available. This is expected behavior on systems without all three package managers installed.
