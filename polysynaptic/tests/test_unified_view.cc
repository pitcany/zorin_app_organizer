/* test_unified_view.cc - Unit tests for RGUnifiedPkgList TreeModel
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This file tests that the unified package list TreeModel correctly
 * emits signals when data changes, which is required for GTK to update
 * the display.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include <iostream>
#include <cassert>
#include <sstream>

#include <gtk/gtk.h>

#include "rgunifiedview.h"
#include "backendmanager.h"

using namespace std;
using namespace PolySynaptic;

// ============================================================================
// Test Utilities
// ============================================================================

int g_testsPassed = 0;
int g_testsFailed = 0;
int g_insertSignals = 0;
int g_deleteSignals = 0;
int g_changeSignals = 0;

#define TEST(name) \
    void test_##name(); \
    struct Test_##name { \
        Test_##name() { \
            cout << "Running test: " << #name << "... "; \
            try { \
                g_insertSignals = 0; \
                g_deleteSignals = 0; \
                g_changeSignals = 0; \
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

#define ASSERT_GT(a, b) \
    if (!((a) > (b))) { \
        ostringstream ss; \
        ss << "Assertion failed: " << #a << " > " << #b << " (got " << (a) << " <= " << (b) << ")"; \
        throw runtime_error(ss.str()); \
    }

// Signal handlers
void on_row_inserted(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data) {
    g_insertSignals++;
}

void on_row_deleted(GtkTreeModel *model, GtkTreePath *path, gpointer data) {
    g_deleteSignals++;
}

void on_row_changed(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data) {
    g_changeSignals++;
}

// ============================================================================
// Tests
// ============================================================================

TEST(SetPackages_EmitsInsertSignals) {
    // Create a list model
    RGUnifiedPkgList* list = rg_unified_pkg_list_new(nullptr);
    ASSERT_TRUE(list != nullptr);

    // Connect signal handlers
    g_signal_connect(list, "row-inserted", G_CALLBACK(on_row_inserted), nullptr);

    // Create test packages
    vector<PackageInfo> packages;
    PackageInfo pkg1("firefox", "Firefox Browser", BackendType::SNAP);
    packages.push_back(pkg1);
    PackageInfo pkg2("vlc", "VLC Media Player", BackendType::FLATPAK);
    packages.push_back(pkg2);
    PackageInfo pkg3("gimp", "GIMP", BackendType::APT);
    packages.push_back(pkg3);

    // Set packages - this should emit row-inserted signals
    rg_unified_pkg_list_set_packages(list, &packages);

    // Verify signals were emitted
    cout << "(got " << g_insertSignals << " insert signals) ";
    ASSERT_EQ(g_insertSignals, 3);

    g_object_unref(list);
}

TEST(SetPackages_WithFilter_EmitsCorrectSignals) {
    RGUnifiedPkgList* list = rg_unified_pkg_list_new(nullptr);
    g_signal_connect(list, "row-inserted", G_CALLBACK(on_row_inserted), nullptr);

    // Set filter to exclude APT
    BackendFilter filter;
    filter.includeApt = false;
    filter.includeSnap = true;
    filter.includeFlatpak = true;
    rg_unified_pkg_list_set_filter(list, filter);

    // Create test packages (mix of backends)
    vector<PackageInfo> packages;
    packages.push_back(PackageInfo("firefox", "Firefox", BackendType::SNAP));
    packages.push_back(PackageInfo("apt-pkg", "APT Package", BackendType::APT));
    packages.push_back(PackageInfo("vlc", "VLC", BackendType::FLATPAK));

    // Reset counter and set packages
    g_insertSignals = 0;
    rg_unified_pkg_list_set_packages(list, &packages);

    // Should only emit 2 signals (Snap and Flatpak, not APT)
    cout << "(got " << g_insertSignals << " insert signals, expected 2) ";
    ASSERT_EQ(g_insertSignals, 2);

    g_object_unref(list);
}

TEST(SetFilter_EmitsDeleteAndInsertSignals) {
    RGUnifiedPkgList* list = rg_unified_pkg_list_new(nullptr);
    g_signal_connect(list, "row-inserted", G_CALLBACK(on_row_inserted), nullptr);
    g_signal_connect(list, "row-deleted", G_CALLBACK(on_row_deleted), nullptr);

    // Create and set packages first
    vector<PackageInfo> packages;
    packages.push_back(PackageInfo("firefox", "Firefox", BackendType::SNAP));
    packages.push_back(PackageInfo("apt-pkg", "APT Package", BackendType::APT));
    packages.push_back(PackageInfo("vlc", "VLC", BackendType::FLATPAK));
    rg_unified_pkg_list_set_packages(list, &packages);

    // Reset counters
    g_insertSignals = 0;
    g_deleteSignals = 0;

    // Change filter to only show Snap
    BackendFilter snapOnly;
    snapOnly.includeApt = false;
    snapOnly.includeSnap = true;
    snapOnly.includeFlatpak = false;
    rg_unified_pkg_list_set_filter(list, snapOnly);

    // Should have deleted 3 rows and inserted 1
    cout << "(got " << g_deleteSignals << " delete, " << g_insertSignals << " insert) ";
    ASSERT_EQ(g_deleteSignals, 3);
    ASSERT_EQ(g_insertSignals, 1);

    g_object_unref(list);
}

TEST(TreeModel_IterNChildren) {
    RGUnifiedPkgList* list = rg_unified_pkg_list_new(nullptr);

    vector<PackageInfo> packages;
    packages.push_back(PackageInfo("pkg1", "Package 1", BackendType::SNAP));
    packages.push_back(PackageInfo("pkg2", "Package 2", BackendType::FLATPAK));
    packages.push_back(PackageInfo("pkg3", "Package 3", BackendType::APT));
    rg_unified_pkg_list_set_packages(list, &packages);

    // Check row count via TreeModel interface
    GtkTreeModel* model = GTK_TREE_MODEL(list);
    gint count = gtk_tree_model_iter_n_children(model, nullptr);

    cout << "(model reports " << count << " rows) ";
    ASSERT_EQ(count, 3);

    g_object_unref(list);
}

TEST(TreeModel_GetValue_Name) {
    RGUnifiedPkgList* list = rg_unified_pkg_list_new(nullptr);

    vector<PackageInfo> packages;
    packages.push_back(PackageInfo("test-snap", "Test Snap", BackendType::SNAP));
    packages.back().summary = "A test snap package";
    rg_unified_pkg_list_set_packages(list, &packages);

    // Get value via TreeModel interface
    GtkTreeModel* model = GTK_TREE_MODEL(list);
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
    ASSERT_TRUE(valid);

    GValue value = G_VALUE_INIT;
    gtk_tree_model_get_value(model, &iter, UPKG_COL_PACKAGE_NAME, &value);

    const gchar* name = g_value_get_string(&value);
    cout << "(name: '" << (name ? name : "null") << "') ";
    ASSERT_TRUE(name != nullptr);
    ASSERT_EQ(string(name), "Test Snap");

    g_value_unset(&value);
    g_object_unref(list);
}

TEST(TreeModel_GetValue_BackendBadge) {
    RGUnifiedPkgList* list = rg_unified_pkg_list_new(nullptr);

    vector<PackageInfo> packages;
    packages.push_back(PackageInfo("snap-pkg", "Snap Pkg", BackendType::SNAP));
    rg_unified_pkg_list_set_packages(list, &packages);

    GtkTreeModel* model = GTK_TREE_MODEL(list);
    GtkTreeIter iter;
    gtk_tree_model_get_iter_first(model, &iter);

    GValue value = G_VALUE_INIT;
    gtk_tree_model_get_value(model, &iter, UPKG_COL_BACKEND_BADGE, &value);

    const gchar* badge = g_value_get_string(&value);
    cout << "(badge: '" << (badge ? badge : "null") << "') ";
    ASSERT_TRUE(badge != nullptr);
    ASSERT_EQ(string(badge), "snap");

    g_value_unset(&value);
    g_object_unref(list);
}

TEST(TreeModel_IterNext) {
    RGUnifiedPkgList* list = rg_unified_pkg_list_new(nullptr);

    vector<PackageInfo> packages;
    packages.push_back(PackageInfo("pkg1", "First", BackendType::SNAP));
    packages.push_back(PackageInfo("pkg2", "Second", BackendType::FLATPAK));
    packages.push_back(PackageInfo("pkg3", "Third", BackendType::APT));
    rg_unified_pkg_list_set_packages(list, &packages);

    GtkTreeModel* model = GTK_TREE_MODEL(list);
    GtkTreeIter iter;

    // Get first row
    gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
    ASSERT_TRUE(valid);

    // Move to second
    valid = gtk_tree_model_iter_next(model, &iter);
    ASSERT_TRUE(valid);

    // Move to third
    valid = gtk_tree_model_iter_next(model, &iter);
    ASSERT_TRUE(valid);

    // No more rows
    valid = gtk_tree_model_iter_next(model, &iter);
    ASSERT_FALSE(valid);

    g_object_unref(list);
}

TEST(Refresh_EmitsChangeSignals) {
    RGUnifiedPkgList* list = rg_unified_pkg_list_new(nullptr);
    g_signal_connect(list, "row-changed", G_CALLBACK(on_row_changed), nullptr);

    vector<PackageInfo> packages;
    packages.push_back(PackageInfo("pkg1", "Package 1", BackendType::SNAP));
    packages.push_back(PackageInfo("pkg2", "Package 2", BackendType::FLATPAK));
    rg_unified_pkg_list_set_packages(list, &packages);

    // Reset counter
    g_changeSignals = 0;

    // Refresh should emit change signals
    rg_unified_pkg_list_refresh(list);

    cout << "(got " << g_changeSignals << " change signals) ";
    ASSERT_EQ(g_changeSignals, 2);

    g_object_unref(list);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv)
{
    // Initialize GTK (required for GObject type system)
    gtk_init(&argc, &argv);

    cout << "=== RGUnifiedPkgList TreeModel Tests ===" << endl << endl;

    // Tests are run automatically by TEST macro constructors

    cout << endl << "=== Test Results ===" << endl;
    cout << "Passed: " << g_testsPassed << endl;
    cout << "Failed: " << g_testsFailed << endl;

    return g_testsFailed > 0 ? 1 : 0;
}

// vim:ts=4:sw=4:et
