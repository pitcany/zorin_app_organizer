/* rgunifiedview.h - Unified package view for PolySynaptic
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This file provides a GTK widget that displays packages from all
 * backends (APT, Snap, Flatpak) in a unified list with filtering.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef _RGUNIFIEDVIEW_H_
#define _RGUNIFIEDVIEW_H_

#include <gtk/gtk.h>
#include "rggtkbuilderwindow.h"
#include "backendmanager.h"

using namespace PolySynaptic;

/**
 * RGUnifiedPackageList - GTK TreeModel for unified package display
 *
 * This custom GtkTreeModel displays PackageInfo items from any backend
 * with appropriate columns and sorting.
 */

// Column definitions for the unified package list
enum UnifiedPkgListCols {
    UPKG_COL_SUPPORTED_ICON,    // Status icon
    UPKG_COL_PACKAGE_NAME,      // Package name
    UPKG_COL_BACKEND_BADGE,     // Backend badge (APT/Snap/Flatpak)
    UPKG_COL_INSTALLED_VERSION, // Installed version
    UPKG_COL_AVAILABLE_VERSION, // Available version
    UPKG_COL_DESCRIPTION,       // Summary/description
    UPKG_COL_SIZE,              // Download/installed size
    UPKG_COL_STATUS,            // Installation status text
    UPKG_COL_PACKAGE_PTR,       // Pointer to PackageInfo
    UPKG_COL_BACKEND_TYPE,      // BackendType enum value
    UPKG_NUM_COLS
};

// GObject type registration
#define RG_TYPE_UNIFIED_PKG_LIST (rg_unified_pkg_list_get_type())
#define RG_UNIFIED_PKG_LIST(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), RG_TYPE_UNIFIED_PKG_LIST, RGUnifiedPkgList))

typedef struct _RGUnifiedPkgList RGUnifiedPkgList;
typedef struct _RGUnifiedPkgListClass RGUnifiedPkgListClass;

struct _RGUnifiedPkgList {
    GObject parent;

    // Package data
    vector<PackageInfo>* packages;

    // Sorting
    gint sort_column_id;
    GtkSortType sort_order;

    // Backend filter
    BackendFilter filter;

    // Backend manager reference
    BackendManager* manager;
};

struct _RGUnifiedPkgListClass {
    GObjectClass parent_class;
};

GType rg_unified_pkg_list_get_type();
RGUnifiedPkgList* rg_unified_pkg_list_new(BackendManager* manager);

// Set the package data to display
void rg_unified_pkg_list_set_packages(RGUnifiedPkgList* list, vector<PackageInfo>* packages);

// Set the backend filter
void rg_unified_pkg_list_set_filter(RGUnifiedPkgList* list, const BackendFilter& filter);

// Refresh the view
void rg_unified_pkg_list_refresh(RGUnifiedPkgList* list);

/**
 * RGBackendFilterBar - Backend filter toggle widget
 *
 * Provides checkboxes for enabling/disabling each backend in searches.
 */
class RGBackendFilterBar {
public:
    RGBackendFilterBar(BackendManager* manager);
    ~RGBackendFilterBar();

    // Get the GTK widget to add to a container
    GtkWidget* getWidget() { return _container; }

    // Get current filter state
    BackendFilter getFilter() const;

    // Set filter state
    void setFilter(const BackendFilter& filter);

    // Update availability (grey out unavailable backends)
    void updateAvailability();

    // Set callback for filter changes
    void setChangedCallback(function<void(const BackendFilter&)> callback) {
        _changedCallback = callback;
    }

private:
    BackendManager* _manager;
    GtkWidget* _container;
    GtkWidget* _aptCheck;
    GtkWidget* _snapCheck;
    GtkWidget* _flatpakCheck;

    function<void(const BackendFilter&)> _changedCallback;

    static void onToggled(GtkToggleButton* button, gpointer userData);
    void handleToggle();
};

/**
 * RGBackendStatusBar - Backend status indicator
 *
 * Shows the status of each backend with icons and tooltips.
 */
class RGBackendStatusBar {
public:
    RGBackendStatusBar(BackendManager* manager);
    ~RGBackendStatusBar();

    // Get the GTK widget
    GtkWidget* getWidget() { return _container; }

    // Update status display
    void refresh();

private:
    BackendManager* _manager;
    GtkWidget* _container;
    GtkWidget* _aptStatus;
    GtkWidget* _snapStatus;
    GtkWidget* _flatpakStatus;

    void createStatusIcon(GtkWidget** widget, const char* iconName, const char* label);
    void updateStatusIcon(GtkWidget* widget, bool available, const string& tooltip);
};

/**
 * Helper functions for unified package list cell rendering
 */

// Get pixbuf for backend badge
GdkPixbuf* get_backend_badge_pixbuf(BackendType backend);

// Get status icon for installation status
const char* get_status_icon_name(InstallStatus status);

// Format size for display
string format_size(long bytes);

// Get color for backend badge
void get_backend_badge_color(BackendType backend, GdkRGBA* color);

#endif // _RGUNIFIEDVIEW_H_

// vim:ts=4:sw=4:et
