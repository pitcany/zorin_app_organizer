#!/bin/bash
# Uninstallation script for Unified Package Manager

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root
if [ "$EUID" -eq 0 ]; then
    INSTALL_PREFIX="/usr/local"
    print_info "Running as root - uninstalling system-wide installation"
else
    INSTALL_PREFIX="$HOME/.local"
    print_info "Running as user - uninstalling user installation"
fi

print_info "Unified Package Manager Uninstallation Script"
echo "==========================================="
echo ""

# Ask for confirmation
read -p "Are you sure you want to uninstall Unified Package Manager? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    print_info "Uninstallation cancelled"
    exit 0
fi

# Uninstall Python package
print_info "Uninstalling Python package..."
pip3 uninstall -y zorin-unified-package-manager 2>/dev/null || print_warning "Package not found via pip"
print_success "Python package uninstalled"

# Remove desktop entry
DESKTOP_FILE="$INSTALL_PREFIX/share/applications/upm.desktop"
if [ -f "$DESKTOP_FILE" ]; then
    print_info "Removing desktop entry..."
    rm -f "$DESKTOP_FILE"
    print_success "Desktop entry removed"
fi

# Remove icon
ICON_FILE="$INSTALL_PREFIX/share/icons/hicolor/scalable/apps/upm.svg"
if [ -f "$ICON_FILE" ]; then
    print_info "Removing icon..."
    rm -f "$ICON_FILE"
    print_success "Icon removed"
fi

# Remove documentation
DOC_DIR="$INSTALL_PREFIX/share/doc/unified-package-manager"
if [ -d "$DOC_DIR" ]; then
    print_info "Removing documentation..."
    rm -rf "$DOC_DIR"
    print_success "Documentation removed"
fi

# Update desktop database
if command -v update-desktop-database &> /dev/null; then
    print_info "Updating desktop database..."
    update-desktop-database "$INSTALL_PREFIX/share/applications" 2>/dev/null || true
    print_success "Desktop database updated"
fi

# Update icon cache
if command -v gtk-update-icon-cache &> /dev/null; then
    print_info "Updating icon cache..."
    gtk-update-icon-cache -f -t "$INSTALL_PREFIX/share/icons/hicolor" 2>/dev/null || true
    print_success "Icon cache updated"
fi

# Ask about user data
echo ""
read -p "Do you want to remove user data (database and logs)? (y/N) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    print_info "Removing user data..."
    rm -f "$HOME/.upm.db" 2>/dev/null || true
    rm -f upm.db 2>/dev/null || true
    rm -rf "$HOME/.config/upm" 2>/dev/null || true
    rm -rf logs/ 2>/dev/null || true
    print_success "User data removed"
else
    print_info "User data preserved"
fi

echo ""
print_success "Uninstallation complete!"
echo ""
