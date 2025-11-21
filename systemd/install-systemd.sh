#!/bin/bash
# Installation script for UPM systemd backup service
# This script installs and enables automated database backups

set -e

echo "Installing UPM Backup systemd service..."

# Check if running as root or with sudo
if [ "$EUID" -eq 0 ]; then
    SUDO=""
    echo "Running as root"
else
    SUDO="sudo"
    echo "Running with sudo"
fi

# Get the current user (even if running via sudo)
REAL_USER="${SUDO_USER:-$USER}"
REAL_HOME=$(eval echo "~$REAL_USER")

echo "Installing for user: $REAL_USER"
echo "Home directory: $REAL_HOME"

# 1. Copy Python backup script to /usr/local/bin
echo "Installing backup script..."
$SUDO cp backup.py /usr/local/bin/upm-backup
$SUDO chmod 755 /usr/local/bin/upm-backup

# Make it executable with proper shebang
$SUDO sed -i '1s|^|#!/usr/bin/env python3\n|' /usr/local/bin/upm-backup 2>/dev/null || true

# 2. Install systemd service files to user systemd directory
USER_SYSTEMD_DIR="$REAL_HOME/.config/systemd/user"
echo "Creating user systemd directory: $USER_SYSTEMD_DIR"
mkdir -p "$USER_SYSTEMD_DIR"

echo "Installing systemd units..."
cp systemd/upm-backup.service "$USER_SYSTEMD_DIR/"
cp systemd/upm-backup.timer "$USER_SYSTEMD_DIR/"

# 3. Reload systemd user daemon
echo "Reloading systemd user daemon..."
systemctl --user daemon-reload

# 4. Enable and start the timer
echo "Enabling backup timer..."
systemctl --user enable upm-backup.timer
systemctl --user start upm-backup.timer

# 5. Check status
echo ""
echo "✅ Installation complete!"
echo ""
echo "Backup timer status:"
systemctl --user status upm-backup.timer --no-pager || true
echo ""
echo "Next scheduled backup:"
systemctl --user list-timers upm-backup.timer --no-pager
echo ""
echo "Manual backup commands:"
echo "  Run backup now:     systemctl --user start upm-backup.service"
echo "  Check backup logs:  journalctl --user -u upm-backup.service"
echo "  Stop timer:         systemctl --user stop upm-backup.timer"
echo "  Disable timer:      systemctl --user disable upm-backup.timer"
echo ""
