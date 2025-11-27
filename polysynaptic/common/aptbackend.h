/* aptbackend.h - APT package management backend for PolySynaptic
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This file wraps the existing Synaptic APT integration (RPackage,
 * RPackageLister, etc.) to implement the IPackageBackend interface.
 *
 * This is the primary backend that provides full APT/dpkg functionality,
 * including dependency resolution, package caching, and transactions.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef _APTBACKEND_H_
#define _APTBACKEND_H_

#include "ipackagebackend.h"
#include "rpackage.h"
#include "rpackagelister.h"
#include "rpackagecache.h"

#include <apt-pkg/configuration.h>
#include <apt-pkg/version.h>

#include <mutex>

namespace PolySynaptic {

/**
 * AptBackend - APT/dpkg package management backend
 *
 * This backend wraps the existing Synaptic classes to provide APT
 * functionality through the unified IPackageBackend interface.
 *
 * Architecture:
 *   AptBackend
 *     └── RPackageLister (existing Synaptic class)
 *           ├── RPackageCache (APT cache wrapper)
 *           └── vector<RPackage*> (package list)
 *
 * Thread Safety:
 *   Read operations are thread-safe with internal locking.
 *   Write operations (install/remove) should be called from main thread.
 */
class AptBackend : public IPackageBackend {
public:
    /**
     * Constructor
     *
     * @param lister Shared RPackageLister instance (owned by caller)
     *
     * The lister should already be initialized with an open cache.
     * AptBackend does not take ownership - the caller manages its lifecycle.
     */
    explicit AptBackend(RPackageLister* lister);

    ~AptBackend() override;

    // ========================================================================
    // Backend Information
    // ========================================================================

    BackendType getType() const override { return BackendType::APT; }
    string getName() const override { return "APT"; }
    bool isAvailable() const override;
    string getUnavailableReason() const override;
    string getVersion() const override;

    // ========================================================================
    // Package Discovery & Search
    // ========================================================================

    vector<PackageInfo> searchPackages(
        const SearchOptions& options,
        ProgressCallback progress = nullptr) override;

    vector<PackageInfo> getInstalledPackages(
        ProgressCallback progress = nullptr) override;

    PackageInfo getPackageDetails(const string& packageId) override;

    InstallStatus getInstallStatus(const string& packageId) override;

    vector<PackageInfo> getUpgradablePackages(
        ProgressCallback progress = nullptr) override;

    // ========================================================================
    // Package Operations
    // ========================================================================

    OperationResult installPackage(
        const string& packageId,
        ProgressCallback progress = nullptr) override;

    OperationResult removePackage(
        const string& packageId,
        bool purge = false,
        ProgressCallback progress = nullptr) override;

    OperationResult updatePackage(
        const string& packageId,
        ProgressCallback progress = nullptr) override;

    OperationResult refreshCache(
        ProgressCallback progress = nullptr) override;

    // ========================================================================
    // Repository Management
    // ========================================================================

    bool supportsRepositories() const override { return true; }
    vector<string> getRepositories() override;
    OperationResult addRepository(const string& repo) override;
    OperationResult removeRepository(const string& repo) override;

    // ========================================================================
    // APT-Specific Methods
    // ========================================================================

    /**
     * Get the underlying RPackageLister for advanced operations
     */
    RPackageLister* getLister() { return _lister; }

    /**
     * Find an RPackage by name
     */
    RPackage* findPackageByName(const string& name);

    /**
     * Convert RPackage to PackageInfo
     */
    PackageInfo rpackageToPackageInfo(RPackage* pkg);

    /**
     * Mark a package for installation (deferred transaction)
     */
    void markForInstall(const string& packageId);

    /**
     * Mark a package for removal (deferred transaction)
     */
    void markForRemoval(const string& packageId, bool purge = false);

    /**
     * Commit all pending marks (perform actual install/remove)
     */
    OperationResult commitChanges(ProgressCallback progress = nullptr);

    /**
     * Get packages marked for changes
     */
    vector<PackageInfo> getMarkedPackages();

    /**
     * Clear all marks
     */
    void clearMarks();

    /**
     * Run apt-get upgrade/dist-upgrade
     */
    OperationResult performUpgrade(bool distUpgrade, ProgressCallback progress);

    /**
     * Fix broken packages
     */
    OperationResult fixBroken();

private:
    RPackageLister* _lister;        // Borrowed reference (not owned)
    mutable mutex _mutex;           // Thread safety lock
    string _unavailableReason;      // Cached reason if unavailable

    // Helper to get package flags as InstallStatus
    InstallStatus flagsToInstallStatus(int flags);

    // Helper to validate package name (prevent injection)
    bool isValidPackageName(const string& name);
};

} // namespace PolySynaptic

#endif // _APTBACKEND_H_

// vim:ts=4:sw=4:et
