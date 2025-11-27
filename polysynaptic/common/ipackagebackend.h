/* ipackagebackend.h - Abstract interface for package management backends
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * PolySynaptic Backend Abstraction Layer
 * ========================================
 *
 * This file defines the interface that all package management backends
 * (APT, Snap, Flatpak) must implement. The abstraction allows the UI
 * to work with packages uniformly regardless of their source.
 *
 * Architecture:
 *   IPackageBackend (interface)
 *       ├── AptBackend    - Uses existing Synaptic/libapt-pkg logic
 *       ├── SnapBackend   - Uses snap CLI commands
 *       └── FlatpakBackend - Uses flatpak CLI commands
 *
 * To add a new backend (e.g., AppImage, Nix):
 *   1. Create a new class implementing IPackageBackend
 *   2. Register it in BackendManager
 *   3. Add UI toggle in settings
 */

#ifndef _IPACKAGEBACKEND_H_
#define _IPACKAGEBACKEND_H_

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>

using namespace std;

namespace PolySynaptic {

// ============================================================================
// Backend Types Enumeration
// ============================================================================

enum class BackendType {
    APT,
    SNAP,
    FLATPAK,
    UNKNOWN
};

// Convert BackendType to human-readable string
inline const char* backendTypeToString(BackendType type) {
    switch (type) {
        case BackendType::APT:      return "APT";
        case BackendType::SNAP:     return "Snap";
        case BackendType::FLATPAK:  return "Flatpak";
        default:                    return "Unknown";
    }
}

// Get short badge text for UI
inline const char* backendTypeToBadge(BackendType type) {
    switch (type) {
        case BackendType::APT:      return "deb";
        case BackendType::SNAP:     return "snap";
        case BackendType::FLATPAK:  return "flatpak";
        default:                    return "?";
    }
}

// ============================================================================
// Package Installation Status
// ============================================================================

enum class InstallStatus {
    NOT_INSTALLED,      // Package is available but not installed
    INSTALLED,          // Package is installed and up to date
    UPDATE_AVAILABLE,   // Package is installed but update exists
    INSTALLING,         // Currently being installed
    REMOVING,           // Currently being removed
    BROKEN,             // Installed but in broken state
    UNKNOWN             // Status could not be determined
};

inline const char* installStatusToString(InstallStatus status) {
    switch (status) {
        case InstallStatus::NOT_INSTALLED:   return "Available";
        case InstallStatus::INSTALLED:       return "Installed";
        case InstallStatus::UPDATE_AVAILABLE: return "Update Available";
        case InstallStatus::INSTALLING:      return "Installing...";
        case InstallStatus::REMOVING:        return "Removing...";
        case InstallStatus::BROKEN:          return "Broken";
        default:                             return "Unknown";
    }
}

// ============================================================================
// Unified Package Information Structure
// ============================================================================

/**
 * PackageInfo - Unified package representation across all backends
 *
 * This structure normalizes package metadata from different sources
 * (APT, Snap, Flatpak) into a common format that the UI can display.
 *
 * Not all fields may be available for all backends - check for empty
 * strings or 0 values where appropriate.
 */
struct PackageInfo {
    // === Identification ===
    string id;                  // Unique identifier (backend-specific format)
    string name;                // Human-readable package name
    BackendType backend;        // Source backend (APT/SNAP/FLATPAK)

    // === Description ===
    string summary;             // One-line description/summary
    string description;         // Full description text

    // === Versioning ===
    string version;             // Available/candidate version
    string installedVersion;    // Currently installed version (empty if not installed)
    InstallStatus installStatus; // Current installation status

    // === Metadata ===
    string section;             // Category/section (e.g., "utils", "games")
    string homepage;            // Project homepage URL
    string maintainer;          // Package maintainer
    string license;             // License type (if available)

    // === Size Information ===
    long downloadSize;          // Download size in bytes (0 if unknown)
    long installedSize;         // Installed size in bytes (0 if unknown)

    // === Backend-Specific Data ===
    string origin;              // Package origin/repository
    string architecture;        // Target architecture

    // === Snap-Specific ===
    string channel;             // Snap channel (stable/beta/edge)
    string confinement;         // Snap confinement (strict/classic/devmode)
    string publisher;           // Snap publisher name
    bool isClassic;             // Requires --classic flag

    // === Flatpak-Specific ===
    string remote;              // Flatpak remote name (e.g., "flathub")
    string ref;                 // Full flatpak ref (e.g., "app/org.example.App/x86_64/stable")
    string branch;              // Flatpak branch (usually "stable")
    string runtimeRef;          // Required runtime ref

    // === UI State ===
    bool isMarkedForInstall;    // User has marked for installation
    bool isMarkedForRemoval;    // User has marked for removal
    bool isMarkedForUpgrade;    // User has marked for upgrade

    // Default constructor
    PackageInfo()
        : backend(BackendType::UNKNOWN)
        , installStatus(InstallStatus::UNKNOWN)
        , downloadSize(0)
        , installedSize(0)
        , isClassic(false)
        , isMarkedForInstall(false)
        , isMarkedForRemoval(false)
        , isMarkedForUpgrade(false)
    {}

    // Convenience constructor with basic fields
    PackageInfo(const string& _id, const string& _name, BackendType _backend)
        : id(_id)
        , name(_name)
        , backend(_backend)
        , installStatus(InstallStatus::UNKNOWN)
        , downloadSize(0)
        , installedSize(0)
        , isClassic(false)
        , isMarkedForInstall(false)
        , isMarkedForRemoval(false)
        , isMarkedForUpgrade(false)
    {}

    // Check if package is installed (any version)
    bool isInstalled() const {
        return installStatus == InstallStatus::INSTALLED ||
               installStatus == InstallStatus::UPDATE_AVAILABLE ||
               installStatus == InstallStatus::BROKEN;
    }

    // Get display version (installed if available, otherwise available)
    string getDisplayVersion() const {
        return installedVersion.empty() ? version : installedVersion;
    }

    // Get unique key for deduplication across backends
    string getUniqueKey() const {
        return name + ":" + backendTypeToString(backend);
    }
};

// ============================================================================
// Backend Operation Result
// ============================================================================

/**
 * OperationResult - Result of a backend operation
 *
 * Contains success/failure status along with any messages or errors.
 */
struct OperationResult {
    bool success;               // Whether operation succeeded
    string message;             // Human-readable message
    string errorDetails;        // Technical error details (for logging)
    int exitCode;               // Process exit code (for CLI operations)

    OperationResult() : success(false), exitCode(0) {}

    static OperationResult Success(const string& msg = "") {
        OperationResult r;
        r.success = true;
        r.message = msg;
        return r;
    }

    static OperationResult Failure(const string& msg, const string& details = "", int code = 1) {
        OperationResult r;
        r.success = false;
        r.message = msg;
        r.errorDetails = details;
        r.exitCode = code;
        return r;
    }
};

// ============================================================================
// Progress Callback Interface
// ============================================================================

/**
 * ProgressCallback - Called during long-running operations
 *
 * Parameters:
 *   - current: Current progress (0.0 to 1.0)
 *   - message: Current status message
 *
 * Return: false to cancel the operation, true to continue
 */
using ProgressCallback = function<bool(double current, const string& message)>;

// ============================================================================
// Backend Query Options
// ============================================================================

/**
 * SearchOptions - Options for package search operations
 */
struct SearchOptions {
    string query;               // Search query string
    bool searchNames;           // Search in package names
    bool searchDescriptions;    // Search in descriptions
    bool installedOnly;         // Only return installed packages
    bool availableOnly;         // Only return non-installed packages
    int maxResults;             // Maximum results to return (0 = unlimited)

    SearchOptions()
        : searchNames(true)
        , searchDescriptions(true)
        , installedOnly(false)
        , availableOnly(false)
        , maxResults(500)
    {}
};

// ============================================================================
// Package Backend Interface
// ============================================================================

/**
 * IPackageBackend - Abstract interface for package management backends
 *
 * All package management systems (APT, Snap, Flatpak) must implement
 * this interface to be usable in PolySynaptic.
 *
 * Thread Safety:
 *   Backend implementations should be thread-safe for read operations.
 *   Write operations (install/remove/update) should be serialized.
 *
 * Error Handling:
 *   Methods should not throw exceptions. Instead, return error information
 *   via OperationResult or empty vectors with error state set.
 */
class IPackageBackend {
public:
    virtual ~IPackageBackend() = default;

    // ========================================================================
    // Backend Information
    // ========================================================================

    /**
     * Get the type of this backend
     */
    virtual BackendType getType() const = 0;

    /**
     * Get human-readable backend name
     */
    virtual string getName() const = 0;

    /**
     * Check if this backend is available on the system
     * (e.g., snap is installed and running, flatpak is configured)
     */
    virtual bool isAvailable() const = 0;

    /**
     * Get reason why backend is unavailable (if !isAvailable())
     */
    virtual string getUnavailableReason() const = 0;

    /**
     * Get backend version string
     */
    virtual string getVersion() const = 0;

    // ========================================================================
    // Package Discovery & Search
    // ========================================================================

    /**
     * Search for packages matching the given options
     *
     * @param options Search criteria
     * @param progress Optional progress callback
     * @return Vector of matching packages
     */
    virtual vector<PackageInfo> searchPackages(
        const SearchOptions& options,
        ProgressCallback progress = nullptr) = 0;

    /**
     * Get all installed packages from this backend
     *
     * @param progress Optional progress callback
     * @return Vector of installed packages
     */
    virtual vector<PackageInfo> getInstalledPackages(
        ProgressCallback progress = nullptr) = 0;

    /**
     * Get detailed information about a specific package
     *
     * @param packageId Backend-specific package identifier
     * @return Package info, or empty PackageInfo if not found
     */
    virtual PackageInfo getPackageDetails(const string& packageId) = 0;

    /**
     * Check if a package is installed
     *
     * @param packageId Backend-specific package identifier
     * @return Current installation status
     */
    virtual InstallStatus getInstallStatus(const string& packageId) = 0;

    /**
     * Get packages that have updates available
     *
     * @param progress Optional progress callback
     * @return Vector of packages with available updates
     */
    virtual vector<PackageInfo> getUpgradablePackages(
        ProgressCallback progress = nullptr) = 0;

    // ========================================================================
    // Package Operations
    // ========================================================================

    /**
     * Install a package
     *
     * @param packageId Package identifier
     * @param progress Progress callback
     * @return Operation result
     */
    virtual OperationResult installPackage(
        const string& packageId,
        ProgressCallback progress = nullptr) = 0;

    /**
     * Remove a package
     *
     * @param packageId Package identifier
     * @param purge If true, remove configuration files as well
     * @param progress Progress callback
     * @return Operation result
     */
    virtual OperationResult removePackage(
        const string& packageId,
        bool purge = false,
        ProgressCallback progress = nullptr) = 0;

    /**
     * Update a package to latest version
     *
     * @param packageId Package identifier
     * @param progress Progress callback
     * @return Operation result
     */
    virtual OperationResult updatePackage(
        const string& packageId,
        ProgressCallback progress = nullptr) = 0;

    /**
     * Refresh package cache/index from repositories
     *
     * @param progress Progress callback
     * @return Operation result
     */
    virtual OperationResult refreshCache(
        ProgressCallback progress = nullptr) = 0;

    // ========================================================================
    // Batch Operations
    // ========================================================================

    /**
     * Install multiple packages
     *
     * @param packageIds Package identifiers to install
     * @param progress Progress callback
     * @return Operation result
     */
    virtual OperationResult installPackages(
        const vector<string>& packageIds,
        ProgressCallback progress = nullptr);

    /**
     * Remove multiple packages
     *
     * @param packageIds Package identifiers to remove
     * @param purge If true, remove configuration files
     * @param progress Progress callback
     * @return Operation result
     */
    virtual OperationResult removePackages(
        const vector<string>& packageIds,
        bool purge = false,
        ProgressCallback progress = nullptr);

    // ========================================================================
    // Repository/Source Management (optional)
    // ========================================================================

    /**
     * Check if this backend supports repository management
     */
    virtual bool supportsRepositories() const { return false; }

    /**
     * Get list of configured repositories/remotes
     */
    virtual vector<string> getRepositories() { return {}; }

    /**
     * Add a repository/remote
     */
    virtual OperationResult addRepository(const string& repo) {
        return OperationResult::Failure("Not supported by this backend");
    }

    /**
     * Remove a repository/remote
     */
    virtual OperationResult removeRepository(const string& repo) {
        return OperationResult::Failure("Not supported by this backend");
    }
};

// Default implementation for batch operations (can be overridden)
inline OperationResult IPackageBackend::installPackages(
    const vector<string>& packageIds,
    ProgressCallback progress)
{
    int total = packageIds.size();
    int current = 0;

    for (const auto& id : packageIds) {
        if (progress) {
            double pct = static_cast<double>(current) / total;
            if (!progress(pct, "Installing " + id + "...")) {
                return OperationResult::Failure("Operation cancelled");
            }
        }

        auto result = installPackage(id, nullptr);
        if (!result.success) {
            return result;
        }

        current++;
    }

    return OperationResult::Success("Installed " + to_string(total) + " packages");
}

inline OperationResult IPackageBackend::removePackages(
    const vector<string>& packageIds,
    bool purge,
    ProgressCallback progress)
{
    int total = packageIds.size();
    int current = 0;

    for (const auto& id : packageIds) {
        if (progress) {
            double pct = static_cast<double>(current) / total;
            if (!progress(pct, "Removing " + id + "...")) {
                return OperationResult::Failure("Operation cancelled");
            }
        }

        auto result = removePackage(id, purge, nullptr);
        if (!result.success) {
            return result;
        }

        current++;
    }

    return OperationResult::Success("Removed " + to_string(total) + " packages");
}

} // namespace PolySynaptic

#endif // _IPACKAGEBACKEND_H_

// vim:ts=4:sw=4:et
