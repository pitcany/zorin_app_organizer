/* aptprovider.cc - APT package source provider implementation
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include "aptprovider.h"
#include "rpackage.h"

#include <algorithm>
#include <regex>

namespace PolySynaptic {

// ============================================================================
// Constructor
// ============================================================================

APTProvider::APTProvider() : _lister(nullptr) {}

APTProvider::APTProvider(RPackageLister* lister) : _lister(lister) {}

// ============================================================================
// Availability & Configuration
// ============================================================================

bool APTProvider::isAvailable() const {
    // APT is always available on Debian-based systems
    // Check if apt-get exists
    return access("/usr/bin/apt-get", X_OK) == 0;
}

ProviderStatus APTProvider::getStatus() const {
    ProviderStatus status;
    status.available = isAvailable();
    status.enabled = true;  // APT is always enabled when available
    status.configured = (_lister != nullptr);
    status.errorMessage = "";

    if (!status.available) {
        status.errorMessage = "APT is not available on this system";
    } else if (!status.configured) {
        status.errorMessage = "APT lister not initialized";
    }

    // Get package counts if lister is available
    if (_lister) {
        int installed = 0, broken = 0, upgradable = 0;
        for (unsigned int i = 0; i < _lister->count(); i++) {
            RPackage* pkg = _lister->getElement(i);
            if (pkg) {
                if (pkg->installedVersion()) installed++;
                if (pkg->isBroken()) broken++;
                if (pkg->getMarkedStatus() == RPackage::MUpgrade) upgradable++;
            }
        }
        status.installedCount = installed;
        status.availableCount = _lister->count();
        status.version = "APT " + std::string(_lister->getAPTCacheFile() ? "OK" : "N/A");
    }

    return status;
}

bool APTProvider::configure() {
    // Configuration is done through the Synaptic lister initialization
    // This is a no-op if lister is already set
    return (_lister != nullptr);
}

// ============================================================================
// Package Operations
// ============================================================================

std::vector<UnifiedPackage> APTProvider::search(
    const std::string& query,
    const SearchOptions& options)
{
    LOG(LogLevel::DEBUG)
        .provider("APT")
        .operation("search")
        .field("query", query)
        .message("Starting APT search")
        .emit();

    std::vector<UnifiedPackage> results;

    if (!_lister) {
        LOG(LogLevel::ERROR)
            .provider("APT")
            .operation("search")
            .message("APT lister not initialized")
            .emit();
        return results;
    }

    auto startTime = std::chrono::steady_clock::now();

    std::regex queryRegex;
    bool useRegex = false;

    try {
        queryRegex = std::regex(query, std::regex::icase);
        useRegex = true;
    } catch (...) {
        // Invalid regex, use substring matching
        useRegex = false;
    }

    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (unsigned int i = 0; i < _lister->count() && results.size() < options.maxResults; i++) {
        RPackage* pkg = _lister->getElement(i);
        if (!pkg) continue;

        // Filter by install status
        bool isInstalled = (pkg->installedVersion() != nullptr);
        if (options.installedOnly && !isInstalled) continue;
        if (options.availableOnly && isInstalled) continue;

        bool matches = false;

        // Search in name
        if (options.searchNames) {
            std::string name = pkg->name();
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);

            if (useRegex) {
                matches = std::regex_search(name, queryRegex);
            } else {
                matches = (name.find(lowerQuery) != std::string::npos);
            }
        }

        // Search in description
        if (!matches && options.searchDescriptions) {
            const char* desc = pkg->summary();
            if (desc) {
                std::string description = desc;
                std::transform(description.begin(), description.end(),
                              description.begin(), ::tolower);

                if (useRegex) {
                    matches = std::regex_search(description, queryRegex);
                } else {
                    matches = (description.find(lowerQuery) != std::string::npos);
                }
            }
        }

        if (matches) {
            results.push_back(convertPackage(pkg));
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    LOG(LogLevel::INFO)
        .provider("APT")
        .operation("search")
        .field("query", query)
        .field("results", std::to_string(results.size()))
        .duration(duration)
        .message("APT search completed")
        .emit();

    return results;
}

std::vector<UnifiedPackage> APTProvider::getInstalled() {
    std::vector<UnifiedPackage> results;

    if (!_lister) return results;

    for (unsigned int i = 0; i < _lister->count(); i++) {
        RPackage* pkg = _lister->getElement(i);
        if (pkg && pkg->installedVersion()) {
            results.push_back(convertPackage(pkg));
        }
    }

    return results;
}

std::vector<UnifiedPackage> APTProvider::getUpdatable() {
    std::vector<UnifiedPackage> results;

    if (!_lister) return results;

    for (unsigned int i = 0; i < _lister->count(); i++) {
        RPackage* pkg = _lister->getElement(i);
        if (pkg && pkg->installedVersion() && pkg->availableVersion()) {
            const char* installed = pkg->installedVersion();
            const char* available = pkg->availableVersion();
            if (installed && available && strcmp(installed, available) != 0) {
                // Check if available is newer (simplified check)
                results.push_back(convertPackage(pkg));
            }
        }
    }

    return results;
}

std::optional<UnifiedPackage> APTProvider::getPackageDetails(
    const std::string& packageId)
{
    if (!_lister) return std::nullopt;

    RPackage* pkg = _lister->getElement(packageId.c_str());
    if (!pkg) return std::nullopt;

    UnifiedPackage result = convertPackage(pkg);

    // Add detailed metadata
    result.metadata = getMetadata(pkg);

    return result;
}

// ============================================================================
// Install/Remove Operations
// ============================================================================

OperationResult APTProvider::install(
    const std::string& packageId,
    const InstallOptions& options)
{
    LOG(LogLevel::INFO)
        .provider("APT")
        .operation("install")
        .package(packageId)
        .message("Starting APT install")
        .emit();

    if (!_lister) {
        return OperationResult::Failure("APT lister not initialized", "", -1);
    }

    RPackage* pkg = _lister->getElement(packageId.c_str());
    if (!pkg) {
        return OperationResult::Failure("Package not found: " + packageId, "", -1);
    }

    // Mark for installation
    pkg->setInstall();

    // The actual installation is handled by the Synaptic UI
    // We just mark the package and return success
    return OperationResult::Success("Package marked for installation: " + packageId);
}

OperationResult APTProvider::remove(
    const std::string& packageId,
    const RemoveOptions& options)
{
    LOG(LogLevel::INFO)
        .provider("APT")
        .operation("remove")
        .package(packageId)
        .message("Starting APT remove")
        .emit();

    if (!_lister) {
        return OperationResult::Failure("APT lister not initialized", "", -1);
    }

    RPackage* pkg = _lister->getElement(packageId.c_str());
    if (!pkg) {
        return OperationResult::Failure("Package not found: " + packageId, "", -1);
    }

    // Mark for removal
    if (options.purge) {
        pkg->setPurge();
    } else {
        pkg->setRemove();
    }

    return OperationResult::Success("Package marked for removal: " + packageId);
}

OperationResult APTProvider::update(const std::string& packageId) {
    LOG(LogLevel::INFO)
        .provider("APT")
        .operation("update")
        .package(packageId)
        .message("Starting APT update")
        .emit();

    if (!_lister) {
        return OperationResult::Failure("APT lister not initialized", "", -1);
    }

    RPackage* pkg = _lister->getElement(packageId.c_str());
    if (!pkg) {
        return OperationResult::Failure("Package not found: " + packageId, "", -1);
    }

    // Mark for upgrade
    pkg->setInstall();  // This will upgrade if already installed

    return OperationResult::Success("Package marked for update: " + packageId);
}

// ============================================================================
// Batch Operations
// ============================================================================

OperationResult APTProvider::batchInstall(
    const std::vector<std::string>& packageIds,
    const InstallOptions& options)
{
    if (!_lister) {
        return OperationResult::Failure("APT lister not initialized", "", -1);
    }

    int marked = 0;
    for (const auto& id : packageIds) {
        RPackage* pkg = _lister->getElement(id.c_str());
        if (pkg) {
            pkg->setInstall();
            marked++;
        }
    }

    return OperationResult::Success(
        "Marked " + std::to_string(marked) + " packages for installation");
}

OperationResult APTProvider::batchRemove(
    const std::vector<std::string>& packageIds,
    const RemoveOptions& options)
{
    if (!_lister) {
        return OperationResult::Failure("APT lister not initialized", "", -1);
    }

    int marked = 0;
    for (const auto& id : packageIds) {
        RPackage* pkg = _lister->getElement(id.c_str());
        if (pkg) {
            if (options.purge) {
                pkg->setPurge();
            } else {
                pkg->setRemove();
            }
            marked++;
        }
    }

    return OperationResult::Success(
        "Marked " + std::to_string(marked) + " packages for removal");
}

// ============================================================================
// Repository Management
// ============================================================================

std::vector<Repository> APTProvider::getRepositories() {
    std::vector<Repository> repos;

    // Parse /etc/apt/sources.list and /etc/apt/sources.list.d/
    // This is a simplified implementation - full implementation would use apt-pkg

    FILE* fp = fopen("/etc/apt/sources.list", "r");
    if (fp) {
        char line[1024];
        int lineNum = 0;
        while (fgets(line, sizeof(line), fp)) {
            lineNum++;
            std::string l = line;

            // Skip comments and empty lines
            size_t pos = l.find_first_not_of(" \t");
            if (pos == std::string::npos || l[pos] == '#') continue;

            Repository repo;
            repo.id = "sources.list:" + std::to_string(lineNum);
            repo.name = l.substr(pos);
            repo.enabled = true;
            repos.push_back(repo);
        }
        fclose(fp);
    }

    return repos;
}

OperationResult APTProvider::addRepository(const Repository& repo) {
    // This would require root privileges and modifying sources.list
    // In practice, this is done through the Synaptic repository manager
    return OperationResult::Failure(
        "Repository management through Synaptic UI", "", -1);
}

OperationResult APTProvider::removeRepository(const std::string& repoId) {
    return OperationResult::Failure(
        "Repository management through Synaptic UI", "", -1);
}

OperationResult APTProvider::refreshRepositories() {
    if (!_lister) {
        return OperationResult::Failure("APT lister not initialized", "", -1);
    }

    // Trigger apt update through the lister
    // The actual update is handled by Synaptic's update mechanism
    return OperationResult::Success("Use Synaptic's Reload to refresh repositories");
}

// ============================================================================
// Trust & Security
// ============================================================================

TrustLevel APTProvider::getTrustLevel(const std::string& packageId) {
    if (!_lister) return TrustLevel::UNKNOWN;

    RPackage* pkg = _lister->getElement(packageId.c_str());
    if (!pkg) return TrustLevel::UNKNOWN;

    return determineTrustLevel(pkg);
}

TrustLevel APTProvider::determineTrustLevel(RPackage* pkg) {
    if (!pkg) return TrustLevel::UNKNOWN;

    // Check if package is from official Ubuntu/Debian repos
    const char* origin = pkg->origin();
    if (origin) {
        std::string orig = origin;
        if (orig.find("Ubuntu") != std::string::npos ||
            orig.find("Debian") != std::string::npos) {
            return TrustLevel::OFFICIAL;
        }
        if (orig.find("Canonical") != std::string::npos) {
            return TrustLevel::VERIFIED;
        }
    }

    // Check authentication status
    // Note: This is simplified - real implementation would check GPG signatures

    return TrustLevel::COMMUNITY;
}

// ============================================================================
// APT-Specific Methods
// ============================================================================

std::vector<UnifiedPackage> APTProvider::getPackagesBySection(
    const std::string& section)
{
    std::vector<UnifiedPackage> results;

    if (!_lister) return results;

    for (unsigned int i = 0; i < _lister->count(); i++) {
        RPackage* pkg = _lister->getElement(i);
        if (pkg) {
            const char* sec = pkg->section();
            if (sec && std::string(sec) == section) {
                results.push_back(convertPackage(pkg));
            }
        }
    }

    return results;
}

std::vector<UnifiedPackage> APTProvider::getPackagesByOrigin(
    const std::string& origin)
{
    std::vector<UnifiedPackage> results;

    if (!_lister) return results;

    for (unsigned int i = 0; i < _lister->count(); i++) {
        RPackage* pkg = _lister->getElement(i);
        if (pkg) {
            const char* orig = pkg->origin();
            if (orig && std::string(orig) == origin) {
                results.push_back(convertPackage(pkg));
            }
        }
    }

    return results;
}

std::vector<UnifiedPackage> APTProvider::getBrokenPackages() {
    std::vector<UnifiedPackage> results;

    if (!_lister) return results;

    for (unsigned int i = 0; i < _lister->count(); i++) {
        RPackage* pkg = _lister->getElement(i);
        if (pkg && pkg->isBroken()) {
            results.push_back(convertPackage(pkg));
        }
    }

    return results;
}

OperationResult APTProvider::fixBroken() {
    if (!_lister) {
        return OperationResult::Failure("APT lister not initialized", "", -1);
    }

    // Fix broken packages through lister
    _lister->fixBroken();

    return OperationResult::Success("Attempted to fix broken packages");
}

OperationResult APTProvider::autoRemove() {
    // This would be handled by apt autoremove
    return OperationResult::Failure(
        "Use apt autoremove from command line", "", -1);
}

OperationResult APTProvider::clean() {
    // This would be handled by apt clean
    return OperationResult::Failure(
        "Use apt clean from command line", "", -1);
}

// ============================================================================
// Private Helpers
// ============================================================================

UnifiedPackage APTProvider::convertPackage(RPackage* pkg) {
    UnifiedPackage unified;

    unified.id = pkg->name();
    unified.name = pkg->name();
    unified.providerId = "apt";

    // Version info
    const char* installed = pkg->installedVersion();
    const char* available = pkg->availableVersion();

    if (installed) {
        unified.installedVersion = installed;
        unified.status = available && strcmp(installed, available) != 0
            ? InstallStatus::UPDATE_AVAILABLE
            : InstallStatus::INSTALLED;
    } else {
        unified.status = InstallStatus::NOT_INSTALLED;
    }

    if (available) {
        unified.availableVersion = available;
    }

    // Description
    const char* summary = pkg->summary();
    if (summary) {
        unified.summary = summary;
    }

    const char* desc = pkg->description();
    if (desc) {
        unified.description = desc;
    }

    // Size
    unified.downloadSize = pkg->downloadSize();
    unified.installedSize = pkg->installedSize();

    // Section
    const char* section = pkg->section();
    if (section) {
        unified.categories.push_back(section);
    }

    // Trust level
    unified.trustLevel = determineTrustLevel(pkg);

    // APT packages are not sandboxed
    unified.confinement = ConfinementLevel::UNCONFINED;

    return unified;
}

PackageMetadata APTProvider::getMetadata(RPackage* pkg) {
    PackageMetadata meta;

    if (!pkg) return meta;

    // Homepage
    const char* homepage = pkg->homepage();
    if (homepage) {
        meta.homepage = homepage;
    }

    // Maintainer
    const char* maintainer = pkg->maintainer();
    if (maintainer) {
        meta.developer = maintainer;
    }

    // Dependencies
    // Note: Full dependency extraction would require more detailed parsing

    // Origin
    const char* origin = pkg->origin();
    if (origin) {
        meta.sourceRepo = origin;
    }

    // License (if available from package info)
    // APT doesn't provide this directly

    return meta;
}

void APTProvider::reportProgress(double fraction, const std::string& status) {
    if (_progressCallback) {
        _progressCallback(fraction, status);
    }
}

} // namespace PolySynaptic

// vim:ts=4:sw=4:et
