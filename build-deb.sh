#!/bin/bash
# Script to build Debian package (.deb) for Unified Package Manager

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

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

print_info "Unified Package Manager - Debian Package Builder"
echo "=================================================="
echo ""

# Check for required tools
print_info "Checking for required build tools..."

MISSING_TOOLS=()

if ! command -v dpkg-buildpackage &> /dev/null; then
    MISSING_TOOLS+=("dpkg-dev")
fi

if ! command -v debuild &> /dev/null; then
    MISSING_TOOLS+=("devscripts")
fi

if ! command -v dh &> /dev/null; then
    MISSING_TOOLS+=("debhelper")
fi

if ! dpkg -l | grep -q "dh-python"; then
    MISSING_TOOLS+=("dh-python")
fi

if [ ${#MISSING_TOOLS[@]} -gt 0 ]; then
    print_error "Missing required build tools: ${MISSING_TOOLS[*]}"
    print_info "Install with: sudo apt install ${MISSING_TOOLS[*]}"
    exit 1
fi

print_success "All required build tools are installed"

# Check for build dependencies
print_info "Checking build dependencies..."
if ! dpkg -l | grep -q "python3-pyqt5"; then
    print_warning "python3-pyqt5 not installed - installing..."
    sudo apt install -y python3-pyqt5
fi

if ! dpkg -l | grep -q "python3-requests"; then
    print_warning "python3-requests not installed - installing..."
    sudo apt install -y python3-requests
fi

print_success "Build dependencies satisfied"

# Clean previous builds
print_info "Cleaning previous builds..."
make clean 2>/dev/null || true
rm -f ../*.deb ../*.buildinfo ../*.changes 2>/dev/null || true
print_success "Cleaned!"

# Build the package
print_info "Building Debian package..."
echo ""

dpkg-buildpackage -us -uc -b

echo ""
print_success "Debian package built successfully!"
echo ""

# List created packages
DEB_FILE=$(ls -1 ../unified-package-manager_*.deb 2>/dev/null | head -1)

if [ -n "$DEB_FILE" ]; then
    print_info "Package details:"
    echo "  File: $DEB_FILE"
    echo "  Size: $(du -h "$DEB_FILE" | cut -f1)"
    echo ""

    print_info "Package contents:"
    dpkg-deb --contents "$DEB_FILE" | head -20

    if [ $(dpkg-deb --contents "$DEB_FILE" | wc -l) -gt 20 ]; then
        echo "  ... ($(dpkg-deb --contents "$DEB_FILE" | wc -l) files total)"
    fi

    echo ""
    print_info "To install the package, run:"
    echo "  sudo dpkg -i $DEB_FILE"
    echo ""
    print_info "Or to install with dependencies:"
    echo "  sudo apt install $DEB_FILE"
    echo ""
else
    print_error "Could not find built package!"
    exit 1
fi

print_success "Build complete!"
