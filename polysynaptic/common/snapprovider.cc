/* snapprovider.cc - Snap package source provider implementation
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include "snapprovider.h"

#include <algorithm>
#include <sstream>
#include <array>
#include <cstring>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>

namespace PolySynaptic {

// ============================================================================
// Constructor
// ============================================================================

SnapProvider::SnapProvider() {
    // Known verified publishers (Canonical ecosystem)
    _verifiedPublishers = {
        "canonical",
        "snapcrafters",
        "ubuntu",
        "kde",
        "gnome",
        "mozilla",
        "libreoffice",
        "microsoft"
    };
}

// ============================================================================
// Availability & Configuration
// ============================================================================

bool SnapProvider::isAvailable() const {
    // Check if snap command exists and snapd is running
    return access("/usr/bin/snap", X_OK) == 0;
}

ProviderStatus SnapProvider::getStatus() const {
    ProviderStatus status;
    status.available = isAvailable();
    status.enabled = status.available;
    status.configured = status.available;

    if (!status.available) {
        status.errorMessage = "Snap is not available (snapd not installed)";
        return status;
    }

    // Get snap version and counts
    auto result = const_cast<SnapProvider*>(this)->executeCommand(
        "snap version 2>/dev/null | head -1");

    if (result.exitCode == 0) {
        status.version = result.stdout;
        // Remove newline
        while (!status.version.empty() &&
               (status.version.back() == '\n' || status.version.back() == '\r')) {
            status.version.pop_back();
        }
    }

    // Count installed snaps
    result = const_cast<SnapProvider*>(this)->executeCommand(
        "snap list 2>/dev/null | tail -n +2 | wc -l");
    if (result.exitCode == 0) {
        try {
            status.installedCount = std::stoi(result.stdout);
        } catch (...) {}
    }

    return status;
}

bool SnapProvider::configure() {
    // Snap doesn't require additional configuration
    return isAvailable();
}

// ============================================================================
// Package Operations
// ============================================================================

std::vector<UnifiedPackage> SnapProvider::search(
    const std::string& query,
    const SearchOptions& options)
{
    LOG(LogLevel::DEBUG)
        .provider("Snap")
        .operation("search")
        .field("query", query)
        .message("Starting Snap search")
        .emit();

    auto startTime = std::chrono::steady_clock::now();

    // If installed only, just filter installed list
    if (options.installedOnly) {
        auto installed = getInstalled();
        std::vector<UnifiedPackage> filtered;

        std::string lowerQuery = query;
        std::transform(lowerQuery.begin(), lowerQuery.end(),
                      lowerQuery.begin(), ::tolower);

        for (const auto& pkg : installed) {
            std::string name = pkg.name;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);

            if (name.find(lowerQuery) != std::string::npos) {
                filtered.push_back(pkg);
            }

            if (filtered.size() >= options.maxResults) break;
        }

        return filtered;
    }

    // Search Snap store
    if (!isValidSnapName(query) && query.find(' ') != std::string::npos) {
        // Query contains spaces or invalid chars, can't search
        return {};
    }

    std::string cmd = "snap find '" + query + "' 2>/dev/null";
    auto result = executeCommand(cmd, 60000);  // 60s timeout for searches

    if (result.exitCode != 0) {
        LOG(LogLevel::WARN)
            .provider("Snap")
            .operation("search")
            .field("query", query)
            .exitCode(result.exitCode)
            .stderr(result.stderr)
            .message("Snap search failed")
            .emit();
        return {};
    }

    auto packages = parseSnapFind(result.stdout);

    // Limit results
    if (packages.size() > options.maxResults) {
        packages.resize(options.maxResults);
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    LOG(LogLevel::INFO)
        .provider("Snap")
        .operation("search")
        .field("query", query)
        .field("results", std::to_string(packages.size()))
        .duration(duration)
        .message("Snap search completed")
        .emit();

    return packages;
}

std::vector<UnifiedPackage> SnapProvider::getInstalled() {
    auto result = executeCommand("snap list 2>/dev/null", 30000);

    if (result.exitCode != 0) {
        LOG(LogLevel::WARN)
            .provider("Snap")
            .operation("list")
            .exitCode(result.exitCode)
            .message("Failed to list installed snaps")
            .emit();
        return {};
    }

    return parseSnapList(result.stdout);
}

std::vector<UnifiedPackage> SnapProvider::getUpdatable() {
    auto result = executeCommand("snap refresh --list 2>/dev/null", 30000);

    if (result.exitCode != 0) {
        // No updates available returns non-zero
        return {};
    }

    // Parse refresh list (similar format to snap list)
    return parseSnapList(result.stdout);
}

std::optional<UnifiedPackage> SnapProvider::getPackageDetails(
    const std::string& packageId)
{
    if (!isValidSnapName(packageId)) {
        return std::nullopt;
    }

    std::string cmd = "snap info '" + packageId + "' 2>/dev/null";
    auto result = executeCommand(cmd, 30000);

    if (result.exitCode != 0) {
        return std::nullopt;
    }

    return parseSnapInfo(result.stdout);
}

// ============================================================================
// Install/Remove Operations
// ============================================================================

OperationResult SnapProvider::install(
    const std::string& packageId,
    const InstallOptions& options)
{
    LOG(LogLevel::INFO)
        .provider("Snap")
        .operation("install")
        .package(packageId)
        .message("Starting Snap install")
        .emit();

    if (!isValidSnapName(packageId)) {
        return OperationResult::Failure(
            "Invalid snap name: " + packageId, "", -1);
    }

    std::string cmd = "snap install '" + packageId + "'";

    // Add channel if specified
    if (!options.version.empty()) {
        cmd += " --channel=" + options.version;
    }

    auto result = executeCommand(cmd, 300000);  // 5 minute timeout

    if (result.exitCode != 0) {
        LOG(LogLevel::ERROR)
            .provider("Snap")
            .operation("install")
            .package(packageId)
            .exitCode(result.exitCode)
            .stderr(result.stderr)
            .message("Snap install failed")
            .emit();

        return OperationResult::Failure(
            "Failed to install " + packageId,
            result.stderr,
            result.exitCode);
    }

    LOG(LogLevel::INFO)
        .provider("Snap")
        .operation("install")
        .package(packageId)
        .message("Snap install completed")
        .emit();

    return OperationResult::Success("Successfully installed " + packageId);
}

OperationResult SnapProvider::remove(
    const std::string& packageId,
    const RemoveOptions& options)
{
    LOG(LogLevel::INFO)
        .provider("Snap")
        .operation("remove")
        .package(packageId)
        .message("Starting Snap remove")
        .emit();

    if (!isValidSnapName(packageId)) {
        return OperationResult::Failure(
            "Invalid snap name: " + packageId, "", -1);
    }

    std::string cmd = "snap remove '" + packageId + "'";

    if (options.purge) {
        cmd += " --purge";
    }

    auto result = executeCommand(cmd, 120000);  // 2 minute timeout

    if (result.exitCode != 0) {
        LOG(LogLevel::ERROR)
            .provider("Snap")
            .operation("remove")
            .package(packageId)
            .exitCode(result.exitCode)
            .stderr(result.stderr)
            .message("Snap remove failed")
            .emit();

        return OperationResult::Failure(
            "Failed to remove " + packageId,
            result.stderr,
            result.exitCode);
    }

    LOG(LogLevel::INFO)
        .provider("Snap")
        .operation("remove")
        .package(packageId)
        .message("Snap remove completed")
        .emit();

    return OperationResult::Success("Successfully removed " + packageId);
}

OperationResult SnapProvider::update(const std::string& packageId) {
    LOG(LogLevel::INFO)
        .provider("Snap")
        .operation("refresh")
        .package(packageId)
        .message("Starting Snap refresh")
        .emit();

    if (!isValidSnapName(packageId)) {
        return OperationResult::Failure(
            "Invalid snap name: " + packageId, "", -1);
    }

    std::string cmd = "snap refresh '" + packageId + "'";
    auto result = executeCommand(cmd, 300000);  // 5 minute timeout

    if (result.exitCode != 0) {
        LOG(LogLevel::ERROR)
            .provider("Snap")
            .operation("refresh")
            .package(packageId)
            .exitCode(result.exitCode)
            .stderr(result.stderr)
            .message("Snap refresh failed")
            .emit();

        return OperationResult::Failure(
            "Failed to refresh " + packageId,
            result.stderr,
            result.exitCode);
    }

    return OperationResult::Success("Successfully refreshed " + packageId);
}

// ============================================================================
// Trust & Security
// ============================================================================

TrustLevel SnapProvider::getTrustLevel(const std::string& packageId) {
    auto details = getPackageDetails(packageId);
    if (details) {
        return details->trustLevel;
    }
    return TrustLevel::UNKNOWN;
}

PackagePermissions SnapProvider::getPermissions(const std::string& packageId) {
    PackagePermissions perms;

    auto connections = getConnections(packageId);
    for (const auto& [plug, connected] : connections) {
        PackagePermissions::Permission p;
        p.name = plug;
        p.granted = connected;

        // Categorize permissions
        if (plug.find("network") != std::string::npos) {
            p.description = "Network access";
            p.category = "network";
        } else if (plug.find("home") != std::string::npos ||
                   plug.find("removable") != std::string::npos) {
            p.description = "File system access";
            p.category = "filesystem";
        } else if (plug.find("audio") != std::string::npos ||
                   plug.find("camera") != std::string::npos) {
            p.description = "Hardware access";
            p.category = "hardware";
        } else {
            p.description = plug;
            p.category = "other";
        }

        perms.permissions.push_back(p);
    }

    return perms;
}

// ============================================================================
// Snap-Specific Methods
// ============================================================================

std::vector<std::string> SnapProvider::getChannels(const std::string& snapName) {
    std::vector<std::string> channels;

    auto details = getPackageDetails(snapName);
    // Channels would be parsed from snap info output
    // Common channels: stable, candidate, beta, edge

    channels.push_back("stable");
    channels.push_back("candidate");
    channels.push_back("beta");
    channels.push_back("edge");

    return channels;
}

OperationResult SnapProvider::switchChannel(
    const std::string& snapName,
    const std::string& channel)
{
    if (!isValidSnapName(snapName)) {
        return OperationResult::Failure("Invalid snap name", "", -1);
    }

    std::string cmd = "snap switch '" + snapName + "' --channel=" + channel;
    auto result = executeCommand(cmd, 30000);

    if (result.exitCode != 0) {
        return OperationResult::Failure(
            "Failed to switch channel",
            result.stderr,
            result.exitCode);
    }

    return OperationResult::Success(
        "Switched " + snapName + " to channel " + channel);
}

OperationResult SnapProvider::connectPlug(
    const std::string& snap,
    const std::string& plug,
    const std::string& targetSnap,
    const std::string& slot)
{
    std::string cmd = "snap connect " + snap + ":" + plug;
    if (!targetSnap.empty()) {
        cmd += " " + targetSnap + ":" + slot;
    }

    auto result = executeCommand(cmd, 30000);

    if (result.exitCode != 0) {
        return OperationResult::Failure(
            "Failed to connect plug",
            result.stderr,
            result.exitCode);
    }

    return OperationResult::Success("Connected " + snap + ":" + plug);
}

OperationResult SnapProvider::disconnectPlug(
    const std::string& snap,
    const std::string& plug)
{
    std::string cmd = "snap disconnect " + snap + ":" + plug;
    auto result = executeCommand(cmd, 30000);

    if (result.exitCode != 0) {
        return OperationResult::Failure(
            "Failed to disconnect plug",
            result.stderr,
            result.exitCode);
    }

    return OperationResult::Success("Disconnected " + snap + ":" + plug);
}

std::vector<std::pair<std::string, bool>> SnapProvider::getConnections(
    const std::string& snapName)
{
    std::vector<std::pair<std::string, bool>> connections;

    std::string cmd = "snap connections '" + snapName + "' 2>/dev/null";
    auto result = executeCommand(cmd, 10000);

    if (result.exitCode != 0) {
        return connections;
    }

    // Parse connections output
    std::istringstream stream(result.stdout);
    std::string line;

    // Skip header
    std::getline(stream, line);

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        // Format: Interface  Plug  Slot  Notes
        std::istringstream lineStream(line);
        std::string interface, plug, slot, notes;
        lineStream >> interface >> plug >> slot >> notes;

        if (!plug.empty()) {
            // Extract plug name after ':'
            size_t colonPos = plug.find(':');
            if (colonPos != std::string::npos) {
                plug = plug.substr(colonPos + 1);
            }

            bool connected = (slot != "-");
            connections.push_back({plug, connected});
        }
    }

    return connections;
}

OperationResult SnapProvider::enable(const std::string& snapName) {
    if (!isValidSnapName(snapName)) {
        return OperationResult::Failure("Invalid snap name", "", -1);
    }

    std::string cmd = "snap enable '" + snapName + "'";
    auto result = executeCommand(cmd, 30000);

    if (result.exitCode != 0) {
        return OperationResult::Failure(
            "Failed to enable snap",
            result.stderr,
            result.exitCode);
    }

    return OperationResult::Success("Enabled " + snapName);
}

OperationResult SnapProvider::disable(const std::string& snapName) {
    if (!isValidSnapName(snapName)) {
        return OperationResult::Failure("Invalid snap name", "", -1);
    }

    std::string cmd = "snap disable '" + snapName + "'";
    auto result = executeCommand(cmd, 30000);

    if (result.exitCode != 0) {
        return OperationResult::Failure(
            "Failed to disable snap",
            result.stderr,
            result.exitCode);
    }

    return OperationResult::Success("Disabled " + snapName);
}

OperationResult SnapProvider::revert(const std::string& snapName) {
    if (!isValidSnapName(snapName)) {
        return OperationResult::Failure("Invalid snap name", "", -1);
    }

    std::string cmd = "snap revert '" + snapName + "'";
    auto result = executeCommand(cmd, 120000);

    if (result.exitCode != 0) {
        return OperationResult::Failure(
            "Failed to revert snap",
            result.stderr,
            result.exitCode);
    }

    return OperationResult::Success("Reverted " + snapName);
}

OperationResult SnapProvider::holdUpdates(
    const std::string& snapName,
    bool hold)
{
    if (!isValidSnapName(snapName)) {
        return OperationResult::Failure("Invalid snap name", "", -1);
    }

    std::string cmd = hold
        ? "snap refresh --hold '" + snapName + "'"
        : "snap refresh --unhold '" + snapName + "'";

    auto result = executeCommand(cmd, 30000);

    if (result.exitCode != 0) {
        return OperationResult::Failure(
            "Failed to " + std::string(hold ? "hold" : "unhold") + " updates",
            result.stderr,
            result.exitCode);
    }

    return OperationResult::Success(
        std::string(hold ? "Held" : "Unheld") + " updates for " + snapName);
}

// ============================================================================
// Private Helpers
// ============================================================================

SnapProvider::CommandResult SnapProvider::executeCommand(
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
        // Child process
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);
        close(stdoutPipe[1]);
        close(stderrPipe[1]);

        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
        _exit(127);
    }

    // Parent process
    close(stdoutPipe[1]);
    close(stderrPipe[1]);

    // Set non-blocking
    fcntl(stdoutPipe[0], F_SETFL, O_NONBLOCK);
    fcntl(stderrPipe[0], F_SETFL, O_NONBLOCK);

    // Poll for output with timeout
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

        // Check if child has exited
        int status;
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w > 0) {
            // Read any remaining output
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

std::vector<UnifiedPackage> SnapProvider::parseSnapList(
    const std::string& output)
{
    std::vector<UnifiedPackage> packages;
    std::istringstream stream(output);
    std::string line;

    // Skip header line
    std::getline(stream, line);

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        // Format: Name  Version  Rev  Tracking  Publisher  Notes
        std::istringstream lineStream(line);
        std::string name, version, rev, tracking, publisher, notes;

        lineStream >> name >> version >> rev >> tracking >> publisher;
        std::getline(lineStream, notes);

        if (!name.empty()) {
            UnifiedPackage pkg;
            pkg.id = name;
            pkg.name = name;
            pkg.providerId = "snap";
            pkg.installedVersion = version;
            pkg.status = InstallStatus::INSTALLED;

            // Parse publisher for verification status
            bool verified = (publisher.find("**") != std::string::npos ||
                            publisher.find("*") != std::string::npos);
            std::string cleanPublisher = publisher;
            cleanPublisher.erase(
                std::remove(cleanPublisher.begin(), cleanPublisher.end(), '*'),
                cleanPublisher.end());

            pkg.trustLevel = determineTrustLevel(cleanPublisher, verified);

            // Parse confinement from notes
            if (notes.find("classic") != std::string::npos) {
                pkg.confinement = ConfinementLevel::CLASSIC;
            } else if (notes.find("devmode") != std::string::npos) {
                pkg.confinement = ConfinementLevel::DEVMODE;
            } else {
                pkg.confinement = ConfinementLevel::STRICT;
            }

            packages.push_back(pkg);
        }
    }

    return packages;
}

std::vector<UnifiedPackage> SnapProvider::parseSnapFind(
    const std::string& output)
{
    std::vector<UnifiedPackage> packages;
    std::istringstream stream(output);
    std::string line;

    // Skip header line
    std::getline(stream, line);

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        // Format: Name  Version  Publisher  Notes  Summary
        std::istringstream lineStream(line);
        std::string name, version, publisher, notes;

        lineStream >> name >> version >> publisher >> notes;

        // Rest is summary
        std::string summary;
        std::getline(lineStream, summary);
        // Trim leading whitespace
        size_t start = summary.find_first_not_of(" \t");
        if (start != std::string::npos) {
            summary = summary.substr(start);
        }

        if (!name.empty()) {
            UnifiedPackage pkg;
            pkg.id = name;
            pkg.name = name;
            pkg.providerId = "snap";
            pkg.availableVersion = version;
            pkg.summary = summary;
            pkg.status = InstallStatus::NOT_INSTALLED;

            // Parse publisher
            bool verified = (publisher.find("**") != std::string::npos ||
                            publisher.find("*") != std::string::npos);
            std::string cleanPublisher = publisher;
            cleanPublisher.erase(
                std::remove(cleanPublisher.begin(), cleanPublisher.end(), '*'),
                cleanPublisher.end());

            pkg.trustLevel = determineTrustLevel(cleanPublisher, verified);

            packages.push_back(pkg);
        }
    }

    return packages;
}

std::optional<UnifiedPackage> SnapProvider::parseSnapInfo(
    const std::string& output)
{
    UnifiedPackage pkg;
    pkg.providerId = "snap";

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

        if (key == "name") {
            pkg.id = value;
            pkg.name = value;
        } else if (key == "summary") {
            pkg.summary = value;
        } else if (key == "publisher") {
            bool verified = (value.find("**") != std::string::npos ||
                            value.find("*") != std::string::npos);
            std::string cleanPublisher = value;
            cleanPublisher.erase(
                std::remove(cleanPublisher.begin(), cleanPublisher.end(), '*'),
                cleanPublisher.end());
            pkg.trustLevel = determineTrustLevel(cleanPublisher, verified);

            pkg.metadata.developer = cleanPublisher;
        } else if (key == "store-url") {
            pkg.metadata.homepage = value;
        } else if (key == "contact") {
            pkg.metadata.supportUrl = value;
        } else if (key == "license") {
            pkg.metadata.license = value;
        } else if (key == "description") {
            pkg.description = value;
        } else if (key == "confinement") {
            pkg.confinement = parseConfinement(value);
        } else if (key == "installed") {
            // Parse installed version
            std::istringstream vs(value);
            std::string ver;
            vs >> ver;
            pkg.installedVersion = ver;
            pkg.status = InstallStatus::INSTALLED;
        } else if (key == "snap-id") {
            // Store for unique identification
        }
    }

    if (pkg.id.empty()) {
        return std::nullopt;
    }

    return pkg;
}

bool SnapProvider::isValidSnapName(const std::string& name) {
    if (name.empty() || name.length() > 40) return false;

    // Snap names must be lowercase, start with letter, contain only
    // letters, digits, and hyphens (not at start/end or consecutive)
    static std::regex snapNameRegex("^[a-z][a-z0-9]*(-[a-z0-9]+)*$");
    return std::regex_match(name, snapNameRegex);
}

TrustLevel SnapProvider::determineTrustLevel(
    const std::string& publisher,
    bool isVerified)
{
    std::string lowerPublisher = publisher;
    std::transform(lowerPublisher.begin(), lowerPublisher.end(),
                  lowerPublisher.begin(), ::tolower);

    if (_verifiedPublishers.count(lowerPublisher) > 0) {
        return TrustLevel::OFFICIAL;
    }

    if (isVerified) {
        return TrustLevel::VERIFIED;
    }

    return TrustLevel::COMMUNITY;
}

ConfinementLevel SnapProvider::parseConfinement(const std::string& confStr) {
    if (confStr == "strict") return ConfinementLevel::STRICT;
    if (confStr == "classic") return ConfinementLevel::CLASSIC;
    if (confStr == "devmode") return ConfinementLevel::DEVMODE;
    return ConfinementLevel::UNKNOWN;
}

void SnapProvider::reportProgress(double fraction, const std::string& status) {
    if (_progressCallback) {
        _progressCallback(fraction, status);
    }
}

} // namespace PolySynaptic

// vim:ts=4:sw=4:et
