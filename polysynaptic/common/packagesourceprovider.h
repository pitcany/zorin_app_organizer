/* packagesourceprovider.h - Clean abstraction for package source providers
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This file defines the PackageSourceProvider interface - a cleaner, more
 * extensible abstraction than IPackageBackend. It supports:
 *   - Dependency injection
 *   - Provider registration at runtime
 *   - Capability-based feature detection
 *   - Structured metadata for ranking/comparison
 *
 * To add a new provider (AppImage, Nix, Homebrew, etc.):
 *   1. Implement PackageSourceProvider
 *   2. Register with ProviderRegistry::registerProvider()
 *   3. No core modifications needed
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef _PACKAGESOURCEPROVIDER_H_
#define _PACKAGESOURCEPROVIDER_H_

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <set>
#include <chrono>
#include <mutex>
#include <any>

namespace PolySynaptic {

// ============================================================================
// Forward Declarations
// ============================================================================

class PackageSourceProvider;
class ProviderRegistry;
struct UnifiedPackage;
struct ProviderCapabilities;
struct PackageMetadata;

// ============================================================================
// Provider Identification
// ============================================================================

/**
 * SourceType - Unique identifier for package sources
 */
enum class SourceType {
    APT,
    SNAP,
    FLATPAK,
    APPIMAGE,
    NIX,
    HOMEBREW,
    CUSTOM
};

inline const char* sourceTypeToString(SourceType type) {
    switch (type) {
        case SourceType::APT:       return "APT";
        case SourceType::SNAP:      return "Snap";
        case SourceType::FLATPAK:   return "Flatpak";
        case SourceType::APPIMAGE:  return "AppImage";
        case SourceType::NIX:       return "Nix";
        case SourceType::HOMEBREW:  return "Homebrew";
        case SourceType::CUSTOM:    return "Custom";
    }
    return "Unknown";
}

inline const char* sourceTypeToBadge(SourceType type) {
    switch (type) {
        case SourceType::APT:       return "deb";
        case SourceType::SNAP:      return "snap";
        case SourceType::FLATPAK:   return "flatpak";
        case SourceType::APPIMAGE:  return "appimage";
        case SourceType::NIX:       return "nix";
        case SourceType::HOMEBREW:  return "brew";
        case SourceType::CUSTOM:    return "custom";
    }
    return "?";
}

// ============================================================================
// Trust Level for Package Ranking
// ============================================================================

/**
 * TrustLevel - Used for ranking packages from different sources
 *
 * Higher value = more trusted
 */
enum class TrustLevel {
    UNTRUSTED = 0,      // Unknown or unverified source
    COMMUNITY = 1,      // Community-maintained
    VERIFIED = 2,       // Verified publisher
    OFFICIAL = 3,       // Official distribution packages
    SYSTEM = 4          // Core system packages
};

inline int trustLevelToScore(TrustLevel level) {
    return static_cast<int>(level);
}

// ============================================================================
// Provider Capabilities
// ============================================================================

/**
 * ProviderCapabilities - Describes what a provider can do
 *
 * Used for feature detection and UI adaptation
 */
struct ProviderCapabilities {
    // Basic operations
    bool canSearch = false;
    bool canInstall = false;
    bool canRemove = false;
    bool canUpdate = false;
    bool canListInstalled = false;

    // Advanced features
    bool supportsDependencies = false;      // Handles dependencies automatically
    bool supportsRollback = false;          // Can rollback to previous versions
    bool supportsChannels = false;          // Multiple release channels
    bool supportsRemotes = false;           // Multiple package sources
    bool supportsUserInstall = false;       // Per-user installation
    bool supportsSystemInstall = false;     // System-wide installation
    bool supportsConfinement = false;       // Sandboxing/confinement info
    bool supportsPermissions = false;       // Permission management
    bool supportsAutoUpdate = false;        // Background updates

    // Metadata availability
    bool providesSize = false;
    bool providesLicense = false;
    bool providesScreenshots = false;
    bool providesRatings = false;
    bool providesChangelog = false;
    bool providesUpdateTime = false;

    // Security
    bool verifiedPublisher = false;         // Can verify publisher identity
    bool signedPackages = false;            // Cryptographic signatures
};

// ============================================================================
// Package Status
// ============================================================================

enum class PackageStatus {
    AVAILABLE,          // Not installed, available
    INSTALLED,          // Installed, up to date
    UPGRADABLE,         // Installed, update available
    INSTALLING,         // Currently installing
    REMOVING,           // Currently removing
    BROKEN,             // Installation broken
    HELD,               // Held back from updates
    UNKNOWN
};

// ============================================================================
// Confinement Level (for sandboxed packages)
// ============================================================================

enum class ConfinementLevel {
    NONE,               // No sandboxing
    STRICT,             // Fully sandboxed
    CLASSIC,            // Has system access
    DEVMODE,            // Development mode (loose sandbox)
    CUSTOM
};

inline const char* confinementToString(ConfinementLevel level) {
    switch (level) {
        case ConfinementLevel::NONE:    return "None";
        case ConfinementLevel::STRICT:  return "Strict";
        case ConfinementLevel::CLASSIC: return "Classic";
        case ConfinementLevel::DEVMODE: return "DevMode";
        case ConfinementLevel::CUSTOM:  return "Custom";
    }
    return "Unknown";
}

// ============================================================================
// Permission Types (for sandboxed packages)
// ============================================================================

struct PackagePermissions {
    bool hasNetworkAccess = false;
    bool hasHomeAccess = false;
    bool hasSystemBusAccess = false;
    bool hasSessionBusAccess = false;
    bool hasDeviceAccess = false;
    bool hasAudioAccess = false;
    bool hasDisplayAccess = false;
    bool hasGpuAccess = false;
    bool hasCameraAccess = false;
    bool hasBluetoothAccess = false;
    bool hasUsbAccess = false;
    bool hasFileSystemAccess = false;

    std::vector<std::string> customPermissions;

    // Get human-readable permission list
    std::vector<std::string> getPermissionList() const {
        std::vector<std::string> perms;
        if (hasNetworkAccess) perms.push_back("Network");
        if (hasHomeAccess) perms.push_back("Home Folder");
        if (hasSystemBusAccess) perms.push_back("System Bus");
        if (hasSessionBusAccess) perms.push_back("Session Bus");
        if (hasDeviceAccess) perms.push_back("Devices");
        if (hasAudioAccess) perms.push_back("Audio");
        if (hasDisplayAccess) perms.push_back("Display");
        if (hasGpuAccess) perms.push_back("GPU");
        if (hasCameraAccess) perms.push_back("Camera");
        if (hasBluetoothAccess) perms.push_back("Bluetooth");
        if (hasUsbAccess) perms.push_back("USB");
        if (hasFileSystemAccess) perms.push_back("File System");
        for (const auto& p : customPermissions) {
            perms.push_back(p);
        }
        return perms;
    }
};

// ============================================================================
// Unified Package Metadata
// ============================================================================

/**
 * PackageMetadata - Extended metadata for package comparison and display
 */
struct PackageMetadata {
    // Timestamps
    std::chrono::system_clock::time_point publishedAt;
    std::chrono::system_clock::time_point lastUpdatedAt;
    std::chrono::system_clock::time_point installedAt;

    // Trust & Verification
    TrustLevel trustLevel = TrustLevel::COMMUNITY;
    bool isVerifiedPublisher = false;
    bool hasValidSignature = false;
    std::string signatureInfo;

    // Sandboxing
    ConfinementLevel confinement = ConfinementLevel::NONE;
    PackagePermissions permissions;

    // Source-specific
    std::string remoteName;         // Flatpak remote
    std::string channel;            // Snap channel
    std::string branch;             // Flatpak branch
    std::string runtime;            // Required runtime

    // Statistics
    int downloadCount = 0;
    double rating = 0.0;
    int ratingCount = 0;

    // Extended info
    std::vector<std::string> screenshots;
    std::vector<std::string> categories;
    std::vector<std::string> keywords;
    std::string changelogUrl;
    std::string supportUrl;
    std::string bugTrackerUrl;
    std::string donationUrl;

    // Dependencies
    std::vector<std::string> dependencies;
    std::vector<std::string> recommends;
    std::vector<std::string> conflicts;

    // Arbitrary provider-specific data
    std::map<std::string, std::any> customData;
};

// ============================================================================
// Unified Package Structure
// ============================================================================

/**
 * UnifiedPackage - Complete package representation for ranking and display
 */
struct UnifiedPackage {
    // Identification
    std::string id;                     // Provider-specific ID
    std::string name;                   // Display name
    std::string uniqueKey;              // Globally unique (name:source)
    SourceType source;                  // Which provider

    // Description
    std::string summary;                // One-line description
    std::string description;            // Full description

    // Versions
    std::string availableVersion;
    std::string installedVersion;
    PackageStatus status = PackageStatus::UNKNOWN;

    // Size
    int64_t downloadSize = 0;           // In bytes
    int64_t installedSize = 0;          // In bytes

    // Basic metadata
    std::string homepage;
    std::string maintainer;
    std::string publisher;
    std::string license;
    std::string section;
    std::string architecture;

    // Extended metadata
    PackageMetadata metadata;

    // UI state
    bool isMarkedInstall = false;
    bool isMarkedRemove = false;
    bool isMarkedUpgrade = false;

    // Ranking score (computed)
    double rankingScore = 0.0;
    std::string rankingExplanation;

    // Default constructor
    UnifiedPackage() = default;

    // Convenience constructor
    UnifiedPackage(const std::string& _id, const std::string& _name, SourceType _source)
        : id(_id), name(_name), source(_source)
    {
        uniqueKey = _name + ":" + sourceTypeToString(_source);
    }

    bool isInstalled() const {
        return status == PackageStatus::INSTALLED ||
               status == PackageStatus::UPGRADABLE ||
               status == PackageStatus::HELD;
    }

    std::string getDisplayVersion() const {
        return installedVersion.empty() ? availableVersion : installedVersion;
    }
};

// ============================================================================
// Search Query
// ============================================================================

struct SearchQuery {
    std::string text;                   // Search text
    bool searchNames = true;            // Search in package names
    bool searchDescriptions = true;     // Search in descriptions
    bool searchKeywords = true;         // Search in keywords

    // Filters
    std::set<SourceType> sources;       // Empty = all sources
    std::set<std::string> categories;   // Filter by category
    bool installedOnly = false;
    bool upgradableOnly = false;

    // Pagination
    int offset = 0;
    int limit = 500;

    // Sorting
    enum class SortBy { RELEVANCE, NAME, SIZE, TRUST, UPDATE_TIME } sortBy = SortBy::RELEVANCE;
    bool sortDescending = false;
};

// ============================================================================
// Operation Result
// ============================================================================

struct ProviderResult {
    bool success = false;
    std::string message;
    std::string errorCode;
    std::string rawStderr;              // For debugging
    int exitCode = 0;

    // Timing info for logging
    std::chrono::milliseconds duration{0};

    static ProviderResult Success(const std::string& msg = "") {
        ProviderResult r;
        r.success = true;
        r.message = msg;
        return r;
    }

    static ProviderResult Failure(const std::string& msg,
                                   const std::string& code = "",
                                   const std::string& stderr_ = "",
                                   int exit = 1) {
        ProviderResult r;
        r.success = false;
        r.message = msg;
        r.errorCode = code;
        r.rawStderr = stderr_;
        r.exitCode = exit;
        return r;
    }
};

// ============================================================================
// Progress Callback
// ============================================================================

using ProgressCallback = std::function<bool(double progress, const std::string& message)>;

// ============================================================================
// Package Source Provider Interface
// ============================================================================

/**
 * PackageSourceProvider - Base interface for all package providers
 *
 * This is the main abstraction that providers must implement.
 * It supports dependency injection and runtime registration.
 */
class PackageSourceProvider {
public:
    virtual ~PackageSourceProvider() = default;

    // ========================================================================
    // Provider Identity
    // ========================================================================

    /**
     * Get unique identifier for this provider type
     */
    virtual SourceType getSourceType() const = 0;

    /**
     * Get human-readable provider name
     */
    virtual std::string getName() const = 0;

    /**
     * Get provider version string
     */
    virtual std::string getVersion() const = 0;

    /**
     * Get provider capabilities
     */
    virtual ProviderCapabilities getCapabilities() const = 0;

    // ========================================================================
    // Availability & Health
    // ========================================================================

    /**
     * Check if provider is available on this system
     */
    virtual bool isAvailable() const = 0;

    /**
     * Get reason if not available
     */
    virtual std::string getUnavailableReason() const = 0;

    /**
     * Get current health status (for status bar)
     */
    virtual std::string getHealthStatus() const { return isAvailable() ? "OK" : "Unavailable"; }

    // ========================================================================
    // Package Discovery
    // ========================================================================

    /**
     * Search for packages
     */
    virtual std::vector<UnifiedPackage> search(
        const SearchQuery& query,
        ProgressCallback progress = nullptr) = 0;

    /**
     * Get all installed packages
     */
    virtual std::vector<UnifiedPackage> getInstalled(
        ProgressCallback progress = nullptr) = 0;

    /**
     * Get packages with available updates
     */
    virtual std::vector<UnifiedPackage> getUpgradable(
        ProgressCallback progress = nullptr) = 0;

    /**
     * Get detailed package info
     */
    virtual UnifiedPackage getPackageDetails(const std::string& id) = 0;

    // ========================================================================
    // Package Operations
    // ========================================================================

    /**
     * Install a package
     */
    virtual ProviderResult install(
        const std::string& id,
        const std::map<std::string, std::string>& options = {},
        ProgressCallback progress = nullptr) = 0;

    /**
     * Remove a package
     */
    virtual ProviderResult remove(
        const std::string& id,
        bool purge = false,
        ProgressCallback progress = nullptr) = 0;

    /**
     * Update a package
     */
    virtual ProviderResult update(
        const std::string& id,
        ProgressCallback progress = nullptr) = 0;

    /**
     * Refresh package cache
     */
    virtual ProviderResult refreshCache(
        ProgressCallback progress = nullptr) = 0;

    // ========================================================================
    // Trust & Ranking
    // ========================================================================

    /**
     * Get default trust level for packages from this provider
     */
    virtual TrustLevel getDefaultTrustLevel() const = 0;

    /**
     * Calculate trust score for a specific package (0.0 - 1.0)
     */
    virtual double calculateTrustScore(const UnifiedPackage& pkg) const {
        return trustLevelToScore(getDefaultTrustLevel()) / 4.0;
    }

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * Get provider-specific configuration options
     */
    virtual std::map<std::string, std::string> getConfiguration() const { return {}; }

    /**
     * Set provider-specific configuration
     */
    virtual void setConfiguration(const std::map<std::string, std::string>& config) {}

    // ========================================================================
    // Remotes/Repositories (optional)
    // ========================================================================

    /**
     * Get configured remotes/repositories
     */
    virtual std::vector<std::string> getRemotes() const { return {}; }

    /**
     * Add a remote/repository
     */
    virtual ProviderResult addRemote(const std::string& name, const std::string& url) {
        return ProviderResult::Failure("Not supported");
    }

    /**
     * Remove a remote/repository
     */
    virtual ProviderResult removeRemote(const std::string& name) {
        return ProviderResult::Failure("Not supported");
    }
};

// ============================================================================
// Provider Factory Function
// ============================================================================

/**
 * Factory function type for creating providers
 *
 * Used for dependency injection - pass dependencies via factory
 */
using ProviderFactory = std::function<std::unique_ptr<PackageSourceProvider>()>;

// ============================================================================
// Provider Registry
// ============================================================================

/**
 * ProviderRegistry - Singleton registry for package providers
 *
 * Supports:
 *   - Runtime provider registration
 *   - Dependency injection via factories
 *   - Provider discovery
 */
class ProviderRegistry {
public:
    /**
     * Get singleton instance
     */
    static ProviderRegistry& instance() {
        static ProviderRegistry registry;
        return registry;
    }

    /**
     * Register a provider factory
     *
     * @param type Source type identifier
     * @param factory Factory function to create provider instance
     */
    void registerProvider(SourceType type, ProviderFactory factory) {
        std::lock_guard<std::mutex> lock(_mutex);
        _factories[type] = std::move(factory);
    }

    /**
     * Create a provider instance
     */
    std::unique_ptr<PackageSourceProvider> createProvider(SourceType type) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _factories.find(type);
        if (it != _factories.end() && it->second) {
            return it->second();
        }
        return nullptr;
    }

    /**
     * Check if a provider type is registered
     */
    bool hasProvider(SourceType type) const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _factories.find(type) != _factories.end();
    }

    /**
     * Get all registered provider types
     */
    std::vector<SourceType> getRegisteredTypes() const {
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<SourceType> types;
        for (const auto& [type, _] : _factories) {
            types.push_back(type);
        }
        return types;
    }

    /**
     * Create all registered providers
     */
    std::vector<std::unique_ptr<PackageSourceProvider>> createAllProviders() {
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<std::unique_ptr<PackageSourceProvider>> providers;
        for (const auto& [type, factory] : _factories) {
            if (factory) {
                auto provider = factory();
                if (provider) {
                    providers.push_back(std::move(provider));
                }
            }
        }
        return providers;
    }

private:
    ProviderRegistry() = default;
    ProviderRegistry(const ProviderRegistry&) = delete;
    ProviderRegistry& operator=(const ProviderRegistry&) = delete;

    mutable std::mutex _mutex;
    std::map<SourceType, ProviderFactory> _factories;
};

// ============================================================================
// Helper Macros for Provider Registration
// ============================================================================

/**
 * REGISTER_PROVIDER - Register a provider at static initialization
 *
 * Usage:
 *   REGISTER_PROVIDER(SourceType::SNAP, SnapProvider);
 */
#define REGISTER_PROVIDER(type, ProviderClass) \
    namespace { \
        struct ProviderClass##Registrar { \
            ProviderClass##Registrar() { \
                PolySynaptic::ProviderRegistry::instance().registerProvider( \
                    type, \
                    []() -> std::unique_ptr<PolySynaptic::PackageSourceProvider> { \
                        return std::make_unique<ProviderClass>(); \
                    } \
                ); \
            } \
        } g_##ProviderClass##Registrar; \
    }

} // namespace PolySynaptic

#endif // _PACKAGESOURCEPROVIDER_H_

// vim:ts=4:sw=4:et
