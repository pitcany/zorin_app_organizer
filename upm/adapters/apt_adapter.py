from typing import List, Optional, Callable
from upm.adapters.base import PackageManagerAdapter
from upm.models import PackageSearchResult, PackageInfo, PackageDetails


class AptAdapter(PackageManagerAdapter):
    """Adapter for APT package manager using python-apt"""

    def __init__(self):
        self._cache = None
        self._available = self._check_availability()

    def _check_availability(self) -> bool:
        """Check if python-apt is available"""
        try:
            import apt
            return True
        except ImportError:
            return False

    def is_available(self) -> bool:
        """Check if APT is available on the system"""
        return self._available

    def search(self, query: str) -> List[PackageSearchResult]:
        """Search for packages in APT"""
        if not self.is_available():
            return []

        try:
            import apt

            # Initialize cache if needed
            if self._cache is None:
                self._cache = apt.Cache()

            results = []
            query_lower = query.lower()

            # Search through packages
            for pkg in self._cache:
                # Check if query matches package name or description
                name_match = query_lower in pkg.name.lower()
                desc_match = False

                if pkg.candidate and pkg.candidate.description:
                    desc_match = query_lower in pkg.candidate.description.lower()

                if name_match or desc_match:
                    # Get version from candidate
                    version = pkg.candidate.version if pkg.candidate else "unknown"
                    description = pkg.candidate.summary if pkg.candidate else ""

                    result = PackageSearchResult(
                        package_id=pkg.name,
                        name=pkg.name,
                        description=description,
                        version=version,
                        package_manager="apt"
                    )
                    results.append(result)

            return results

        except Exception as e:
            print(f"Error searching APT: {e}")
            return []

    def install(self, package_id: str, progress_callback: Optional[Callable] = None) -> bool:
        """Install package via APT"""
        if not self.is_available():
            return False

        # TODO: Implement install
        return False

    def remove(self, package_id: str, progress_callback: Optional[Callable] = None) -> bool:
        """Remove package via APT"""
        if not self.is_available():
            return False

        # TODO: Implement remove
        return False

    def list_installed(self) -> List[PackageInfo]:
        """List installed APT packages"""
        if not self.is_available():
            return []

        try:
            import apt
            from datetime import datetime

            # Initialize cache if needed
            if self._cache is None:
                self._cache = apt.Cache()

            installed = []

            for pkg in self._cache:
                if pkg.is_installed:
                    info = PackageInfo(
                        package_id=pkg.name,
                        name=pkg.name,
                        version=pkg.installed.version,
                        package_manager="apt",
                        install_date=None  # APT doesn't easily provide install date
                    )
                    installed.append(info)

            return installed

        except Exception as e:
            print(f"Error listing installed APT packages: {e}")
            return []

    def get_details(self, package_id: str) -> Optional[PackageDetails]:
        """Get package details from APT"""
        if not self.is_available():
            return None

        # TODO: Implement get_details
        return None
