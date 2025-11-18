# Makefile for Unified Package Manager
# Provides convenient targets for building, installing, and testing

.PHONY: help install uninstall build clean test deb sdist wheel upload-test upload run dev-install

# Default target
help:
	@echo "Unified Package Manager - Build Automation"
	@echo "==========================================="
	@echo ""
	@echo "Available targets:"
	@echo "  make install       - Install the application (user installation)"
	@echo "  make install-sys   - Install the application (system-wide, requires root)"
	@echo "  make uninstall     - Uninstall the application"
	@echo "  make build         - Build the Python package"
	@echo "  make deb           - Build Debian package (.deb)"
	@echo "  make sdist         - Build source distribution"
	@echo "  make wheel         - Build wheel distribution"
	@echo "  make clean         - Clean build artifacts"
	@echo "  make test          - Run tests (if available)"
	@echo "  make run           - Run the application"
	@echo "  make dev-install   - Install in development mode"
	@echo "  make upload-test   - Upload to TestPyPI"
	@echo "  make upload        - Upload to PyPI"
	@echo ""

# Install the application (user)
install:
	@echo "Installing Unified Package Manager (user installation)..."
	./install.sh

# Install the application (system-wide)
install-sys:
	@echo "Installing Unified Package Manager (system-wide)..."
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "Error: System-wide installation requires root. Use: sudo make install-sys"; \
		exit 1; \
	fi
	./install.sh

# Uninstall the application
uninstall:
	@echo "Uninstalling Unified Package Manager..."
	./uninstall.sh

# Build Python package
build: clean
	@echo "Building Python package..."
	python3 setup.py build
	@echo "Build complete!"

# Build Debian package
deb: clean
	@echo "Building Debian package..."
	@if ! command -v dpkg-buildpackage >/dev/null 2>&1; then \
		echo "Error: dpkg-buildpackage not found. Install with: sudo apt install dpkg-dev"; \
		exit 1; \
	fi
	dpkg-buildpackage -us -uc -b
	@echo "Debian package built successfully!"
	@echo "Package location: ../unified-package-manager_*.deb"

# Build source distribution
sdist: clean
	@echo "Building source distribution..."
	python3 setup.py sdist
	@echo "Source distribution created in dist/"

# Build wheel distribution
wheel: clean
	@echo "Building wheel distribution..."
	python3 setup.py bdist_wheel
	@echo "Wheel distribution created in dist/"

# Build both sdist and wheel
dist: sdist wheel
	@echo "All distributions created in dist/"

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf build/
	rm -rf dist/
	rm -rf *.egg-info/
	rm -rf debian/unified-package-manager/
	rm -rf debian/.debhelper/
	rm -rf debian/files
	rm -rf debian/*.substvars
	rm -rf debian/*.log
	rm -f ../*.deb
	rm -f ../*.buildinfo
	rm -f ../*.changes
	find . -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
	find . -type f -name "*.pyc" -delete
	find . -type f -name "*.pyo" -delete
	find . -type f -name "*.so" -delete
	rm -f upm.db
	@echo "Clean complete!"

# Run tests (placeholder - add when tests are available)
test:
	@echo "Running tests..."
	@echo "Note: Test suite not yet implemented"
	# python3 -m pytest tests/

# Run the application
run:
	@echo "Running Unified Package Manager..."
	python3 main.py

# Development installation (editable)
dev-install:
	@echo "Installing in development mode..."
	pip3 install --user -e .
	@echo "Development installation complete!"

# Check package validity
check:
	@echo "Checking package..."
	python3 setup.py check
	@if command -v twine >/dev/null 2>&1; then \
		python3 setup.py sdist bdist_wheel; \
		twine check dist/*; \
	else \
		echo "Install twine for full package checking: pip install twine"; \
	fi

# Upload to TestPyPI
upload-test: dist
	@echo "Uploading to TestPyPI..."
	@if ! command -v twine >/dev/null 2>&1; then \
		echo "Error: twine not found. Install with: pip install twine"; \
		exit 1; \
	fi
	twine upload --repository testpypi dist/*

# Upload to PyPI
upload: dist
	@echo "Uploading to PyPI..."
	@if ! command -v twine >/dev/null 2>&1; then \
		echo "Error: twine not found. Install with: pip install twine"; \
		exit 1; \
	fi
	@echo "WARNING: This will upload to the real PyPI!"
	@read -p "Are you sure? [y/N] " -n 1 -r; \
	echo; \
	if [[ $$REPLY =~ ^[Yy]$$ ]]; then \
		twine upload dist/*; \
	else \
		echo "Upload cancelled"; \
	fi

# Install build dependencies
install-build-deps:
	@echo "Installing build dependencies..."
	sudo apt-get update
	sudo apt-get install -y python3-setuptools python3-wheel python3-pip \
		dpkg-dev debhelper dh-python python3-all \
		python3-pyqt5 python3-requests policykit-1
	pip3 install --user twine build
	@echo "Build dependencies installed!"

# Format code (requires black)
format:
	@if command -v black >/dev/null 2>&1; then \
		echo "Formatting code with black..."; \
		black *.py; \
	else \
		echo "black not installed. Install with: pip install black"; \
	fi

# Lint code (requires pylint)
lint:
	@if command -v pylint >/dev/null 2>&1; then \
		echo "Linting code with pylint..."; \
		pylint *.py; \
	else \
		echo "pylint not installed. Install with: pip install pylint"; \
	fi

# Show package info
info:
	@echo "Package Information"
	@echo "==================="
	@echo "Name: unified-package-manager"
	@echo "Version: 1.0.0"
	@echo "Python: $$(python3 --version)"
	@echo "Location: $$(pwd)"
	@echo ""
	@echo "Dependencies:"
	@cat requirements.txt
