"""
APT package manager backend for Unified Package Manager
Handles APT operations including search, install, uninstall, and package info
"""

import subprocess
import re
from typing import List, Dict, Optional, Tuple
from package_verification import PackageVerifier


def validate_package_name(package_name: str) -> bool:
    """
    P0 Fix: Validate package name to prevent command injection

    APT package names must follow Debian naming conventions:
    - Lowercase letters, digits, plus, minus, and dot characters
    - Must start with a lowercase letter or digit
    """
    if not package_name or len(package_name) > 200:
        return False

    # Debian package naming rules
    pattern = r'^[a-z0-9][a-z0-9+.-]*$'
    return bool(re.match(pattern, package_name))


class AptBackend:
    """Backend for APT package management"""

    def __init__(self):
        """Initialize APT backend"""
        self.package_type = "apt"
        self.source_repo = "Ubuntu/APT"
        self.verifier = PackageVerifier()

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
        # P0 Fix: Validate package name to prevent command injection
        if not validate_package_name(package_name):
            return False, f"Invalid package name: {package_name}"

        # P1 Enhancement: Verify package signature before installation
        print(f"🔐 Verifying package signature for {package_name}...")
        verified, verify_msg = self.verifier.verify_apt_package_signature(package_name)
        if not verified:
            print(f"⚠️  Warning: {verify_msg}")
            # Continue with installation but warn user
            # APT has built-in signature verification, so this is informational
        else:
            print(verify_msg)

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
        # P0 Fix: Validate package name to prevent command injection
        if not validate_package_name(package_name):
            return False, f"Invalid package name: {package_name}"

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

    def get_system_repositories(self) -> List[Dict]:
        """Scan system for existing APT repositories

        Returns:
            List of dictionaries containing repository information
        """
        import os
        import glob

        repositories = []
        sources_dir = "/etc/apt/sources.list.d/"

        try:
            # P0 Fix: Remove TOCTOU race by not checking existence before use
            # Parse .list files in sources.list.d/
            try:
                list_files = glob.glob(os.path.join(sources_dir, "*.list"))

                for list_file in list_files:
                    try:
                        with open(list_file, 'r') as f:
                            content = f.read()

                        # Parse each line
                        for line in content.split('\n'):
                            line = line.strip()

                            # Skip empty lines and comments
                            if not line or line.startswith('#'):
                                continue

                            # Parse repository line
                            repo_info = self._parse_repo_line(line, list_file)
                            if repo_info:
                                repositories.append(repo_info)

                    except (PermissionError, FileNotFoundError):
                        # Skip files we can't read or that were deleted
                        continue
                    except Exception as e:
                        # Log error but continue
                        print(f"Error parsing {list_file}: {e}")
                        continue
            except (FileNotFoundError, PermissionError):
                # Directory doesn't exist or can't be read
                pass

            # Also parse main sources.list
            main_sources = "/etc/apt/sources.list"
            try:
                with open(main_sources, 'r') as f:
                    content = f.read()

                for line in content.split('\n'):
                    line = line.strip()

                    if not line or line.startswith('#'):
                        continue

                    repo_info = self._parse_repo_line(line, main_sources)
                    if repo_info:
                        repositories.append(repo_info)

            except (PermissionError, FileNotFoundError):
                # File doesn't exist or can't be read
                pass
            except Exception as e:
                print(f"Error parsing sources.list: {e}")

        except Exception as e:
            print(f"Error scanning repositories: {e}")

        return repositories

    def _parse_repo_line(self, line: str, source_file: str) -> Optional[Dict]:
        """Parse a repository line from sources.list file

        Args:
            line: Repository line (e.g., "deb http://... focal main")
            source_file: Source file path

        Returns:
            Dictionary with repository info or None
        """
        import os

        try:
            parts = line.split()

            if len(parts) < 3:
                return None

            repo_type = parts[0]  # deb or deb-src

            if repo_type not in ['deb', 'deb-src']:
                return None

            # Extract URL (may have options in brackets before it)
            url_start_idx = 1
            if parts[1].startswith('['):
                # Find end of options
                for i in range(1, len(parts)):
                    if parts[i].endswith(']'):
                        url_start_idx = i + 1
                        break

            if url_start_idx >= len(parts):
                return None

            url = parts[url_start_idx]

            # Determine repository name from file or URL
            file_basename = os.path.basename(source_file)
            if file_basename.endswith('.list'):
                repo_name = file_basename[:-5]  # Remove .list extension
            else:
                # Try to extract from URL
                repo_name = url.split('//')[-1].split('/')[0]

            # Full repository line for storage
            full_line = line

            return {
                'name': repo_name,
                'url': full_line,
                'type': repo_type.upper(),
                'source_file': source_file,
                'is_system': True
            }

        except Exception as e:
            return None
