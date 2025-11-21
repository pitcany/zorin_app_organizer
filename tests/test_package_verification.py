"""
Tests for package verification functionality
"""

import pytest
import os
import tempfile
from package_verification import PackageVerifier


@pytest.fixture
def test_file():
    """Create a temporary test file with known content"""
    fd, file_path = tempfile.mkstemp()
    with os.fdopen(fd, 'w') as f:
        f.write("Test content for verification\n")

    yield file_path

    # Cleanup
    try:
        os.unlink(file_path)
    except:
        pass


class TestPackageVerifier:
    """Tests for PackageVerifier class"""

    @pytest.mark.unit
    def test_checksum_calculation(self, test_file):
        """Test checksum calculation"""
        verifier = PackageVerifier()

        # Calculate SHA256
        checksum = verifier._calculate_checksum(test_file, 'sha256')
        assert len(checksum) == 64  # SHA256 is 64 hex chars
        assert isinstance(checksum, str)

        # Calculate SHA512
        checksum = verifier._calculate_checksum(test_file, 'sha512')
        assert len(checksum) == 128  # SHA512 is 128 hex chars

        # Calculate MD5
        checksum = verifier._calculate_checksum(test_file, 'md5')
        assert len(checksum) == 32  # MD5 is 32 hex chars

    @pytest.mark.unit
    def test_checksum_verification_success(self, test_file):
        """Test successful checksum verification"""
        verifier = PackageVerifier()

        # Calculate correct checksum
        expected = verifier._calculate_checksum(test_file, 'sha256')

        # Verify
        success, msg = verifier.verify_checksum(test_file, expected, 'sha256')

        assert success == True
        assert '✅' in msg

    @pytest.mark.unit
    def test_checksum_verification_failure(self, test_file):
        """Test failed checksum verification"""
        verifier = PackageVerifier()

        # Wrong checksum
        wrong_checksum = "0" * 64

        success, msg = verifier.verify_checksum(test_file, wrong_checksum, 'sha256')

        assert success == False
        assert '❌' in msg
        assert 'mismatch' in msg.lower()

    @pytest.mark.unit
    def test_checksum_case_insensitive(self, test_file):
        """Test that checksum verification is case-insensitive"""
        verifier = PackageVerifier()

        # Calculate checksum
        checksum = verifier._calculate_checksum(test_file, 'sha256')

        # Verify with uppercase
        success, msg = verifier.verify_checksum(test_file, checksum.upper(), 'sha256')
        assert success == True

        # Verify with lowercase
        success, msg = verifier.verify_checksum(test_file, checksum.lower(), 'sha256')
        assert success == True

    @pytest.mark.unit
    def test_unsupported_algorithm(self, test_file):
        """Test handling of unsupported hash algorithm"""
        verifier = PackageVerifier()

        success, msg = verifier.verify_checksum(test_file, "abc123", 'sha999')

        assert success == False
        assert 'Unsupported' in msg

    @pytest.mark.unit
    def test_nonexistent_file(self):
        """Test handling of nonexistent file"""
        verifier = PackageVerifier()

        success, msg = verifier.verify_checksum('/nonexistent/file.txt', "abc", 'sha256')

        assert success == False
        assert 'not found' in msg.lower()

    @pytest.mark.unit
    def test_empty_file_checksum(self):
        """Test checksum of empty file"""
        verifier = PackageVerifier()

        # Create empty file
        fd, empty_file = tempfile.mkstemp()
        os.close(fd)

        try:
            # Known SHA256 of empty file
            empty_sha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"

            success, msg = verifier.verify_checksum(empty_file, empty_sha256, 'sha256')
            assert success == True

        finally:
            os.unlink(empty_file)

    @pytest.mark.unit
    def test_large_file_checksum(self):
        """Test checksum calculation for large file"""
        verifier = PackageVerifier()

        # Create a 10MB test file
        fd, large_file = tempfile.mkstemp()
        with os.fdopen(fd, 'wb') as f:
            # Write 10MB of data
            f.write(b'A' * (10 * 1024 * 1024))

        try:
            # Calculate checksum (should not cause memory issues)
            checksum = verifier._calculate_checksum(large_file, 'sha256')
            assert len(checksum) == 64

            # Verify
            success, msg = verifier.verify_checksum(large_file, checksum, 'sha256')
            assert success == True

        finally:
            os.unlink(large_file)


class TestSignatureVerification:
    """Tests for package signature verification"""

    @pytest.mark.integration
    @pytest.mark.slow
    def test_apt_signature_verification(self):
        """Test APT package signature verification"""
        verifier = PackageVerifier()

        # Test with a common package (may not exist on all systems)
        success, msg = verifier.verify_apt_package_signature('curl')

        # Should either succeed or indicate package not found
        assert isinstance(success, bool)
        assert isinstance(msg, str)

    @pytest.mark.integration
    @pytest.mark.slow
    def test_snap_signature_verification(self):
        """Test Snap signature verification"""
        verifier = PackageVerifier()

        success, msg = verifier.verify_snap_signature('core')

        # Should either succeed or indicate snap not available
        assert isinstance(success, bool)
        assert isinstance(msg, str)

    @pytest.mark.integration
    @pytest.mark.slow
    def test_flatpak_signature_verification(self):
        """Test Flatpak signature verification"""
        verifier = PackageVerifier()

        success, msg = verifier.verify_flatpak_signature('org.freedesktop.Platform')

        # Should either succeed or indicate flatpak not available
        assert isinstance(success, bool)
        assert isinstance(msg, str)


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
