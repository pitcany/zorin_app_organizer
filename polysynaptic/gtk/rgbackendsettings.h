/* rgbackendsettings.h - Backend configuration dialog for PolySynaptic
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This dialog allows users to configure backend settings including:
 *   - Enable/disable each backend (APT, Snap, Flatpak)
 *   - Configure Flatpak default remote
 *   - Configure Flatpak installation scope (user/system)
 *   - View backend status and versions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef _RGBACKENDSETTINGS_H_
#define _RGBACKENDSETTINGS_H_

#include "rgwindow.h"
#include "backendmanager.h"

using namespace PolySynaptic;

/**
 * RGBackendSettingsWindow - Backend configuration dialog
 *
 * This dialog provides a settings interface for managing the three
 * package backends (APT, Snap, Flatpak). Built programmatically (no .ui file).
 */
class RGBackendSettingsWindow : public RGWindow {
public:
    RGBackendSettingsWindow(RGWindow* parent, BackendManager* manager);
    virtual ~RGBackendSettingsWindow();

    // Show the dialog modally
    void run();

private:
    BackendManager* _manager;

    // Widgets
    GtkWidget* _aptEnabledCheck;
    GtkWidget* _aptVersionLabel;
    GtkWidget* _aptStatusLabel;

    GtkWidget* _snapEnabledCheck;
    GtkWidget* _snapVersionLabel;
    GtkWidget* _snapStatusLabel;

    GtkWidget* _flatpakEnabledCheck;
    GtkWidget* _flatpakVersionLabel;
    GtkWidget* _flatpakStatusLabel;
    GtkWidget* _flatpakRemoteCombo;
    GtkWidget* _flatpakScopeCombo;

    GtkWidget* _applyButton;
    GtkWidget* _closeButton;

    // Initialization
    void createWidgets();
    void loadCurrentSettings();
    void populateFlatpakRemotes();
    void updateStatus();

    // Callbacks
    static void onApply(GtkButton* button, gpointer userData);
    static void onClose(GtkButton* button, gpointer userData);
    static void onRefresh(GtkButton* button, gpointer userData);
    static void onAddFlathub(GtkButton* button, gpointer userData);

    void applySettings();
    void refresh();
    void addFlathub();
};

#endif // _RGBACKENDSETTINGS_H_

// vim:ts=4:sw=4:et
