/* backendmanager.cc - Multi-backend coordinator implementation
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include "backendmanager.h"
#include "rconfiguration.h"

#include <fstream>
#include <algorithm>
#include <chrono>

namespace PolySynaptic {

// ============================================================================
// Constructor / Destructor
// ============================================================================

BackendManager::BackendManager(RPackageLister* lister)
    : _aptEnabled(true)
    , _snapEnabled(true)
    , _flatpakEnabled(true)
{
    initializeBackends(lister);
    loadConfiguration();
}

BackendManager::~BackendManager()
{
    saveConfiguration();
}

void BackendManager::initializeBackends(RPackageLister* lister)
{
    // Initialize APT backend if lister provided
    if (lister) {
        _aptBackend = make_unique<AptBackend>(lister);
    }

    // Initialize Snap backend
    _snapBackend = make_unique<SnapBackend>();

    // Initialize Flatpak backend
    _flatpakBackend = make_unique<FlatpakBackend>();

    // Detect availability
    detectBackendAvailability();
}

void BackendManager::detectBackendAvailability()
{
    if (_statusCallback) {
        if (_aptBackend) {
            _statusCallback(BackendType::APT, _aptBackend->isAvailable());
        }
        if (_snapBackend) {
            _statusCallback(BackendType::SNAP, _snapBackend->isAvailable());
        }
        if (_flatpakBackend) {
            _statusCallback(BackendType::FLATPAK, _flatpakBackend->isAvailable());
        }
    }
}

// ============================================================================
// Backend Access
// ============================================================================

IPackageBackend* BackendManager::getBackend(BackendType type)
{
    switch (type) {
        case BackendType::APT:
            if (_aptBackend && _aptEnabled && _aptBackend->isAvailable())
                return _aptBackend.get();
            break;
        case BackendType::SNAP:
            if (_snapBackend && _snapEnabled && _snapBackend->isAvailable())
                return _snapBackend.get();
            break;
        case BackendType::FLATPAK:
            if (_flatpakBackend && _flatpakEnabled && _flatpakBackend->isAvailable())
                return _flatpakBackend.get();
            break;
        default:
            break;
    }
    return nullptr;
}

vector<IPackageBackend*> BackendManager::getAllBackends()
{
    vector<IPackageBackend*> backends;

    if (_aptBackend) backends.push_back(_aptBackend.get());
    if (_snapBackend) backends.push_back(_snapBackend.get());
    if (_flatpakBackend) backends.push_back(_flatpakBackend.get());

    return backends;
}

vector<IPackageBackend*> BackendManager::getEnabledBackends()
{
    vector<IPackageBackend*> backends;

    if (_aptBackend && _aptEnabled && _aptBackend->isAvailable())
        backends.push_back(_aptBackend.get());
    if (_snapBackend && _snapEnabled && _snapBackend->isAvailable())
        backends.push_back(_snapBackend.get());
    if (_flatpakBackend && _flatpakEnabled && _flatpakBackend->isAvailable())
        backends.push_back(_flatpakBackend.get());

    return backends;
}

// ============================================================================
// Backend Status & Configuration
// ============================================================================

vector<BackendStatus> BackendManager::getBackendStatuses()
{
    vector<BackendStatus> statuses;

    if (_aptBackend) {
        BackendStatus s;
        s.type = BackendType::APT;
        s.name = _aptBackend->getName();
        s.available = _aptBackend->isAvailable();
        s.enabled = _aptEnabled;
        s.version = _aptBackend->getVersion();
        s.unavailableReason = _aptBackend->getUnavailableReason();
        s.packageCount = _aptBackend->getLister() ?
                         _aptBackend->getLister()->packagesSize() : 0;
        statuses.push_back(s);
    }

    if (_snapBackend) {
        BackendStatus s;
        s.type = BackendType::SNAP;
        s.name = _snapBackend->getName();
        s.available = _snapBackend->isAvailable();
        s.enabled = _snapEnabled;
        s.version = _snapBackend->getVersion();
        s.unavailableReason = _snapBackend->getUnavailableReason();
        s.packageCount = 0;  // Would need to query installed count
        statuses.push_back(s);
    }

    if (_flatpakBackend) {
        BackendStatus s;
        s.type = BackendType::FLATPAK;
        s.name = _flatpakBackend->getName();
        s.available = _flatpakBackend->isAvailable();
        s.enabled = _flatpakEnabled;
        s.version = _flatpakBackend->getVersion();
        s.unavailableReason = _flatpakBackend->getUnavailableReason();
        s.packageCount = 0;
        statuses.push_back(s);
    }

    return statuses;
}

bool BackendManager::isBackendAvailable(BackendType type)
{
    switch (type) {
        case BackendType::APT:
            return _aptBackend && _aptBackend->isAvailable();
        case BackendType::SNAP:
            return _snapBackend && _snapBackend->isAvailable();
        case BackendType::FLATPAK:
            return _flatpakBackend && _flatpakBackend->isAvailable();
        default:
            return false;
    }
}

bool BackendManager::isBackendEnabled(BackendType type)
{
    switch (type) {
        case BackendType::APT:
            return _aptEnabled;
        case BackendType::SNAP:
            return _snapEnabled;
        case BackendType::FLATPAK:
            return _flatpakEnabled;
        default:
            return false;
    }
}

void BackendManager::setBackendEnabled(BackendType type, bool enabled)
{
    switch (type) {
        case BackendType::APT:
            _aptEnabled = enabled;
            break;
        case BackendType::SNAP:
            _snapEnabled = enabled;
            break;
        case BackendType::FLATPAK:
            _flatpakEnabled = enabled;
            break;
        default:
            break;
    }

    saveConfiguration();
}

void BackendManager::refreshBackendDetection()
{
    detectBackendAvailability();
}

// ============================================================================
// Unified Package Operations
// ============================================================================

vector<PackageInfo> BackendManager::searchPackages(
    const SearchOptions& options,
    const BackendFilter& filter,
    ProgressCallback progress)
{
    lock_guard<mutex> lock(_mutex);
    vector<PackageInfo> results;

    auto backends = getEnabledBackends();
    if (backends.empty()) {
        return results;
    }

    // Filter backends
    vector<IPackageBackend*> filteredBackends;
    for (auto* backend : backends) {
        if (filter.includes(backend->getType())) {
            filteredBackends.push_back(backend);
        }
    }

    if (filteredBackends.empty()) {
        return results;
    }

    // Launch parallel searches
    // Note: We capture options by value to ensure it remains valid even if
    // the caller's stack frame changes. Backend pointers are guaranteed valid
    // because we hold _mutex and getEnabledBackends returns owned pointers.
    vector<future<vector<PackageInfo>>> futures;
    auto completedCount = make_shared<atomic<int>>(0);
    int totalBackends = filteredBackends.size();

    for (auto* backend : filteredBackends) {
        // Capture options by value, backend by raw pointer (valid while lock held)
        // Capture progress by value (it's a std::function, copyable)
        SearchOptions capturedOptions = options;  // Copy for thread safety
        futures.push_back(async(launch::async, [backend, capturedOptions, progress, completedCount, totalBackends]() {
            // Create backend-specific progress wrapper
            ProgressCallback backendProgress = nullptr;
            if (progress) {
                backendProgress = [progress, completedCount, totalBackends, backend](double pct, const string& msg) {
                    int completed = completedCount->load();
                    double overallPct = (completed + pct) / totalBackends;
                    string fullMsg = "[" + backend->getName() + "] " + msg;
                    return progress(overallPct, fullMsg);
                };
            }

            auto pkgs = backend->searchPackages(capturedOptions, backendProgress);
            (*completedCount)++;
            return pkgs;
        }));
    }

    // Collect results
    for (auto& future : futures) {
        try {
            auto pkgs = future.get();
            results.insert(results.end(), pkgs.begin(), pkgs.end());
        } catch (const exception& e) {
            // Log error but continue
        }
    }

    // Sort results by relevance/name
    sort(results.begin(), results.end(), [](const PackageInfo& a, const PackageInfo& b) {
        return a.name < b.name;
    });

    // Apply overall result limit
    if (options.maxResults > 0 && results.size() > static_cast<size_t>(options.maxResults)) {
        results.resize(options.maxResults);
    }

    if (progress) {
        progress(1.0, "Found " + to_string(results.size()) + " packages");
    }

    return results;
}

vector<PackageInfo> BackendManager::getInstalledPackages(
    const BackendFilter& filter,
    ProgressCallback progress)
{
    lock_guard<mutex> lock(_mutex);
    vector<PackageInfo> results;

    auto backends = getEnabledBackends();
    int current = 0;
    int total = backends.size();

    for (auto* backend : backends) {
        if (!filter.includes(backend->getType())) {
            continue;
        }

        if (progress) {
            double pct = static_cast<double>(current) / total;
            if (!progress(pct, "Loading " + backend->getName() + " packages...")) {
                break;
            }
        }

        auto pkgs = backend->getInstalledPackages(nullptr);
        results.insert(results.end(), pkgs.begin(), pkgs.end());

        current++;
    }

    if (progress) {
        progress(1.0, "Loaded " + to_string(results.size()) + " installed packages");
    }

    return results;
}

vector<PackageInfo> BackendManager::getUpgradablePackages(
    const BackendFilter& filter,
    ProgressCallback progress)
{
    lock_guard<mutex> lock(_mutex);
    vector<PackageInfo> results;

    auto backends = getEnabledBackends();
    int current = 0;
    int total = backends.size();

    for (auto* backend : backends) {
        if (!filter.includes(backend->getType())) {
            continue;
        }

        if (progress) {
            double pct = static_cast<double>(current) / total;
            if (!progress(pct, "Checking " + backend->getName() + " updates...")) {
                break;
            }
        }

        auto pkgs = backend->getUpgradablePackages(nullptr);
        results.insert(results.end(), pkgs.begin(), pkgs.end());

        current++;
    }

    if (progress) {
        progress(1.0, "Found " + to_string(results.size()) + " updates");
    }

    return results;
}

PackageInfo BackendManager::getPackageDetails(const string& packageId, BackendType backend)
{
    auto* be = getBackend(backend);
    if (be) {
        return be->getPackageDetails(packageId);
    }
    return PackageInfo();
}

// ============================================================================
// Transaction Management
// ============================================================================

Transaction BackendManager::getCurrentTransaction() const
{
    lock_guard<mutex> lock(_txMutex);
    return _currentTransaction;
}

void BackendManager::queueInstall(const PackageInfo& package)
{
    lock_guard<mutex> lock(_txMutex);

    Transaction::Operation op;
    op.backend = package.backend;
    op.packageId = package.id;
    op.packageName = package.name;
    op.type = Transaction::Operation::Type::INSTALL;

    _currentTransaction.operations.push_back(op);
    notifyTransactionChanged();
}

void BackendManager::queueRemove(const PackageInfo& package, bool purge)
{
    lock_guard<mutex> lock(_txMutex);

    Transaction::Operation op;
    op.backend = package.backend;
    op.packageId = package.id;
    op.packageName = package.name;
    op.type = Transaction::Operation::Type::REMOVE;
    op.purge = purge;

    _currentTransaction.operations.push_back(op);
    notifyTransactionChanged();
}

void BackendManager::queueUpdate(const PackageInfo& package)
{
    lock_guard<mutex> lock(_txMutex);

    Transaction::Operation op;
    op.backend = package.backend;
    op.packageId = package.id;
    op.packageName = package.name;
    op.type = Transaction::Operation::Type::UPDATE;

    _currentTransaction.operations.push_back(op);
    notifyTransactionChanged();
}

void BackendManager::unqueue(const string& packageId, BackendType backend)
{
    lock_guard<mutex> lock(_txMutex);

    _currentTransaction.operations.erase(
        remove_if(_currentTransaction.operations.begin(),
                  _currentTransaction.operations.end(),
                  [&](const Transaction::Operation& op) {
                      return op.packageId == packageId && op.backend == backend;
                  }),
        _currentTransaction.operations.end());

    notifyTransactionChanged();
}

void BackendManager::clearTransaction()
{
    lock_guard<mutex> lock(_txMutex);
    _currentTransaction.clear();
    notifyTransactionChanged();
}

TransactionResult BackendManager::commitTransaction(ProgressCallback progress)
{
    lock_guard<mutex> lock(_txMutex);
    TransactionResult result;
    result.success = true;
    result.successCount = 0;
    result.failureCount = 0;

    int total = _currentTransaction.operations.size();
    int current = 0;

    // Process APT operations first (they may have dependencies)
    if (_aptBackend && _aptEnabled) {
        auto aptOps = _currentTransaction.getOperationsForBackend(BackendType::APT);

        for (const auto& op : aptOps) {
            if (progress) {
                double pct = static_cast<double>(current) / total;
                string action = (op.type == Transaction::Operation::Type::INSTALL) ? "Installing" :
                               (op.type == Transaction::Operation::Type::REMOVE) ? "Removing" : "Updating";
                if (!progress(pct, "[APT] " + action + " " + op.packageName + "...")) {
                    result.success = false;
                    result.errors.push_back({"", "Operation cancelled"});
                    return result;
                }
            }

            OperationResult opResult;
            switch (op.type) {
                case Transaction::Operation::Type::INSTALL:
                    opResult = _aptBackend->installPackage(op.packageId, nullptr);
                    break;
                case Transaction::Operation::Type::REMOVE:
                    opResult = _aptBackend->removePackage(op.packageId, op.purge, nullptr);
                    break;
                case Transaction::Operation::Type::UPDATE:
                    opResult = _aptBackend->updatePackage(op.packageId, nullptr);
                    break;
            }

            if (opResult.success) {
                result.successCount++;
            } else {
                result.failureCount++;
                result.errors.push_back({op.packageId, opResult.message});
                result.success = false;
            }

            current++;
        }

        // APT requires a commit step
        if (!aptOps.empty()) {
            _aptBackend->commitChanges(nullptr);
        }
    }

    // Process Snap operations
    if (_snapBackend && _snapEnabled) {
        auto snapOps = _currentTransaction.getOperationsForBackend(BackendType::SNAP);

        for (const auto& op : snapOps) {
            if (progress) {
                double pct = static_cast<double>(current) / total;
                string action = (op.type == Transaction::Operation::Type::INSTALL) ? "Installing" :
                               (op.type == Transaction::Operation::Type::REMOVE) ? "Removing" : "Updating";
                if (!progress(pct, "[Snap] " + action + " " + op.packageName + "...")) {
                    result.success = false;
                    result.errors.push_back({"", "Operation cancelled"});
                    return result;
                }
            }

            OperationResult opResult;
            switch (op.type) {
                case Transaction::Operation::Type::INSTALL:
                    opResult = _snapBackend->installPackage(op.packageId, nullptr);
                    break;
                case Transaction::Operation::Type::REMOVE:
                    opResult = _snapBackend->removePackage(op.packageId, op.purge, nullptr);
                    break;
                case Transaction::Operation::Type::UPDATE:
                    opResult = _snapBackend->updatePackage(op.packageId, nullptr);
                    break;
            }

            if (opResult.success) {
                result.successCount++;
            } else {
                result.failureCount++;
                result.errors.push_back({op.packageId, opResult.message});
                result.success = false;
            }

            current++;
        }
    }

    // Process Flatpak operations
    if (_flatpakBackend && _flatpakEnabled) {
        auto flatpakOps = _currentTransaction.getOperationsForBackend(BackendType::FLATPAK);

        for (const auto& op : flatpakOps) {
            if (progress) {
                double pct = static_cast<double>(current) / total;
                string action = (op.type == Transaction::Operation::Type::INSTALL) ? "Installing" :
                               (op.type == Transaction::Operation::Type::REMOVE) ? "Removing" : "Updating";
                if (!progress(pct, "[Flatpak] " + action + " " + op.packageName + "...")) {
                    result.success = false;
                    result.errors.push_back({"", "Operation cancelled"});
                    return result;
                }
            }

            OperationResult opResult;
            switch (op.type) {
                case Transaction::Operation::Type::INSTALL:
                    opResult = _flatpakBackend->installPackage(op.packageId, nullptr);
                    break;
                case Transaction::Operation::Type::REMOVE:
                    opResult = _flatpakBackend->removePackage(op.packageId, op.purge, nullptr);
                    break;
                case Transaction::Operation::Type::UPDATE:
                    opResult = _flatpakBackend->updatePackage(op.packageId, nullptr);
                    break;
            }

            if (opResult.success) {
                result.successCount++;
            } else {
                result.failureCount++;
                result.errors.push_back({op.packageId, opResult.message});
                result.success = false;
            }

            current++;
        }
    }

    // Clear completed transaction
    _currentTransaction.clear();

    if (progress) {
        progress(1.0, result.getSummary());
    }

    return result;
}

bool BackendManager::hasQueuedOperations() const
{
    lock_guard<mutex> lock(_txMutex);
    return !_currentTransaction.empty();
}

string BackendManager::getTransactionSummary() const
{
    lock_guard<mutex> lock(_txMutex);

    if (_currentTransaction.empty()) {
        return "No pending changes";
    }

    ostringstream ss;

    int aptInstall = 0, aptRemove = 0, aptUpdate = 0;
    int snapInstall = 0, snapRemove = 0, snapUpdate = 0;
    int flatpakInstall = 0, flatpakRemove = 0, flatpakUpdate = 0;

    for (const auto& op : _currentTransaction.operations) {
        switch (op.backend) {
            case BackendType::APT:
                if (op.type == Transaction::Operation::Type::INSTALL) aptInstall++;
                else if (op.type == Transaction::Operation::Type::REMOVE) aptRemove++;
                else aptUpdate++;
                break;
            case BackendType::SNAP:
                if (op.type == Transaction::Operation::Type::INSTALL) snapInstall++;
                else if (op.type == Transaction::Operation::Type::REMOVE) snapRemove++;
                else snapUpdate++;
                break;
            case BackendType::FLATPAK:
                if (op.type == Transaction::Operation::Type::INSTALL) flatpakInstall++;
                else if (op.type == Transaction::Operation::Type::REMOVE) flatpakRemove++;
                else flatpakUpdate++;
                break;
            default:
                break;
        }
    }

    bool first = true;
    auto addLine = [&ss, &first](const string& backend, int install, int remove, int update) {
        if (install > 0 || remove > 0 || update > 0) {
            if (!first) ss << "\n";
            ss << backend << ": ";
            vector<string> parts;
            if (install > 0) parts.push_back(to_string(install) + " to install");
            if (remove > 0) parts.push_back(to_string(remove) + " to remove");
            if (update > 0) parts.push_back(to_string(update) + " to update");
            for (size_t i = 0; i < parts.size(); i++) {
                if (i > 0) ss << ", ";
                ss << parts[i];
            }
            first = false;
        }
    };

    addLine("APT", aptInstall, aptRemove, aptUpdate);
    addLine("Snap", snapInstall, snapRemove, snapUpdate);
    addLine("Flatpak", flatpakInstall, flatpakRemove, flatpakUpdate);

    return ss.str();
}

// ============================================================================
// Cache Operations
// ============================================================================

OperationResult BackendManager::refreshAllCaches(ProgressCallback progress)
{
    auto backends = getEnabledBackends();
    int current = 0;
    int total = backends.size();
    int failures = 0;

    for (auto* backend : backends) {
        if (progress) {
            double pct = static_cast<double>(current) / total;
            if (!progress(pct, "Refreshing " + backend->getName() + " cache...")) {
                return OperationResult::Failure("Cancelled");
            }
        }

        auto result = backend->refreshCache(nullptr);
        if (!result.success) {
            failures++;
        }

        current++;
    }

    if (progress) {
        progress(1.0, "Cache refresh complete");
    }

    if (failures > 0) {
        return OperationResult::Failure("Some caches failed to refresh");
    }

    return OperationResult::Success("All caches refreshed");
}

// ============================================================================
// Configuration
// ============================================================================

string BackendManager::getConfigDir()
{
    // Use Synaptic's config directory
    return RConfDir();
}

void BackendManager::loadConfiguration(const string& path)
{
    string configPath = path.empty() ? getConfigDir() + "/polysynaptic.conf" : path;

    ifstream file(configPath);
    if (!file.is_open()) {
        return;  // No config file, use defaults
    }

    string line;
    while (getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;

        size_t eqPos = line.find('=');
        if (eqPos == string::npos) continue;

        string key = line.substr(0, eqPos);
        string value = line.substr(eqPos + 1);

        // Trim whitespace
        auto trim = [](string& s) {
            size_t start = s.find_first_not_of(" \t");
            size_t end = s.find_last_not_of(" \t\r\n");
            if (start != string::npos && end != string::npos) {
                s = s.substr(start, end - start + 1);
            }
        };
        trim(key);
        trim(value);

        if (key == "apt_enabled") {
            _aptEnabled = (value == "true" || value == "1");
        } else if (key == "snap_enabled") {
            _snapEnabled = (value == "true" || value == "1");
        } else if (key == "flatpak_enabled") {
            _flatpakEnabled = (value == "true" || value == "1");
        }
    }
}

void BackendManager::saveConfiguration(const string& path)
{
    string configPath = path.empty() ? getConfigDir() + "/polysynaptic.conf" : path;

    ofstream file(configPath);
    if (!file.is_open()) {
        return;
    }

    file << "# PolySynaptic Configuration\n";
    file << "# Generated automatically\n\n";

    file << "apt_enabled=" << (_aptEnabled ? "true" : "false") << "\n";
    file << "snap_enabled=" << (_snapEnabled ? "true" : "false") << "\n";
    file << "flatpak_enabled=" << (_flatpakEnabled ? "true" : "false") << "\n";
}

void BackendManager::notifyTransactionChanged()
{
    if (_txCallback) {
        _txCallback(_currentTransaction);
    }
}

} // namespace PolySynaptic

// vim:ts=4:sw=4:et
