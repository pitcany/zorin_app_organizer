from abc import ABC, abstractmethod
from typing import List, Optional, Callable
from upm.models import PackageSearchResult, PackageInfo, PackageDetails


class PackageManagerAdapter(ABC):
    """Abstract base class for package manager adapters"""

    @abstractmethod
    def search(self, query: str) -> List[PackageSearchResult]:
        """
        Search for packages matching query

        Args:
            query: Search term

        Returns:
            List of matching packages
        """
        pass

    @abstractmethod
    def install(self, package_id: str, progress_callback: Optional[Callable] = None) -> bool:
        """
        Install a package

        Args:
            package_id: Unique identifier for the package
            progress_callback: Optional callback for progress updates

        Returns:
            True if successful, False otherwise
        """
        pass

    @abstractmethod
    def remove(self, package_id: str, progress_callback: Optional[Callable] = None) -> bool:
        """
        Remove a package

        Args:
            package_id: Unique identifier for the package
            progress_callback: Optional callback for progress updates

        Returns:
            True if successful, False otherwise
        """
        pass

    @abstractmethod
    def list_installed(self) -> List[PackageInfo]:
        """
        List all installed packages from this package manager

        Returns:
            List of installed packages
        """
        pass

    @abstractmethod
    def get_details(self, package_id: str) -> Optional[PackageDetails]:
        """
        Get detailed information about a package

        Args:
            package_id: Unique identifier for the package

        Returns:
            Package details or None if not found
        """
        pass

    @abstractmethod
    def is_available(self) -> bool:
        """
        Check if this package manager is available on the system

        Returns:
            True if available, False otherwise
        """
        pass
