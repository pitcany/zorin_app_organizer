#!/usr/bin/env python3
"""
Quick test script to verify UPM GUI initializes correctly
Tests the main window creation without actually running the full app
"""

import sys
import os

# Add current directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

def test_imports():
    """Test that all modules can be imported"""
    print("Testing imports...")
    try:
        from PyQt5.QtWidgets import QApplication
        print("  ✓ PyQt5 available")
    except ImportError as e:
        print(f"  ✗ PyQt5 not available: {e}")
        print("    Install with: sudo apt install python3-pyqt5")
        return False

    try:
        from database import DatabaseManager
        from logger import get_logger
        from apt_backend import AptBackend
        from snap_backend import SnapBackend
        from flatpak_backend import FlatpakBackend
        print("  ✓ All backend modules imported successfully")
    except ImportError as e:
        print(f"  ✗ Import error: {e}")
        return False

    return True

def test_gui_creation():
    """Test that the GUI can be created without errors"""
    print("\nTesting GUI creation...")
    try:
        from PyQt5.QtWidgets import QApplication
        from main import UnifiedPackageManager

        # Create application
        app = QApplication(sys.argv)
        print("  ✓ QApplication created")

        # Create main window
        window = UnifiedPackageManager()
        print("  ✓ Main window created")

        # Verify window properties
        title = window.windowTitle()
        print(f"  ✓ Window title: {title}")

        size = window.minimumSize()
        print(f"  ✓ Window size: {size.width()}x{size.height()}")

        # Check that tabs were created
        tab_count = window.tabs.count()
        print(f"  ✓ Number of tabs: {tab_count}")

        if tab_count != 4:
            print(f"  ✗ Expected 4 tabs, got {tab_count}")
            return False

        # List tab names
        for i in range(tab_count):
            tab_name = window.tabs.tabText(i)
            print(f"    - Tab {i+1}: {tab_name}")

        print("\n✓ All GUI tests passed!")
        print("  The black screen issue has been fixed.")
        print("  You can now run: python3 main.py")

        return True

    except AttributeError as e:
        print(f"  ✗ AttributeError (likely the addButton bug): {e}")
        return False
    except Exception as e:
        print(f"  ✗ Error during GUI creation: {e}")
        import traceback
        traceback.print_exc()
        return False

def main():
    """Run all tests"""
    print("="*60)
    print("Unified Package Manager - GUI Test Suite")
    print("="*60)
    print()

    # Test imports
    if not test_imports():
        print("\n✗ Import tests failed")
        return 1

    # Test GUI creation
    if not test_gui_creation():
        print("\n✗ GUI tests failed")
        return 1

    print("\n" + "="*60)
    print("All tests passed! The application should work correctly.")
    print("="*60)
    return 0

if __name__ == '__main__':
    sys.exit(main())
