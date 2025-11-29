/* test_backend_diagnosis.cc - Diagnostic tests for Snap/Flatpak discovery
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This file diagnoses why snaps and flatpaks may not be appearing in the UI.
 * Run with: ./test_backend_diagnosis
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include <iostream>
#include <iomanip>
#include <cassert>
#include <sstream>
#include <chrono>
#include <cstdlib>

#include "ipackagebackend.h"
#include "snapbackend.h"
#include "flatpakbackend.h"
#include "backendmanager.h"

using namespace std;
using namespace PolySynaptic;

// ============================================================================
// Test Utilities
// ============================================================================

#define DIAG_PASS "\033[32m[PASS]\033[0m"
#define DIAG_FAIL "\033[31m[FAIL]\033[0m"
#define DIAG_WARN "\033[33m[WARN]\033[0m"
#define DIAG_INFO "\033[34m[INFO]\033[0m"

int g_passCount = 0;
int g_failCount = 0;
int g_warnCount = 0;

void diagPass(const string& msg) {
    cout << DIAG_PASS << " " << msg << endl;
    g_passCount++;
}

void diagFail(const string& msg) {
    cout << DIAG_FAIL << " " << msg << endl;
    g_failCount++;
}

void diagWarn(const string& msg) {
    cout << DIAG_WARN << " " << msg << endl;
    g_warnCount++;
}

void diagInfo(const string& msg) {
    cout << DIAG_INFO << " " << msg << endl;
}

void printHeader(const string& title) {
    cout << endl;
    cout << "═══════════════════════════════════════════════════════════════════" << endl;
    cout << "  " << title << endl;
    cout << "═══════════════════════════════════════════════════════════════════" << endl;
}

// ============================================================================
// System Command Tests
// ============================================================================

bool testCommand(const string& cmd, const string& description) {
    diagInfo("Testing: " + description);
    diagInfo("Command: " + cmd);

    int result = system((cmd + " > /tmp/polysynaptic_test_out.txt 2>&1").c_str());
    int exitCode = WEXITSTATUS(result);

    // Read output
    FILE* f = fopen("/tmp/polysynaptic_test_out.txt", "r");
    string output;
    if (f) {
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), f)) {
            output += buffer;
        }
        fclose(f);
    }

    // Truncate output for display
    if (output.length() > 500) {
        output = output.substr(0, 500) + "...(truncated)";
    }

    cout << "  Exit code: " << exitCode << endl;
    if (!output.empty()) {
        cout << "  Output: " << output << endl;
    }

    return exitCode == 0;
}

// ============================================================================
// Snap Diagnostics
// ============================================================================

void diagnoseSnap() {
    printHeader("SNAP BACKEND DIAGNOSTICS");

    // Test 1: Check if snap command exists
    diagInfo("Step 1: Checking if 'snap' command exists");
    if (!testCommand("which snap", "Check snap binary location")) {
        diagFail("snap command not found. Install snapd package.");
        return;
    }
    diagPass("snap command found");

    // Test 2: Check snapd service
    diagInfo("Step 2: Checking snapd service status");
    if (!testCommand("systemctl is-active snapd 2>/dev/null || service snapd status 2>/dev/null", "Check snapd service")) {
        diagFail("snapd service is not running");
        diagInfo("Try: sudo systemctl start snapd");
        return;
    }
    diagPass("snapd service is running");

    // Test 3: Test snap list
    diagInfo("Step 3: Testing 'snap list' command");
    if (!testCommand("snap list", "List installed snaps")) {
        diagFail("'snap list' failed - snapd may not be responding");
        return;
    }
    diagPass("snap list works");

    // Test 4: Test snap find with a known package
    diagInfo("Step 4: Testing 'snap find' command");
    if (!testCommand("snap find hello 2>&1 | head -5", "Search for hello snap")) {
        diagWarn("'snap find' may not be working or store unavailable");
    } else {
        diagPass("snap find works");
    }

    // Test 5: Test SnapBackend class
    diagInfo("Step 5: Testing SnapBackend class directly");
    SnapBackend snapBackend;

    cout << "  isAvailable(): " << (snapBackend.isAvailable() ? "true" : "false") << endl;
    cout << "  getVersion(): " << snapBackend.getVersion() << endl;

    if (!snapBackend.isAvailable()) {
        diagFail("SnapBackend reports unavailable");
        cout << "  Reason: " << snapBackend.getUnavailableReason() << endl;
        return;
    }
    diagPass("SnapBackend is available");

    // Test 6: Test getInstalledPackages
    diagInfo("Step 6: Testing getInstalledPackages()");
    auto installed = snapBackend.getInstalledPackages(nullptr);
    cout << "  Found " << installed.size() << " installed snaps" << endl;

    if (installed.empty()) {
        diagWarn("No snaps installed (this is ok if you don't have any snaps)");
    } else {
        diagPass("getInstalledPackages returned " + to_string(installed.size()) + " packages");

        // Show first few packages
        cout << "  Sample packages:" << endl;
        int count = 0;
        for (const auto& pkg : installed) {
            if (count++ >= 3) break;
            cout << "    - " << pkg.name << " (" << pkg.version << ")" << endl;
        }
    }

    // Test 7: Test searchPackages
    diagInfo("Step 7: Testing searchPackages('hello')");
    SearchOptions opts;
    opts.query = "hello";
    opts.maxResults = 5;

    auto searchResults = snapBackend.searchPackages(opts, nullptr);
    cout << "  Found " << searchResults.size() << " results for 'hello'" << endl;

    if (searchResults.empty()) {
        diagFail("searchPackages returned no results");
    } else {
        diagPass("searchPackages returned " + to_string(searchResults.size()) + " results");

        cout << "  Search results:" << endl;
        for (const auto& pkg : searchResults) {
            cout << "    - " << pkg.name << ": " << pkg.summary.substr(0, 50) << endl;
        }
    }
}

// ============================================================================
// Flatpak Diagnostics
// ============================================================================

void diagnoseFlatpak() {
    printHeader("FLATPAK BACKEND DIAGNOSTICS");

    // Test 1: Check if flatpak command exists
    diagInfo("Step 1: Checking if 'flatpak' command exists");
    if (!testCommand("which flatpak", "Check flatpak binary location")) {
        diagFail("flatpak command not found. Install flatpak package.");
        return;
    }
    diagPass("flatpak command found");

    // Test 2: Check flatpak version
    diagInfo("Step 2: Checking flatpak version");
    testCommand("flatpak --version", "Get flatpak version");

    // Test 3: Check configured remotes
    diagInfo("Step 3: Checking configured remotes");
    if (!testCommand("flatpak remotes", "List remotes")) {
        diagFail("Could not list remotes");
    }

    // Check if flathub is configured
    int result = system("flatpak remotes | grep -q flathub 2>/dev/null");
    if (WEXITSTATUS(result) != 0) {
        diagWarn("flathub remote not configured");
        diagInfo("Add flathub: flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo");
    } else {
        diagPass("flathub remote is configured");
    }

    // Test 4: Test flatpak list
    diagInfo("Step 4: Testing 'flatpak list' command");
    if (!testCommand("flatpak list --columns=application,name 2>&1 | head -10", "List installed flatpaks")) {
        diagWarn("'flatpak list' returned error or empty");
    } else {
        diagPass("flatpak list works");
    }

    // Test 5: Test flatpak search
    diagInfo("Step 5: Testing 'flatpak search' command");
    if (!testCommand("flatpak search firefox 2>&1 | head -5", "Search for firefox")) {
        diagWarn("'flatpak search' may not be working or no remotes configured");
    } else {
        diagPass("flatpak search works");
    }

    // Test 6: Test FlatpakBackend class
    diagInfo("Step 6: Testing FlatpakBackend class directly");
    FlatpakBackend flatpakBackend;

    cout << "  isAvailable(): " << (flatpakBackend.isAvailable() ? "true" : "false") << endl;
    cout << "  getVersion(): " << flatpakBackend.getVersion() << endl;

    if (!flatpakBackend.isAvailable()) {
        diagFail("FlatpakBackend reports unavailable");
        cout << "  Reason: " << flatpakBackend.getUnavailableReason() << endl;
        return;
    }
    diagPass("FlatpakBackend is available");

    // Test 7: Test getInstalledPackages
    diagInfo("Step 7: Testing getInstalledPackages()");
    auto installed = flatpakBackend.getInstalledPackages(nullptr);
    cout << "  Found " << installed.size() << " installed flatpaks" << endl;

    if (installed.empty()) {
        diagWarn("No flatpaks installed (this is ok if you don't have any flatpaks)");
    } else {
        diagPass("getInstalledPackages returned " + to_string(installed.size()) + " packages");

        cout << "  Sample packages:" << endl;
        int count = 0;
        for (const auto& pkg : installed) {
            if (count++ >= 3) break;
            cout << "    - " << pkg.name << " (" << pkg.id << ")" << endl;
        }
    }

    // Test 8: Test searchPackages
    diagInfo("Step 8: Testing searchPackages('firefox')");
    SearchOptions opts;
    opts.query = "firefox";
    opts.maxResults = 5;

    auto searchResults = flatpakBackend.searchPackages(opts, nullptr);
    cout << "  Found " << searchResults.size() << " results for 'firefox'" << endl;

    if (searchResults.empty()) {
        diagFail("searchPackages returned no results");
        diagInfo("This could be because flathub is not configured or search failed");
    } else {
        diagPass("searchPackages returned " + to_string(searchResults.size()) + " results");

        cout << "  Search results:" << endl;
        for (const auto& pkg : searchResults) {
            cout << "    - " << pkg.name << " (" << pkg.id << ")" << endl;
        }
    }
}

// ============================================================================
// BackendManager Diagnostics
// ============================================================================

void diagnoseBackendManager() {
    printHeader("BACKEND MANAGER DIAGNOSTICS");

    // Test 1: Create BackendManager
    diagInfo("Step 1: Creating BackendManager (without APT lister)");
    BackendManager manager(nullptr);  // No APT lister for this test

    // Test 2: Check backend statuses
    diagInfo("Step 2: Getting backend statuses");
    auto statuses = manager.getBackendStatuses();

    cout << "  Backend statuses:" << endl;
    for (const auto& status : statuses) {
        cout << "    - " << status.name << ":" << endl;
        cout << "      Available: " << (status.available ? "YES" : "NO") << endl;
        cout << "      Enabled:   " << (status.enabled ? "YES" : "NO") << endl;
        cout << "      Version:   " << status.version << endl;
        if (!status.available && !status.unavailableReason.empty()) {
            cout << "      Reason:    " << status.unavailableReason << endl;
        }
    }

    // Test 3: Get enabled backends
    diagInfo("Step 3: Getting enabled backends");
    auto enabled = manager.getEnabledBackends();
    cout << "  Enabled backends: " << enabled.size() << endl;

    for (auto* backend : enabled) {
        cout << "    - " << backend->getName() << endl;
    }

    if (enabled.empty()) {
        diagFail("No backends are enabled!");
        return;
    }

    bool hasSnap = false, hasFlatpak = false;
    for (auto* backend : enabled) {
        if (backend->getType() == BackendType::SNAP) hasSnap = true;
        if (backend->getType() == BackendType::FLATPAK) hasFlatpak = true;
    }

    if (!hasSnap) {
        diagFail("Snap backend is not in enabled list");
    } else {
        diagPass("Snap backend is enabled");
    }

    if (!hasFlatpak) {
        diagFail("Flatpak backend is not in enabled list");
    } else {
        diagPass("Flatpak backend is enabled");
    }

    // Test 4: Test searchPackages through BackendManager
    diagInfo("Step 4: Testing unified search via BackendManager");

    SearchOptions opts;
    opts.query = "firefox";
    opts.maxResults = 10;

    BackendFilter filter = BackendFilter::All();
    filter.includeApt = false;  // Exclude APT since we don't have lister

    cout << "  Searching for 'firefox' (Snap + Flatpak only)..." << endl;

    auto start = chrono::steady_clock::now();
    auto results = manager.searchPackages(opts, filter, nullptr);
    auto end = chrono::steady_clock::now();

    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    cout << "  Found " << results.size() << " results in " << duration << "ms" << endl;

    if (results.empty()) {
        diagFail("BackendManager searchPackages returned no results");
        diagInfo("This is likely the root cause of snaps/flatpaks not appearing");
    } else {
        diagPass("BackendManager searchPackages returned " + to_string(results.size()) + " results");

        // Group by backend
        int snapCount = 0, flatpakCount = 0;
        for (const auto& pkg : results) {
            if (pkg.backend == BackendType::SNAP) snapCount++;
            if (pkg.backend == BackendType::FLATPAK) flatpakCount++;
        }

        cout << "  Results by backend:" << endl;
        cout << "    - Snap:    " << snapCount << endl;
        cout << "    - Flatpak: " << flatpakCount << endl;

        if (snapCount == 0) {
            diagWarn("No Snap results in unified search");
        }
        if (flatpakCount == 0) {
            diagWarn("No Flatpak results in unified search");
        }

        cout << "  Sample results:" << endl;
        int count = 0;
        for (const auto& pkg : results) {
            if (count++ >= 5) break;
            cout << "    - [" << backendTypeToString(pkg.backend) << "] "
                 << pkg.name << endl;
        }
    }
}

// ============================================================================
// Summary
// ============================================================================

void printSummary() {
    printHeader("DIAGNOSTIC SUMMARY");

    cout << "  Passed: " << g_passCount << endl;
    cout << "  Failed: " << g_failCount << endl;
    cout << "  Warnings: " << g_warnCount << endl;
    cout << endl;

    if (g_failCount == 0) {
        cout << DIAG_PASS << " All diagnostics passed! The backends appear to be working." << endl;
        cout << endl;
        cout << "If snaps/flatpaks still don't appear in the UI, the issue is likely:" << endl;
        cout << "  1. UI is not in 'unified view' mode" << endl;
        cout << "  2. Backend filter in the UI is excluding Snap/Flatpak" << endl;
        cout << "  3. The TreeView/ListModel is not being updated correctly" << endl;
        cout << "  4. The search query in the UI is different from what the backends expect" << endl;
    } else {
        cout << DIAG_FAIL << " Some diagnostics failed. Review the output above." << endl;
        cout << endl;
        cout << "Common fixes:" << endl;
        cout << "  - For Snap: sudo systemctl start snapd" << endl;
        cout << "  - For Flatpak: flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo" << endl;
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv)
{
    cout << endl;
    cout << "╔═══════════════════════════════════════════════════════════════════╗" << endl;
    cout << "║       PolySynaptic Backend Diagnostic Tool                        ║" << endl;
    cout << "║       Diagnosing Snap/Flatpak Package Discovery                   ║" << endl;
    cout << "╚═══════════════════════════════════════════════════════════════════╝" << endl;

    // Run all diagnostics
    diagnoseSnap();
    diagnoseFlatpak();
    diagnoseBackendManager();
    printSummary();

    return g_failCount > 0 ? 1 : 0;
}

// vim:ts=4:sw=4:et
