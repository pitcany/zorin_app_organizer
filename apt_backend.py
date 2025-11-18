"""
APT package manager backend for Unified Package Manager
Handles APT operations including search, install, uninstall, and package info
"""

import subprocess
import re
from typing import List, Dict, Optional, Tuple


class AptBackend:
    """Backend for APT package management"""

    def __init__(self):
        """Initialize APT backend"""
        self.package_type = "apt"
        self.source_repo = "Ubuntu/APT"

    def is_available(self) -> bool:
        """Check if APT is available on the system"""
        try:
            result = subprocess.run(['which', 'apt'],
                                  capture_output=True,
                                  text=True,
                                  timeout=5)
            return result.returncode == 0
        except Exception:
            return False

    def search_packages(self, query: str, limit: int = 100) -> List[Dict]:
        """Search for packages in APT repositories

        Args:
            query: Search query string
            limit: Maximum number of results to return

        Returns:
            List of package dictionaries with name, description, version, etc.
        """
        if not query.strip():
            return []

        try:
            # Use apt-cache search for package search
            result = subprocess.run(
                ['apt-cache', 'search', query],
                capture_output=True,
                text=True,
                timeout=30
            )

            if result.returncode != 0:
                return []

            packages = []
            lines = result.stdout.strip().split('\n')

            for line in lines[:limit]:
                if ' - ' in line:
                    parts = line.split(' - ', 1)
                    package_name = parts[0].strip()
                    description = parts[1].strip() if len(parts) > 1 else ""

                    # Get additional package info
                    version = self._get_package_version(package_name)
                    installed = self._is_package_installed(package_name)

                    packages.append({
                        'name': package_name,
                        'package_name': package_name,
                        'description': description,
                        'version': version,
                        'package_type': self.package_type,
                        'source_repo': self.source_repo,
                        'installed': installed,
                        'size': self._get_package_size(package_name)
                    })

            return packages

        except subprocess.TimeoutExpired:
            raise Exception("APT search timed out")
        except Exception as e:
            raise Exception(f"APT search failed: {str(e)}")

    def _get_package_version(self, package_name: str) -> str:
        """Get the available version of a package"""
        try:
            result = subprocess.run(
                ['apt-cache', 'policy', package_name],
                capture_output=True,
                text=True,
                timeout=10
            )

            if result.returncode == 0:
                # Parse the Candidate line
                for line in result.stdout.split('\n'):
                    if 'Candidate:' in line:
                        version = line.split('Candidate:')[1].strip()
                        if version != '(none)':
                            return version
            return ""
        except Exception:
            return ""

    def _get_package_size(self, package_name: str) -> str:
        """Get the installed size of a package"""
        try:
            result = subprocess.run(
                ['apt-cache', 'show', package_name],
                capture_output=True,
                text=True,
                timeout=10
            )

            if result.returncode == 0:
                for line in result.stdout.split('\n'):
                    if line.startswith('Installed-Size:'):
                        size_kb = line.split(':')[1].strip()
                        return self._format_size(int(size_kb))
            return ""
        except Exception:
            return ""

    def _format_size(self, size_kb: int) -> str:
        """Format size in KB to human-readable format"""
        if size_kb < 1024:
            return f"{size_kb} KB"
        elif size_kb < 1024 * 1024:
            return f"{size_kb / 1024:.1f} MB"
        else:
            return f"{size_kb / (1024 * 1024):.1f} GB"

    def _is_package_installed(self, package_name: str) -> bool:
        """Check if a package is installed"""
        try:
            result = subprocess.run(
                ['dpkg', '-s', package_name],
                capture_output=True,
                text=True,
                timeout=10
            )
            return result.returncode == 0
        except Exception:
            return False

    def get_installed_packages(self) -> List[Dict]:
        """Get list of all installed APT packages"""
        try:
            result = subprocess.run(
                ['dpkg', '--get-selections'],
                capture_output=True,
                text=True,
                timeout=30
            )

            if result.returncode != 0:
                return []

            packages = []
            lines = result.stdout.strip().split('\n')

            for line in lines:
                if '\t' in line or '  ' in line:
                    parts = re.split(r'\s+', line.strip())
                    if len(parts) >= 2 and parts[1] == 'install':
                        package_name = parts[0]
                        version = self._get_installed_version(package_name)

                        packages.append({
                            'name': package_name,
                            'package_name': package_name,
                            'package_type': self.package_type,
                            'source_repo': self.source_repo,
                            'version': version,
                            'description': self._get_package_description(package_name)
                        })

            return packages

        except Exception as e:
            raise Exception(f"Failed to get installed packages: {str(e)}")

    def _get_installed_version(self, package_name: str) -> str:
        """Get the installed version of a package"""
        try:
            result = subprocess.run(
                ['dpkg', '-s', package_name],
                capture_output=True,
                text=True,
                timeout=10
            )

            if result.returncode == 0:
                for line in result.stdout.split('\n'):
                    if line.startswith('Version:'):
                        return line.split(':', 1)[1].strip()
            return ""
        except Exception:
            return ""

    def _get_package_description(self, package_name: str) -> str:
        """Get package description"""
        try:
            result = subprocess.run(
                ['dpkg', '-s', package_name],
                capture_output=True,
                text=True,
                timeout=10
            )

            if result.returncode == 0:
                for line in result.stdout.split('\n'):
                    if line.startswith('Description:'):
                        return line.split(':', 1)[1].strip()
            return ""
        except Exception:
            return ""

    def install_package(self, package_name: str) -> Tuple[bool, str]:
        """Install a package using APT

        Args:
            package_name: Name of the package to install

        Returns:
            Tuple of (success: bool, message: str)
        """
        try:
            # Use pkexec to get sudo privileges with GUI prompt
            result = subprocess.run(
                ['pkexec', 'apt', 'install', '-y', package_name],
                capture_output=True,
                text=True,
                timeout=300  # 5 minutes timeout
            )

            if result.returncode == 0:
                return True, f"Successfully installed {package_name}"
            else:
                error_msg = result.stderr if result.stderr else result.stdout
                return False, f"Failed to install {package_name}: {error_msg}"

        except subprocess.TimeoutExpired:
            return False, "Installation timed out"
        except Exception as e:
            return False, f"Installation failed: {str(e)}"

    def uninstall_package(self, package_name: str) -> Tuple[bool, str]:
        """Uninstall a package using APT

        Args:
            package_name: Name of the package to uninstall

        Returns:
            Tuple of (success: bool, message: str)
        """
        try:
            # Use pkexec to get sudo privileges with GUI prompt
            result = subprocess.run(
                ['pkexec', 'apt', 'remove', '-y', package_name],
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

    def update_cache(self) -> Tuple[bool, str]:
        """Update APT package cache

        Returns:
            Tuple of (success: bool, message: str)
        """
        try:
            result = subprocess.run(
                ['pkexec', 'apt', 'update'],
                capture_output=True,
                text=True,
                timeout=120
            )

            if result.returncode == 0:
                return True, "APT cache updated successfully"
            else:
                return False, f"Failed to update APT cache: {result.stderr}"

        except subprocess.TimeoutExpired:
            return False, "APT cache update timed out"
        except Exception as e:
            return False, f"Failed to update APT cache: {str(e)}"

    def add_repository(self, repo_url: str) -> Tuple[bool, str]:
        """Add a PPA or custom repository

        Args:
            repo_url: Repository URL (e.g., ppa:user/repo)

        Returns:
            Tuple of (success: bool, message: str)
        """
        try:
            result = subprocess.run(
                ['pkexec', 'add-apt-repository', '-y', repo_url],
                capture_output=True,
                text=True,
                timeout=60
            )

            if result.returncode == 0:
                # Update cache after adding repository
                self.update_cache()
                return True, f"Successfully added repository: {repo_url}"
            else:
                return False, f"Failed to add repository: {result.stderr}"

        except subprocess.TimeoutExpired:
            return False, "Add repository timed out"
        except Exception as e:
            return False, f"Failed to add repository: {str(e)}"

    def remove_repository(self, repo_url: str) -> Tuple[bool, str]:
        """Remove a PPA or custom repository

        Args:
            repo_url: Repository URL (e.g., ppa:user/repo)

        Returns:
            Tuple of (success: bool, message: str)
        """
        try:
            result = subprocess.run(
                ['pkexec', 'add-apt-repository', '--remove', '-y', repo_url],
                capture_output=True,
                text=True,
                timeout=60
            )

            if result.returncode == 0:
                return True, f"Successfully removed repository: {repo_url}"
            else:
                return False, f"Failed to remove repository: {result.stderr}"

        except subprocess.TimeoutExpired:
            return False, "Remove repository timed out"
        except Exception as e:
            return False, f"Failed to remove repository: {str(e)}"
