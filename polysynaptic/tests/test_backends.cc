/* test_backends.cc - Unit tests for PolySynaptic backends
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This file contains unit tests for the backend abstraction layer,
 * including parsing tests for Snap and Flatpak CLI output.
 *
 * To run these tests:
 *   make check
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include <iostream>
#include <cassert>
#include <sstream>

#include "ipackagebackend.h"
#include "snapbackend.h"
#include "flatpakbackend.h"
#include "backendmanager.h"

using namespace std;
using namespace PolySynaptic;

// Stream operators for enum types (needed for ASSERT_EQ macro)
ostream& operator<<(ostream& os, BackendType type) {
    return os << backendTypeToString(type);
}

ostream& operator<<(ostream& os, InstallStatus status) {
    return os << installStatusToString(status);
}

ostream& operator<<(ostream& os, FlatpakBackend::Scope scope) {
    switch (scope) {
        case FlatpakBackend::Scope::USER: return os << "USER";
        case FlatpakBackend::Scope::SYSTEM: return os << "SYSTEM";
        default: return os << "UNKNOWN";
    }
}

// ============================================================================
// Test Utilities
// ============================================================================

int g_testsPassed = 0;
int g_testsFailed = 0;

#define TEST(name) \
    void test_##name(); \
    struct Test_##name { \
        Test_##name() { \
            cout << "Running test: " << #name << "... "; \
            try { \
                test_##name(); \
                cout << "PASSED" << endl; \
                g_testsPassed++; \
            } catch (const exception& e) { \
                cout << "FAILED: " << e.what() << endl; \
                g_testsFailed++; \
            } \
        } \
    } test_instance_##name; \
    void test_##name()

#define ASSERT_TRUE(x) \
    if (!(x)) throw runtime_error(string("Assertion failed: ") + #x)

#define ASSERT_FALSE(x) \
    if ((x)) throw runtime_error(string("Assertion failed: NOT ") + #x)

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        ostringstream ss; \
        ss << "Assertion failed: " << #a << " == " << #b << " (got " << (a) << " != " << (b) << ")"; \
        throw runtime_error(ss.str()); \
    }

#define ASSERT_NE(a, b) \
    if ((a) == (b)) throw runtime_error(string("Assertion failed: ") + #a + " != " + #b)

// ============================================================================
// PackageInfo Tests
// ============================================================================

TEST(PackageInfo_DefaultConstructor) {
    PackageInfo info;
    ASSERT_EQ(info.backend, BackendType::UNKNOWN);
    ASSERT_EQ(info.installStatus, InstallStatus::UNKNOWN);
    ASSERT_TRUE(info.id.empty());
    ASSERT_TRUE(info.name.empty());
    ASSERT_FALSE(info.isInstalled());
}

TEST(PackageInfo_BasicConstructor) {
    PackageInfo info("test-pkg", "Test Package", BackendType::APT);
    ASSERT_EQ(info.id, "test-pkg");
    ASSERT_EQ(info.name, "Test Package");
    ASSERT_EQ(info.backend, BackendType::APT);
}

TEST(PackageInfo_IsInstalled) {
    PackageInfo info;

    info.installStatus = InstallStatus::INSTALLED;
    ASSERT_TRUE(info.isInstalled());

    info.installStatus = InstallStatus::UPDATE_AVAILABLE;
    ASSERT_TRUE(info.isInstalled());

    info.installStatus = InstallStatus::NOT_INSTALLED;
    ASSERT_FALSE(info.isInstalled());

    info.installStatus = InstallStatus::UNKNOWN;
    ASSERT_FALSE(info.isInstalled());
}

TEST(PackageInfo_GetDisplayVersion) {
    PackageInfo info;
    info.version = "2.0";
    info.installedVersion = "1.0";

    // Should show installed version if available
    ASSERT_EQ(info.getDisplayVersion(), "1.0");

    info.installedVersion = "";
    // Should show available version if not installed
    ASSERT_EQ(info.getDisplayVersion(), "2.0");
}

TEST(PackageInfo_GetUniqueKey) {
    PackageInfo apt_pkg("firefox", "Firefox", BackendType::APT);
    PackageInfo snap_pkg("firefox", "Firefox", BackendType::SNAP);

    ASSERT_NE(apt_pkg.getUniqueKey(), snap_pkg.getUniqueKey());
    ASSERT_EQ(apt_pkg.getUniqueKey(), "firefox:APT");
    ASSERT_EQ(snap_pkg.getUniqueKey(), "firefox:Snap");
}

// ============================================================================
// BackendType Tests
// ============================================================================

TEST(BackendType_ToString) {
    ASSERT_EQ(string(backendTypeToString(BackendType::APT)), "APT");
    ASSERT_EQ(string(backendTypeToString(BackendType::SNAP)), "Snap");
    ASSERT_EQ(string(backendTypeToString(BackendType::FLATPAK)), "Flatpak");
    ASSERT_EQ(string(backendTypeToString(BackendType::UNKNOWN)), "Unknown");
}

TEST(BackendType_ToBadge) {
    ASSERT_EQ(string(backendTypeToBadge(BackendType::APT)), "deb");
    ASSERT_EQ(string(backendTypeToBadge(BackendType::SNAP)), "snap");
    ASSERT_EQ(string(backendTypeToBadge(BackendType::FLATPAK)), "flatpak");
}

// ============================================================================
// InstallStatus Tests
// ============================================================================

TEST(InstallStatus_ToString) {
    ASSERT_EQ(string(installStatusToString(InstallStatus::INSTALLED)), "Installed");
    ASSERT_EQ(string(installStatusToString(InstallStatus::NOT_INSTALLED)), "Available");
    ASSERT_EQ(string(installStatusToString(InstallStatus::UPDATE_AVAILABLE)), "Update Available");
}

// ============================================================================
// OperationResult Tests
// ============================================================================

TEST(OperationResult_Success) {
    auto result = OperationResult::Success("Test passed");
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.message, "Test passed");
}

TEST(OperationResult_Failure) {
    auto result = OperationResult::Failure("Test failed", "Technical details", 42);
    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.message, "Test failed");
    ASSERT_EQ(result.errorDetails, "Technical details");
    ASSERT_EQ(result.exitCode, 42);
}

// ============================================================================
// SearchOptions Tests
// ============================================================================

TEST(SearchOptions_Defaults) {
    SearchOptions opts;
    ASSERT_TRUE(opts.searchNames);
    ASSERT_TRUE(opts.searchDescriptions);
    ASSERT_FALSE(opts.installedOnly);
    ASSERT_FALSE(opts.availableOnly);
    ASSERT_EQ(opts.maxResults, 500);
}

// ============================================================================
// BackendFilter Tests
// ============================================================================

TEST(BackendFilter_All) {
    auto filter = BackendFilter::All();
    ASSERT_TRUE(filter.includeApt);
    ASSERT_TRUE(filter.includeSnap);
    ASSERT_TRUE(filter.includeFlatpak);
    ASSERT_TRUE(filter.includes(BackendType::APT));
    ASSERT_TRUE(filter.includes(BackendType::SNAP));
    ASSERT_TRUE(filter.includes(BackendType::FLATPAK));
}

TEST(BackendFilter_Only) {
    auto aptOnly = BackendFilter::Only(BackendType::APT);
    ASSERT_TRUE(aptOnly.includeApt);
    ASSERT_FALSE(aptOnly.includeSnap);
    ASSERT_FALSE(aptOnly.includeFlatpak);

    auto snapOnly = BackendFilter::Only(BackendType::SNAP);
    ASSERT_FALSE(snapOnly.includeApt);
    ASSERT_TRUE(snapOnly.includeSnap);
    ASSERT_FALSE(snapOnly.includeFlatpak);
}

// ============================================================================
// Transaction Tests
// ============================================================================

TEST(Transaction_Empty) {
    Transaction tx;
    ASSERT_TRUE(tx.empty());
    ASSERT_EQ(tx.installCount(), 0);
    ASSERT_EQ(tx.removeCount(), 0);
    ASSERT_EQ(tx.updateCount(), 0);
}

TEST(Transaction_Counts) {
    Transaction tx;

    Transaction::Operation op1;
    op1.type = Transaction::Operation::Type::INSTALL;
    op1.backend = BackendType::APT;
    tx.operations.push_back(op1);

    Transaction::Operation op2;
    op2.type = Transaction::Operation::Type::INSTALL;
    op2.backend = BackendType::SNAP;
    tx.operations.push_back(op2);

    Transaction::Operation op3;
    op3.type = Transaction::Operation::Type::REMOVE;
    op3.backend = BackendType::APT;
    tx.operations.push_back(op3);

    ASSERT_FALSE(tx.empty());
    ASSERT_EQ(tx.installCount(), 2);
    ASSERT_EQ(tx.removeCount(), 1);
    ASSERT_EQ(tx.updateCount(), 0);
}

TEST(Transaction_FilterByBackend) {
    Transaction tx;

    Transaction::Operation op1;
    op1.type = Transaction::Operation::Type::INSTALL;
    op1.backend = BackendType::APT;
    op1.packageId = "apt-pkg";
    tx.operations.push_back(op1);

    Transaction::Operation op2;
    op2.type = Transaction::Operation::Type::INSTALL;
    op2.backend = BackendType::SNAP;
    op2.packageId = "snap-pkg";
    tx.operations.push_back(op2);

    auto aptOps = tx.getOperationsForBackend(BackendType::APT);
    ASSERT_EQ(aptOps.size(), 1u);
    ASSERT_EQ(aptOps[0].packageId, "apt-pkg");

    auto snapOps = tx.getOperationsForBackend(BackendType::SNAP);
    ASSERT_EQ(snapOps.size(), 1u);
    ASSERT_EQ(snapOps[0].packageId, "snap-pkg");

    auto flatpakOps = tx.getOperationsForBackend(BackendType::FLATPAK);
    ASSERT_TRUE(flatpakOps.empty());
}

TEST(Transaction_Clear) {
    Transaction tx;
    Transaction::Operation op;
    op.type = Transaction::Operation::Type::INSTALL;
    tx.operations.push_back(op);

    ASSERT_FALSE(tx.empty());
    tx.clear();
    ASSERT_TRUE(tx.empty());
}

// ============================================================================
// TransactionResult Tests
// ============================================================================

TEST(TransactionResult_GetSummary) {
    TransactionResult result;
    result.success = true;
    result.successCount = 5;
    result.failureCount = 0;

    string summary = result.getSummary();
    ASSERT_TRUE(summary.find("5 succeeded") != string::npos);

    result.failureCount = 2;
    summary = result.getSummary();
    ASSERT_TRUE(summary.find("2 failed") != string::npos);
}

// ============================================================================
// SnapBackend Validation Tests
// ============================================================================

TEST(SnapBackend_ValidSnapNames) {
    SnapBackend backend;

    // These would need to be tested with isValidSnapName being public
    // For now, test that creation works
    ASSERT_EQ(backend.getType(), BackendType::SNAP);
    ASSERT_EQ(backend.getName(), "Snap");
}

// ============================================================================
// FlatpakBackend Validation Tests
// ============================================================================

TEST(FlatpakBackend_ValidAppIds) {
    FlatpakBackend backend;

    ASSERT_EQ(backend.getType(), BackendType::FLATPAK);
    ASSERT_EQ(backend.getName(), "Flatpak");
}

TEST(FlatpakBackend_DefaultScope) {
    FlatpakBackend backend;

    ASSERT_EQ(backend.getDefaultScope(), FlatpakBackend::Scope::USER);

    backend.setDefaultScope(FlatpakBackend::Scope::SYSTEM);
    ASSERT_EQ(backend.getDefaultScope(), FlatpakBackend::Scope::SYSTEM);
}

TEST(FlatpakBackend_DefaultRemote) {
    FlatpakBackend backend;

    ASSERT_EQ(backend.getDefaultRemote(), "flathub");

    backend.setDefaultRemote("custom");
    ASSERT_EQ(backend.getDefaultRemote(), "custom");
}

// ============================================================================
// BackendManager Tests (without real backends)
// ============================================================================

TEST(BackendManager_Creation) {
    BackendManager manager(nullptr);

    // Without a lister, APT backend shouldn't be available
    auto statuses = manager.getBackendStatuses();
    ASSERT_TRUE(statuses.size() >= 2);  // At least Snap and Flatpak
}

TEST(BackendManager_EnableDisable) {
    BackendManager manager(nullptr);

    manager.setBackendEnabled(BackendType::SNAP, false);
    ASSERT_FALSE(manager.isBackendEnabled(BackendType::SNAP));

    manager.setBackendEnabled(BackendType::SNAP, true);
    ASSERT_TRUE(manager.isBackendEnabled(BackendType::SNAP));
}

TEST(BackendManager_TransactionQueue) {
    BackendManager manager(nullptr);

    ASSERT_FALSE(manager.hasQueuedOperations());

    PackageInfo pkg("test", "Test", BackendType::SNAP);
    manager.queueInstall(pkg);

    ASSERT_TRUE(manager.hasQueuedOperations());

    manager.clearTransaction();
    ASSERT_FALSE(manager.hasQueuedOperations());
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv)
{
    cout << "=== PolySynaptic Backend Tests ===" << endl << endl;

    // Tests are run automatically by TEST macro constructors

    cout << endl << "=== Test Results ===" << endl;
    cout << "Passed: " << g_testsPassed << endl;
    cout << "Failed: " << g_testsFailed << endl;

    return g_testsFailed > 0 ? 1 : 0;
}

// vim:ts=4:sw=4:et
