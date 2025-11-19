#!/usr/bin/env python3
"""
Absolute minimal test - import main and see what errors occur
"""

import sys
import os

# Ensure we're importing from current directory
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

print("="*60)
print("Minimal Import Test")
print("="*60)
print()

# Test 1: Import main module
print("Test 1: Importing main module...")
try:
    import main
    print("  ✓ main module imported")
except Exception as e:
    print(f"  ✗ Import failed: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)

# Test 2: Try to import PyQt5 and create minimal window
print("\nTest 2: Creating minimal PyQt5 window...")
try:
    from PyQt5.QtWidgets import QApplication, QMainWindow, QLabel

    app = QApplication(sys.argv)
    window = QMainWindow()
    window.setWindowTitle("Test Window")
    window.setGeometry(100, 100, 400, 300)

    label = QLabel("If you can see this, PyQt5 works!", window)
    label.setGeometry(50, 100, 300, 50)

    window.show()
    print("  ✓ Test window created and shown")
    print("  You should see a window with text")
    print("  Press Ctrl+C to continue to next test...")

    try:
        app.exec_()
    except KeyboardInterrupt:
        print("\n  Continuing...")

except ImportError as e:
    print(f"  ✗ PyQt5 not available: {e}")
    print("  Install with: sudo apt install python3-pyqt5")
    sys.exit(1)
except Exception as e:
    print(f"  ✗ Window creation failed: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)

# Test 3: Create UPM window
print("\nTest 3: Creating UPM window...")
try:
    from PyQt5.QtWidgets import QApplication
    import main

    # Add verbose error handling to window creation
    app2 = QApplication(sys.argv)

    print("  Creating UnifiedPackageManager instance...")
    try:
        window = main.UnifiedPackageManager()
        print(f"  ✓ Instance created")
        print(f"    Window title: {window.windowTitle()}")
        print(f"    Tabs: {window.tabs.count()}")

        window.show()
        print(f"  ✓ Window shown")
        print(f"    Is visible: {window.isVisible()}")
        print(f"    Size: {window.width()}x{window.height()}")

        # Check central widget
        central = window.centralWidget()
        if central:
            print(f"  ✓ Central widget present: {central.width()}x{central.height()}")
        else:
            print("  ✗ NO CENTRAL WIDGET!")

        print("\n  Window should now be visible. Press Ctrl+C to exit.")
        app2.exec_()

    except AttributeError as e:
        print(f"  ✗ AttributeError during window creation: {e}")
        print("  This usually means a method call on a Qt object is wrong")
        import traceback
        traceback.print_exc()
        sys.exit(1)
    except Exception as e:
        print(f"  ✗ Window creation failed: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

except KeyboardInterrupt:
    print("\n\nTest interrupted by user")
except Exception as e:
    print(f"  ✗ Unexpected error: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)

print("\n" + "="*60)
print("All tests passed!")
print("="*60)
