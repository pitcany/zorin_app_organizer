"""
Snap Store backend for Unified Package Manager
Handles Snap operations including search, install, uninstall, and package info
"""

import subprocess
import json
import re
from typing import List, Dict, Optional, Tuple
from rate_limiter import rate_limit
from package_verification import PackageVerifier


def validate_snap_name(snap_name: str) -> bool:
    """
    P0 Fix: Validate snap name to prevent command injection

    Snap names can contain lowercase letters, numbers, and hyphens
    """
    if not snap_name or len(snap_name) > 200:
        return False

    # Snap naming rules
    pattern = r'^[a-z0-9][a-z0-9-]*$'
    return bool(re.match(pattern, snap_name))


class SnapBackend:
    """Backend for Snap package management"""

    def __init__(self):
        """Initialize Snap backend"""
        self.package_type = "snap"
        self.source_repo = "Snap Store"
        self.verifier = PackageVerifier()

    def is_available(self) -> bool:
        """Check if Snap is available on the system"""
        try:
            result = subprocess.run(['which', 'snap'],
                                  capture_output=True,
                                  text=True,
                                  timeout=5)
            return result.returncode == 0
        except Exception:
            return False

    def is_snapd_running(self) -> bool:
        """Check if snapd service is running"""
        try:
            result = subprocess.run(
                ['systemctl', 'is-active', 'snapd'],
                capture_output=True,
                text=True,
                timeout=5
            )
            return result.returncode == 0 and result.stdout.strip() == 'active'
        except Exception:
            return False

    @rate_limit('snap', wait=True)
    def search_packages(self, query: str, limit: int = 100) -> List[Dict]:
        """Search for packages in Snap Store

        Args:
            query: Search query string
            limit: Maximum number of results to return

        Returns:
            List of package dictionaries with name, description, version, etc.
        """
        if not query.strip():
            return []

        try:
            # Use snap find command
            result = subprocess.run(
                ['snap', 'find', query],
                capture_output=True,
                text=True,
                timeout=30
            )

            if result.returncode != 0:
                return []

            packages = []
            lines = result.stdout.strip().split('\n')

            # Skip header line
            if len(lines) > 1:
                for line in lines[1:limit+1]:
                    parts = line.split()
                    if len(parts) >= 4:
                        package_name = parts[0]
                        version = parts[1]
                        publisher = parts[2]
                        # Description is the rest of the line
                        description = ' '.join(parts[4:]) if len(parts) > 4 else parts[3]

                        installed = self._is_package_installed(package_name)

                        packages.append({
                            'name': package_name,
                            'package_name': package_name,
                            'description': description,
                            'version': version,
                            'package_type': self.package_type,
                            'source_repo': self.source_repo,
                            'publisher': publisher,
                            'installed': installed
                        })

            return packages

        except subprocess.TimeoutExpired:
            raise Exception("Snap search timed out")
        except Exception as e:
            raise Exception(f"Snap search failed: {str(e)}")

    def _is_package_installed(self, package_name: str) -> bool:
        """Check if a snap package is installed"""
        try:
            result = subprocess.run(
                ['snap', 'list', package_name],
                capture_output=True,
                text=True,
                timeout=10
            )
            return result.returncode == 0
        except Exception:
            return False

    def get_installed_packages(self) -> List[Dict]:
        """Get list of all installed Snap packages"""
        try:
            result = subprocess.run(
                ['snap', 'list'],
                capture_output=True,
                text=True,
                timeout=30
            )

            if result.returncode != 0:
                return []

            packages = []
            lines = result.stdout.strip().split('\n')

            # Skip header line
            if len(lines) > 1:
                for line in lines[1:]:
                    parts = line.split()
                    if len(parts) >= 4:
                        package_name = parts[0]
                        version = parts[1]
                        rev = parts[2]
                        tracking = parts[3]
                        publisher = parts[4] if len(parts) > 4 else ""

                        # Get additional info
                        info = self._get_package_info(package_name)

                        packages.append({
                            'name': package_name,
                            'package_name': package_name,
                            'package_type': self.package_type,
                            'source_repo': self.source_repo,
                            'version': version,
                            'revision': rev,
                            'tracking': tracking,
                            'publisher': publisher,
                            'description': info.get('description', '')
                        })

            return packages

        except Exception as e:
            raise Exception(f"Failed to get installed Snap packages: {str(e)}")

    def _get_package_info(self, package_name: str) -> Dict:
        """Get detailed information about a snap package"""
        try:
            result = subprocess.run(
                ['snap', 'info', package_name],
                capture_output=True,
                text=True,
                timeout=15
            )

            if result.returncode != 0:
                return {}

            info = {}
            lines = result.stdout.strip().split('\n')

            for line in lines:
                if ':' in line:
                    key, value = line.split(':', 1)
                    key = key.strip().lower().replace(' ', '_')
                    value = value.strip()
                    info[key] = value

            return info

        except Exception:
            return {}

    def install_package(self, package_name: str, classic: bool = False) -> Tuple[bool, str]:
        """Install a snap package

        Args:
            package_name: Name of the snap to install
            classic: Whether to install with --classic flag

        Returns:
            Tuple of (success: bool, message: str)
        """
        # P0 Fix: Validate snap name to prevent command injection
        if not validate_snap_name(package_name):
            return False, f"Invalid snap name: {package_name}"

        # P1 Enhancement: Verify package signature before installation
        print(f"🔐 Verifying package signature for {package_name}...")
        verified, verify_msg = self.verifier.verify_snap_signature(package_name)
        if not verified:
            print(f"⚠️  Warning: {verify_msg}")
            # Continue with installation but warn user
            # Snap has built-in signature verification, so this is informational
        else:
            print(verify_msg)

        try:
            cmd = ['pkexec', 'snap', 'install', package_name]
            if classic:
                cmd.append('--classic')

            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=600  # 10 minutes timeout for large snaps
            )

            if result.returncode == 0:
                return True, f"Successfully installed {package_name}"
            else:
                error_msg = result.stderr if result.stderr else result.stdout
                # Check if classic mode is needed
                if 'requires classic confinement' in error_msg.lower():
                    return False, f"This snap requires classic confinement. Try installing with classic mode."
                return False, f"Failed to install {package_name}: {error_msg}"

        except subprocess.TimeoutExpired:
            return False, "Installation timed out"
        except Exception as e:
            return False, f"Installation failed: {str(e)}"

    def uninstall_package(self, package_name: str) -> Tuple[bool, str]:
        """Uninstall a snap package

        Args:
            package_name: Name of the snap to uninstall

        Returns:
            Tuple of (success: bool, message: str)
        """
        # P0 Fix: Validate snap name to prevent command injection
        if not validate_snap_name(package_name):
            return False, f"Invalid snap name: {package_name}"

        try:
            result = subprocess.run(
                ['pkexec', 'snap', 'remove', package_name],
                capture_output=True,
                text=True,
                timeout=300  # 5 minutes timeout
            )

            if result.returncode == 0:
                return True, f"Successfully uninstalled {package_name}"
            else:
                error_msg = result.stderr if result.stderr else result.stdout
                return False, f"Failed to uninstall {package_name}: {error_msg}"

        except subprocess.TimeoutExpired:
            return False, "Uninstallation timed out"
        except Exception as e:
            return False, f"Uninstallation failed: {str(e)}"

    def refresh_package(self, package_name: str) -> Tuple[bool, str]:
        """Update/refresh a snap package

        Args:
            package_name: Name of the snap to refresh

        Returns:
            Tuple of (success: bool, message: str)
        """
        try:
            result = subprocess.run(
                ['pkexec', 'snap', 'refresh', package_name],
                capture_output=True,
                text=True,
                timeout=600
            )

            if result.returncode == 0:
                return True, f"Successfully refreshed {package_name}"
            else:
                error_msg = result.stderr if result.stderr else result.stdout
                return False, f"Failed to refresh {package_name}: {error_msg}"

        except subprocess.TimeoutExpired:
            return False, "Refresh timed out"
        except Exception as e:
            return False, f"Refresh failed: {str(e)}"

    def refresh_all(self) -> Tuple[bool, str]:
        """Refresh all installed snap packages

        Returns:
            Tuple of (success: bool, message: str)
        """
        try:
            result = subprocess.run(
                ['pkexec', 'snap', 'refresh'],
                capture_output=True,
                text=True,
                timeout=1200  # 20 minutes for all updates
            )

            if result.returncode == 0:
                return True, "Successfully refreshed all snaps"
            else:
                error_msg = result.stderr if result.stderr else result.stdout
                return False, f"Failed to refresh all snaps: {error_msg}"

        except subprocess.TimeoutExpired:
            return False, "Refresh all timed out"
        except Exception as e:
            return False, f"Refresh all failed: {str(e)}"

    def get_snap_info(self, package_name: str) -> Optional[Dict]:
        """Get detailed information about a snap package

        Args:
            package_name: Name of the snap

        Returns:
            Dictionary with snap information or None if not found
        """
        return self._get_package_info(package_name)
