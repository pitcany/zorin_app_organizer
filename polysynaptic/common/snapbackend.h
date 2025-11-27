/* snapbackend.h - Snap package management backend for PolySynaptic
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This file implements the IPackageBackend interface for Snap packages.
 * It uses the snap CLI tool to discover, install, and manage snaps.
 *
 * Requirements:
 *   - snapd installed and running
 *   - snap command available in PATH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef _SNAPBACKEND_H_
#define _SNAPBACKEND_H_

#include "ipackagebackend.h"
#include <mutex>
#include <chrono>

namespace PolySynaptic {

/**
 * SnapBackend - Snap package management backend
 *
 * Uses the snap CLI to manage Snap packages. All operations are
 * performed by invoking snap commands and parsing their output.
 *
 * CLI Commands Used:
 *   snap find <query>       - Search for snaps
 *   snap list               - List installed snaps
 *   snap info <name>        - Get detailed snap info
 *   snap install <name>     - Install a snap
 *   snap remove <name>      - Remove a snap
 *   snap refresh <name>     - Update a snap
 *   snap refresh --list     - List available updates
 *
 * Thread Safety:
 *   All read operations are thread-safe.
 *   Write operations use a lock to prevent concurrent installs.
 *
 * Error Handling:
 *   CLI errors are captured and returned as OperationResult failures.
 *   Timeouts are enforced on all CLI operations.
 */
class SnapBackend : public IPackageBackend {
public:
    SnapBackend();
    ~SnapBackend() override;

    // ========================================================================
    // Backend Information
    // ========================================================================

    BackendType getType() const override { return BackendType::SNAP; }
    string getName() const override { return "Snap"; }
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
    // Snap-Specific Methods
    // ========================================================================

    /**
     * Install a snap with specific options
     *
     * @param snapName Name of the snap
     * @param classic Install with classic confinement
     * @param channel Channel to install from (stable/beta/edge)
     * @param progress Progress callback
     */
    OperationResult installSnap(
        const string& snapName,
        bool classic,
        const string& channel = "stable",
        ProgressCallback progress = nullptr);

    /**
     * Get available channels for a snap
     */
    vector<string> getChannels(const string& snapName);

    /**
     * Switch a snap to a different channel
     */
    OperationResult switchChannel(
        const string& snapName,
        const string& channel);

    /**
     * Check if snapd is running
     */
    bool isSnapdRunning() const;

    /**
     * Get the snap store base URL
     */
    string getStoreUrl() const;

    /**
     * Set command timeout (default: 120 seconds)
     */
    void setTimeout(int seconds) { _timeoutSeconds = seconds; }

private:
    mutable mutex _mutex;           // Thread safety lock
    mutable bool _availabilityChecked;
    mutable bool _isAvailable;
    mutable string _unavailableReason;
    mutable string _version;
    int _timeoutSeconds;

    // CLI execution helpers
    struct CommandResult {
        bool success;
        int exitCode;
        string stdout;
        string stderr;
    };

    CommandResult executeCommand(
        const vector<string>& args,
        int timeoutSeconds = 0) const;

    // Parsing helpers
    PackageInfo parseSnapInfo(const string& output);
    vector<PackageInfo> parseSnapFind(const string& output);
    vector<PackageInfo> parseSnapList(const string& output);
    vector<PackageInfo> parseSnapRefreshList(const string& output);

    // Validation
    bool isValidSnapName(const string& name) const;

    // Check availability (cached)
    void checkAvailability() const;
};

} // namespace PolySynaptic

#endif // _SNAPBACKEND_H_

// vim:ts=4:sw=4:et
