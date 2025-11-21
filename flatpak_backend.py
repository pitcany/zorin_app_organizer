"""
Flatpak/Flathub backend for Unified Package Manager
Handles Flatpak operations including search, install, uninstall, and package info
"""

import subprocess
import requests
import json
import re
from typing import List, Dict, Optional, Tuple


def validate_flatpak_id(flatpak_id: str) -> bool:
    """
    P0 Fix: Validate Flatpak app ID to prevent command injection

    Flatpak IDs use reverse DNS notation: org.example.AppName
    """
    if not flatpak_id or len(flatpak_id) > 255:
        return False

    # Flatpak ID pattern: reverse DNS notation
    pattern = r'^[a-zA-Z0-9_-]+(\.[a-zA-Z0-9_-]+)+$'
    return bool(re.match(pattern, flatpak_id))


class FlatpakBackend:
    """Backend for Flatpak/Flathub package management"""

    def __init__(self):
        """Initialize Flatpak backend"""
        self.package_type = "flatpak"
        self.source_repo = "Flathub"
        self.flathub_api = "https://flathub.org/api/v2"
        self.flathub_apps_cache = None

    def is_available(self) -> bool:
        """Check if Flatpak is available on the system"""
        try:
            result = subprocess.run(['which', 'flatpak'],
                                  capture_output=True,
                                  text=True,
                                  timeout=5)
            return result.returncode == 0
        except Exception:
            return False

    def is_flathub_configured(self) -> bool:
        """Check if Flathub repository is configured"""
        try:
            result = subprocess.run(
                ['flatpak', 'remotes'],
                capture_output=True,
                text=True,
                timeout=10
            )
            return 'flathub' in result.stdout.lower()
        except Exception:
            return False

    def setup_flathub(self) -> Tuple[bool, str]:
        """Add Flathub repository if not already configured

        Returns:
            Tuple of (success: bool, message: str)
        """
        if self.is_flathub_configured():
            return True, "Flathub is already configured"

        try:
            result = subprocess.run(
                ['pkexec', 'flatpak', 'remote-add', '--if-not-exists', 'flathub',
                 'https://flathub.org/repo/flathub.flatpakrepo'],
                capture_output=True,
                text=True,
                timeout=60
            )

            if result.returncode == 0:
                return True, "Flathub repository added successfully"
            else:
                return False, f"Failed to add Flathub: {result.stderr}"

        except Exception as e:
            return False, f"Failed to add Flathub: {str(e)}"

    def search_packages(self, query: str, limit: int = 100) -> List[Dict]:
        """Search for packages in Flathub

        Args:
            query: Search query string
            limit: Maximum number of results to return

        Returns:
            List of package dictionaries with name, description, version, etc.
        """
        if not query.strip():
            return []

        packages = []

        # Method 1: Try using Flathub API (faster and more detailed)
        try:
            packages = self._search_via_api(query, limit)
            if packages:
                return packages
        except Exception as e:
            # Fall back to CLI search if API fails
            pass

        # Method 2: Fall back to flatpak search command
        try:
            packages = self._search_via_cli(query, limit)
        except Exception as e:
            raise Exception(f"Flatpak search failed: {str(e)}")

        return packages

    def _search_via_api(self, query: str, limit: int) -> List[Dict]:
        """Search packages using Flathub API"""
        try:
            # Search endpoint
            url = f"{self.flathub_api}/search/{query}"
            response = requests.get(url, timeout=15)

            if response.status_code != 200:
                return []

            data = response.json()
            packages = []

            # Parse results
            if 'hits' in data:
                for hit in data['hits'][:limit]:
                    app_id = hit.get('app_id', '')
                    name = hit.get('name', app_id)
                    summary = hit.get('summary', '')

                    installed = self._is_package_installed(app_id)

                    packages.append({
                        'name': name,
                        'package_name': app_id,
                        'description': summary,
                        'package_type': self.package_type,
                        'source_repo': self.source_repo,
                        'installed': installed,
                        'app_id': app_id
                    })

            return packages

        except Exception as e:
            # Return empty list to trigger CLI fallback
            return []

    def _search_via_cli(self, query: str, limit: int) -> List[Dict]:
        """Search packages using flatpak CLI"""
        try:
            result = subprocess.run(
                ['flatpak', 'search', query],
                capture_output=True,
                text=True,
                timeout=30
            )

            if result.returncode != 0:
                return []

            packages = []
            lines = result.stdout.strip().split('\n')

            # Skip header line
            for line in lines[1:limit+1]:
                parts = line.split('\t')
                if len(parts) >= 3:
                    name = parts[0].strip()
                    description = parts[1].strip()
                    app_id = parts[2].strip()
                    version = parts[3].strip() if len(parts) > 3 else ""
                    branch = parts[4].strip() if len(parts) > 4 else ""

                    installed = self._is_package_installed(app_id)

                    packages.append({
                        'name': name,
                        'package_name': app_id,
                        'description': description,
                        'version': version,
                        'branch': branch,
                        'package_type': self.package_type,
                        'source_repo': self.source_repo,
                        'installed': installed,
                        'app_id': app_id
                    })

            return packages

        except subprocess.TimeoutExpired:
            raise Exception("Flatpak search timed out")
        except Exception as e:
            raise Exception(f"Flatpak CLI search failed: {str(e)}")

    def _is_package_installed(self, app_id: str) -> bool:
        """Check if a flatpak is installed"""
        try:
            result = subprocess.run(
                ['flatpak', 'list', '--app', '--columns=application'],
                capture_output=True,
                text=True,
                timeout=10
            )
            return app_id in result.stdout
        except Exception:
            return False

    def get_installed_packages(self) -> List[Dict]:
        """Get list of all installed Flatpak applications"""
        try:
            result = subprocess.run(
                ['flatpak', 'list', '--app'],
                capture_output=True,
                text=True,
                timeout=30
            )

            if result.returncode != 0:
                return []

            packages = []
            lines = result.stdout.strip().split('\n')

            for line in lines:
                if not line.strip():
                    continue

                parts = line.split('\t')
                if len(parts) >= 3:
                    name = parts[0].strip()
                    app_id = parts[1].strip()
                    version = parts[2].strip()
                    branch = parts[3].strip() if len(parts) > 3 else ""

                    # Get additional info
                    info = self._get_package_info(app_id)

                    packages.append({
                        'name': name,
                        'package_name': app_id,
                        'package_type': self.package_type,
                        'source_repo': self.source_repo,
                        'version': version,
                        'branch': branch,
                        'app_id': app_id,
                        'description': info.get('description', '')
                    })

            return packages

        except Exception as e:
            raise Exception(f"Failed to get installed Flatpaks: {str(e)}")

    def _get_package_info(self, app_id: str) -> Dict:
        """Get detailed information about a flatpak"""
        try:
            result = subprocess.run(
                ['flatpak', 'info', app_id],
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

    def install_package(self, app_id: str, system_wide: bool = False) -> Tuple[bool, str]:
        """Install a flatpak application

        Args:
            app_id: Application ID (e.g., org.mozilla.firefox)
            system_wide: Whether to install system-wide (requires root)

        Returns:
            Tuple of (success: bool, message: str)
        """
        # P0 Fix: Validate Flatpak ID to prevent command injection
        if not validate_flatpak_id(app_id):
            return False, f"Invalid Flatpak ID: {app_id}"

        try:
            if system_wide:
                cmd = ['pkexec', 'flatpak', 'install', '-y', 'flathub', app_id]
            else:
                cmd = ['flatpak', 'install', '--user', '-y', 'flathub', app_id]

            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=600  # 10 minutes timeout
            )

            if result.returncode == 0:
                return True, f"Successfully installed {app_id}"
            else:
                error_msg = result.stderr if result.stderr else result.stdout
                return False, f"Failed to install {app_id}: {error_msg}"

        except subprocess.TimeoutExpired:
            return False, "Installation timed out"
        except Exception as e:
            return False, f"Installation failed: {str(e)}"

    def uninstall_package(self, app_id: str, system_wide: bool = False) -> Tuple[bool, str]:
        """Uninstall a flatpak application

        Args:
            app_id: Application ID
            system_wide: Whether the app is installed system-wide

        Returns:
            Tuple of (success: bool, message: str)
        """
        # P0 Fix: Validate Flatpak ID to prevent command injection
        if not validate_flatpak_id(app_id):
            return False, f"Invalid Flatpak ID: {app_id}"

        try:
            if system_wide:
                cmd = ['pkexec', 'flatpak', 'uninstall', '-y', app_id]
            else:
                cmd = ['flatpak', 'uninstall', '--user', '-y', app_id]

            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=300  # 5 minutes timeout
            )

            if result.returncode == 0:
                return True, f"Successfully uninstalled {app_id}"
            else:
                error_msg = result.stderr if result.stderr else result.stdout
                return False, f"Failed to uninstall {app_id}: {error_msg}"

        except subprocess.TimeoutExpired:
            return False, "Uninstallation timed out"
        except Exception as e:
            return False, f"Uninstallation failed: {str(e)}"

    def update_package(self, app_id: str) -> Tuple[bool, str]:
        """Update a flatpak application

        Args:
            app_id: Application ID

        Returns:
            Tuple of (success: bool, message: str)
        """
        try:
            result = subprocess.run(
                ['flatpak', 'update', '-y', app_id],
                capture_output=True,
                text=True,
                timeout=600
            )

            if result.returncode == 0:
                return True, f"Successfully updated {app_id}"
            else:
                error_msg = result.stderr if result.stderr else result.stdout
                return False, f"Failed to update {app_id}: {error_msg}"

        except subprocess.TimeoutExpired:
            return False, "Update timed out"
        except Exception as e:
            return False, f"Update failed: {str(e)}"

    def update_all(self) -> Tuple[bool, str]:
        """Update all installed flatpak applications

        Returns:
            Tuple of (success: bool, message: str)
        """
        try:
            result = subprocess.run(
                ['flatpak', 'update', '-y'],
                capture_output=True,
                text=True,
                timeout=1200  # 20 minutes for all updates
            )

            if result.returncode == 0:
                return True, "Successfully updated all flatpaks"
            else:
                error_msg = result.stderr if result.stderr else result.stdout
                return False, f"Failed to update all flatpaks: {error_msg}"

        except subprocess.TimeoutExpired:
            return False, "Update all timed out"
        except Exception as e:
            return False, f"Update all failed: {str(e)}"
