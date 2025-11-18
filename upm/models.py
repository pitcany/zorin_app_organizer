from dataclasses import dataclass
from datetime import datetime
from typing import Optional


@dataclass
class PackageSearchResult:
    """Result from searching for packages"""
    package_id: str
    name: str
    description: str
    version: str
    package_manager: str  # 'apt', 'flatpak', 'snap'


@dataclass
class PackageInfo:
    """Information about an installed package"""
    package_id: str
    name: str
    version: str
    package_manager: str
    install_date: Optional[datetime] = None


@dataclass
class PackageDetails:
    """Detailed information about a package"""
    package_id: str
    name: str
    description: str
    version: str
    package_manager: str
    homepage: Optional[str] = None
    license: Optional[str] = None
    size: Optional[int] = None  # bytes
    dependencies: Optional[list[str]] = None


@dataclass
class ActionLog:
    """Log entry for package operations"""
    timestamp: datetime
    action_type: str  # 'install', 'remove', 'sync'
    package_name: str
    package_manager: str
    version: Optional[str] = None
    success: bool = True
    error_message: Optional[str] = None
    user_note: Optional[str] = None
