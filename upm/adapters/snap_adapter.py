import subprocess
import shutil
import re
from typing import List, Optional, Callable
from upm.adapters.base import PackageManagerAdapter
from upm.models import PackageSearchResult, PackageInfo, PackageDetails


class SnapAdapter(PackageManagerAdapter):
    """Adapter for Snap package manager using CLI"""

    def __init__(self):
        self._available = self._check_availability()

    def _check_availability(self) -> bool:
        """Check if snap command is available and snapd is running"""
        if shutil.which('snap') is None:
            return False

        # Check if snapd is running
        try:
            result = subprocess.run(
                ['snap', 'version'],
                capture_output=True,
                timeout=5
            )
            return result.returncode == 0
        except Exception:
            return False

    def is_available(self) -> bool:
        """Check if Snap is available on the system"""
        return self._available

    def _run_command(self, args: List[str]) -> tuple[bool, str]:
        """
        Run a snap command

        Returns:
            Tuple of (success, output)
        """
        try:
            result = subprocess.run(
                ['snap'] + args,
                capture_output=True,
                text=True,
                timeout=30
            )
            return result.returncode == 0, result.stdout
        except Exception as e:
            return False, str(e)

    def search(self, query: str) -> List[PackageSearchResult]:
        """Search for packages in Snap Store"""
        if not self.is_available():
            return []

        success, output = self._run_command(['find', query])

        if not success:
            return []

        results = []
        lines = output.strip().split('\n')

        # Skip header line
        if len(lines) > 0 and 'Name' in lines[0]:
            lines = lines[1:]

        for line in lines:
            if not line.strip():
                continue

            # Parse space-separated output
            # Format: Name  Version  Publisher  Notes  Summary
            parts = re.split(r'\s{2,}', line.strip())

            if len(parts) >= 2:
                name = parts[0].strip()
                version = parts[1].strip() if len(parts) > 1 else "unknown"
                description = parts[-1].strip() if len(parts) > 4 else ""

                result = PackageSearchResult(
                    package_id=name,
                    name=name,
                    description=description,
                    version=version,
                    package_manager="snap"
                )
                results.append(result)

        return results

    def install(self, package_id: str, progress_callback: Optional[Callable] = None) -> bool:
        """Install package via Snap"""
        if not self.is_available():
            return False

        success, _ = self._run_command(['install', package_id])
        return success

    def remove(self, package_id: str, progress_callback: Optional[Callable] = None) -> bool:
        """Remove package via Snap"""
        if not self.is_available():
            return False

        success, _ = self._run_command(['remove', package_id])
        return success

    def list_installed(self) -> List[PackageInfo]:
        """List installed Snap packages"""
        if not self.is_available():
            return []

        success, output = self._run_command(['list'])

        if not success:
            return []

        installed = []
        lines = output.strip().split('\n')

        # Skip header line
        if len(lines) > 0 and 'Name' in lines[0]:
            lines = lines[1:]

        for line in lines:
            if not line.strip():
                continue

            # Parse space-separated output
            parts = re.split(r'\s{2,}', line.strip())

            if len(parts) >= 2:
                name = parts[0].strip()
                version = parts[1].strip()

                info = PackageInfo(
                    package_id=name,
                    name=name,
                    version=version,
                    package_manager="snap",
                    install_date=None  # Snap CLI doesn't easily provide this
                )
                installed.append(info)

        return installed

    def get_details(self, package_id: str) -> Optional[PackageDetails]:
        """Get package details from Snap"""
        if not self.is_available():
            return None

        success, output = self._run_command(['info', package_id])

        if not success:
            return None

        # Parse info output
        # This is simplified - real implementation would parse more fields
        return PackageDetails(
            package_id=package_id,
            name=package_id,
            description="",
            version="unknown",
            package_manager="snap"
        )
