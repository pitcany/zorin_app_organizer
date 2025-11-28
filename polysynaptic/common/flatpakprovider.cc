/* flatpakprovider.cc - Flatpak package source provider implementation
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include "flatpakprovider.h"

#include <algorithm>
#include <sstream>
#include <regex>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>

namespace PolySynaptic {

// ============================================================================
// Constructor
// ============================================================================

FlatpakProvider::FlatpakProvider() {
    // Known trusted remotes
    _trustedRemotes = {
        "flathub",
        "flathub-beta",
        "fedora",
        "gnome-nightly",
        "kde-nightly"
    };
}

// ============================================================================
// Availability & Configuration
// ============================================================================

bool FlatpakProvider::isAvailable() const {
    return access("/usr/bin/flatpak", X_OK) == 0;
}

ProviderStatus FlatpakProvider::getStatus() const {
    ProviderStatus status;
    status.available = isAvailable();
    status.enabled = status.available;
    status.configured = status.available;

    if (!status.available) {
        status.errorMessage = "Flatpak is not installed";
        return status;
    }

    // Get version
    auto result = const_cast<FlatpakProvider*>(this)->executeCommand(
        "flatpak --version 2>/dev/null");

    if (result.exitCode == 0) {
        status.version = result.stdout;
        while (!status.version.empty() &&
               (status.version.back() == '\n' || status.version.back() == '\r')) {
            status.version.pop_back();
        }
    }

    // Count installed apps
    result = const_cast<FlatpakProvider*>(this)->executeCommand(
        "flatpak list --app --columns=application 2>/dev/null | wc -l");
    if (result.exitCode == 0) {
        try {
            status.installedCount = std::stoi(result.stdout);
        } catch (...) {}
    }

    return status;
}

bool FlatpakProvider::configure() {
    // Ensure Flathub is configured
    auto remotes = getRemotes(Scope::USER);
    bool hasFlathub = std::find(remotes.begin(), remotes.end(), "flathub")
                      != remotes.end();

    if (!hasFlathub) {
        // Check system scope
        remotes = getRemotes(Scope::SYSTEM);
        hasFlathub = std::find(remotes.begin(), remotes.end(), "flathub")
                     != remotes.end();
    }

    return hasFlathub;
}

// ============================================================================
// Package Operations
// ============================================================================

std::vector<UnifiedPackage> FlatpakProvider::search(
    const std::string& query,
    const SearchOptions& options)
{
    LOG(LogLevel::DEBUG)
        .provider("Flatpak")
        .operation("search")
        .field("query", query)
        .message("Starting Flatpak search")
        .emit();

    auto startTime = std::chrono::steady_clock::now();
    const std::size_t maxResultsLimit =
        static_cast<std::size_t>(options.maxResults);

    // If installed only, filter installed list
    if (options.installedOnly) {
        auto installed = getInstalled();
        std::vector<UnifiedPackage> filtered;

        std::string lowerQuery = query;
        std::transform(lowerQuery.begin(), lowerQuery.end(),
                      lowerQuery.begin(), ::tolower);

        for (const auto& pkg : installed) {
            std::string name = pkg.name;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);

            bool matches = (name.find(lowerQuery) != std::string::npos);

            if (!matches && options.searchDescriptions) {
                std::string desc = pkg.summary;
                std::transform(desc.begin(), desc.end(), desc.begin(), ::tolower);
                matches = (desc.find(lowerQuery) != std::string::npos);
            }

            if (matches) {
                filtered.push_back(pkg);
            }

            if (options.maxResults != 0 &&
                filtered.size() >= maxResultsLimit) break;
        }

        return filtered;
    }

    // Search Flathub
    std::string cmd = "flatpak search '" + query + "' --columns=application,name,description,version,remotes 2>/dev/null";
    auto result = executeCommand(cmd, 60000);

    if (result.exitCode != 0) {
        LOG(LogLevel::WARN)
            .provider("Flatpak")
            .operation("search")
            .field("query", query)
            .exitCode(result.exitCode)
            .stderr(result.stderr)
            .message("Flatpak search failed")
            .emit();
        return {};
    }

    auto packages = parseFlatpakSearch(result.stdout);

    // Limit results
    if (options.maxResults != 0 &&
        packages.size() > maxResultsLimit) {
        packages.resize(maxResultsLimit);
    }

    // Mark installed packages
    auto installed = getInstalled();
    std::set<std::string> installedIds;
    for (const auto& pkg : installed) {
        installedIds.insert(pkg.id);
    }

    for (auto& pkg : packages) {
        if (installedIds.count(pkg.id) > 0) {
            // Find installed version
            for (const auto& inst : installed) {
                if (inst.id == pkg.id) {
                    pkg.installedVersion = inst.installedVersion;
                    pkg.status = InstallStatus::INSTALLED;
                    break;
                }
            }
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    LOG(LogLevel::INFO)
        .provider("Flatpak")
        .operation("search")
        .field("query", query)
        .field("results", std::to_string(packages.size()))
        .duration(duration)
        .message("Flatpak search completed")
        .emit();

    return packages;
}

std::vector<UnifiedPackage> FlatpakProvider::getInstalled() {
    std::string cmd = "flatpak list --app --columns=application,name,version,origin,installation 2>/dev/null";
    auto result = executeCommand(cmd, 30000);

    if (result.exitCode != 0) {
        LOG(LogLevel::WARN)
            .provider("Flatpak")
            .operation("list")
            .exitCode(result.exitCode)
            .message("Failed to list installed Flatpaks")
            .emit();
        return {};
    }

    return parseFlatpakList(result.stdout, true);
}

std::vector<UnifiedPackage> FlatpakProvider::getUpdatable() {
    std::string cmd = "flatpak remote-ls --app --updates --columns=application,name,version,origin 2>/dev/null";
    auto result = executeCommand(cmd, 60000);

    if (result.exitCode != 0) {
        return {};
    }

    return parseRemoteLs(result.stdout);
}

std::optional<UnifiedPackage> FlatpakProvider::getPackageDetails(
    const std::string& packageId)
{
    // Try installed first
    std::string cmd = "flatpak info " + packageId + " 2>/dev/null";
    auto result = executeCommand(cmd, 10000);

    if (result.exitCode == 0) {
        return parseFlatpakInfo(result.stdout);
    }

    // Try remote
    cmd = "flatpak remote-info flathub " + packageId + " 2>/dev/null";
    result = executeCommand(cmd, 30000);

    if (result.exitCode == 0) {
        return parseFlatpakInfo(result.stdout);
    }

    return std::nullopt;
}

// ============================================================================
// Install/Remove Operations
// ============================================================================

OperationResult FlatpakProvider::install(
    const std::string& packageId,
    const InstallOptions& options)
{
    LOG(LogLevel::INFO)
        .provider("Flatpak")
        .operation("install")
        .package(packageId)
        .message("Starting Flatpak install")
        .emit();

    if (!isValidAppId(packageId)) {
        return OperationResult::Failure(
            "Invalid Flatpak app ID: " + packageId, "", -1);
    }

    std::string cmd = "flatpak install -y " + getScopeFlag(_defaultScope);
    cmd += " " + _defaultRemote + " " + packageId;

    auto result = executeCommand(cmd, 600000);  // 10 minute timeout

    if (result.exitCode != 0) {
        LOG(LogLevel::ERROR)
            .provider("Flatpak")
            .operation("install")
            .package(packageId)
            .exitCode(result.exitCode)
            .stderr(result.stderr)
            .message("Flatpak install failed")
            .emit();

        return OperationResult::Failure(
            "Failed to install " + packageId,
            result.stderr,
            result.exitCode);
    }

    LOG(LogLevel::INFO)
        .provider("Flatpak")
        .operation("install")
        .package(packageId)
        .message("Flatpak install completed")
        .emit();

    return OperationResult::Success("Successfully installed " + packageId);
}

OperationResult FlatpakProvider::remove(
    const std::string& packageId,
    const RemoveOptions& options)
{
    LOG(LogLevel::INFO)
        .provider("Flatpak")
        .operation("uninstall")
        .package(packageId)
        .message("Starting Flatpak uninstall")
        .emit();

    if (!isValidAppId(packageId)) {
        return OperationResult::Failure(
            "Invalid Flatpak app ID: " + packageId, "", -1);
    }

    std::string cmd = "flatpak uninstall -y";

    if (options.purge) {
        cmd += " --delete-data";
    }

    cmd += " " + packageId;

    auto result = executeCommand(cmd, 120000);

    if (result.exitCode != 0) {
        LOG(LogLevel::ERROR)
            .provider("Flatpak")
            .operation("uninstall")
            .package(packageId)
            .exitCode(result.exitCode)
            .stderr(result.stderr)
            .message("Flatpak uninstall failed")
            .emit();

        return OperationResult::Failure(
            "Failed to uninstall " + packageId,
            result.stderr,
            result.exitCode);
    }

    LOG(LogLevel::INFO)
        .provider("Flatpak")
        .operation("uninstall")
        .package(packageId)
        .message("Flatpak uninstall completed")
        .emit();

    return OperationResult::Success("Successfully uninstalled " + packageId);
}

OperationResult FlatpakProvider::update(const std::string& packageId) {
    LOG(LogLevel::INFO)
        .provider("Flatpak")
        .operation("update")
        .package(packageId)
        .message("Starting Flatpak update")
        .emit();

    std::string cmd = "flatpak update -y " + packageId;
    auto result = executeCommand(cmd, 600000);

    if (result.exitCode != 0) {
        LOG(LogLevel::ERROR)
            .provider("Flatpak")
            .operation("update")
            .package(packageId)
            .exitCode(result.exitCode)
            .stderr(result.stderr)
            .message("Flatpak update failed")
            .emit();

        return OperationResult::Failure(
            "Failed to update " + packageId,
            result.stderr,
            result.exitCode);
    }

    return OperationResult::Success("Successfully updated " + packageId);
}

// ============================================================================
// Batch Operations
// ============================================================================

OperationResult FlatpakProvider::batchInstall(
    const std::vector<std::string>& packageIds,
    const InstallOptions& options)
{
    std::string idList;
    for (const auto& id : packageIds) {
        if (!isValidAppId(id)) {
            return OperationResult::Failure(
                "Invalid app ID: " + id, "", -1);
        }
        idList += " " + id;
    }

    std::string cmd = "flatpak install -y " + getScopeFlag(_defaultScope);
    cmd += " " + _defaultRemote + idList;

    auto result = executeCommand(cmd, 1200000);  // 20 minute timeout

    if (result.exitCode != 0) {
        return OperationResult::Failure(
            "Batch install failed",
            result.stderr,
            result.exitCode);
    }

    return OperationResult::Success(
        "Successfully installed " + std::to_string(packageIds.size()) + " packages");
}

OperationResult FlatpakProvider::batchRemove(
    const std::vector<std::string>& packageIds,
    const RemoveOptions& options)
{
    std::string idList;
    for (const auto& id : packageIds) {
        idList += " " + id;
    }

    std::string cmd = "flatpak uninstall -y";
    if (options.purge) {
        cmd += " --delete-data";
    }
    cmd += idList;

    auto result = executeCommand(cmd, 300000);

    if (result.exitCode != 0) {
        return OperationResult::Failure(
            "Batch uninstall failed",
            result.stderr,
            result.exitCode);
    }

    return OperationResult::Success(
        "Successfully uninstalled " + std::to_string(packageIds.size()) + " packages");
}

// ============================================================================
// Repository Management
// ============================================================================

std::vector<Repository> FlatpakProvider::getRepositories() {
    std::vector<Repository> repos;

    // Get user remotes
    for (const auto& remote : getRemotes(Scope::USER)) {
        Repository repo;
        repo.id = remote + ":user";
        repo.name = remote + " (user)";
        repo.enabled = true;
        repos.push_back(repo);
    }

    // Get system remotes
    for (const auto& remote : getRemotes(Scope::SYSTEM)) {
        Repository repo;
        repo.id = remote + ":system";
        repo.name = remote + " (system)";
        repo.enabled = true;
        repos.push_back(repo);
    }

    return repos;
}

OperationResult FlatpakProvider::addRepository(const Repository& repo) {
    // Parse scope from repo.id
    Scope scope = Scope::USER;
    std::string name = repo.name;

    if (repo.id.find(":system") != std::string::npos) {
        scope = Scope::SYSTEM;
    }

    return addRemote(name, repo.url, scope);
}

OperationResult FlatpakProvider::removeRepository(const std::string& repoId) {
    Scope scope = Scope::USER;
    std::string name = repoId;

    size_t colonPos = repoId.find(':');
    if (colonPos != std::string::npos) {
        name = repoId.substr(0, colonPos);
        if (repoId.substr(colonPos + 1) == "system") {
            scope = Scope::SYSTEM;
        }
    }

    return removeRemote(name, scope);
}

OperationResult FlatpakProvider::refreshRepositories() {
    std::string cmd = "flatpak update --appstream 2>/dev/null";
    auto result = executeCommand(cmd, 120000);

    if (result.exitCode != 0) {
        return OperationResult::Failure(
            "Failed to refresh app stream data",
            result.stderr,
            result.exitCode);
    }

    return OperationResult::Success("Refreshed repository metadata");
}

// ============================================================================
// Trust & Security
// ============================================================================

TrustLevel FlatpakProvider::getTrustLevel(const std::string& packageId) {
    auto details = getPackageDetails(packageId);
    if (details) {
        return details->trustLevel;
    }
    return TrustLevel::UNKNOWN;
}

PackagePermissions FlatpakProvider::getPermissions(const std::string& packageId) {
    PackagePermissions perms;

    std::string cmd = "flatpak info --show-permissions " + packageId + " 2>/dev/null";
    auto result = executeCommand(cmd, 10000);

    if (result.exitCode != 0) {
        return perms;
    }

    // Parse permissions output
    std::istringstream stream(result.stdout);
    std::string line;
    std::string currentSection;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        // Section header
        if (line[0] == '[') {
            size_t end = line.find(']');
            if (end != std::string::npos) {
                currentSection = line.substr(1, end - 1);
            }
            continue;
        }

        // Parse permission
        PackagePermissions::Permission p;
        p.name = line;
        p.category = currentSection;
        p.granted = true;  // If listed, it's granted

        // Categorize
        if (currentSection == "Context" && line.find("shared=network") != std::string::npos) {
            p.description = "Network access";
        } else if (line.find("filesystems=") != std::string::npos) {
            p.description = "File system access";
        } else if (line.find("devices=") != std::string::npos) {
            p.description = "Device access";
        } else {
            p.description = line;
        }

        perms.permissions.push_back(p);
    }

    return perms;
}

// ============================================================================
// Flatpak-Specific Methods
// ============================================================================

std::vector<std::string> FlatpakProvider::getRemotes(Scope scope) {
    std::vector<std::string> remotes;

    std::string cmd = "flatpak remotes " + getScopeFlag(scope)
                    + " --columns=name 2>/dev/null";
    auto result = executeCommand(cmd, 10000);

    if (result.exitCode != 0) {
        return remotes;
    }

    std::istringstream stream(result.stdout);
    std::string line;

    while (std::getline(stream, line)) {
        if (!line.empty()) {
            remotes.push_back(line);
        }
    }

    return remotes;
}

OperationResult FlatpakProvider::addRemote(
    const std::string& name,
    const std::string& url,
    Scope scope)
{
    std::string cmd = "flatpak remote-add --if-not-exists "
                    + getScopeFlag(scope) + " " + name + " " + url;

    auto result = executeCommand(cmd, 60000);

    if (result.exitCode != 0) {
        return OperationResult::Failure(
            "Failed to add remote " + name,
            result.stderr,
            result.exitCode);
    }

    return OperationResult::Success("Added remote " + name);
}

OperationResult FlatpakProvider::removeRemote(
    const std::string& name,
    Scope scope)
{
    std::string cmd = "flatpak remote-delete " + getScopeFlag(scope)
                    + " " + name;

    auto result = executeCommand(cmd, 30000);

    if (result.exitCode != 0) {
        return OperationResult::Failure(
            "Failed to remove remote " + name,
            result.stderr,
            result.exitCode);
    }

    return OperationResult::Success("Removed remote " + name);
}

std::vector<UnifiedPackage> FlatpakProvider::getRuntimes() {
    std::string cmd = "flatpak list --runtime --columns=application,name,version,origin 2>/dev/null";
    auto result = executeCommand(cmd, 30000);

    if (result.exitCode != 0) {
        return {};
    }

    return parseFlatpakList(result.stdout, true);
}

OperationResult FlatpakProvider::overridePermission(
    const std::string& appId,
    const std::string& permission,
    bool grant)
{
    std::string cmd = "flatpak override ";
    cmd += grant ? "--" : "--no";
    cmd += permission + " " + appId;

    auto result = executeCommand(cmd, 10000);

    if (result.exitCode != 0) {
        return OperationResult::Failure(
            "Failed to override permission",
            result.stderr,
            result.exitCode);
    }

    return OperationResult::Success(
        std::string(grant ? "Granted" : "Revoked") + " " + permission);
}

OperationResult FlatpakProvider::resetPermissions(const std::string& appId) {
    std::string cmd = "flatpak override --reset " + appId;
    auto result = executeCommand(cmd, 10000);

    if (result.exitCode != 0) {
        return OperationResult::Failure(
            "Failed to reset permissions",
            result.stderr,
            result.exitCode);
    }

    return OperationResult::Success("Reset permissions for " + appId);
}

std::map<std::string, std::string> FlatpakProvider::getAppMetadata(
    const std::string& appId)
{
    std::map<std::string, std::string> metadata;

    std::string cmd = "flatpak info --show-metadata " + appId + " 2>/dev/null";
    auto result = executeCommand(cmd, 10000);

    if (result.exitCode != 0) {
        return metadata;
    }

    std::istringstream stream(result.stdout);
    std::string line;
    std::string currentSection;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        if (line[0] == '[') {
            size_t end = line.find(']');
            if (end != std::string::npos) {
                currentSection = line.substr(1, end - 1);
            }
            continue;
        }

        size_t eqPos = line.find('=');
        if (eqPos != std::string::npos) {
            std::string key = currentSection + "." + line.substr(0, eqPos);
            std::string value = line.substr(eqPos + 1);
            metadata[key] = value;
        }
    }

    return metadata;
}

OperationResult FlatpakProvider::run(
    const std::string& appId,
    const std::vector<std::string>& args)
{
    std::string cmd = "flatpak run " + appId;
    for (const auto& arg : args) {
        cmd += " '" + arg + "'";
    }
    cmd += " &";  // Run in background

    auto result = executeCommand(cmd, 5000);

    return OperationResult::Success("Launched " + appId);
}

OperationResult FlatpakProvider::repair() {
    std::string cmd = "flatpak repair";
    auto result = executeCommand(cmd, 300000);

    if (result.exitCode != 0) {
        return OperationResult::Failure(
            "Repair failed",
            result.stderr,
            result.exitCode);
    }

    return OperationResult::Success("Flatpak installation repaired");
}

// ============================================================================
// Private Helpers
// ============================================================================

FlatpakProvider::CommandResult FlatpakProvider::executeCommand(
    const std::string& command,
    int timeoutMs)
{
    CommandResult result;
    result.exitCode = -1;

    int stdoutPipe[2], stderrPipe[2];
    if (pipe(stdoutPipe) < 0 || pipe(stderrPipe) < 0) {
        result.stderr = "Failed to create pipes";
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        result.stderr = "Fork failed";
        close(stdoutPipe[0]); close(stdoutPipe[1]);
        close(stderrPipe[0]); close(stderrPipe[1]);
        return result;
    }

    if (pid == 0) {
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);
        close(stdoutPipe[1]);
        close(stderrPipe[1]);

        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
        _exit(127);
    }

    close(stdoutPipe[1]);
    close(stderrPipe[1]);

    fcntl(stdoutPipe[0], F_SETFL, O_NONBLOCK);
    fcntl(stderrPipe[0], F_SETFL, O_NONBLOCK);

    struct pollfd fds[2];
    fds[0].fd = stdoutPipe[0];
    fds[0].events = POLLIN;
    fds[1].fd = stderrPipe[0];
    fds[1].events = POLLIN;

    auto startTime = std::chrono::steady_clock::now();
    char buffer[4096];

    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();

        if (elapsed >= timeoutMs) {
            kill(pid, SIGTERM);
            result.exitCode = -1;
            result.stderr = "Command timed out";
            break;
        }

        int remainingMs = timeoutMs - elapsed;
        int ret = poll(fds, 2, std::min(remainingMs, 100));

        if (ret > 0) {
            if (fds[0].revents & POLLIN) {
                ssize_t n = read(stdoutPipe[0], buffer, sizeof(buffer) - 1);
                if (n > 0) {
                    buffer[n] = '\0';
                    result.stdout += buffer;
                }
            }
            if (fds[1].revents & POLLIN) {
                ssize_t n = read(stderrPipe[0], buffer, sizeof(buffer) - 1);
                if (n > 0) {
                    buffer[n] = '\0';
                    result.stderr += buffer;
                }
            }
        }

        int status;
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w > 0) {
            while (true) {
                ssize_t n = read(stdoutPipe[0], buffer, sizeof(buffer) - 1);
                if (n <= 0) break;
                buffer[n] = '\0';
                result.stdout += buffer;
            }
            while (true) {
                ssize_t n = read(stderrPipe[0], buffer, sizeof(buffer) - 1);
                if (n <= 0) break;
                buffer[n] = '\0';
                result.stderr += buffer;
            }

            if (WIFEXITED(status)) {
                result.exitCode = WEXITSTATUS(status);
            }
            break;
        }
    }

    close(stdoutPipe[0]);
    close(stderrPipe[0]);

    return result;
}

std::string FlatpakProvider::getScopeFlag(Scope scope) const {
    return scope == Scope::SYSTEM ? "--system" : "--user";
}

std::vector<UnifiedPackage> FlatpakProvider::parseFlatpakList(
    const std::string& output,
    bool installed)
{
    std::vector<UnifiedPackage> packages;
    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        // Tab-separated: application, name, version, origin, installation
        std::vector<std::string> fields;
        std::istringstream lineStream(line);
        std::string field;

        while (std::getline(lineStream, field, '\t')) {
            fields.push_back(field);
        }

        if (fields.size() >= 3) {
            UnifiedPackage pkg;
            pkg.id = fields[0];
            pkg.name = fields.size() > 1 ? fields[1] : fields[0];
            pkg.providerId = "flatpak";

            if (installed) {
                pkg.installedVersion = fields.size() > 2 ? fields[2] : "";
                pkg.status = InstallStatus::INSTALLED;
            } else {
                pkg.availableVersion = fields.size() > 2 ? fields[2] : "";
                pkg.status = InstallStatus::NOT_INSTALLED;
            }

            std::string origin = fields.size() > 3 ? fields[3] : "unknown";
            pkg.trustLevel = determineTrustLevel(origin, pkg.id);

            // Flatpak apps are sandboxed
            pkg.confinement = ConfinementLevel::STRICT;

            packages.push_back(pkg);
        }
    }

    return packages;
}

std::vector<UnifiedPackage> FlatpakProvider::parseFlatpakSearch(
    const std::string& output)
{
    std::vector<UnifiedPackage> packages;
    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        // Tab-separated: application, name, description, version, remotes
        std::vector<std::string> fields;
        std::istringstream lineStream(line);
        std::string field;

        while (std::getline(lineStream, field, '\t')) {
            fields.push_back(field);
        }

        if (fields.size() >= 2) {
            UnifiedPackage pkg;
            pkg.id = fields[0];
            pkg.name = fields.size() > 1 ? fields[1] : fields[0];
            pkg.summary = fields.size() > 2 ? fields[2] : "";
            pkg.availableVersion = fields.size() > 3 ? fields[3] : "";
            pkg.providerId = "flatpak";
            pkg.status = InstallStatus::NOT_INSTALLED;

            std::string remotes = fields.size() > 4 ? fields[4] : "";
            pkg.trustLevel = determineTrustLevel(remotes, pkg.id);
            pkg.confinement = ConfinementLevel::STRICT;

            packages.push_back(pkg);
        }
    }

    return packages;
}

std::optional<UnifiedPackage> FlatpakProvider::parseFlatpakInfo(
    const std::string& output)
{
    UnifiedPackage pkg;
    pkg.providerId = "flatpak";

    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);

        // Trim whitespace
        while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
            value = value.substr(1);
        }

        if (key == "ID" || key == "Ref") {
            // Extract app ID from ref if needed
            if (value.find('/') != std::string::npos) {
                // Format: app/com.example.App/x86_64/stable
                size_t start = value.find('/') + 1;
                size_t end = value.find('/', start);
                pkg.id = value.substr(start, end - start);
            } else {
                pkg.id = value;
            }
        } else if (key == "Name") {
            pkg.name = value;
        } else if (key == "Summary") {
            pkg.summary = value;
        } else if (key == "Version") {
            pkg.installedVersion = value;
            pkg.status = InstallStatus::INSTALLED;
        } else if (key == "Origin") {
            pkg.metadata.sourceRepo = value;
            pkg.trustLevel = determineTrustLevel(value, pkg.id);
        } else if (key == "License") {
            pkg.metadata.license = value;
        } else if (key == "Homepage") {
            pkg.metadata.homepage = value;
        }
    }

    if (pkg.id.empty()) {
        return std::nullopt;
    }

    pkg.confinement = ConfinementLevel::STRICT;
    return pkg;
}

std::vector<UnifiedPackage> FlatpakProvider::parseRemoteLs(
    const std::string& output)
{
    // Similar format to search
    return parseFlatpakSearch(output);
}

bool FlatpakProvider::isValidAppId(const std::string& appId) {
    if (appId.empty() || appId.length() > 255) return false;

    // App IDs are reverse DNS notation: com.example.AppName
    static std::regex appIdRegex("^[a-zA-Z][a-zA-Z0-9_]*(\\.[a-zA-Z][a-zA-Z0-9_]*)+$");
    return std::regex_match(appId, appIdRegex);
}

TrustLevel FlatpakProvider::determineTrustLevel(
    const std::string& remote,
    const std::string& appId)
{
    std::string lowerRemote = remote;
    std::transform(lowerRemote.begin(), lowerRemote.end(),
                  lowerRemote.begin(), ::tolower);

    // Flathub verified apps
    if (lowerRemote.find("flathub") != std::string::npos) {
        // Check for verified badge (would require API call in real impl)
        // For now, consider all Flathub apps as community
        return TrustLevel::COMMUNITY;
    }

    // Official distribution remotes
    if (lowerRemote.find("fedora") != std::string::npos ||
        lowerRemote.find("gnome") != std::string::npos ||
        lowerRemote.find("kde") != std::string::npos) {
        return TrustLevel::OFFICIAL;
    }

    if (_trustedRemotes.count(lowerRemote) > 0) {
        return TrustLevel::VERIFIED;
    }

    return TrustLevel::UNKNOWN;
}

void FlatpakProvider::reportProgress(double fraction, const std::string& status) {
    if (_progressCallback) {
        _progressCallback(fraction, status);
    }
}

} // namespace PolySynaptic

// vim:ts=4:sw=4:et
