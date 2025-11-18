# Unified Package Manager (UPM) for Zorin OS

A comprehensive, user-friendly graphical package manager for Zorin OS that unifies APT, Snap, and Flatpak package management in a single intuitive interface.

## Features

### Core Functionality
- **Unified Search**: Search for applications across APT repositories, Snap Store, and Flathub simultaneously
- **Easy Installation**: Install packages from any supported repository with a single click
- **Package Management**: View and manage all installed applications in one place
- **Smart Uninstallation**: Remove applications with confirmation dialogs and proper cleanup

### Third-Party Repository Support
- **APT/Ubuntu Repositories**: Access to default Ubuntu packages and custom PPAs
- **Snap Store**: Search and install snaps from the Snap Store
- **Flathub**: Browse and install Flatpak applications from Flathub

### Advanced Features
- **Settings Panel**: Configure repository access, notifications, and database behavior
- **Comprehensive Logging**: Track all installations, removals, and system events
- **In-App Notifications**: Real-time alerts for successful/failed operations
- **Log Export**: Export logs to text files for debugging or record-keeping
- **Customizable Preferences**: Toggle repositories, set log retention, and more

## Screenshots

### Search & Install Tab
Search across multiple package repositories and install applications with one click.

### Installed Applications Tab
View all installed applications, filter by package type, and uninstall with ease.

### Settings Tab
Configure repository access, notifications, and system preferences.

### Logs Tab
View detailed logs of all operations and export them for analysis.

## Installation

### Prerequisites

Ensure your system has the following installed:

```bash
# Python 3.6 or higher
python3 --version

# pip (Python package manager)
sudo apt install python3-pip

# PyQt5 dependencies
sudo apt install python3-pyqt5

# Optional: Package managers
sudo apt install snapd flatpak
```

### Setup Flathub (Recommended)

To enable Flatpak support with Flathub:

```bash
# Install Flatpak if not already installed
sudo apt install flatpak

# Add Flathub repository
flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo

# Restart your system to complete setup
sudo reboot
```

### Install UPM

1. **Clone the repository:**
   ```bash
   git clone https://github.com/pitcany/zorin_app_organizer.git
   cd zorin_app_organizer
   ```

2. **Install Python dependencies:**
   ```bash
   pip3 install -r requirements.txt
   ```

3. **Make the main script executable:**
   ```bash
   chmod +x main.py
   ```

4. **Run the application:**
   ```bash
   python3 main.py
   ```

### Create Desktop Launcher (Optional)

To create a desktop launcher for easy access:

```bash
cat > ~/.local/share/applications/upm.desktop <<EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=Unified Package Manager
Comment=Manage APT, Snap, and Flatpak packages
Exec=python3 $(pwd)/main.py
Icon=system-software-install
Terminal=false
Categories=System;PackageManager;
EOF

# Update desktop database
update-desktop-database ~/.local/share/applications/
```

## Usage Guide

### Searching for Applications

1. Navigate to the **Search & Install** tab
2. Enter a search term (e.g., "firefox", "gimp", "vlc")
3. Click **Search** or press Enter
4. Browse results from all enabled repositories
5. Filter by repository type using the dropdown (All, APT, Snap, Flatpak)

### Installing Applications

1. Find the application in search results
2. Click the **Install** button next to the application
3. Confirm the installation when prompted
4. Enter your password if required (via pkexec dialog)
5. Wait for installation to complete
6. Check notifications for success/failure status

### Viewing Installed Applications

1. Navigate to the **Installed Applications** tab
2. Click **Refresh** to update the list
3. Filter by package type (All, APT, Snap, Flatpak)
4. View details including name, version, and source

### Uninstalling Applications

1. Go to **Installed Applications** tab
2. Find the application you want to remove
3. Click the **Uninstall** button
4. Confirm removal when prompted
5. Enter your password if required
6. Check notifications for completion status

### Configuring Settings

1. Navigate to the **Settings** tab
2. **Repository Settings:**
   - Toggle APT, Snap, or Flatpak repositories on/off
   - Disabled repositories won't appear in search results
3. **Notification Settings:**
   - Enable/disable pop-up notifications
4. **Database Settings:**
   - Configure auto-save behavior
   - Set log retention period (in days)
5. Click **Save Settings** to apply changes

### Viewing and Exporting Logs

1. Go to the **Logs** tab
2. Click **Refresh** to load latest logs
3. Review installation/uninstallation history
4. **Export Logs:**
   - Click **Export Logs**
   - Choose save location
   - Logs are exported as text files
5. **Clear Logs:**
   - Click **Clear Logs** to remove old entries
   - Respects log retention settings

## Architecture

### Project Structure

```
zorin_app_organizer/
├── main.py                 # Main application entry point & GUI
├── database.py            # SQLite database management
├── logger.py              # Logging and notification system
├── apt_backend.py         # APT package manager backend
├── snap_backend.py        # Snap Store backend
├── flatpak_backend.py     # Flatpak/Flathub backend
├── requirements.txt       # Python dependencies
├── .gitignore            # Git ignore rules
└── README.md             # This file
```

### Database Schema

The application uses SQLite with the following tables:

#### `installed_apps`
Tracks installed applications across all package managers.
- `id`, `name`, `package_name`, `package_type`, `source_repo`, `version`, `description`, `install_date`

#### `user_prefs`
Stores user preferences and settings.
- `repo_flathub_enabled`, `repo_snapd_enabled`, `repo_apt_enabled`
- `notifications_enabled`, `log_retention_days`, `auto_save_metadata`

#### `logs`
Records all user actions and system events.
- `timestamp`, `action_type`, `package_name`, `package_type`, `status`, `message`, `error_details`

#### `custom_repositories`
Manages custom APT repositories (PPAs).
- `repo_name`, `repo_url`, `repo_type`, `enabled`, `added_date`

### Backend Modules

#### APT Backend (`apt_backend.py`)
- Interfaces with `apt-cache` and `apt` commands
- Supports package search, installation, and removal
- Handles PPA management
- Uses `pkexec` for privilege escalation

#### Snap Backend (`snap_backend.py`)
- Uses `snap` CLI for all operations
- Supports classic confinement detection
- Handles snap refresh and updates
- Checks snapd service status

#### Flatpak Backend (`flatpak_backend.py`)
- Primary: Uses Flathub API v2 for fast searching
- Fallback: CLI-based search via `flatpak search`
- Supports user and system-wide installations
- Handles Flathub repository setup

## Troubleshooting

### Snap Store Not Working

**Issue:** "Snapd service is not running"

**Solution:**
```bash
sudo systemctl start snapd
sudo systemctl enable snapd
```

### Flathub Not Configured

**Issue:** "Flathub repository is not configured"

**Solution:**
```bash
flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
```

### Permission Errors

**Issue:** Installation fails with permission errors

**Solution:**
- Ensure `pkexec` is installed: `sudo apt install policykit-1`
- Make sure you're in the sudo group: `groups $USER`
- Try running with sudo (not recommended for regular use): `sudo python3 main.py`

### PyQt5 Import Errors

**Issue:** "No module named 'PyQt5'"

**Solution:**
```bash
sudo apt install python3-pyqt5
# Or via pip:
pip3 install PyQt5
```

### Database Locked Errors

**Issue:** "database is locked"

**Solution:**
- Close any other instances of UPM
- Check for stale lock files: `rm upm.db-journal`
- Restart the application

## Security Considerations

### Privilege Escalation
- UPM uses `pkexec` for GUI-based privilege escalation
- Never stores passwords or credentials
- Only requests privileges for installation/removal operations

### Third-Party Repositories
- **Flathub**: Sandboxed applications with limited system access
- **Snap Store**: Confined applications with permission system
- **APT/PPAs**: Full system access - add trusted sources only

### Recommendations
- Only enable repositories you trust
- Review application permissions before installation
- Keep UPM and all packages updated
- Export logs regularly for audit purposes

## Contributing

Contributions are welcome! Please follow these guidelines:

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/your-feature`
3. Make your changes and test thoroughly
4. Commit with clear messages: `git commit -m "Add feature: description"`
5. Push to your fork: `git push origin feature/your-feature`
6. Submit a pull request

### Development Setup

```bash
# Clone your fork
git clone https://github.com/YOUR_USERNAME/zorin_app_organizer.git
cd zorin_app_organizer

# Install development dependencies
pip3 install -r requirements.txt

# Run tests (when available)
python3 -m pytest

# Run the application
python3 main.py
```

## Known Issues

- **Large Package Lists**: Loading all installed APT packages can be slow
- **Network Timeouts**: Flathub API may timeout on slow connections
- **Classic Snaps**: Some snaps require `--classic` flag (not auto-detected yet)

## Future Enhancements

- [ ] Package update checker and bulk update functionality
- [ ] Application ratings and reviews integration
- [ ] Package screenshots and detailed app pages
- [ ] Custom APT PPA management UI
- [ ] Automatic Flathub setup wizard
- [ ] Package dependency viewer
- [ ] System resource usage monitoring
- [ ] Dark mode theme support
- [ ] Multi-language support
- [ ] Desktop notifications (libnotify)

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- **Zorin OS Team**: For creating an excellent Linux distribution
- **Ubuntu/Canonical**: For APT and Snap technologies
- **Flathub/Flatpak**: For universal application distribution
- **PyQt Project**: For the excellent Qt bindings for Python

## Support

For issues, questions, or suggestions:
- **GitHub Issues**: https://github.com/pitcany/zorin_app_organizer/issues
- **Documentation**: See this README
- **Logs**: Export logs from the Logs tab for debugging

## Version History

### v1.0.0 (Initial Release)
- Unified search across APT, Snap, and Flatpak
- Installation and removal of packages
- Settings panel with repository toggles
- Comprehensive logging system
- In-app notifications
- Log export functionality

---

**Built with Python and PyQt5 for Zorin OS**
