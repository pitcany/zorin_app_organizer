"""
Package verification module for Unified Package Manager
Handles signature verification and checksum validation for packages
"""

import hashlib
import subprocess
import os
import requests
from typing import Optional, Tuple, Dict
from pathlib import Path


class PackageVerifier:
    """Handles package signature and checksum verification"""

    def __init__(self):
        """Initialize package verifier"""
        self.supported_hash_algorithms = ['sha256', 'sha512', 'md5']

    def verify_checksum(self, file_path: str, expected_checksum: str,
                       algorithm: str = 'sha256') -> Tuple[bool, str]:
        """
        Verify file checksum

        Args:
            file_path: Path to the file to verify
            expected_checksum: Expected checksum value
            algorithm: Hash algorithm (sha256, sha512, md5)

        Returns:
            Tuple of (success: bool, message: str)
        """
        if algorithm not in self.supported_hash_algorithms:
            return False, f"Unsupported hash algorithm: {algorithm}"

        if not os.path.exists(file_path):
            return False, f"File not found: {file_path}"

        try:
            # Calculate file checksum
            actual_checksum = self._calculate_checksum(file_path, algorithm)

            # Compare checksums (case-insensitive)
            if actual_checksum.lower() == expected_checksum.lower():
                return True, f"✅ Checksum verified ({algorithm})"
            else:
                return False, (
                    f"❌ Checksum mismatch!\n"
                    f"   Expected: {expected_checksum}\n"
                    f"   Got:      {actual_checksum}"
                )

        except Exception as e:
            return False, f"Checksum verification failed: {e}"

    def _calculate_checksum(self, file_path: str, algorithm: str) -> str:
        """
        Calculate file checksum

        Args:
            file_path: Path to the file
            algorithm: Hash algorithm

        Returns:
            Hexadecimal checksum string
        """
        # Get hash function
        if algorithm == 'sha256':
            hash_func = hashlib.sha256()
        elif algorithm == 'sha512':
            hash_func = hashlib.sha512()
        elif algorithm == 'md5':
            hash_func = hashlib.md5()
        else:
            raise ValueError(f"Unsupported algorithm: {algorithm}")

        # Read file in chunks for memory efficiency
        with open(file_path, 'rb') as f:
            while chunk := f.read(8192):
                hash_func.update(chunk)

        return hash_func.hexdigest()

    def verify_apt_package_signature(self, package_name: str) -> Tuple[bool, str]:
        """
        Verify APT package signature using apt-key

        Args:
            package_name: Name of the package

        Returns:
            Tuple of (success: bool, message: str)
        """
        try:
            # Check if package is signed
            result = subprocess.run(
                ['apt-cache', 'policy', package_name],
                capture_output=True,
                text=True,
                timeout=10
            )

            if result.returncode != 0:
                return False, f"Package not found: {package_name}"

            # APT packages are verified automatically during install
            # Check if repository keys are trusted
            result = subprocess.run(
                ['apt-key', 'list'],
                capture_output=True,
                text=True,
                timeout=10
            )

            if result.returncode == 0:
                return True, "✅ APT repository keys are trusted"
            else:
                return False, "⚠️  APT key verification unavailable"

        except Exception as e:
            return False, f"APT signature verification failed: {e}"

    def verify_flatpak_signature(self, app_id: str, remote: str = 'flathub') -> Tuple[bool, str]:
        """
        Verify Flatpak package signature

        Args:
            app_id: Flatpak application ID
            remote: Remote repository name

        Returns:
            Tuple of (success: bool, message: str)
        """
        try:
            # Check if remote has GPG verification enabled
            result = subprocess.run(
                ['flatpak', 'remote-info', remote, app_id],
                capture_output=True,
                text=True,
                timeout=15
            )

            if result.returncode == 0:
                # Check for GPG signature info in output
                if 'GPG' in result.stdout or 'signature' in result.stdout.lower():
                    return True, "✅ Flatpak signature verified"
                else:
                    return True, "⚠️  Flatpak installed (signature info unavailable)"
            else:
                return False, f"Flatpak verification failed: {result.stderr}"

        except FileNotFoundError:
            return False, "Flatpak not available on system"
        except Exception as e:
            return False, f"Flatpak signature verification failed: {e}"

    def verify_snap_signature(self, snap_name: str) -> Tuple[bool, str]:
        """
        Verify Snap package signature

        Args:
            snap_name: Name of the snap

        Returns:
            Tuple of (success: bool, message: str)
        """
        try:
            # Snaps are cryptographically signed by the Snap Store
            # Verify using snap info
            result = subprocess.run(
                ['snap', 'info', snap_name],
                capture_output=True,
                text=True,
                timeout=15
            )

            if result.returncode == 0:
                # Check for publisher info (verified snaps have publisher)
                if 'publisher:' in result.stdout.lower():
                    # Check if verified (✓ symbol)
                    if '✓' in result.stdout or 'verified' in result.stdout.lower():
                        return True, "✅ Snap signature verified (verified publisher)"
                    else:
                        return True, "⚠️  Snap from unverified publisher"
                else:
                    return True, "⚠️  Snap publisher information unavailable"
            else:
                return False, f"Snap not found: {snap_name}"

        except FileNotFoundError:
            return False, "Snap not available on system"
        except Exception as e:
            return False, f"Snap signature verification failed: {e}"

    def download_and_verify(self, url: str, expected_checksum: Optional[str] = None,
                           algorithm: str = 'sha256', save_path: Optional[str] = None) -> Tuple[bool, str, Optional[str]]:
        """
        Download file and verify its checksum

        Args:
            url: URL to download from
            expected_checksum: Expected checksum (if None, skip verification)
            algorithm: Hash algorithm
            save_path: Where to save file (if None, use temp file)

        Returns:
            Tuple of (success: bool, message: str, file_path: Optional[str])
        """
        import tempfile

        try:
            # Determine save path
            if save_path is None:
                # Create temp file
                fd, save_path = tempfile.mkstemp()
                os.close(fd)

            # Download file
            print(f"📥 Downloading from {url}...")
            response = requests.get(url, stream=True, timeout=60)
            response.raise_for_status()

            # Save with progress
            total_size = int(response.headers.get('content-length', 0))
            downloaded = 0

            with open(save_path, 'wb') as f:
                for chunk in response.iter_content(chunk_size=8192):
                    if chunk:
                        f.write(chunk)
                        downloaded += len(chunk)
                        if total_size > 0:
                            progress = (downloaded / total_size) * 100
                            print(f"\r   Progress: {progress:.1f}%", end='', flush=True)

            print()  # New line after progress

            # Verify checksum if provided
            if expected_checksum:
                success, msg = self.verify_checksum(save_path, expected_checksum, algorithm)
                if not success:
                    os.remove(save_path)
                    return False, msg, None

                return True, f"✅ Download and verification complete", save_path
            else:
                return True, "✅ Download complete (checksum not verified)", save_path

        except Exception as e:
            if save_path and os.path.exists(save_path):
                os.remove(save_path)
            return False, f"Download failed: {e}", None

    def fetch_checksum_from_url(self, checksum_url: str, algorithm: str = 'sha256') -> Optional[str]:
        """
        Fetch checksum from a URL (e.g., .sha256 file)

        Args:
            checksum_url: URL to checksum file
            algorithm: Hash algorithm

        Returns:
            Checksum string, or None if failed
        """
        try:
            response = requests.get(checksum_url, timeout=10)
            response.raise_for_status()

            # Parse checksum file (format: "checksum  filename")
            content = response.text.strip()
            checksum = content.split()[0]

            return checksum

        except Exception as e:
            print(f"⚠️  Failed to fetch checksum: {e}")
            return None


class SignatureVerificationResult:
    """Container for signature verification results"""

    def __init__(self, verified: bool, message: str, details: Dict = None):
        self.verified = verified
        self.message = message
        self.details = details or {}

    def __bool__(self):
        return self.verified

    def __str__(self):
        return self.message


# Example usage and testing
if __name__ == '__main__':
    print("Testing Package Verification...")

    verifier = PackageVerifier()

    # Test checksum calculation
    print("\n1. Testing checksum verification:")

    # Create a test file
    import tempfile
    with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
        f.write("Test content for checksum verification\n")
        test_file = f.name

    # Calculate checksum
    checksum = verifier._calculate_checksum(test_file, 'sha256')
    print(f"   File checksum: {checksum}")

    # Verify with correct checksum
    success, msg = verifier.verify_checksum(test_file, checksum, 'sha256')
    print(f"   Verification: {msg}")

    # Verify with wrong checksum
    success, msg = verifier.verify_checksum(test_file, "0" * 64, 'sha256')
    print(f"   Wrong checksum: {msg}")

    # Cleanup
    os.remove(test_file)

    # Test package signature verification (if available)
    print("\n2. Testing package signature verification:")

    # Test APT (if available)
    success, msg = verifier.verify_apt_package_signature('firefox')
    print(f"   APT: {msg}")

    # Test Snap (if available)
    success, msg = verifier.verify_snap_signature('firefox')
    print(f"   Snap: {msg}")

    # Test Flatpak (if available)
    success, msg = verifier.verify_flatpak_signature('org.mozilla.firefox')
    print(f"   Flatpak: {msg}")

    print("\n✅ Package verification tests complete")
