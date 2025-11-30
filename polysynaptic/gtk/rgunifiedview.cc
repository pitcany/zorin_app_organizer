/* rgunifiedview.cc - Unified package view implementation
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include "rgunifiedview.h"
#include "rgutils.h"

#include <sstream>
#include <iomanip>
#include <cmath>

// ============================================================================
// Helper Functions
// ============================================================================

const char* get_status_icon_name(InstallStatus status)
{
    switch (status) {
        case InstallStatus::INSTALLED:
            return "package-installed-updated";
        case InstallStatus::UPDATE_AVAILABLE:
            return "package-installed-outdated";
        case InstallStatus::NOT_INSTALLED:
            return "package-available";
        case InstallStatus::INSTALLING:
            return "package-install";
        case InstallStatus::REMOVING:
            return "package-remove";
        case InstallStatus::BROKEN:
            return "package-broken";
        default:
            return "package-available";
    }
}

string format_size(long bytes)
{
    if (bytes <= 0) return "";

    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = bytes;

    while (size >= 1024 && unit < 4) {
        size /= 1024;
        unit++;
    }

    ostringstream ss;
    ss << fixed << setprecision(1) << size << " " << units[unit];
    return ss.str();
}

void get_backend_badge_color(BackendType backend, GdkRGBA* color)
{
    switch (backend) {
        case BackendType::APT:
            // Debian red
            gdk_rgba_parse(color, "#A80030");
            break;
        case BackendType::SNAP:
            // Ubuntu orange
            gdk_rgba_parse(color, "#E95420");
            break;
        case BackendType::FLATPAK:
            // Flathub blue
            gdk_rgba_parse(color, "#4A90D9");
            break;
        default:
            gdk_rgba_parse(color, "#888888");
            break;
    }
}

// ============================================================================
// RGUnifiedPkgList - GtkTreeModel Implementation
// ============================================================================

static void rg_unified_pkg_list_tree_model_init(GtkTreeModelIface* iface);
static void rg_unified_pkg_list_sortable_init(GtkTreeSortableIface* iface);

G_DEFINE_TYPE_WITH_CODE(RGUnifiedPkgList, rg_unified_pkg_list, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL, rg_unified_pkg_list_tree_model_init)
    G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_SORTABLE, rg_unified_pkg_list_sortable_init))

static void rg_unified_pkg_list_init(RGUnifiedPkgList* list)
{
    list->packages = nullptr;
    list->sort_column_id = GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID;
    list->sort_order = GTK_SORT_ASCENDING;
    list->filter = BackendFilter::All();
    list->manager = nullptr;
}

static void rg_unified_pkg_list_class_init(RGUnifiedPkgListClass* klass)
{
    // No special class initialization needed
}

// TreeModel interface: Get column count
static gint rg_unified_pkg_list_get_n_columns(GtkTreeModel* model)
{
    return UPKG_NUM_COLS;
}

// TreeModel interface: Get column type
static GType rg_unified_pkg_list_get_column_type(GtkTreeModel* model, gint column)
{
    switch (column) {
        case UPKG_COL_SUPPORTED_ICON:
            return GDK_TYPE_PIXBUF;
        case UPKG_COL_PACKAGE_NAME:
        case UPKG_COL_BACKEND_BADGE:
        case UPKG_COL_INSTALLED_VERSION:
        case UPKG_COL_AVAILABLE_VERSION:
        case UPKG_COL_DESCRIPTION:
        case UPKG_COL_SIZE:
        case UPKG_COL_STATUS:
            return G_TYPE_STRING;
        case UPKG_COL_PACKAGE_PTR:
            return G_TYPE_POINTER;
        case UPKG_COL_BACKEND_TYPE:
            return G_TYPE_INT;
        default:
            return G_TYPE_INVALID;
    }
}

// TreeModel interface: Get iterator from path
// NOTE: The path index is the VISIBLE row number, not the raw vector index.
// We must translate from visible row number to raw vector index.
static gboolean rg_unified_pkg_list_get_iter(GtkTreeModel* model,
                                              GtkTreeIter* iter,
                                              GtkTreePath* path)
{
    RGUnifiedPkgList* list = RG_UNIFIED_PKG_LIST(model);

    if (!list->packages) return FALSE;

    gint* indices = gtk_tree_path_get_indices(path);
    gint depth = gtk_tree_path_get_depth(path);

    if (depth != 1) return FALSE;

    gint visible_row = indices[0];
    if (visible_row < 0) return FALSE;

    // Find the nth visible package (same logic as iter_nth_child)
    gint count = 0;
    for (gint i = 0; i < (gint)list->packages->size(); i++) {
        const PackageInfo& pkg = (*list->packages)[i];
        if (list->filter.includes(pkg.backend)) {
            if (count == visible_row) {
                iter->stamp = 1;
                iter->user_data = GINT_TO_POINTER(i);
                iter->user_data2 = nullptr;
                iter->user_data3 = nullptr;
                return TRUE;
            }
            count++;
        }
    }

    return FALSE;
}

// TreeModel interface: Get path from iterator
// NOTE: The iterator stores the raw vector index in user_data.
// The path must be the VISIBLE row number, not the raw index.
static GtkTreePath* rg_unified_pkg_list_get_path(GtkTreeModel* model,
                                                  GtkTreeIter* iter)
{
    RGUnifiedPkgList* list = RG_UNIFIED_PKG_LIST(model);
    gint raw_idx = GPOINTER_TO_INT(iter->user_data);

    // Count how many visible packages come before this one
    gint visible_row = 0;
    for (gint i = 0; i < raw_idx && i < (gint)list->packages->size(); i++) {
        const PackageInfo& pkg = (*list->packages)[i];
        if (list->filter.includes(pkg.backend)) {
            visible_row++;
        }
    }

    GtkTreePath* path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, visible_row);
    return path;
}

// TreeModel interface: Get value at iterator/column
static void rg_unified_pkg_list_get_value(GtkTreeModel* model,
                                           GtkTreeIter* iter,
                                           gint column,
                                           GValue* value)
{
    RGUnifiedPkgList* list = RG_UNIFIED_PKG_LIST(model);

    if (!list->packages) return;

    gint idx = GPOINTER_TO_INT(iter->user_data);
    if (idx < 0 || idx >= (gint)list->packages->size()) return;

    const PackageInfo& pkg = (*list->packages)[idx];

    switch (column) {
        case UPKG_COL_SUPPORTED_ICON:
            g_value_init(value, GDK_TYPE_PIXBUF);
            // Load icon based on status
            {
                const char* iconName = get_status_icon_name(pkg.installStatus);
                GdkPixbuf* pixbuf = gtk_icon_theme_load_icon(
                    gtk_icon_theme_get_default(),
                    iconName, 16, GTK_ICON_LOOKUP_USE_BUILTIN, nullptr);
                if (pixbuf) {
                    g_value_take_object(value, pixbuf);
                } else {
                    // Fallback: set to NULL if icon not found
                    g_value_set_object(value, nullptr);
                }
            }
            break;

        case UPKG_COL_PACKAGE_NAME:
            g_value_init(value, G_TYPE_STRING);
            g_value_set_string(value, pkg.name.c_str());
            break;

        case UPKG_COL_BACKEND_BADGE:
            g_value_init(value, G_TYPE_STRING);
            g_value_set_string(value, backendTypeToBadge(pkg.backend));
            break;

        case UPKG_COL_INSTALLED_VERSION:
            g_value_init(value, G_TYPE_STRING);
            g_value_set_string(value, pkg.installedVersion.c_str());
            break;

        case UPKG_COL_AVAILABLE_VERSION:
            g_value_init(value, G_TYPE_STRING);
            g_value_set_string(value, pkg.version.c_str());
            break;

        case UPKG_COL_DESCRIPTION:
            g_value_init(value, G_TYPE_STRING);
            g_value_set_string(value, pkg.summary.c_str());
            break;

        case UPKG_COL_SIZE:
            g_value_init(value, G_TYPE_STRING);
            g_value_set_string(value, format_size(pkg.downloadSize).c_str());
            break;

        case UPKG_COL_STATUS:
            g_value_init(value, G_TYPE_STRING);
            g_value_set_string(value, installStatusToString(pkg.installStatus));
            break;

        case UPKG_COL_PACKAGE_PTR:
            g_value_init(value, G_TYPE_POINTER);
            // Return pointer to the actual element in the vector, not to local reference
            // The vector is owned by the model and remains valid while the model exists
            g_value_set_pointer(value, (gpointer)&((*list->packages)[idx]));
            break;

        case UPKG_COL_BACKEND_TYPE:
            g_value_init(value, G_TYPE_INT);
            g_value_set_int(value, static_cast<int>(pkg.backend));
            break;
    }
}

// TreeModel interface: Move to next sibling
static gboolean rg_unified_pkg_list_iter_next(GtkTreeModel* model,
                                               GtkTreeIter* iter)
{
    RGUnifiedPkgList* list = RG_UNIFIED_PKG_LIST(model);

    if (!list->packages) return FALSE;

    gint idx = GPOINTER_TO_INT(iter->user_data);
    idx++;

    // Skip filtered packages
    while (idx < (gint)list->packages->size()) {
        const PackageInfo& pkg = (*list->packages)[idx];
        if (list->filter.includes(pkg.backend)) {
            iter->user_data = GINT_TO_POINTER(idx);
            return TRUE;
        }
        idx++;
    }

    return FALSE;
}

// TreeModel interface: Get first child
static gboolean rg_unified_pkg_list_iter_children(GtkTreeModel* model,
                                                   GtkTreeIter* iter,
                                                   GtkTreeIter* parent)
{
    RGUnifiedPkgList* list = RG_UNIFIED_PKG_LIST(model);

    if (parent != nullptr) return FALSE;  // Flat list, no children
    if (!list->packages || list->packages->empty()) return FALSE;

    // Find first non-filtered package
    for (gint i = 0; i < (gint)list->packages->size(); i++) {
        const PackageInfo& pkg = (*list->packages)[i];
        if (list->filter.includes(pkg.backend)) {
            iter->stamp = 1;
            iter->user_data = GINT_TO_POINTER(i);
            return TRUE;
        }
    }

    return FALSE;
}

// TreeModel interface: Check if has children
static gboolean rg_unified_pkg_list_iter_has_child(GtkTreeModel* model,
                                                    GtkTreeIter* iter)
{
    return FALSE;  // Flat list
}

// TreeModel interface: Get child count
static gint rg_unified_pkg_list_iter_n_children(GtkTreeModel* model,
                                                 GtkTreeIter* iter)
{
    RGUnifiedPkgList* list = RG_UNIFIED_PKG_LIST(model);

    if (iter != nullptr) return 0;  // No children for rows
    if (!list->packages) return 0;

    // Count non-filtered packages
    gint count = 0;
    for (const auto& pkg : *list->packages) {
        if (list->filter.includes(pkg.backend)) {
            count++;
        }
    }

    return count;
}

// TreeModel interface: Get nth child
static gboolean rg_unified_pkg_list_iter_nth_child(GtkTreeModel* model,
                                                    GtkTreeIter* iter,
                                                    GtkTreeIter* parent,
                                                    gint n)
{
    RGUnifiedPkgList* list = RG_UNIFIED_PKG_LIST(model);

    if (parent != nullptr) return FALSE;
    if (!list->packages) return FALSE;

    // Find nth non-filtered package
    gint count = 0;
    for (gint i = 0; i < (gint)list->packages->size(); i++) {
        const PackageInfo& pkg = (*list->packages)[i];
        if (list->filter.includes(pkg.backend)) {
            if (count == n) {
                iter->stamp = 1;
                iter->user_data = GINT_TO_POINTER(i);
                return TRUE;
            }
            count++;
        }
    }

    return FALSE;
}

// TreeModel interface: Get parent
static gboolean rg_unified_pkg_list_iter_parent(GtkTreeModel* model,
                                                 GtkTreeIter* iter,
                                                 GtkTreeIter* child)
{
    return FALSE;  // Flat list, no parents
}

// TreeModel interface: Get flags
static GtkTreeModelFlags rg_unified_pkg_list_get_flags(GtkTreeModel* model)
{
    return GTK_TREE_MODEL_LIST_ONLY;
}

static void rg_unified_pkg_list_tree_model_init(GtkTreeModelIface* iface)
{
    iface->get_flags = rg_unified_pkg_list_get_flags;
    iface->get_n_columns = rg_unified_pkg_list_get_n_columns;
    iface->get_column_type = rg_unified_pkg_list_get_column_type;
    iface->get_iter = rg_unified_pkg_list_get_iter;
    iface->get_path = rg_unified_pkg_list_get_path;
    iface->get_value = rg_unified_pkg_list_get_value;
    iface->iter_next = rg_unified_pkg_list_iter_next;
    iface->iter_children = rg_unified_pkg_list_iter_children;
    iface->iter_has_child = rg_unified_pkg_list_iter_has_child;
    iface->iter_n_children = rg_unified_pkg_list_iter_n_children;
    iface->iter_nth_child = rg_unified_pkg_list_iter_nth_child;
    iface->iter_parent = rg_unified_pkg_list_iter_parent;
}

// Sortable interface: Get sort column
static gboolean rg_unified_pkg_list_get_sort_column_id(GtkTreeSortable* sortable,
                                                        gint* sort_column_id,
                                                        GtkSortType* order)
{
    RGUnifiedPkgList* list = RG_UNIFIED_PKG_LIST(sortable);

    if (sort_column_id) *sort_column_id = list->sort_column_id;
    if (order) *order = list->sort_order;

    return (list->sort_column_id != GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID);
}

// Sortable interface: Set sort column
static void rg_unified_pkg_list_set_sort_column_id(GtkTreeSortable* sortable,
                                                    gint sort_column_id,
                                                    GtkSortType order)
{
    RGUnifiedPkgList* list = RG_UNIFIED_PKG_LIST(sortable);

    list->sort_column_id = sort_column_id;
    list->sort_order = order;

    // TODO: Actually sort the data
    // For now, emit signal so view updates
    gtk_tree_sortable_sort_column_changed(sortable);
}

// Sortable interface: Check if sortable
static gboolean rg_unified_pkg_list_has_default_sort_func(GtkTreeSortable* sortable)
{
    return FALSE;
}

static void rg_unified_pkg_list_sortable_init(GtkTreeSortableIface* iface)
{
    iface->get_sort_column_id = rg_unified_pkg_list_get_sort_column_id;
    iface->set_sort_column_id = rg_unified_pkg_list_set_sort_column_id;
    iface->has_default_sort_func = rg_unified_pkg_list_has_default_sort_func;
}

// Public API

RGUnifiedPkgList* rg_unified_pkg_list_new(BackendManager* manager)
{
    RGUnifiedPkgList* list = (RGUnifiedPkgList*)g_object_new(RG_TYPE_UNIFIED_PKG_LIST, nullptr);
    list->manager = manager;
    return list;
}

void rg_unified_pkg_list_set_packages(RGUnifiedPkgList* list, vector<PackageInfo>* packages)
{
    GtkTreeModel* model = GTK_TREE_MODEL(list);

    // If we had old data, emit row-deleted for all old rows first
    if (list->packages && !list->packages->empty()) {
        // Count old visible rows (respecting filter)
        gint oldCount = 0;
        for (const auto& pkg : *list->packages) {
            if (list->filter.includes(pkg.backend)) {
                oldCount++;
            }
        }
        // Emit deletion signals in reverse order (required by GTK)
        for (gint i = oldCount - 1; i >= 0; i--) {
            GtkTreePath* path = gtk_tree_path_new_from_indices(i, -1);
            gtk_tree_model_row_deleted(model, path);
            gtk_tree_path_free(path);
        }
    }

    // Set new packages
    list->packages = packages;

    // Emit row-inserted for all new visible rows
    if (list->packages && !list->packages->empty()) {
        gint visibleIdx = 0;
        for (gint i = 0; i < (gint)list->packages->size(); i++) {
            const PackageInfo& pkg = (*list->packages)[i];
            if (list->filter.includes(pkg.backend)) {
                GtkTreePath* path = gtk_tree_path_new_from_indices(visibleIdx, -1);
                GtkTreeIter iter;
                iter.stamp = 1;
                iter.user_data = GINT_TO_POINTER(i);  // Store actual vector index
                iter.user_data2 = nullptr;
                iter.user_data3 = nullptr;

                gtk_tree_model_row_inserted(model, path, &iter);
                gtk_tree_path_free(path);
                visibleIdx++;
            }
        }
    }
}

void rg_unified_pkg_list_set_filter(RGUnifiedPkgList* list, const BackendFilter& filter)
{
    if (!list->packages || list->packages->empty()) {
        list->filter = filter;
        return;
    }

    GtkTreeModel* model = GTK_TREE_MODEL(list);
    BackendFilter oldFilter = list->filter;

    // Calculate old visible count
    gint oldCount = 0;
    for (const auto& pkg : *list->packages) {
        if (oldFilter.includes(pkg.backend)) {
            oldCount++;
        }
    }

    // Emit deletion signals for old visible rows (in reverse order)
    for (gint i = oldCount - 1; i >= 0; i--) {
        GtkTreePath* path = gtk_tree_path_new_from_indices(i, -1);
        gtk_tree_model_row_deleted(model, path);
        gtk_tree_path_free(path);
    }

    // Apply new filter
    list->filter = filter;

    // Emit insertion signals for new visible rows
    gint visibleIdx = 0;
    for (gint i = 0; i < (gint)list->packages->size(); i++) {
        const PackageInfo& pkg = (*list->packages)[i];
        if (filter.includes(pkg.backend)) {
            GtkTreePath* path = gtk_tree_path_new_from_indices(visibleIdx, -1);
            GtkTreeIter iter;
            iter.stamp = 1;
            iter.user_data = GINT_TO_POINTER(i);
            iter.user_data2 = nullptr;
            iter.user_data3 = nullptr;

            gtk_tree_model_row_inserted(model, path, &iter);
            gtk_tree_path_free(path);
            visibleIdx++;
        }
    }
}

void rg_unified_pkg_list_refresh(RGUnifiedPkgList* list)
{
    if (!list->packages || list->packages->empty()) return;

    GtkTreeModel* model = GTK_TREE_MODEL(list);

    // Emit row-changed for all visible rows to refresh the display
    gint visibleIdx = 0;
    for (gint i = 0; i < (gint)list->packages->size(); i++) {
        const PackageInfo& pkg = (*list->packages)[i];
        if (list->filter.includes(pkg.backend)) {
            GtkTreePath* path = gtk_tree_path_new_from_indices(visibleIdx, -1);
            GtkTreeIter iter;
            iter.stamp = 1;
            iter.user_data = GINT_TO_POINTER(i);
            iter.user_data2 = nullptr;
            iter.user_data3 = nullptr;

            gtk_tree_model_row_changed(model, path, &iter);
            gtk_tree_path_free(path);
            visibleIdx++;
        }
    }
}

// ============================================================================
// RGBackendFilterBar
// ============================================================================

RGBackendFilterBar::RGBackendFilterBar(BackendManager* manager)
    : _manager(manager)
{
    _container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(_container, 5);
    gtk_widget_set_margin_end(_container, 5);

    // Create label
    GtkWidget* label = gtk_label_new("Show:");
    gtk_box_pack_start(GTK_BOX(_container), label, FALSE, FALSE, 0);

    // APT checkbox
    _aptCheck = gtk_check_button_new_with_label("APT");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(_aptCheck), TRUE);
    g_signal_connect(_aptCheck, "toggled", G_CALLBACK(onToggled), this);
    gtk_box_pack_start(GTK_BOX(_container), _aptCheck, FALSE, FALSE, 0);

    // Snap checkbox
    _snapCheck = gtk_check_button_new_with_label("Snap");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(_snapCheck), TRUE);
    g_signal_connect(_snapCheck, "toggled", G_CALLBACK(onToggled), this);
    gtk_box_pack_start(GTK_BOX(_container), _snapCheck, FALSE, FALSE, 0);

    // Flatpak checkbox
    _flatpakCheck = gtk_check_button_new_with_label("Flatpak");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(_flatpakCheck), TRUE);
    g_signal_connect(_flatpakCheck, "toggled", G_CALLBACK(onToggled), this);
    gtk_box_pack_start(GTK_BOX(_container), _flatpakCheck, FALSE, FALSE, 0);

    updateAvailability();

    gtk_widget_show_all(_container);
}

RGBackendFilterBar::~RGBackendFilterBar()
{
    // GTK will handle widget destruction
}

BackendFilter RGBackendFilterBar::getFilter() const
{
    BackendFilter filter;
    filter.includeApt = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(_aptCheck));
    filter.includeSnap = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(_snapCheck));
    filter.includeFlatpak = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(_flatpakCheck));
    return filter;
}

void RGBackendFilterBar::setFilter(const BackendFilter& filter)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(_aptCheck), filter.includeApt);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(_snapCheck), filter.includeSnap);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(_flatpakCheck), filter.includeFlatpak);
}

void RGBackendFilterBar::updateAvailability()
{
    if (!_manager) return;

    auto statuses = _manager->getBackendStatuses();

    for (const auto& status : statuses) {
        GtkWidget* check = nullptr;
        switch (status.type) {
            case BackendType::APT: check = _aptCheck; break;
            case BackendType::SNAP: check = _snapCheck; break;
            case BackendType::FLATPAK: check = _flatpakCheck; break;
            default: continue;
        }

        gtk_widget_set_sensitive(check, status.available);

        if (!status.available && !status.unavailableReason.empty()) {
            gtk_widget_set_tooltip_text(check, status.unavailableReason.c_str());
        } else {
            gtk_widget_set_tooltip_text(check, nullptr);
        }
    }
}

void RGBackendFilterBar::onToggled(GtkToggleButton* button, gpointer userData)
{
    RGBackendFilterBar* self = static_cast<RGBackendFilterBar*>(userData);
    self->handleToggle();
}

void RGBackendFilterBar::handleToggle()
{
    if (_changedCallback) {
        _changedCallback(getFilter());
    }
}

// ============================================================================
// RGBackendStatusBar
// ============================================================================

RGBackendStatusBar::RGBackendStatusBar(BackendManager* manager)
    : _manager(manager)
{
    _container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    createStatusIcon(&_aptStatus, "package-x-generic", "APT");
    createStatusIcon(&_snapStatus, "package-x-generic", "Snap");
    createStatusIcon(&_flatpakStatus, "package-x-generic", "Flatpak");

    gtk_box_pack_start(GTK_BOX(_container), _aptStatus, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(_container), _snapStatus, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(_container), _flatpakStatus, FALSE, FALSE, 0);

    refresh();

    gtk_widget_show_all(_container);
}

RGBackendStatusBar::~RGBackendStatusBar()
{
}

void RGBackendStatusBar::createStatusIcon(GtkWidget** widget, const char* iconName, const char* label)
{
    *widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);

    GtkWidget* icon = gtk_image_new_from_icon_name(iconName, GTK_ICON_SIZE_SMALL_TOOLBAR);
    GtkWidget* text = gtk_label_new(label);

    gtk_box_pack_start(GTK_BOX(*widget), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(*widget), text, FALSE, FALSE, 0);
}

void RGBackendStatusBar::updateStatusIcon(GtkWidget* widget, bool available, const string& tooltip)
{
    gtk_widget_set_sensitive(widget, available);
    if (!tooltip.empty()) {
        gtk_widget_set_tooltip_text(widget, tooltip.c_str());
    }
}

void RGBackendStatusBar::refresh()
{
    if (!_manager) return;

    auto statuses = _manager->getBackendStatuses();

    for (const auto& status : statuses) {
        GtkWidget* widget = nullptr;
        switch (status.type) {
            case BackendType::APT: widget = _aptStatus; break;
            case BackendType::SNAP: widget = _snapStatus; break;
            case BackendType::FLATPAK: widget = _flatpakStatus; break;
            default: continue;
        }

        string tooltip = status.name + " " + status.version;
        if (!status.available) {
            tooltip = status.unavailableReason;
        }

        updateStatusIcon(widget, status.available, tooltip);
    }
}

// vim:ts=4:sw=4:et
