/* aptprovider.h - APT package source provider for PolySynaptic
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This file implements the PackageSourceProvider interface for APT/dpkg
 * packages, wrapping the existing Synaptic RPackageLister functionality.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef _APTPROVIDER_H_
#define _APTPROVIDER_H_

#include "packagesourceprovider.h"
#include "rpackagelister.h"
#include "structuredlog.h"

#include <memory>
#include <set>

namespace PolySynaptic {

/**
 * APTProvider - APT/dpkg package source provider
 *
 * This provider wraps the existing Synaptic RPackageLister to provide
 * APT package management through the unified PackageSourceProvider interface.
 */
class APTProvider : public PackageSourceProvider {
public:
    APTProvider();
    explicit APTProvider(RPackageLister* lister);
    ~APTProvider() override = default;

    // ========================================================================
    // Provider Identity
    // ========================================================================

    std::string getId() const override { return "apt"; }
    std::string getName() const override { return "APT"; }
    std::string getDescription() const override {
        return "Debian/Ubuntu package manager (APT/dpkg)";
    }
    std::string getIconName() const override { return "package-x-generic"; }

    // ========================================================================
    // Availability & Configuration
    // ========================================================================

    bool isAvailable() const override;
    ProviderStatus getStatus() const override;
    bool configure() override;

    // ========================================================================
    // Package Operations
    // ========================================================================

    std::vector<UnifiedPackage> search(
        const std::string& query,
        const SearchOptions& options = SearchOptions()) override;

    std::vector<UnifiedPackage> getInstalled() override;
    std::vector<UnifiedPackage> getUpdatable() override;

    std::optional<UnifiedPackage> getPackageDetails(
        const std::string& packageId) override;

    // ========================================================================
    // Install/Remove Operations
    // ========================================================================

    OperationResult install(
        const std::string& packageId,
        const InstallOptions& options = InstallOptions()) override;

    OperationResult remove(
        const std::string& packageId,
        const RemoveOptions& options = RemoveOptions()) override;

    OperationResult update(
        const std::string& packageId) override;

    // ========================================================================
    // Batch Operations
    // ========================================================================

    bool supportsBatchOperations() const override { return true; }

    OperationResult batchInstall(
        const std::vector<std::string>& packageIds,
        const InstallOptions& options = InstallOptions()) override;

    OperationResult batchRemove(
        const std::vector<std::string>& packageIds,
        const RemoveOptions& options = RemoveOptions()) override;

    // ========================================================================
    // Repository Management
    // ========================================================================

    bool supportsRepositories() const override { return true; }
    std::vector<Repository> getRepositories() override;

    OperationResult addRepository(const Repository& repo) override;
    OperationResult removeRepository(const std::string& repoId) override;
    OperationResult refreshRepositories() override;

    // ========================================================================
    // Trust & Security
    // ========================================================================

    TrustLevel getTrustLevel(const std::string& packageId) override;

    // ========================================================================
    // Progress Callbacks
    // ========================================================================

    void setProgressCallback(ProgressCallback callback) override {
        _progressCallback = std::move(callback);
    }

    // ========================================================================
    // APT-Specific Methods
    // ========================================================================

    /**
     * Set the RPackageLister to use for APT operations.
     * This allows integration with the existing Synaptic UI.
     */
    void setLister(RPackageLister* lister) { _lister = lister; }
    RPackageLister* getLister() const { return _lister; }

    /**
     * Get packages by section
     */
    std::vector<UnifiedPackage> getPackagesBySection(const std::string& section);

    /**
     * Get packages by origin
     */
    std::vector<UnifiedPackage> getPackagesByOrigin(const std::string& origin);

    /**
     * Get broken packages
     */
    std::vector<UnifiedPackage> getBrokenPackages();

    /**
     * Fix broken packages
     */
    OperationResult fixBroken();

    /**
     * Run apt autoremove
     */
    OperationResult autoRemove();

    /**
     * Run apt clean
     */
    OperationResult clean();

private:
    RPackageLister* _lister = nullptr;
    ProgressCallback _progressCallback;

    // Convert RPackage to UnifiedPackage
    UnifiedPackage convertPackage(RPackage* pkg);

    // Get metadata for a package
    PackageMetadata getMetadata(RPackage* pkg);

    // Helper to report progress
    void reportProgress(double fraction, const std::string& status);

    // Determine trust level from package signatures
    TrustLevel determineTrustLevel(RPackage* pkg);
};

// Register APT provider with the registry
REGISTER_PROVIDER(APTProvider, "apt");

} // namespace PolySynaptic

#endif // _APTPROVIDER_H_

// vim:ts=4:sw=4:et
