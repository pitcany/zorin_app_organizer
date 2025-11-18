import subprocess
import shutil
from typing import List, Optional, Callable
from upm.adapters.base import PackageManagerAdapter
from upm.models import PackageSearchResult, PackageInfo, PackageDetails


class FlatpakAdapter(PackageManagerAdapter):
    """Adapter for Flatpak package manager using CLI"""

    def __init__(self):
        self._available = self._check_availability()

    def _check_availability(self) -> bool:
        """Check if flatpak command is available"""
        return shutil.which('flatpak') is not None

    def is_available(self) -> bool:
        """Check if Flatpak is available on the system"""
        return self._available

    def _run_command(self, args: List[str]) -> tuple[bool, str]:
        """
        Run a flatpak command

        Returns:
            Tuple of (success, output)
        """
        try:
            result = subprocess.run(
                ['flatpak'] + args,
                capture_output=True,
                text=True,
                timeout=30
            )
            return result.returncode == 0, result.stdout
        except Exception as e:
            return False, str(e)

    def search(self, query: str) -> List[PackageSearchResult]:
        """Search for packages in Flatpak"""
        if not self.is_available():
            return []

        success, output = self._run_command([
            'search',
            '--columns=application,name,description,version',
            query
        ])

        if not success:
            return []

        results = []
        lines = output.strip().split('\n')

        for line in lines:
            if not line.strip():
                continue

            # Parse tab-separated output
            parts = line.split('\t')
            if len(parts) >= 4:
                result = PackageSearchResult(
                    package_id=parts[0].strip(),
                    name=parts[1].strip(),
                    description=parts[2].strip(),
                    version=parts[3].strip() if len(parts) > 3 else "unknown",
                    package_manager="flatpak"
                )
                results.append(result)

        return results

    def install(self, package_id: str, progress_callback: Optional[Callable] = None) -> bool:
        """Install package via Flatpak"""
        if not self.is_available():
            return False

        # Use -y to auto-confirm
        success, _ = self._run_command(['install', '-y', 'flathub', package_id])
        return success

    def remove(self, package_id: str, progress_callback: Optional[Callable] = None) -> bool:
        """Remove package via Flatpak"""
        if not self.is_available():
            return False

        success, _ = self._run_command(['uninstall', '-y', package_id])
        return success

    def list_installed(self) -> List[PackageInfo]:
        """List installed Flatpak packages"""
        if not self.is_available():
            return []

        success, output = self._run_command([
            'list',
            '--app',
            '--columns=application,name,version'
        ])

        if not success:
            return []

        installed = []
        lines = output.strip().split('\n')

        for line in lines:
            if not line.strip():
                continue

            parts = line.split('\t')
            if len(parts) >= 3:
                info = PackageInfo(
                    package_id=parts[0].strip(),
                    name=parts[1].strip(),
                    version=parts[2].strip(),
                    package_manager="flatpak",
                    install_date=None  # Flatpak CLI doesn't easily provide this
                )
                installed.append(info)

        return installed

    def get_details(self, package_id: str) -> Optional[PackageDetails]:
        """Get package details from Flatpak"""
        if not self.is_available():
            return None

        # For now, return basic details
        # TODO: Add API integration for rich metadata
        success, output = self._run_command(['info', package_id])

        if not success:
            return None

        # Parse basic info from output
        # This is simplified - real implementation would parse more fields
        return PackageDetails(
            package_id=package_id,
            name=package_id,
            description="",
            version="unknown",
            package_manager="flatpak"
        )
