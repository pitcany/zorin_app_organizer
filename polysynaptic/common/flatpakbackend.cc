/* flatpakbackend.cc - Flatpak package management backend implementation
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include "flatpakbackend.h"

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

FlatpakBackend::FlatpakBackend()
    : _availabilityChecked(false)
    , _isAvailable(false)
    , _defaultScope(Scope::USER)
    , _defaultRemote("flathub")
    , _timeoutSeconds(120)
{
}

FlatpakBackend::~FlatpakBackend()
{
}

// ============================================================================
// Backend Information
// ============================================================================

bool FlatpakBackend::isAvailable() const
{
    checkAvailability();
    return _isAvailable;
}

string FlatpakBackend::getUnavailableReason() const
{
    checkAvailability();
    return _unavailableReason;
}

string FlatpakBackend::getVersion() const
{
    checkAvailability();
    return _version;
}

void FlatpakBackend::checkAvailability() const
{
    if (_availabilityChecked) return;

    lock_guard<mutex> lock(_mutex);
    if (_availabilityChecked) return;

    _availabilityChecked = true;

    // Check if flatpak command exists
    auto result = executeCommand({"which", "flatpak"}, 5);
    if (!result.success || result.exitCode != 0) {
        _isAvailable = false;
        _unavailableReason = "flatpak command not found. Install flatpak to enable Flatpak support.";
        return;
    }

    // Get version
    result = executeCommand({"flatpak", "--version"}, 10);
    if (result.success && result.exitCode == 0) {
        // Output: "Flatpak 1.14.1"
        size_t pos = result.stdout.rfind(' ');
        if (pos != string::npos) {
            _version = result.stdout.substr(pos + 1);
            // Remove trailing newline
            while (!_version.empty() && (_version.back() == '\n' || _version.back() == '\r')) {
                _version.pop_back();
            }
        }
    }

    // Check if any remotes are configured
    refreshRemotesCache();
    if (_remotes.empty()) {
        _isAvailable = true;  // Still available, but warn user
        _unavailableReason = "No Flatpak remotes configured. Add flathub with: flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo";
    } else {
        _isAvailable = true;
    }
}

void FlatpakBackend::refreshRemotesCache() const
{
    auto result = executeCommand({"flatpak", "remotes", "--columns=name"}, 30);
    _remotes.clear();

    if (result.success && result.exitCode == 0) {
        istringstream iss(result.stdout);
        string line;
        while (getline(iss, line)) {
            // Skip header if present
            if (line.empty() || line.find("Name") != string::npos) continue;

            // Trim whitespace
            size_t start = line.find_first_not_of(" \t");
            size_t end = line.find_last_not_of(" \t\r\n");
            if (start != string::npos && end != string::npos) {
                string remote = line.substr(start, end - start + 1);
                if (!remote.empty()) {
                    _remotes.push_back(remote);
                }
            }
        }
    }
}

bool FlatpakBackend::hasRemote(const string& remoteName)
{
    checkAvailability();
    return find(_remotes.begin(), _remotes.end(), remoteName) != _remotes.end();
}

// ============================================================================
// Package Discovery & Search
// ============================================================================

vector<PackageInfo> FlatpakBackend::searchPackages(
    const SearchOptions& options,
    ProgressCallback progress)
{
    vector<PackageInfo> results;

    if (!isAvailable() || _remotes.empty()) {
        return results;
    }

    if (options.query.empty()) {
        return results;
    }

    // Basic validation - allow more characters for search
    if (options.query.length() > 100) {
        return results;
    }

    if (progress) {
        progress(0.1, "Searching Flatpak repositories...");
    }

    // Execute flatpak search
    auto result = executeCommand(
        {"flatpak", "search", "--columns=application,name,description,version,remotes", options.query},
        _timeoutSeconds);

    if (!result.success || result.exitCode != 0) {
        return results;
    }

    if (progress) {
        progress(0.5, "Parsing Flatpak results...");
    }

    results = parseFlatpakSearch(result.stdout);

    // Apply result limit
    if (options.maxResults > 0 && results.size() > static_cast<size_t>(options.maxResults)) {
        results.resize(options.maxResults);
    }

    // Check installation status for each result
    auto installed = getInstalledPackages(nullptr);
    set<string> installedIds;
    for (const auto& pkg : installed) {
        installedIds.insert(pkg.id);
    }

    for (auto& pkg : results) {
        if (installedIds.count(pkg.id) > 0) {
            pkg.installStatus = InstallStatus::INSTALLED;
        }
    }

    if (progress) {
        progress(1.0, "Search complete");
    }

    return results;
}

vector<PackageInfo> FlatpakBackend::getInstalledPackages(
    ProgressCallback progress)
{
    vector<PackageInfo> results;

    if (!isAvailable()) {
        return results;
    }

    if (progress) {
        progress(0.1, "Loading installed Flatpaks...");
    }

    // Get both user and system installations
    auto userResult = executeCommand(
        {"flatpak", "list", "--user", "--columns=application,name,version,branch,origin,size"},
        _timeoutSeconds);

    auto systemResult = executeCommand(
        {"flatpak", "list", "--system", "--columns=application,name,version,branch,origin,size"},
        _timeoutSeconds);

    if (progress) {
        progress(0.5, "Parsing installed Flatpaks...");
    }

    if (userResult.success && userResult.exitCode == 0) {
        auto userApps = parseFlatpakList(userResult.stdout);
        results.insert(results.end(), userApps.begin(), userApps.end());
    }

    if (systemResult.success && systemResult.exitCode == 0) {
        auto systemApps = parseFlatpakList(systemResult.stdout);
        // Mark as system-installed and avoid duplicates
        set<string> userIds;
        for (const auto& pkg : results) {
            userIds.insert(pkg.id);
        }
        for (auto& pkg : systemApps) {
            if (userIds.count(pkg.id) == 0) {
                results.push_back(pkg);
            }
        }
    }

    if (progress) {
        progress(1.0, "Loaded " + to_string(results.size()) + " installed Flatpaks");
    }

    return results;
}

PackageInfo FlatpakBackend::getPackageDetails(const string& packageId)
{
    PackageInfo info;

    if (!isAvailable() || !isValidAppId(packageId)) {
        return info;
    }

    // Try user first, then system
    auto result = executeCommand({"flatpak", "info", "--user", packageId}, _timeoutSeconds);

    if (!result.success || result.exitCode != 0) {
        result = executeCommand({"flatpak", "info", "--system", packageId}, _timeoutSeconds);
    }

    if (!result.success || result.exitCode != 0) {
        // Package not installed, try to get info from remote
        // flatpak remote-info <remote> <app-id>
        for (const auto& remote : _remotes) {
            result = executeCommand({"flatpak", "remote-info", remote, packageId}, _timeoutSeconds);
            if (result.success && result.exitCode == 0) {
                break;
            }
        }
    }

    if (result.success && result.exitCode == 0) {
        info = parseFlatpakInfo(result.stdout, packageId);
    }

    return info;
}

InstallStatus FlatpakBackend::getInstallStatus(const string& packageId)
{
    if (!isAvailable() || !isValidAppId(packageId)) {
        return InstallStatus::UNKNOWN;
    }

    // Check if installed (user or system)
    auto result = executeCommand({"flatpak", "info", "--user", packageId}, 30);
    if (result.success && result.exitCode == 0) {
        return InstallStatus::INSTALLED;
    }

    result = executeCommand({"flatpak", "info", "--system", packageId}, 30);
    if (result.success && result.exitCode == 0) {
        return InstallStatus::INSTALLED;
    }

    return InstallStatus::NOT_INSTALLED;
}

vector<PackageInfo> FlatpakBackend::getUpgradablePackages(
    ProgressCallback progress)
{
    vector<PackageInfo> results;

    if (!isAvailable()) {
        return results;
    }

    if (progress) {
        progress(0.1, "Checking for Flatpak updates...");
    }

    // Check user and system updates
    auto userResult = executeCommand(
        {"flatpak", "remote-ls", "--user", "--updates", "--columns=application,name,version,branch,origin"},
        _timeoutSeconds);

    auto systemResult = executeCommand(
        {"flatpak", "remote-ls", "--system", "--updates", "--columns=application,name,version,branch,origin"},
        _timeoutSeconds);

    if (progress) {
        progress(0.5, "Parsing updates...");
    }

    if (userResult.success && userResult.exitCode == 0) {
        auto userUpdates = parseFlatpakUpdate(userResult.stdout);
        results.insert(results.end(), userUpdates.begin(), userUpdates.end());
    }

    if (systemResult.success && systemResult.exitCode == 0) {
        auto systemUpdates = parseFlatpakUpdate(systemResult.stdout);
        set<string> userIds;
        for (const auto& pkg : results) {
            userIds.insert(pkg.id);
        }
        for (auto& pkg : systemUpdates) {
            if (userIds.count(pkg.id) == 0) {
                results.push_back(pkg);
            }
        }
    }

    if (progress) {
        progress(1.0, "Found " + to_string(results.size()) + " Flatpak updates");
    }

    return results;
}

// ============================================================================
// Package Operations
// ============================================================================

OperationResult FlatpakBackend::installPackage(
    const string& packageId,
    ProgressCallback progress)
{
    return installFlatpak(packageId, _defaultRemote, "stable", _defaultScope, progress);
}

OperationResult FlatpakBackend::installFlatpak(
    const string& appId,
    const string& remote,
    const string& branch,
    Scope scope,
    ProgressCallback progress)
{
    if (!isAvailable()) {
        return OperationResult::Failure("Flatpak backend not available", _unavailableReason);
    }

    if (!isValidAppId(appId)) {
        return OperationResult::Failure("Invalid application ID: " + appId);
    }

    string useRemote = remote.empty() ? _defaultRemote : remote;
    if (!useRemote.empty() && !isValidRemoteName(useRemote)) {
        return OperationResult::Failure("Invalid remote name: " + useRemote);
    }

    if (progress) {
        progress(0.1, "Installing " + appId + "...");
    }

    vector<string> args = {"flatpak", "install", "-y"};

    if (scope == Scope::USER) {
        args.push_back("--user");
    } else {
        args.push_back("--system");
    }

    if (!useRemote.empty()) {
        args.push_back(useRemote);
    }

    args.push_back(appId);

    // System-wide installation requires root
    vector<string> execArgs;
    if (scope == Scope::SYSTEM) {
        execArgs = {"pkexec"};
        execArgs.insert(execArgs.end(), args.begin(), args.end());
    } else {
        execArgs = args;
    }

    auto result = executeCommand(execArgs, 600);  // 10 minute timeout for large apps

    if (progress) {
        progress(1.0, result.success && result.exitCode == 0 ?
                 "Installed " + appId : "Failed to install " + appId);
    }

    if (result.success && result.exitCode == 0) {
        return OperationResult::Success("Successfully installed " + appId);
    } else {
        return OperationResult::Failure(
            "Failed to install " + appId,
            result.stderr.empty() ? result.stdout : result.stderr,
            result.exitCode);
    }
}

OperationResult FlatpakBackend::removePackage(
    const string& packageId,
    bool purge,
    ProgressCallback progress)
{
    if (!isAvailable()) {
        return OperationResult::Failure("Flatpak backend not available");
    }

    if (!isValidAppId(packageId)) {
        return OperationResult::Failure("Invalid application ID: " + packageId);
    }

    if (progress) {
        progress(0.1, "Removing " + packageId + "...");
    }

    // Try user first, then system
    vector<string> userArgs = {"flatpak", "uninstall", "-y", "--user", packageId};
    auto result = executeCommand(userArgs, 300);

    if (!result.success || result.exitCode != 0) {
        // Try system-wide (requires root)
        vector<string> systemArgs = {"pkexec", "flatpak", "uninstall", "-y", "--system", packageId};
        result = executeCommand(systemArgs, 300);
    }

    // Optionally delete data
    if (purge && result.success && result.exitCode == 0) {
        executeCommand({"flatpak", "uninstall", "-y", "--delete-data", packageId}, 60);
    }

    if (progress) {
        progress(1.0, result.success && result.exitCode == 0 ?
                 "Removed " + packageId : "Failed to remove " + packageId);
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

OperationResult FlatpakBackend::updatePackage(
    const string& packageId,
    ProgressCallback progress)
{
    if (!isAvailable()) {
        return OperationResult::Failure("Flatpak backend not available");
    }

    if (!isValidAppId(packageId)) {
        return OperationResult::Failure("Invalid application ID: " + packageId);
    }

    if (progress) {
        progress(0.1, "Updating " + packageId + "...");
    }

    // Try user first
    vector<string> userArgs = {"flatpak", "update", "-y", "--user", packageId};
    auto result = executeCommand(userArgs, 600);

    if (!result.success || result.exitCode != 0) {
        // Try system-wide
        vector<string> systemArgs = {"pkexec", "flatpak", "update", "-y", "--system", packageId};
        result = executeCommand(systemArgs, 600);
    }

    if (progress) {
        progress(1.0, result.success && result.exitCode == 0 ?
                 "Updated " + packageId : "Failed to update " + packageId);
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

OperationResult FlatpakBackend::refreshCache(ProgressCallback progress)
{
    if (!isAvailable()) {
        return OperationResult::Failure("Flatpak backend not available");
    }

    if (progress) {
        progress(0.1, "Refreshing Flatpak appstream data...");
    }

    // Update appstream data from all remotes
    auto result = executeCommand({"flatpak", "update", "--appstream"}, _timeoutSeconds);

    // Refresh our cached remotes list
    refreshRemotesCache();

    if (progress) {
        progress(1.0, "Flatpak data refreshed");
    }

    if (result.success && result.exitCode == 0) {
        return OperationResult::Success("Successfully refreshed Flatpak data");
    } else {
        return OperationResult::Failure(
            "Failed to refresh Flatpak data",
            result.stderr.empty() ? result.stdout : result.stderr,
            result.exitCode);
    }
}

// ============================================================================
// Repository/Remote Management
// ============================================================================

vector<string> FlatpakBackend::getRepositories()
{
    checkAvailability();
    refreshRemotesCache();
    return _remotes;
}

vector<pair<string, string>> FlatpakBackend::getRemotesWithUrls()
{
    vector<pair<string, string>> results;

    auto result = executeCommand({"flatpak", "remotes", "--columns=name,url"}, 30);
    if (result.success && result.exitCode == 0) {
        results = parseFlatpakRemotes(result.stdout);
    }

    return results;
}

OperationResult FlatpakBackend::addRepository(const string& repo)
{
    // Repo format: "name url" or just a flatpakrepo URL
    if (repo.empty()) {
        return OperationResult::Failure("Empty repository specification");
    }

    vector<string> args;

    if (repo.find("http") == 0 || repo.find(".flatpakrepo") != string::npos) {
        // It's a URL to a .flatpakrepo file
        // Extract name from URL
        string name = "remote";
        size_t lastSlash = repo.rfind('/');
        if (lastSlash != string::npos) {
            string filename = repo.substr(lastSlash + 1);
            size_t dotPos = filename.find('.');
            if (dotPos != string::npos) {
                name = filename.substr(0, dotPos);
            }
        }

        args = {"pkexec", "flatpak", "remote-add", "--if-not-exists", name, repo};
    } else {
        // Format: "name url"
        istringstream iss(repo);
        string name, url;
        iss >> name >> url;

        if (name.empty() || url.empty()) {
            return OperationResult::Failure("Invalid format. Use: name url");
        }

        if (!isValidRemoteName(name)) {
            return OperationResult::Failure("Invalid remote name: " + name);
        }

        args = {"pkexec", "flatpak", "remote-add", "--if-not-exists", name, url};
    }

    auto result = executeCommand(args, 60);

    if (result.success && result.exitCode == 0) {
        refreshRemotesCache();
        return OperationResult::Success("Repository added successfully");
    } else {
        return OperationResult::Failure(
            "Failed to add repository",
            result.stderr.empty() ? result.stdout : result.stderr,
            result.exitCode);
    }
}

OperationResult FlatpakBackend::removeRepository(const string& repo)
{
    if (!isValidRemoteName(repo)) {
        return OperationResult::Failure("Invalid remote name: " + repo);
    }

    vector<string> args = {"pkexec", "flatpak", "remote-delete", "--force", repo};
    auto result = executeCommand(args, 60);

    if (result.success && result.exitCode == 0) {
        refreshRemotesCache();
        return OperationResult::Success("Repository removed successfully");
    } else {
        return OperationResult::Failure(
            "Failed to remove repository",
            result.stderr.empty() ? result.stdout : result.stderr,
            result.exitCode);
    }
}

OperationResult FlatpakBackend::addFlathub()
{
    return addRepository("flathub https://dl.flathub.org/repo/flathub.flatpakrepo");
}

// ============================================================================
// Flatpak-Specific Methods
// ============================================================================

vector<string> FlatpakBackend::getBranches(const string& appId, const string& remote)
{
    vector<string> branches;

    if (!isValidAppId(appId)) {
        return branches;
    }

    string useRemote = remote.empty() ? _defaultRemote : remote;

    auto result = executeCommand(
        {"flatpak", "remote-info", "--show-commit", useRemote, appId}, 30);

    if (result.success && result.exitCode == 0) {
        // Parse branch from output
        istringstream iss(result.stdout);
        string line;
        while (getline(iss, line)) {
            if (line.find("Branch:") != string::npos) {
                size_t pos = line.find(':');
                if (pos != string::npos) {
                    string branch = line.substr(pos + 1);
                    size_t start = branch.find_first_not_of(" \t");
                    if (start != string::npos) {
                        branch = branch.substr(start);
                        branches.push_back(branch);
                    }
                }
            }
        }
    }

    // Common branches to try
    if (branches.empty()) {
        branches = {"stable", "beta", "master"};
    }

    return branches;
}

// ============================================================================
// CLI Execution
// ============================================================================

FlatpakBackend::CommandResult FlatpakBackend::executeCommand(
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

    if (pipe(stdoutPipe) != 0 || pipe(stderrPipe) != 0) {
        result.stderr = "Failed to create pipes";
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
        auto elapsed = chrono::duration_cast<chrono::seconds>(
            chrono::steady_clock::now() - startTime).count();

        if (elapsed >= timeout) {
            timedOut = true;
            kill(pid, SIGTERM);
            break;
        }

        pollfd fds[2];
        fds[0].fd = stdoutPipe[0];
        fds[0].events = POLLIN;
        fds[1].fd = stderrPipe[0];
        fds[1].events = POLLIN;

        int ret = poll(fds, 2, 100);

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

        int status;
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w > 0) {
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

vector<PackageInfo> FlatpakBackend::parseFlatpakSearch(const string& output)
{
    vector<PackageInfo> results;

    /*
     * flatpak search --columns=application,name,description,version,remotes output:
     * org.gnome.Calculator	Calculator	Perform calculations	42.1	flathub
     */

    istringstream iss(output);
    string line;

    while (getline(iss, line)) {
        if (line.empty()) continue;

        // Tab-separated columns
        vector<string> cols;
        istringstream lineStream(line);
        string col;
        while (getline(lineStream, col, '\t')) {
            cols.push_back(col);
        }

        if (cols.size() >= 4) {
            PackageInfo info;
            info.backend = BackendType::FLATPAK;
            info.id = cols[0];
            info.name = cols.size() > 1 ? cols[1] : cols[0];
            info.summary = cols.size() > 2 ? cols[2] : "";
            info.version = cols.size() > 3 ? cols[3] : "";
            info.remote = cols.size() > 4 ? cols[4] : "";
            info.installStatus = InstallStatus::NOT_INSTALLED;

            results.push_back(info);
        }
    }

    return results;
}

vector<PackageInfo> FlatpakBackend::parseFlatpakList(const string& output)
{
    vector<PackageInfo> results;

    /*
     * flatpak list --columns=application,name,version,branch,origin,size:
     * org.gnome.Calculator	Calculator	42.1	stable	flathub	98.7 MB
     */

    istringstream iss(output);
    string line;

    while (getline(iss, line)) {
        if (line.empty()) continue;

        vector<string> cols;
        istringstream lineStream(line);
        string col;
        while (getline(lineStream, col, '\t')) {
            cols.push_back(col);
        }

        if (cols.size() >= 2) {
            PackageInfo info;
            info.backend = BackendType::FLATPAK;
            info.id = cols[0];
            info.name = cols.size() > 1 ? cols[1] : cols[0];
            info.version = cols.size() > 2 ? cols[2] : "";
            info.installedVersion = info.version;
            info.branch = cols.size() > 3 ? cols[3] : "stable";
            info.remote = cols.size() > 4 ? cols[4] : "";
            info.installStatus = InstallStatus::INSTALLED;

            // Parse size if available
            if (cols.size() > 5) {
                string sizeStr = cols[5];
                // Parse "98.7 MB" format
                // TODO: Convert to bytes
            }

            results.push_back(info);
        }
    }

    return results;
}

PackageInfo FlatpakBackend::parseFlatpakInfo(const string& output, const string& appId)
{
    PackageInfo info;
    info.backend = BackendType::FLATPAK;
    info.id = appId;

    /*
     * flatpak info output:
     * ID: org.gnome.Calculator
     * Ref: app/org.gnome.Calculator/x86_64/stable
     * Arch: x86_64
     * Branch: stable
     * Origin: flathub
     * Collection: org.flathub.Stable
     * Installation: user
     * Installed: 98.7 MB
     * Runtime: org.gnome.Platform/x86_64/42
     * Sdk: org.gnome.Sdk/x86_64/42
     * Subject: Build Calculator 42.1 (...)
     * Date: 2022-03-25
     * Commit: abc123...
     */

    istringstream iss(output);
    string line;

    while (getline(iss, line)) {
        size_t colonPos = line.find(':');
        if (colonPos == string::npos) continue;

        string key = line.substr(0, colonPos);
        string value = line.substr(colonPos + 1);

        // Trim spaces
        size_t start = value.find_first_not_of(" \t");
        if (start != string::npos) {
            value = value.substr(start);
        }

        if (key == "ID") {
            info.id = value;
        } else if (key == "Ref") {
            info.ref = value;
        } else if (key == "Arch") {
            info.architecture = value;
        } else if (key == "Branch") {
            info.branch = value;
        } else if (key == "Origin") {
            info.remote = value;
        } else if (key == "Installed") {
            info.installStatus = InstallStatus::INSTALLED;
            // Parse size: "98.7 MB"
        } else if (key == "Runtime") {
            info.runtimeRef = value;
        } else if (key == "Version") {
            info.version = value;
            info.installedVersion = value;
        }
    }

    // If no version was found, try to extract from Ref
    if (info.version.empty() && !info.ref.empty()) {
        // ref format: app/org.example.App/x86_64/stable
        size_t lastSlash = info.ref.rfind('/');
        if (lastSlash != string::npos) {
            info.branch = info.ref.substr(lastSlash + 1);
        }
    }

    return info;
}

vector<PackageInfo> FlatpakBackend::parseFlatpakUpdate(const string& output)
{
    // Similar to parseFlatpakList but for updates
    vector<PackageInfo> results = parseFlatpakList(output);

    for (auto& pkg : results) {
        pkg.installStatus = InstallStatus::UPDATE_AVAILABLE;
    }

    return results;
}

vector<pair<string, string>> FlatpakBackend::parseFlatpakRemotes(const string& output)
{
    vector<pair<string, string>> results;

    istringstream iss(output);
    string line;

    while (getline(iss, line)) {
        if (line.empty() || line.find("Name") != string::npos) continue;

        // Tab-separated: name, url
        size_t tabPos = line.find('\t');
        if (tabPos != string::npos) {
            string name = line.substr(0, tabPos);
            string url = line.substr(tabPos + 1);

            // Trim
            size_t start = name.find_first_not_of(" \t");
            size_t end = name.find_last_not_of(" \t");
            if (start != string::npos && end != string::npos) {
                name = name.substr(start, end - start + 1);
            }

            start = url.find_first_not_of(" \t");
            end = url.find_last_not_of(" \t\r\n");
            if (start != string::npos && end != string::npos) {
                url = url.substr(start, end - start + 1);
            }

            if (!name.empty()) {
                results.push_back({name, url});
            }
        }
    }

    return results;
}

// ============================================================================
// Validation
// ============================================================================

bool FlatpakBackend::isValidAppId(const string& appId) const
{
    // Flatpak app IDs: reverse DNS format (e.g., org.gnome.Calculator)
    // At least two components separated by dots
    // Each component: letters, digits, underscores, hyphens
    if (appId.empty() || appId.length() > 255) {
        return false;
    }

    // Security: Prevent command injection
    static const regex validId("^[a-zA-Z][a-zA-Z0-9._-]*(\\.[a-zA-Z][a-zA-Z0-9._-]*)+$");
    return regex_match(appId, validId);
}

bool FlatpakBackend::isValidRemoteName(const string& name) const
{
    // Remote names: alphanumeric, hyphens, underscores
    if (name.empty() || name.length() > 50) {
        return false;
    }

    static const regex validName("^[a-zA-Z][a-zA-Z0-9_-]*$");
    return regex_match(name, validName);
}

} // namespace PolySynaptic

// vim:ts=4:sw=4:et
