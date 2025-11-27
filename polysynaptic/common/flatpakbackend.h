/* flatpakbackend.h - Flatpak package management backend for PolySynaptic
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This file implements the IPackageBackend interface for Flatpak packages.
 * It uses the flatpak CLI tool to discover, install, and manage flatpaks.
 *
 * Requirements:
 *   - flatpak installed
 *   - At least one remote configured (e.g., flathub)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef _FLATPAKBACKEND_H_
#define _FLATPAKBACKEND_H_

#include "ipackagebackend.h"
#include <mutex>
#include <set>

namespace PolySynaptic {

/**
 * FlatpakBackend - Flatpak package management backend
 *
 * Uses the flatpak CLI to manage Flatpak applications and runtimes.
 *
 * CLI Commands Used:
 *   flatpak search <query>              - Search for apps
 *   flatpak list                        - List installed apps
 *   flatpak info <ref>                  - Get app details
 *   flatpak install <remote> <ref>      - Install an app
 *   flatpak uninstall <ref>             - Remove an app
 *   flatpak update <ref>                - Update an app
 *   flatpak remote-list                 - List configured remotes
 *   flatpak remote-add <name> <url>     - Add a remote
 *
 * Thread Safety:
 *   All read operations are thread-safe.
 *   Write operations use a lock to prevent concurrent installs.
 *
 * Installation Scope:
 *   By default, operations are performed at user level (--user).
 *   System-wide installations require root privileges.
 */
class FlatpakBackend : public IPackageBackend {
public:
    // Installation scope for flatpak operations
    enum class Scope {
        USER,       // Install for current user only
        SYSTEM      // Install system-wide (requires root)
    };

    FlatpakBackend();
    ~FlatpakBackend() override;

    // ========================================================================
    // Backend Information
    // ========================================================================

    BackendType getType() const override { return BackendType::FLATPAK; }
    string getName() const override { return "Flatpak"; }
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
    // Repository/Remote Management
    // ========================================================================

    bool supportsRepositories() const override { return true; }
    vector<string> getRepositories() override;
    OperationResult addRepository(const string& repo) override;
    OperationResult removeRepository(const string& repo) override;

    // ========================================================================
    // Flatpak-Specific Methods
    // ========================================================================

    /**
     * Install a flatpak with specific options
     *
     * @param appId Application ID (e.g., org.gnome.Calculator)
     * @param remote Remote name (e.g., flathub)
     * @param branch Branch to install (e.g., stable)
     * @param scope Installation scope (user or system)
     * @param progress Progress callback
     */
    OperationResult installFlatpak(
        const string& appId,
        const string& remote = "",
        const string& branch = "stable",
        Scope scope = Scope::USER,
        ProgressCallback progress = nullptr);

    /**
     * Get list of configured remotes with their URLs
     */
    vector<pair<string, string>> getRemotesWithUrls();

    /**
     * Add flathub remote if not already configured
     */
    OperationResult addFlathub();

    /**
     * Get available branches for an app
     */
    vector<string> getBranches(const string& appId, const string& remote = "");

    /**
     * Set default installation scope
     */
    void setDefaultScope(Scope scope) { _defaultScope = scope; }
    Scope getDefaultScope() const { return _defaultScope; }

    /**
     * Set default remote for installations
     */
    void setDefaultRemote(const string& remote) { _defaultRemote = remote; }
    string getDefaultRemote() const { return _defaultRemote; }

    /**
     * Set command timeout (default: 120 seconds)
     */
    void setTimeout(int seconds) { _timeoutSeconds = seconds; }

    /**
     * Check if a specific remote is configured
     */
    bool hasRemote(const string& remoteName);

private:
    mutable mutex _mutex;
    mutable bool _availabilityChecked;
    mutable bool _isAvailable;
    mutable string _unavailableReason;
    mutable string _version;
    mutable vector<string> _remotes;

    Scope _defaultScope;
    string _defaultRemote;
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
    vector<PackageInfo> parseFlatpakSearch(const string& output);
    vector<PackageInfo> parseFlatpakList(const string& output);
    PackageInfo parseFlatpakInfo(const string& output, const string& appId);
    vector<PackageInfo> parseFlatpakUpdate(const string& output);
    vector<pair<string, string>> parseFlatpakRemotes(const string& output);

    // Validation
    bool isValidAppId(const string& appId) const;
    bool isValidRemoteName(const string& name) const;

    // Check availability (cached)
    void checkAvailability() const;

    // Refresh remotes cache
    void refreshRemotesCache() const;
};

} // namespace PolySynaptic

#endif // _FLATPAKBACKEND_H_

// vim:ts=4:sw=4:et
