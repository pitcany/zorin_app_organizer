#!/bin/bash
# Installation script for Unified Package Manager
# Supports both user and system-wide installation

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print functions
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
    INSTALL_TYPE="system"
    INSTALL_PREFIX="/usr/local"
    print_info "Running as root - installing system-wide"
else
    INSTALL_TYPE="user"
    INSTALL_PREFIX="$HOME/.local"
    print_info "Running as user - installing for current user"
fi

print_info "Unified Package Manager Installation Script"
echo "==========================================="
echo ""

# Check Python version
print_info "Checking Python version..."
if ! command -v python3 &> /dev/null; then
    print_error "Python 3 is not installed. Please install Python 3.6 or higher."
    exit 1
fi

PYTHON_VERSION=$(python3 -c 'import sys; print(".".join(map(str, sys.version_info[:2])))')
print_success "Python $PYTHON_VERSION found"

# Check pip
print_info "Checking pip..."
if ! command -v pip3 &> /dev/null; then
    print_error "pip3 is not installed. Please install pip3."
    exit 1
fi
print_success "pip3 found"

# Install system dependencies
print_info "Checking system dependencies..."

# Check for PyQt5
if ! python3 -c "import PyQt5" 2>/dev/null; then
    print_warning "PyQt5 not found. Installing..."
    if [ "$INSTALL_TYPE" = "system" ]; then
        apt-get update
        apt-get install -y python3-pyqt5
    else
        print_info "Installing PyQt5 via pip (user installation)..."
        pip3 install --user PyQt5
    fi
    print_success "PyQt5 installed"
else
    print_success "PyQt5 is already installed"
fi

# Check for pkexec
if ! command -v pkexec &> /dev/null; then
    print_warning "pkexec not found. This is required for privilege escalation."
    if [ "$INSTALL_TYPE" = "system" ]; then
        apt-get install -y policykit-1
        print_success "policykit-1 installed"
    else
        print_error "Please install policykit-1: sudo apt install policykit-1"
        exit 1
    fi
else
    print_success "pkexec is available"
fi

# Install Python dependencies
print_info "Installing Python dependencies..."
if [ "$INSTALL_TYPE" = "system" ]; then
    pip3 install -r requirements.txt
else
    pip3 install --user -r requirements.txt
fi
print_success "Python dependencies installed"

# Install the application
print_info "Installing Unified Package Manager..."
if [ "$INSTALL_TYPE" = "system" ]; then
    pip3 install .
else
    pip3 install --user .
fi
print_success "Application installed"

# Install desktop entry
print_info "Installing desktop entry..."
DESKTOP_DIR="$INSTALL_PREFIX/share/applications"
mkdir -p "$DESKTOP_DIR"
cp data/upm.desktop "$DESKTOP_DIR/"
print_success "Desktop entry installed to $DESKTOP_DIR"

# Install icon
print_info "Installing icon..."
ICON_DIR="$INSTALL_PREFIX/share/icons/hicolor/scalable/apps"
mkdir -p "$ICON_DIR"
cp data/upm.svg "$ICON_DIR/"
print_success "Icon installed to $ICON_DIR"

# Update desktop database (if available)
if command -v update-desktop-database &> /dev/null; then
    print_info "Updating desktop database..."
    update-desktop-database "$DESKTOP_DIR" 2>/dev/null || true
    print_success "Desktop database updated"
fi

# Update icon cache (if available)
if command -v gtk-update-icon-cache &> /dev/null; then
    print_info "Updating icon cache..."
    if [ "$INSTALL_TYPE" = "system" ]; then
        gtk-update-icon-cache -f -t /usr/local/share/icons/hicolor 2>/dev/null || true
    else
        gtk-update-icon-cache -f -t "$HOME/.local/share/icons/hicolor" 2>/dev/null || true
    fi
    print_success "Icon cache updated"
fi

# Check optional package managers
echo ""
print_info "Checking optional package managers..."

if command -v apt &> /dev/null; then
    print_success "APT is available"
else
    print_warning "APT is not available (expected on non-Debian systems)"
fi

if command -v snap &> /dev/null; then
    print_success "Snap is available"
    if systemctl is-active --quiet snapd; then
        print_success "snapd service is running"
    else
        print_warning "snapd service is not running. Start with: sudo systemctl start snapd"
    fi
else
    print_warning "Snap is not installed. Install with: sudo apt install snapd"
fi

if command -v flatpak &> /dev/null; then
    print_success "Flatpak is available"
    if flatpak remotes | grep -q flathub; then
        print_success "Flathub repository is configured"
    else
        print_warning "Flathub not configured. Add with:"
        echo "  flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo"
    fi
else
    print_warning "Flatpak is not installed. Install with: sudo apt install flatpak"
fi

# Final instructions
echo ""
echo "==========================================="
print_success "Installation complete!"
echo ""
print_info "You can now run the application with:"
echo "  - From terminal: upm"
echo "  - From application menu: Unified Package Manager"
echo ""

if [ "$INSTALL_TYPE" = "user" ]; then
    print_info "Note: User installation. Make sure ~/.local/bin is in your PATH"
    if ! echo "$PATH" | grep -q "$HOME/.local/bin"; then
        print_warning "~/.local/bin is not in your PATH. Add it with:"
        echo '  echo "export PATH=\"$HOME/.local/bin:$PATH\"" >> ~/.bashrc'
        echo "  source ~/.bashrc"
    fi
fi

echo ""
print_info "For documentation, see: README.md"
print_info "For issues, visit: https://github.com/pitcany/zorin_app_organizer/issues"
echo ""
