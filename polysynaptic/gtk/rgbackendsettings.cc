/* rgbackendsettings.cc - Backend configuration dialog implementation
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include "rgbackendsettings.h"

RGBackendSettingsWindow::RGBackendSettingsWindow(RGWindow* parent, BackendManager* manager)
    : RGGtkBuilderWindow(parent, "backend_settings")
    , _manager(manager)
{
    createWidgets();
    loadCurrentSettings();
}

RGBackendSettingsWindow::~RGBackendSettingsWindow()
{
}

void RGBackendSettingsWindow::createWidgets()
{
    // Create main dialog window
    _win = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(_win), "PolySynaptic - Backend Settings");
    gtk_window_set_default_size(GTK_WINDOW(_win), 500, 400);
    gtk_window_set_modal(GTK_WINDOW(_win), TRUE);

    GtkWidget* contentArea = gtk_dialog_get_content_area(GTK_DIALOG(_win));
    gtk_container_set_border_width(GTK_CONTAINER(contentArea), 10);

    // Create notebook for tabs
    GtkWidget* notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(contentArea), notebook, TRUE, TRUE, 0);

    // ========================================================================
    // APT Tab
    // ========================================================================
    GtkWidget* aptPage = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(aptPage), 10);

    // Enabled checkbox
    _aptEnabledCheck = gtk_check_button_new_with_label("Enable APT backend");
    gtk_box_pack_start(GTK_BOX(aptPage), _aptEnabledCheck, FALSE, FALSE, 0);

    // Status info
    GtkWidget* aptInfoGrid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(aptInfoGrid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(aptInfoGrid), 10);

    gtk_grid_attach(GTK_GRID(aptInfoGrid), gtk_label_new("Version:"), 0, 0, 1, 1);
    _aptVersionLabel = gtk_label_new("-");
    gtk_label_set_xalign(GTK_LABEL(_aptVersionLabel), 0);
    gtk_grid_attach(GTK_GRID(aptInfoGrid), _aptVersionLabel, 1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(aptInfoGrid), gtk_label_new("Status:"), 0, 1, 1, 1);
    _aptStatusLabel = gtk_label_new("-");
    gtk_label_set_xalign(GTK_LABEL(_aptStatusLabel), 0);
    gtk_grid_attach(GTK_GRID(aptInfoGrid), _aptStatusLabel, 1, 1, 1, 1);

    gtk_box_pack_start(GTK_BOX(aptPage), aptInfoGrid, FALSE, FALSE, 0);

    // Description
    GtkWidget* aptDesc = gtk_label_new(
        "APT (Advanced Package Tool) is the primary package manager for "
        "Debian-based distributions like Ubuntu and Zorin OS. It manages "
        ".deb packages from configured repositories.");
    gtk_label_set_line_wrap(GTK_LABEL(aptDesc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(aptDesc), 0);
    gtk_box_pack_start(GTK_BOX(aptPage), aptDesc, FALSE, FALSE, 10);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), aptPage,
                             gtk_label_new("APT"));

    // ========================================================================
    // Snap Tab
    // ========================================================================
    GtkWidget* snapPage = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(snapPage), 10);

    _snapEnabledCheck = gtk_check_button_new_with_label("Enable Snap backend");
    gtk_box_pack_start(GTK_BOX(snapPage), _snapEnabledCheck, FALSE, FALSE, 0);

    GtkWidget* snapInfoGrid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(snapInfoGrid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(snapInfoGrid), 10);

    gtk_grid_attach(GTK_GRID(snapInfoGrid), gtk_label_new("Version:"), 0, 0, 1, 1);
    _snapVersionLabel = gtk_label_new("-");
    gtk_label_set_xalign(GTK_LABEL(_snapVersionLabel), 0);
    gtk_grid_attach(GTK_GRID(snapInfoGrid), _snapVersionLabel, 1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(snapInfoGrid), gtk_label_new("Status:"), 0, 1, 1, 1);
    _snapStatusLabel = gtk_label_new("-");
    gtk_label_set_xalign(GTK_LABEL(_snapStatusLabel), 0);
    gtk_grid_attach(GTK_GRID(snapInfoGrid), _snapStatusLabel, 1, 1, 1, 1);

    gtk_box_pack_start(GTK_BOX(snapPage), snapInfoGrid, FALSE, FALSE, 0);

    GtkWidget* snapDesc = gtk_label_new(
        "Snap packages are containerized applications from the Snap Store. "
        "They include all dependencies and run in a sandboxed environment. "
        "Snaps update automatically in the background.");
    gtk_label_set_line_wrap(GTK_LABEL(snapDesc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(snapDesc), 0);
    gtk_box_pack_start(GTK_BOX(snapPage), snapDesc, FALSE, FALSE, 10);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), snapPage,
                             gtk_label_new("Snap"));

    // ========================================================================
    // Flatpak Tab
    // ========================================================================
    GtkWidget* flatpakPage = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(flatpakPage), 10);

    _flatpakEnabledCheck = gtk_check_button_new_with_label("Enable Flatpak backend");
    gtk_box_pack_start(GTK_BOX(flatpakPage), _flatpakEnabledCheck, FALSE, FALSE, 0);

    GtkWidget* flatpakInfoGrid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(flatpakInfoGrid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(flatpakInfoGrid), 10);

    gtk_grid_attach(GTK_GRID(flatpakInfoGrid), gtk_label_new("Version:"), 0, 0, 1, 1);
    _flatpakVersionLabel = gtk_label_new("-");
    gtk_label_set_xalign(GTK_LABEL(_flatpakVersionLabel), 0);
    gtk_grid_attach(GTK_GRID(flatpakInfoGrid), _flatpakVersionLabel, 1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(flatpakInfoGrid), gtk_label_new("Status:"), 0, 1, 1, 1);
    _flatpakStatusLabel = gtk_label_new("-");
    gtk_label_set_xalign(GTK_LABEL(_flatpakStatusLabel), 0);
    gtk_grid_attach(GTK_GRID(flatpakInfoGrid), _flatpakStatusLabel, 1, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(flatpakInfoGrid), gtk_label_new("Default remote:"), 0, 2, 1, 1);
    _flatpakRemoteCombo = gtk_combo_box_text_new();
    gtk_grid_attach(GTK_GRID(flatpakInfoGrid), _flatpakRemoteCombo, 1, 2, 1, 1);

    gtk_grid_attach(GTK_GRID(flatpakInfoGrid), gtk_label_new("Installation scope:"), 0, 3, 1, 1);
    _flatpakScopeCombo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(_flatpakScopeCombo), "User (recommended)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(_flatpakScopeCombo), "System (requires root)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(_flatpakScopeCombo), 0);
    gtk_grid_attach(GTK_GRID(flatpakInfoGrid), _flatpakScopeCombo, 1, 3, 1, 1);

    gtk_box_pack_start(GTK_BOX(flatpakPage), flatpakInfoGrid, FALSE, FALSE, 0);

    // Add Flathub button
    GtkWidget* addFlathubBtn = gtk_button_new_with_label("Add Flathub Repository");
    g_signal_connect(addFlathubBtn, "clicked", G_CALLBACK(onAddFlathub), this);
    gtk_box_pack_start(GTK_BOX(flatpakPage), addFlathubBtn, FALSE, FALSE, 5);

    GtkWidget* flatpakDesc = gtk_label_new(
        "Flatpak packages are sandboxed applications from repositories like "
        "Flathub. They provide a secure, distribution-agnostic way to install "
        "desktop applications with their dependencies.");
    gtk_label_set_line_wrap(GTK_LABEL(flatpakDesc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(flatpakDesc), 0);
    gtk_box_pack_start(GTK_BOX(flatpakPage), flatpakDesc, FALSE, FALSE, 10);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), flatpakPage,
                             gtk_label_new("Flatpak"));

    // ========================================================================
    // Dialog buttons
    // ========================================================================
    GtkWidget* buttonBox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(buttonBox), 10);

    GtkWidget* refreshBtn = gtk_button_new_with_label("Refresh");
    g_signal_connect(refreshBtn, "clicked", G_CALLBACK(onRefresh), this);
    gtk_container_add(GTK_CONTAINER(buttonBox), refreshBtn);

    _applyButton = gtk_button_new_with_label("Apply");
    g_signal_connect(_applyButton, "clicked", G_CALLBACK(onApply), this);
    gtk_container_add(GTK_CONTAINER(buttonBox), _applyButton);

    _closeButton = gtk_button_new_with_label("Close");
    g_signal_connect(_closeButton, "clicked", G_CALLBACK(onClose), this);
    gtk_container_add(GTK_CONTAINER(buttonBox), _closeButton);

    gtk_box_pack_end(GTK_BOX(contentArea), buttonBox, FALSE, FALSE, 10);

    gtk_widget_show_all(_win);
}

void RGBackendSettingsWindow::loadCurrentSettings()
{
    if (!_manager) return;

    auto statuses = _manager->getBackendStatuses();

    for (const auto& status : statuses) {
        switch (status.type) {
            case BackendType::APT:
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(_aptEnabledCheck),
                                            status.enabled);
                gtk_label_set_text(GTK_LABEL(_aptVersionLabel),
                                  status.version.c_str());
                gtk_label_set_text(GTK_LABEL(_aptStatusLabel),
                                  status.available ? "Available" : status.unavailableReason.c_str());
                gtk_widget_set_sensitive(_aptEnabledCheck, status.available);
                break;

            case BackendType::SNAP:
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(_snapEnabledCheck),
                                            status.enabled);
                gtk_label_set_text(GTK_LABEL(_snapVersionLabel),
                                  status.version.c_str());
                gtk_label_set_text(GTK_LABEL(_snapStatusLabel),
                                  status.available ? "Available" : status.unavailableReason.c_str());
                gtk_widget_set_sensitive(_snapEnabledCheck, status.available);
                break;

            case BackendType::FLATPAK:
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(_flatpakEnabledCheck),
                                            status.enabled);
                gtk_label_set_text(GTK_LABEL(_flatpakVersionLabel),
                                  status.version.c_str());
                gtk_label_set_text(GTK_LABEL(_flatpakStatusLabel),
                                  status.available ? "Available" : status.unavailableReason.c_str());
                gtk_widget_set_sensitive(_flatpakEnabledCheck, status.available);
                break;

            default:
                break;
        }
    }

    populateFlatpakRemotes();
}

void RGBackendSettingsWindow::populateFlatpakRemotes()
{
    // Clear existing entries
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(_flatpakRemoteCombo));

    if (!_manager) return;

    auto* flatpak = _manager->getFlatpakBackend();
    if (!flatpak || !flatpak->isAvailable()) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(_flatpakRemoteCombo),
                                       "(no remotes configured)");
        gtk_combo_box_set_active(GTK_COMBO_BOX(_flatpakRemoteCombo), 0);
        gtk_widget_set_sensitive(_flatpakRemoteCombo, FALSE);
        return;
    }

    auto remotes = flatpak->getRepositories();
    if (remotes.empty()) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(_flatpakRemoteCombo),
                                       "(no remotes configured)");
        gtk_combo_box_set_active(GTK_COMBO_BOX(_flatpakRemoteCombo), 0);
        gtk_widget_set_sensitive(_flatpakRemoteCombo, FALSE);
        return;
    }

    string defaultRemote = flatpak->getDefaultRemote();
    int activeIdx = 0;
    int idx = 0;

    for (const auto& remote : remotes) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(_flatpakRemoteCombo),
                                       remote.c_str());
        if (remote == defaultRemote) {
            activeIdx = idx;
        }
        idx++;
    }

    gtk_combo_box_set_active(GTK_COMBO_BOX(_flatpakRemoteCombo), activeIdx);
    gtk_widget_set_sensitive(_flatpakRemoteCombo, TRUE);
}

void RGBackendSettingsWindow::updateStatus()
{
    loadCurrentSettings();
}

void RGBackendSettingsWindow::run()
{
    gtk_dialog_run(GTK_DIALOG(_win));
    gtk_widget_hide(_win);
}

void RGBackendSettingsWindow::onApply(GtkButton* button, gpointer userData)
{
    RGBackendSettingsWindow* self = static_cast<RGBackendSettingsWindow*>(userData);
    self->applySettings();
}

void RGBackendSettingsWindow::onClose(GtkButton* button, gpointer userData)
{
    RGBackendSettingsWindow* self = static_cast<RGBackendSettingsWindow*>(userData);
    gtk_widget_hide(self->_win);
}

void RGBackendSettingsWindow::onRefresh(GtkButton* button, gpointer userData)
{
    RGBackendSettingsWindow* self = static_cast<RGBackendSettingsWindow*>(userData);
    self->refresh();
}

void RGBackendSettingsWindow::onAddFlathub(GtkButton* button, gpointer userData)
{
    RGBackendSettingsWindow* self = static_cast<RGBackendSettingsWindow*>(userData);
    self->addFlathub();
}

void RGBackendSettingsWindow::applySettings()
{
    if (!_manager) return;

    // Apply enable/disable settings
    _manager->setBackendEnabled(BackendType::APT,
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(_aptEnabledCheck)));
    _manager->setBackendEnabled(BackendType::SNAP,
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(_snapEnabledCheck)));
    _manager->setBackendEnabled(BackendType::FLATPAK,
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(_flatpakEnabledCheck)));

    // Apply Flatpak settings
    auto* flatpak = _manager->getFlatpakBackend();
    if (flatpak && flatpak->isAvailable()) {
        // Set default remote
        gchar* remote = gtk_combo_box_text_get_active_text(
            GTK_COMBO_BOX_TEXT(_flatpakRemoteCombo));
        if (remote) {
            flatpak->setDefaultRemote(remote);
            g_free(remote);
        }

        // Set scope
        int scopeIdx = gtk_combo_box_get_active(GTK_COMBO_BOX(_flatpakScopeCombo));
        flatpak->setDefaultScope(scopeIdx == 0 ?
            FlatpakBackend::Scope::USER : FlatpakBackend::Scope::SYSTEM);
    }

    // Show confirmation
    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_WINDOW(_win),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_OK,
        "Settings applied successfully.");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void RGBackendSettingsWindow::refresh()
{
    if (_manager) {
        _manager->refreshBackendDetection();
    }
    updateStatus();
}

void RGBackendSettingsWindow::addFlathub()
{
    if (!_manager) return;

    auto* flatpak = _manager->getFlatpakBackend();
    if (!flatpak) {
        GtkWidget* dialog = gtk_message_dialog_new(
            GTK_WINDOW(_win),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "Flatpak is not available on this system.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    // Check if already configured
    if (flatpak->hasRemote("flathub")) {
        GtkWidget* dialog = gtk_message_dialog_new(
            GTK_WINDOW(_win),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_OK,
            "Flathub is already configured.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    // Add Flathub
    auto result = flatpak->addFlathub();

    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_WINDOW(_win),
        GTK_DIALOG_MODAL,
        result.success ? GTK_MESSAGE_INFO : GTK_MESSAGE_ERROR,
        GTK_BUTTONS_OK,
        "%s",
        result.success ? "Flathub added successfully." : result.message.c_str());
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    // Refresh the combo box
    populateFlatpakRemotes();
}

// vim:ts=4:sw=4:et
