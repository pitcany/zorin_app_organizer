#!/bin/bash
# Uninstallation script for UPM systemd backup service

set -e

echo "Uninstalling UPM Backup systemd service..."

# Check if running as root or with sudo
if [ "$EUID" -eq 0 ]; then
    SUDO=""
else
    SUDO="sudo"
fi

REAL_USER="${SUDO_USER:-$USER}"
REAL_HOME=$(eval echo "~$REAL_USER")
USER_SYSTEMD_DIR="$REAL_HOME/.config/systemd/user"

# 1. Stop and disable timer
echo "Stopping and disabling backup timer..."
systemctl --user stop upm-backup.timer 2>/dev/null || true
systemctl --user disable upm-backup.timer 2>/dev/null || true

# 2. Remove systemd unit files
echo "Removing systemd units..."
rm -f "$USER_SYSTEMD_DIR/upm-backup.service"
rm -f "$USER_SYSTEMD_DIR/upm-backup.timer"

# 3. Reload systemd
echo "Reloading systemd user daemon..."
systemctl --user daemon-reload

# 4. Remove backup script
echo "Removing backup script..."
$SUDO rm -f /usr/local/bin/upm-backup

echo "✅ Uninstallation complete!"
