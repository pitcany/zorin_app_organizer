/* backendmanager.h - Multi-backend coordinator for PolySynaptic
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This file provides the BackendManager class that coordinates all
 * package backends (APT, Snap, Flatpak) and provides unified search,
 * installation, and transaction management.
 *
 * Architecture:
 *   BackendManager
 *     ├── AptBackend     (if available)
 *     ├── SnapBackend    (if available)
 *     └── FlatpakBackend (if available)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef _BACKENDMANAGER_H_
#define _BACKENDMANAGER_H_

#include "ipackagebackend.h"
#include "aptbackend.h"
#include "snapbackend.h"
#include "flatpakbackend.h"

#include <memory>
#include <map>
#include <set>
#include <functional>
#include <thread>
#include <future>
#include <atomic>

// Forward declaration in global namespace (RPackageLister is a legacy Synaptic class)
class RPackageLister;

namespace PolySynaptic {

/**
 * Transaction - Represents a set of pending package operations
 *
 * Transactions group operations across multiple backends and can be
 * reviewed before being committed.
 */
struct Transaction {
    struct Operation {
        BackendType backend;
        string packageId;
        string packageName;
        enum class Type { INSTALL, REMOVE, UPDATE } type;
        bool purge;  // For removals

        Operation() : backend(BackendType::UNKNOWN), type(Type::INSTALL), purge(false) {}
    };

    vector<Operation> operations;

    // Summary counts
    int installCount() const;
    int removeCount() const;
    int updateCount() const;

    // Filter operations by backend
    vector<Operation> getOperationsForBackend(BackendType backend) const;

    // Clear all operations
    void clear() { operations.clear(); }

    // Check if empty
    bool empty() const { return operations.empty(); }
};

/**
 * TransactionResult - Result of committing a transaction
 */
struct TransactionResult {
    bool success;
    int successCount;
    int failureCount;
    vector<pair<string, string>> errors;  // packageId -> error message

    string getSummary() const;
};

/**
 * BackendFilter - Filter for selecting which backends to query
 */
struct BackendFilter {
    bool includeApt;
    bool includeSnap;
    bool includeFlatpak;

    BackendFilter() : includeApt(true), includeSnap(true), includeFlatpak(true) {}

    static BackendFilter All() { return BackendFilter(); }

    static BackendFilter Only(BackendType type) {
        BackendFilter f;
        f.includeApt = (type == BackendType::APT);
        f.includeSnap = (type == BackendType::SNAP);
        f.includeFlatpak = (type == BackendType::FLATPAK);
        return f;
    }

    bool includes(BackendType type) const {
        switch (type) {
            case BackendType::APT: return includeApt;
            case BackendType::SNAP: return includeSnap;
            case BackendType::FLATPAK: return includeFlatpak;
            default: return false;
        }
    }
};

/**
 * BackendStatus - Status information for a backend
 */
struct BackendStatus {
    BackendType type;
    string name;
    bool available;
    bool enabled;
    string version;
    string unavailableReason;
    int packageCount;  // Number of packages known to this backend
};

/**
 * BackendManager - Coordinates multiple package backends
 *
 * This class manages all available package backends and provides:
 *   - Unified search across all backends (parallel execution)
 *   - Combined package listings
 *   - Multi-backend transaction management
 *   - Backend availability detection and status
 *   - Configuration persistence
 *
 * Thread Safety:
 *   All public methods are thread-safe.
 *   Parallel search operations use internal synchronization.
 */
class BackendManager {
public:
    /**
     * Constructor
     *
     * @param lister RPackageLister for APT backend (can be nullptr)
     *
     * If lister is nullptr, APT backend will not be available.
     * Snap and Flatpak backends are automatically detected.
     */
    explicit BackendManager(RPackageLister* lister = nullptr);

    ~BackendManager();

    // ========================================================================
    // Backend Access
    // ========================================================================

    /**
     * Get a specific backend by type
     *
     * @return Backend pointer or nullptr if not available/enabled
     */
    IPackageBackend* getBackend(BackendType type);

    /**
     * Get APT backend (convenience method)
     */
    AptBackend* getAptBackend() { return _aptBackend.get(); }

    /**
     * Get Snap backend (convenience method)
     */
    SnapBackend* getSnapBackend() { return _snapBackend.get(); }

    /**
     * Get Flatpak backend (convenience method)
     */
    FlatpakBackend* getFlatpakBackend() { return _flatpakBackend.get(); }

    /**
     * Get all available backends
     */
    vector<IPackageBackend*> getAllBackends();

    /**
     * Get all enabled backends
     */
    vector<IPackageBackend*> getEnabledBackends();

    // ========================================================================
    // Backend Status & Configuration
    // ========================================================================

    /**
     * Get status information for all backends
     */
    vector<BackendStatus> getBackendStatuses();

    /**
     * Check if a specific backend is available
     */
    bool isBackendAvailable(BackendType type);

    /**
     * Check if a specific backend is enabled
     */
    bool isBackendEnabled(BackendType type);

    /**
     * Enable or disable a backend
     */
    void setBackendEnabled(BackendType type, bool enabled);

    /**
     * Refresh backend detection (re-check availability)
     */
    void refreshBackendDetection();

    // ========================================================================
    // Unified Package Operations
    // ========================================================================

    /**
     * Search all enabled backends in parallel
     *
     * @param options Search options
     * @param filter Backend filter
     * @param progress Progress callback
     * @return Combined results from all backends
     */
    vector<PackageInfo> searchPackages(
        const SearchOptions& options,
        const BackendFilter& filter = BackendFilter::All(),
        ProgressCallback progress = nullptr);

    /**
     * Get all installed packages from enabled backends
     */
    vector<PackageInfo> getInstalledPackages(
        const BackendFilter& filter = BackendFilter::All(),
        ProgressCallback progress = nullptr);

    /**
     * Get packages with available updates
     */
    vector<PackageInfo> getUpgradablePackages(
        const BackendFilter& filter = BackendFilter::All(),
        ProgressCallback progress = nullptr);

    /**
     * Get details for a specific package
     *
     * Automatically routes to the correct backend based on package ID.
     */
    PackageInfo getPackageDetails(const string& packageId, BackendType backend);

    // ========================================================================
    // Transaction Management
    // ========================================================================

    /**
     * Get a snapshot of the current pending transaction
     */
    Transaction getCurrentTransaction() const;

    /**
     * Add a package to the install queue
     */
    void queueInstall(const PackageInfo& package);

    /**
     * Add a package to the removal queue
     */
    void queueRemove(const PackageInfo& package, bool purge = false);

    /**
     * Add a package to the update queue
     */
    void queueUpdate(const PackageInfo& package);

    /**
     * Remove a package from the queue
     */
    void unqueue(const string& packageId, BackendType backend);

    /**
     * Clear all pending operations
     */
    void clearTransaction();

    /**
     * Commit all pending operations
     *
     * Operations are executed backend by backend. APT operations
     * are handled through the existing Synaptic transaction system.
     *
     * @param progress Progress callback
     * @return Transaction result
     */
    TransactionResult commitTransaction(ProgressCallback progress = nullptr);

    /**
     * Check if any operations are pending
     */
    bool hasQueuedOperations() const;

    /**
     * Get transaction summary for display
     */
    string getTransactionSummary() const;

    // ========================================================================
    // Cache Operations
    // ========================================================================

    /**
     * Refresh caches for all enabled backends
     */
    OperationResult refreshAllCaches(ProgressCallback progress = nullptr);

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * Load configuration from file
     */
    void loadConfiguration(const string& path = "");

    /**
     * Save configuration to file
     */
    void saveConfiguration(const string& path = "");

    /**
     * Get configuration directory path
     */
    static string getConfigDir();

    // ========================================================================
    // Callbacks for UI integration
    // ========================================================================

    using BackendStatusCallback = function<void(BackendType, bool available)>;
    using TransactionChangedCallback = function<void(const Transaction&)>;

    void setBackendStatusCallback(BackendStatusCallback cb) { _statusCallback = cb; }
    void setTransactionChangedCallback(TransactionChangedCallback cb) { _txCallback = cb; }

private:
    // Backend instances
    unique_ptr<AptBackend> _aptBackend;
    unique_ptr<SnapBackend> _snapBackend;
    unique_ptr<FlatpakBackend> _flatpakBackend;

    // Enable flags (user can disable even if available)
    bool _aptEnabled;
    bool _snapEnabled;
    bool _flatpakEnabled;

    // Current transaction
    Transaction _currentTransaction;

    // Thread safety
    mutable mutex _mutex;
    mutable mutex _txMutex;

    // Callbacks
    BackendStatusCallback _statusCallback;
    TransactionChangedCallback _txCallback;

    // Initialization
    void initializeBackends(RPackageLister* lister);
    void detectBackendAvailability();

    // Helper to notify transaction changes
    void notifyTransactionChanged();
};

// ============================================================================
// Inline Implementations
// ============================================================================

inline int Transaction::installCount() const {
    int count = 0;
    for (const auto& op : operations) {
        if (op.type == Operation::Type::INSTALL) count++;
    }
    return count;
}

inline int Transaction::removeCount() const {
    int count = 0;
    for (const auto& op : operations) {
        if (op.type == Operation::Type::REMOVE) count++;
    }
    return count;
}

inline int Transaction::updateCount() const {
    int count = 0;
    for (const auto& op : operations) {
        if (op.type == Operation::Type::UPDATE) count++;
    }
    return count;
}

inline vector<Transaction::Operation> Transaction::getOperationsForBackend(BackendType backend) const {
    vector<Operation> result;
    for (const auto& op : operations) {
        if (op.backend == backend) {
            result.push_back(op);
        }
    }
    return result;
}

inline string TransactionResult::getSummary() const {
    ostringstream ss;
    if (success) {
        ss << "Transaction completed: " << successCount << " succeeded";
        if (failureCount > 0) {
            ss << ", " << failureCount << " failed";
        }
    } else {
        ss << "Transaction failed: " << successCount << " succeeded, "
           << failureCount << " failed";
    }
    return ss.str();
}

} // namespace PolySynaptic

#endif // _BACKENDMANAGER_H_

// vim:ts=4:sw=4:et
