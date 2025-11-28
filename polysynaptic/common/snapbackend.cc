/* snapbackend.cc - Snap package management backend implementation
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include "snapbackend.h"

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>

#include <cstring>
#include <sstream>
#include <regex>
#include <algorithm>

namespace PolySynaptic {

// ============================================================================
// Constructor / Destructor
// ============================================================================

SnapBackend::SnapBackend()
    : _availabilityChecked(false)
    , _isAvailable(false)
    , _timeoutSeconds(120)
{
}

SnapBackend::~SnapBackend()
{
}

// ============================================================================
// Backend Information
// ============================================================================

bool SnapBackend::isAvailable() const
{
    checkAvailability();
    return _isAvailable;
}

string SnapBackend::getUnavailableReason() const
{
    checkAvailability();
    return _unavailableReason;
}

string SnapBackend::getVersion() const
{
    checkAvailability();
    return _version;
}

void SnapBackend::checkAvailability() const
{
    if (_availabilityChecked) return;

    lock_guard<mutex> lock(_mutex);
    if (_availabilityChecked) return;

    _availabilityChecked = true;

    // Check if snap command exists
    auto result = executeCommand({"which", "snap"}, 5);
    if (!result.success || result.exitCode != 0) {
        _isAvailable = false;
        _unavailableReason = "snap command not found. Install snapd to enable Snap support.";
        return;
    }

    // Check if snapd is running
    if (!isSnapdRunning()) {
        _isAvailable = false;
        _unavailableReason = "snapd service is not running. Start it with: sudo systemctl start snapd";
        return;
    }

    // Get version
    result = executeCommand({"snap", "version"}, 10);
    if (result.success && result.exitCode == 0) {
        // Parse first line: "snap    X.Y.Z"
        istringstream iss(result.stdout);
        string line;
        if (getline(iss, line)) {
            size_t pos = line.find_last_of(" \t");
            if (pos != string::npos) {
                _version = line.substr(pos + 1);
            }
        }
    }

    _isAvailable = true;
}

bool SnapBackend::isSnapdRunning() const
{
    // Check if snapd socket exists and is accessible
    auto result = executeCommand({"snap", "list"}, 10);
    return result.success && result.exitCode == 0;
}

string SnapBackend::getStoreUrl() const
{
    return "https://snapcraft.io";
}

// ============================================================================
// Package Discovery & Search
// ============================================================================

vector<PackageInfo> SnapBackend::searchPackages(
    const SearchOptions& options,
    ProgressCallback progress)
{
    vector<PackageInfo> results;

    if (!isAvailable()) {
        return results;
    }

    if (options.query.empty()) {
        return results;  // Snap search requires a query
    }

    // Note: Search queries are more permissive than snap names
    // We don't validate against isValidSnapName here because users
    // may search with mixed case (e.g., "Firefox") which snap find accepts.
    // Just do basic sanitization to prevent command injection.
    string sanitizedQuery = options.query;
    // Remove any characters that could be shell metacharacters
    sanitizedQuery.erase(
        remove_if(sanitizedQuery.begin(), sanitizedQuery.end(),
            [](char c) { return !isalnum(c) && c != '-' && c != '_' && c != ' '; }),
        sanitizedQuery.end());

    if (sanitizedQuery.empty() || sanitizedQuery.length() > 100) {
        return results;
    }

    if (progress) {
        progress(0.1, "Searching Snap Store...");
    }

    // Execute snap find
    auto result = executeCommand({"snap", "find", sanitizedQuery}, _timeoutSeconds);

    if (!result.success || result.exitCode != 0) {
        return results;
    }

    if (progress) {
        progress(0.5, "Parsing Snap results...");
    }

    results = parseSnapFind(result.stdout);

    // Apply result limit
    if (options.maxResults > 0 && results.size() > static_cast<size_t>(options.maxResults)) {
        results.resize(options.maxResults);
    }

    // Get installation status for each result
    auto installed = getInstalledPackages(nullptr);
    map<string, PackageInfo> installedMap;
    for (const auto& pkg : installed) {
        installedMap[pkg.name] = pkg;
    }

    for (auto& pkg : results) {
        auto it = installedMap.find(pkg.name);
        if (it != installedMap.end()) {
            pkg.installStatus = InstallStatus::INSTALLED;
            pkg.installedVersion = it->second.installedVersion;
        }
    }

    if (progress) {
        progress(1.0, "Search complete");
    }

    return results;
}

vector<PackageInfo> SnapBackend::getInstalledPackages(
    ProgressCallback progress)
{
    vector<PackageInfo> results;

    if (!isAvailable()) {
        return results;
    }

    if (progress) {
        progress(0.1, "Loading installed Snaps...");
    }

    auto result = executeCommand({"snap", "list"}, _timeoutSeconds);

    if (!result.success || result.exitCode != 0) {
        return results;
    }

    if (progress) {
        progress(0.5, "Parsing installed Snaps...");
    }

    results = parseSnapList(result.stdout);

    if (progress) {
        progress(1.0, "Loaded " + to_string(results.size()) + " installed Snaps");
    }

    return results;
}

PackageInfo SnapBackend::getPackageDetails(const string& packageId)
{
    PackageInfo info;

    if (!isAvailable() || !isValidSnapName(packageId)) {
        return info;
    }

    auto result = executeCommand({"snap", "info", packageId}, _timeoutSeconds);

    if (!result.success || result.exitCode != 0) {
        return info;
    }

    info = parseSnapInfo(result.stdout);
    return info;
}

InstallStatus SnapBackend::getInstallStatus(const string& packageId)
{
    if (!isAvailable() || !isValidSnapName(packageId)) {
        return InstallStatus::UNKNOWN;
    }

    // Check if in installed list
    auto result = executeCommand({"snap", "list", packageId}, 30);

    if (result.success && result.exitCode == 0) {
        // Package is installed, check for updates
        auto upgradable = getUpgradablePackages(nullptr);
        for (const auto& pkg : upgradable) {
            if (pkg.name == packageId) {
                return InstallStatus::UPDATE_AVAILABLE;
            }
        }
        return InstallStatus::INSTALLED;
    }

    return InstallStatus::NOT_INSTALLED;
}

vector<PackageInfo> SnapBackend::getUpgradablePackages(
    ProgressCallback progress)
{
    vector<PackageInfo> results;

    if (!isAvailable()) {
        return results;
    }

    if (progress) {
        progress(0.1, "Checking for Snap updates...");
    }

    auto result = executeCommand({"snap", "refresh", "--list"}, _timeoutSeconds);

    if (!result.success) {
        return results;
    }

    // If exit code is 0, there are updates; parse them
    if (result.exitCode == 0 && !result.stdout.empty()) {
        results = parseSnapRefreshList(result.stdout);
    }

    if (progress) {
        progress(1.0, "Found " + to_string(results.size()) + " Snap updates");
    }

    return results;
}

// ============================================================================
// Package Operations
// ============================================================================

OperationResult SnapBackend::installPackage(
    const string& packageId,
    ProgressCallback progress)
{
    return installSnap(packageId, false, "stable", progress);
}

OperationResult SnapBackend::installSnap(
    const string& snapName,
    bool classic,
    const string& channel,
    ProgressCallback progress)
{
    if (!isAvailable()) {
        return OperationResult::Failure("Snap backend not available", _unavailableReason);
    }

    if (!isValidSnapName(snapName)) {
        return OperationResult::Failure("Invalid snap name: " + snapName);
    }

    if (progress) {
        progress(0.1, "Installing " + snapName + "...");
    }

    vector<string> args = {"snap", "install", snapName};

    if (classic) {
        args.push_back("--classic");
    }

    if (!channel.empty() && channel != "stable") {
        args.push_back("--channel=" + channel);
    }

    // Snap install requires sudo/root
    vector<string> sudoArgs = {"pkexec"};
    sudoArgs.insert(sudoArgs.end(), args.begin(), args.end());

    auto result = executeCommand(sudoArgs, 600);  // 10 minute timeout for large snaps

    if (progress) {
        progress(1.0, result.success ? "Installed " + snapName : "Failed to install " + snapName);
    }

    if (result.success && result.exitCode == 0) {
        return OperationResult::Success("Successfully installed " + snapName);
    } else {
        return OperationResult::Failure(
            "Failed to install " + snapName,
            result.stderr.empty() ? result.stdout : result.stderr,
            result.exitCode);
    }
}

OperationResult SnapBackend::removePackage(
    const string& packageId,
    bool purge,
    ProgressCallback progress)
{
    if (!isAvailable()) {
        return OperationResult::Failure("Snap backend not available");
    }

    if (!isValidSnapName(packageId)) {
        return OperationResult::Failure("Invalid snap name: " + packageId);
    }

    if (progress) {
        progress(0.1, "Removing " + packageId + "...");
    }

    vector<string> args = {"snap", "remove", packageId};

    if (purge) {
        args.push_back("--purge");
    }

    // Snap remove requires sudo/root
    vector<string> sudoArgs = {"pkexec"};
    sudoArgs.insert(sudoArgs.end(), args.begin(), args.end());

    auto result = executeCommand(sudoArgs, 300);

    if (progress) {
        progress(1.0, result.success ? "Removed " + packageId : "Failed to remove " + packageId);
    }

    if (result.success && result.exitCode == 0) {
        return OperationResult::Success("Successfully removed " + packageId);
    } else {
        return OperationResult::Failure(
            "Failed to remove " + packageId,
            result.stderr.empty() ? result.stdout : result.stderr,
            result.exitCode);
    }
}

OperationResult SnapBackend::updatePackage(
    const string& packageId,
    ProgressCallback progress)
{
    if (!isAvailable()) {
        return OperationResult::Failure("Snap backend not available");
    }

    if (!isValidSnapName(packageId)) {
        return OperationResult::Failure("Invalid snap name: " + packageId);
    }

    if (progress) {
        progress(0.1, "Updating " + packageId + "...");
    }

    vector<string> sudoArgs = {"pkexec", "snap", "refresh", packageId};

    auto result = executeCommand(sudoArgs, 600);

    if (progress) {
        progress(1.0, result.success ? "Updated " + packageId : "Failed to update " + packageId);
    }

    if (result.success && result.exitCode == 0) {
        return OperationResult::Success("Successfully updated " + packageId);
    } else {
        return OperationResult::Failure(
            "Failed to update " + packageId,
            result.stderr.empty() ? result.stdout : result.stderr,
            result.exitCode);
    }
}

OperationResult SnapBackend::refreshCache(ProgressCallback progress)
{
    // Snap doesn't have a separate cache refresh; it's automatic
    // But we can trigger a refresh to get latest info
    if (progress) {
        progress(1.0, "Snap store is always up-to-date");
    }
    return OperationResult::Success("Snap store refreshed");
}

// ============================================================================
// Snap-Specific Methods
// ============================================================================

vector<string> SnapBackend::getChannels(const string& snapName)
{
    vector<string> channels;

    if (!isAvailable() || !isValidSnapName(snapName)) {
        return channels;
    }

    auto result = executeCommand({"snap", "info", snapName}, _timeoutSeconds);

    if (!result.success || result.exitCode != 0) {
        return channels;
    }

    // Parse channels from output
    istringstream iss(result.stdout);
    string line;
    bool inChannels = false;

    while (getline(iss, line)) {
        if (line.find("channels:") != string::npos) {
            inChannels = true;
            continue;
        }

        if (inChannels) {
            // Channels are indented with spaces
            if (line.empty() || line[0] != ' ') {
                break;
            }

            // Extract channel name (first column)
            size_t start = line.find_first_not_of(" \t");
            if (start != string::npos) {
                size_t end = line.find_first_of(" \t:", start);
                if (end != string::npos) {
                    string channel = line.substr(start, end - start);
                    if (!channel.empty()) {
                        channels.push_back(channel);
                    }
                }
            }
        }
    }

    return channels;
}

OperationResult SnapBackend::switchChannel(
    const string& snapName,
    const string& channel)
{
    if (!isAvailable()) {
        return OperationResult::Failure("Snap backend not available");
    }

    if (!isValidSnapName(snapName)) {
        return OperationResult::Failure("Invalid snap name");
    }

    vector<string> sudoArgs = {"pkexec", "snap", "switch", "--channel=" + channel, snapName};

    auto result = executeCommand(sudoArgs, 60);

    if (result.success && result.exitCode == 0) {
        return OperationResult::Success("Switched " + snapName + " to " + channel);
    } else {
        return OperationResult::Failure(
            "Failed to switch channel",
            result.stderr.empty() ? result.stdout : result.stderr,
            result.exitCode);
    }
}

// ============================================================================
// CLI Execution
// ============================================================================

SnapBackend::CommandResult SnapBackend::executeCommand(
    const vector<string>& args,
    int timeoutSeconds) const
{
    CommandResult result;
    result.success = false;
    result.exitCode = -1;

    if (args.empty()) {
        result.stderr = "No command specified";
        return result;
    }

    int timeout = (timeoutSeconds > 0) ? timeoutSeconds : _timeoutSeconds;

    // Create pipes for stdout and stderr
    int stdoutPipe[2];
    int stderrPipe[2];

    if (pipe(stdoutPipe) != 0) {
        result.stderr = "Failed to create stdout pipe";
        return result;
    }
    if (pipe(stderrPipe) != 0) {
        result.stderr = "Failed to create stderr pipe";
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        return result;
    }

    pid_t pid = fork();

    if (pid < 0) {
        result.stderr = "Failed to fork process";
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

        // Convert to C-style args
        vector<char*> cargs;
        for (const auto& arg : args) {
            cargs.push_back(const_cast<char*>(arg.c_str()));
        }
        cargs.push_back(nullptr);

        // Execute
        execvp(cargs[0], cargs.data());

        // If we get here, exec failed
        _exit(127);
    }

    // Parent process
    close(stdoutPipe[1]);
    close(stderrPipe[1]);

    // Set non-blocking
    fcntl(stdoutPipe[0], F_SETFL, O_NONBLOCK);
    fcntl(stderrPipe[0], F_SETFL, O_NONBLOCK);

    // Read output with timeout
    auto startTime = chrono::steady_clock::now();
    bool timedOut = false;

    while (true) {
        // Check timeout
        auto elapsed = chrono::duration_cast<chrono::seconds>(
            chrono::steady_clock::now() - startTime).count();

        if (elapsed >= timeout) {
            timedOut = true;
            kill(pid, SIGTERM);
            break;
        }

        // Poll for data
        pollfd fds[2];
        fds[0].fd = stdoutPipe[0];
        fds[0].events = POLLIN;
        fds[1].fd = stderrPipe[0];
        fds[1].events = POLLIN;

        int ret = poll(fds, 2, 100);  // 100ms poll timeout

        if (ret > 0) {
            char buffer[4096];

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
            // Read any remaining data
            char buffer[4096];
            ssize_t n;
            while ((n = read(stdoutPipe[0], buffer, sizeof(buffer) - 1)) > 0) {
                buffer[n] = '\0';
                result.stdout += buffer;
            }
            while ((n = read(stderrPipe[0], buffer, sizeof(buffer) - 1)) > 0) {
                buffer[n] = '\0';
                result.stderr += buffer;
            }

            if (WIFEXITED(status)) {
                result.exitCode = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                result.exitCode = 128 + WTERMSIG(status);
            }
            result.success = true;
            break;
        }
    }

    close(stdoutPipe[0]);
    close(stderrPipe[0]);

    if (timedOut) {
        waitpid(pid, nullptr, 0);
        result.stderr = "Command timed out after " + to_string(timeout) + " seconds";
        result.exitCode = -1;
        result.success = false;
    }

    return result;
}

// ============================================================================
// Parsing Helpers
// ============================================================================

vector<PackageInfo> SnapBackend::parseSnapFind(const string& output)
{
    vector<PackageInfo> results;

    /*
     * snap find output format:
     * Name       Version   Publisher   Notes   Summary
     * hello      2.10      canonical*  -       GNU Hello, the "hello world" snap
     */

    istringstream iss(output);
    string line;
    bool headerSkipped = false;

    while (getline(iss, line)) {
        // Skip header line
        if (!headerSkipped) {
            if (line.find("Name") != string::npos && line.find("Version") != string::npos) {
                headerSkipped = true;
            }
            continue;
        }

        // Skip empty lines
        if (line.empty()) continue;

        // Parse columns (space-separated, but summary can have spaces)
        istringstream lineStream(line);
        string name, version, publisher, notes;

        lineStream >> name >> version >> publisher >> notes;

        // Rest is summary
        string summary;
        getline(lineStream, summary);

        // Trim leading spaces from summary
        size_t start = summary.find_first_not_of(" \t");
        if (start != string::npos) {
            summary = summary.substr(start);
        }

        if (!name.empty()) {
            PackageInfo info;
            info.backend = BackendType::SNAP;
            info.id = name;
            info.name = name;
            info.version = version;
            info.publisher = publisher;
            info.summary = summary;
            info.installStatus = InstallStatus::NOT_INSTALLED;

            // Check for classic confinement note
            if (notes.find("classic") != string::npos) {
                info.isClassic = true;
                info.confinement = "classic";
            }

            results.push_back(info);
        }
    }

    return results;
}

vector<PackageInfo> SnapBackend::parseSnapList(const string& output)
{
    vector<PackageInfo> results;

    /*
     * snap list output format:
     * Name        Version    Rev    Tracking       Publisher   Notes
     * core20      20231123   2105   latest/stable  canonical*  base
     */

    istringstream iss(output);
    string line;
    bool headerSkipped = false;

    while (getline(iss, line)) {
        if (!headerSkipped) {
            if (line.find("Name") != string::npos && line.find("Rev") != string::npos) {
                headerSkipped = true;
            }
            continue;
        }

        if (line.empty()) continue;

        istringstream lineStream(line);
        string name, version, rev, tracking, publisher, notes;

        lineStream >> name >> version >> rev >> tracking >> publisher;

        // Rest is notes
        getline(lineStream, notes);

        if (!name.empty()) {
            PackageInfo info;
            info.backend = BackendType::SNAP;
            info.id = name;
            info.name = name;
            info.version = version;
            info.installedVersion = version;
            info.installStatus = InstallStatus::INSTALLED;
            info.publisher = publisher;
            info.channel = tracking;

            // Parse notes for confinement
            if (notes.find("classic") != string::npos) {
                info.isClassic = true;
                info.confinement = "classic";
            } else if (notes.find("devmode") != string::npos) {
                info.confinement = "devmode";
            } else {
                info.confinement = "strict";
            }

            results.push_back(info);
        }
    }

    return results;
}

PackageInfo SnapBackend::parseSnapInfo(const string& output)
{
    PackageInfo info;
    info.backend = BackendType::SNAP;

    /*
     * snap info output is key: value format
     * name:        hello
     * summary:     GNU Hello, the "hello world" snap
     * publisher:   Canonical*
     * store-url:   https://snapcraft.io/hello
     * contact:     ...
     * license:     GPL-3.0+
     * description: |
     *   GNU hello prints a friendly greeting.
     * channels:
     *   latest/stable:    2.10 2019-04-17 (29) 98kB -
     *   ...
     * installed:   2.10 (29) 98kB -
     */

    istringstream iss(output);
    string line;
    bool inDescription = false;
    string description;

    while (getline(iss, line)) {
        // Handle multi-line description
        if (inDescription) {
            if (line.empty() || line[0] != ' ') {
                inDescription = false;
                info.description = description;
            } else {
                description += line.substr(2) + "\n";  // Strip leading spaces
                continue;
            }
        }

        // Parse key: value
        size_t colonPos = line.find(':');
        if (colonPos == string::npos) continue;

        string key = line.substr(0, colonPos);
        string value = (colonPos + 1 < line.size()) ? line.substr(colonPos + 1) : "";

        // Trim leading spaces from value
        size_t start = value.find_first_not_of(" \t");
        if (start != string::npos) {
            value = value.substr(start);
        }

        if (key == "name") {
            info.id = value;
            info.name = value;
        } else if (key == "summary") {
            info.summary = value;
        } else if (key == "publisher") {
            info.publisher = value;
        } else if (key == "store-url") {
            info.homepage = value;
        } else if (key == "license") {
            info.license = value;
        } else if (key == "description") {
            if (value == "|") {
                inDescription = true;
            } else {
                info.description = value;
            }
        } else if (key == "snap-id") {
            // Unique ID from store
        } else if (key == "tracking") {
            info.channel = value;
        } else if (key == "installed") {
            // Parse installed version
            istringstream vs(value);
            vs >> info.installedVersion;
            info.installStatus = InstallStatus::INSTALLED;
        } else if (key == "confinement") {
            info.confinement = value;
            info.isClassic = (value == "classic");
        }
    }

    // Set status if not installed
    if (info.installStatus != InstallStatus::INSTALLED) {
        info.installStatus = InstallStatus::NOT_INSTALLED;
    }

    return info;
}

vector<PackageInfo> SnapBackend::parseSnapRefreshList(const string& output)
{
    vector<PackageInfo> results;

    /*
     * snap refresh --list output:
     * Name        Version  Rev   Publisher   Notes
     * firefox     123.0    3234  mozilla*    -
     */

    istringstream iss(output);
    string line;
    bool headerSkipped = false;

    while (getline(iss, line)) {
        if (!headerSkipped) {
            if (line.find("Name") != string::npos || line.find("All snaps up to date") != string::npos) {
                headerSkipped = true;
                if (line.find("All snaps up to date") != string::npos) {
                    return results;  // No updates
                }
            }
            continue;
        }

        if (line.empty()) continue;

        istringstream lineStream(line);
        string name, version, rev, publisher;

        lineStream >> name >> version >> rev >> publisher;

        if (!name.empty()) {
            PackageInfo info;
            info.backend = BackendType::SNAP;
            info.id = name;
            info.name = name;
            info.version = version;
            info.installStatus = InstallStatus::UPDATE_AVAILABLE;
            info.publisher = publisher;
            results.push_back(info);
        }
    }

    return results;
}

// ============================================================================
// Validation
// ============================================================================

bool SnapBackend::isValidSnapName(const string& name) const
{
    // Snap names: lowercase letters, digits, hyphens (not at start/end)
    // Security: Prevent command injection
    if (name.empty() || name.length() > 40) {
        return false;
    }

    static const regex validName("^[a-z][a-z0-9-]*[a-z0-9]$|^[a-z]$");
    return regex_match(name, validName);
}

} // namespace PolySynaptic

// vim:ts=4:sw=4:et
