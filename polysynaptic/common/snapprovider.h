/* snapprovider.h - Snap package source provider for PolySynaptic
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This file implements the PackageSourceProvider interface for Snap packages.
 * It uses the snap CLI tool to interact with the snapd daemon.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef _SNAPPROVIDER_H_
#define _SNAPPROVIDER_H_

#include "packagesourceprovider.h"
#include "structuredlog.h"

#include <set>
#include <regex>

namespace PolySynaptic {

/**
 * SnapProvider - Snap package source provider
 *
 * This provider uses the snap CLI to search, install, and manage
 * Snap packages through the unified PackageSourceProvider interface.
 */
class SnapProvider : public PackageSourceProvider {
public:
    SnapProvider();
    ~SnapProvider() override = default;

    // ========================================================================
    // Provider Identity
    // ========================================================================

    std::string getId() const override { return "snap"; }
    std::string getName() const override { return "Snap"; }
    std::string getDescription() const override {
        return "Canonical's universal package manager";
    }
    std::string getIconName() const override { return "snap-symbolic"; }

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

    bool supportsBatchOperations() const override { return false; }

    // ========================================================================
    // Repository Management (Snap Channels)
    // ========================================================================

    bool supportsRepositories() const override { return false; }

    // ========================================================================
    // Trust & Security
    // ========================================================================

    TrustLevel getTrustLevel(const std::string& packageId) override;
    PackagePermissions getPermissions(const std::string& packageId) override;

    // ========================================================================
    // Progress Callbacks
    // ========================================================================

    void setProgressCallback(ProgressCallback callback) override {
        _progressCallback = std::move(callback);
    }

    // ========================================================================
    // Snap-Specific Methods
    // ========================================================================

    /**
     * Get available channels for a snap
     */
    std::vector<std::string> getChannels(const std::string& snapName);

    /**
     * Switch to a different channel
     */
    OperationResult switchChannel(const std::string& snapName,
                                   const std::string& channel);

    /**
     * Connect a plug to a slot
     */
    OperationResult connectPlug(const std::string& snap,
                                const std::string& plug,
                                const std::string& targetSnap,
                                const std::string& slot);

    /**
     * Disconnect a plug
     */
    OperationResult disconnectPlug(const std::string& snap,
                                   const std::string& plug);

    /**
     * Get connection status for a snap
     */
    std::vector<std::pair<std::string, bool>> getConnections(
        const std::string& snapName);

    /**
     * Enable a disabled snap
     */
    OperationResult enable(const std::string& snapName);

    /**
     * Disable a snap (without removing)
     */
    OperationResult disable(const std::string& snapName);

    /**
     * Revert to previous revision
     */
    OperationResult revert(const std::string& snapName);

    /**
     * Hold/unhold updates for a snap
     */
    OperationResult holdUpdates(const std::string& snapName, bool hold);

private:
    ProgressCallback _progressCallback;
    std::set<std::string> _verifiedPublishers;

    // CLI execution helper
    struct CommandResult {
        int exitCode;
        std::string stdout;
        std::string stderr;
    };

    // Safe argument-based execution (no shell - prevents command injection)
    CommandResult executeCommandArgs(const std::vector<std::string>& args,
                                      int timeoutMs = 30000);
    // Legacy shell-based execution (DEPRECATED - wraps executeCommandArgs)
    CommandResult executeCommand(const std::string& command,
                                  int timeoutMs = 30000);

    // Parse snap list output
    std::vector<UnifiedPackage> parseSnapList(const std::string& output);

    // Parse snap find output
    std::vector<UnifiedPackage> parseSnapFind(const std::string& output);

    // Parse snap info output
    std::optional<UnifiedPackage> parseSnapInfo(const std::string& output);

    // Validate snap name
    bool isValidSnapName(const std::string& name);

    // Determine trust level
    TrustLevel determineTrustLevel(const std::string& publisher,
                                   bool isVerified);

    // Determine confinement level
    ConfinementLevel parseConfinement(const std::string& confStr);

    // Report progress
    void reportProgress(double fraction, const std::string& status);
};

// Register Snap provider with the registry
REGISTER_PROVIDER(SnapProvider, "snap");

} // namespace PolySynaptic

#endif // _SNAPPROVIDER_H_

// vim:ts=4:sw=4:et
