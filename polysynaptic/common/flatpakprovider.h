/* flatpakprovider.h - Flatpak package source provider for PolySynaptic
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This file implements the PackageSourceProvider interface for Flatpak
 * applications. It uses the flatpak CLI to interact with Flatpak.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef _FLATPAKPROVIDER_H_
#define _FLATPAKPROVIDER_H_

#include "packagesourceprovider.h"
#include "structuredlog.h"

#include <set>

namespace PolySynaptic {

/**
 * FlatpakProvider - Flatpak package source provider
 *
 * This provider uses the flatpak CLI to search, install, and manage
 * Flatpak applications through the unified PackageSourceProvider interface.
 */
class FlatpakProvider : public PackageSourceProvider {
public:
    /**
     * Installation scope for Flatpak
     */
    enum class Scope {
        USER,    // Install for current user only (~/.local/share/flatpak)
        SYSTEM   // Install system-wide (/var/lib/flatpak)
    };

    FlatpakProvider();
    ~FlatpakProvider() override = default;

    // ========================================================================
    // Provider Identity
    // ========================================================================

    std::string getId() const override { return "flatpak"; }
    std::string getName() const override { return "Flatpak"; }
    std::string getDescription() const override {
        return "Cross-distribution sandboxed application framework";
    }
    std::string getIconName() const override { return "flatpak-symbolic"; }

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
    // Repository Management (Remotes)
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
    PackagePermissions getPermissions(const std::string& packageId) override;

    // ========================================================================
    // Progress Callbacks
    // ========================================================================

    void setProgressCallback(ProgressCallback callback) override {
        _progressCallback = std::move(callback);
    }

    // ========================================================================
    // Flatpak-Specific Methods
    // ========================================================================

    /**
     * Get/set the default installation scope
     */
    Scope getDefaultScope() const { return _defaultScope; }
    void setDefaultScope(Scope scope) { _defaultScope = scope; }

    /**
     * Get/set the default remote
     */
    std::string getDefaultRemote() const { return _defaultRemote; }
    void setDefaultRemote(const std::string& remote) { _defaultRemote = remote; }

    /**
     * Get available remotes
     */
    std::vector<std::string> getRemotes(Scope scope = Scope::USER);

    /**
     * Add a remote (Flatpak repository)
     */
    OperationResult addRemote(const std::string& name,
                              const std::string& url,
                              Scope scope = Scope::USER);

    /**
     * Remove a remote
     */
    OperationResult removeRemote(const std::string& name,
                                 Scope scope = Scope::USER);

    /**
     * Get available runtimes
     */
    std::vector<UnifiedPackage> getRuntimes();

    /**
     * Override permission for an app
     */
    OperationResult overridePermission(const std::string& appId,
                                        const std::string& permission,
                                        bool grant);

    /**
     * Reset all permission overrides for an app
     */
    OperationResult resetPermissions(const std::string& appId);

    /**
     * Get app's metadata (permissions, runtime, etc.)
     */
    std::map<std::string, std::string> getAppMetadata(
        const std::string& appId);

    /**
     * Run a flatpak app
     */
    OperationResult run(const std::string& appId,
                        const std::vector<std::string>& args = {});

    /**
     * Repair the flatpak installation
     */
    OperationResult repair();

private:
    ProgressCallback _progressCallback;
    Scope _defaultScope = Scope::USER;
    std::string _defaultRemote = "flathub";
    std::set<std::string> _trustedRemotes;

    // CLI execution helper
    struct CommandResult {
        int exitCode;
        std::string stdout;
        std::string stderr;
    };

    CommandResult executeCommand(const std::string& command,
                                  int timeoutMs = 30000);

    // Get scope flag for commands
    std::string getScopeFlag(Scope scope) const;

    // Parse flatpak list output
    std::vector<UnifiedPackage> parseFlatpakList(const std::string& output,
                                                  bool installed);

    // Parse flatpak search output
    std::vector<UnifiedPackage> parseFlatpakSearch(const std::string& output);

    // Parse flatpak info output
    std::optional<UnifiedPackage> parseFlatpakInfo(const std::string& output);

    // Parse flatpak remote-ls output
    std::vector<UnifiedPackage> parseRemoteLs(const std::string& output);

    // Validate app ID
    bool isValidAppId(const std::string& appId);

    // Determine trust level based on remote
    TrustLevel determineTrustLevel(const std::string& remote,
                                   const std::string& appId);

    // Report progress
    void reportProgress(double fraction, const std::string& status);
};

// Register Flatpak provider with the registry
REGISTER_PROVIDER(FlatpakProvider, "flatpak");

} // namespace PolySynaptic

#endif // _FLATPAKPROVIDER_H_

// vim:ts=4:sw=4:et
