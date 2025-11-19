#!/usr/bin/env python3
"""
Minimal diagnostic script to identify the exact cause of the black screen
This will show us exactly where the initialization fails
"""

import sys
import os
import traceback

# Add current directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

def test_step_by_step():
    """Test each initialization step individually"""

    print("="*60)
    print("UPM Black Screen Diagnostic Tool")
    print("="*60)
    print()

    # Step 1: Check PyQt5
    print("Step 1: Checking PyQt5...")
    try:
        from PyQt5.QtWidgets import QApplication, QMainWindow, QWidget, QLabel
        from PyQt5.QtCore import Qt
        print("  ✓ PyQt5 imported successfully")
    except ImportError as e:
        print(f"  ✗ PyQt5 import failed: {e}")
        print("  Install with: sudo apt install python3-pyqt5")
        return False

    # Step 2: Create QApplication
    print("\nStep 2: Creating QApplication...")
    try:
        app = QApplication(sys.argv)
        print("  ✓ QApplication created")
    except Exception as e:
        print(f"  ✗ Failed: {e}")
        traceback.print_exc()
        return False

    # Step 3: Test database
    print("\nStep 3: Testing database...")
    try:
        from database import DatabaseManager
        db = DatabaseManager(':memory:')  # Use in-memory database for testing
        print("  ✓ Database created")

        prefs = db.get_preferences()
        print(f"  ✓ Preferences loaded: {prefs}")
    except Exception as e:
        print(f"  ✗ Database failed: {e}")
        traceback.print_exc()
        return False

    # Step 4: Test logger
    print("\nStep 4: Testing logger...")
    try:
        from logger import get_logger
        logger = get_logger()
        print("  ✓ Logger initialized")
    except Exception as e:
        print(f"  ✗ Logger failed: {e}")
        traceback.print_exc()
        return False

    # Step 5: Test backends
    print("\nStep 5: Testing backends...")
    try:
        from apt_backend import AptBackend
        from snap_backend import SnapBackend
        from flatpak_backend import FlatpakBackend

        apt = AptBackend()
        print(f"  ✓ APT backend created (available: {apt.is_available()})")

        snap = SnapBackend()
        print(f"  ✓ Snap backend created (available: {snap.is_available()})")

        flatpak = FlatpakBackend()
        print(f"  ✓ Flatpak backend created (available: {flatpak.is_available()})")
    except Exception as e:
        print(f"  ✗ Backend failed: {e}")
        traceback.print_exc()
        return False

    # Step 6: Create main window (without loading apps)
    print("\nStep 6: Creating main window...")
    try:
        # Import but don't run main()
        import main as main_module

        # Monkey patch load_installed_apps to do nothing
        original_load = main_module.UnifiedPackageManager.load_installed_apps
        main_module.UnifiedPackageManager.load_installed_apps = lambda self: print("  (Skipping load_installed_apps for testing)")

        window = main_module.UnifiedPackageManager()
        print("  ✓ Main window instance created")
        print(f"  ✓ Window title: {window.windowTitle()}")
        print(f"  ✓ Window size: {window.size().width()}x{window.size().height()}")
        print(f"  ✓ Number of tabs: {window.tabs.count()}")

        for i in range(window.tabs.count()):
            print(f"    - Tab {i}: {window.tabs.tabText(i)}")

    except Exception as e:
        print(f"  ✗ Window creation failed: {e}")
        traceback.print_exc()
        return False

    # Step 7: Show the window
    print("\nStep 7: Displaying window...")
    try:
        window.show()
        print("  ✓ window.show() called successfully")
        print(f"  ✓ Window visible: {window.isVisible()}")
        print(f"  ✓ Window size after show: {window.size().width()}x{window.size().height()}")

        # Check if central widget exists
        central = window.centralWidget()
        if central:
            print(f"  ✓ Central widget exists: {type(central).__name__}")
            print(f"  ✓ Central widget size: {central.size().width()}x{central.size().height()}")
        else:
            print("  ✗ No central widget!")

    except Exception as e:
        print(f"  ✗ Show failed: {e}")
        traceback.print_exc()
        return False

    print("\n" + "="*60)
    print("Diagnostic complete - window should be visible!")
    print("If you see a black screen, the issue is with widget rendering")
    print("If you see nothing, the issue is with window creation")
    print("="*60)
    print("\nPress Ctrl+C to exit...")

    # Keep window open for inspection
    try:
        sys.exit(app.exec_())
    except KeyboardInterrupt:
        print("\nExiting...")
        return True

if __name__ == '__main__':
    try:
        success = test_step_by_step()
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"\n✗ Unexpected error: {e}")
        traceback.print_exc()
        sys.exit(1)
