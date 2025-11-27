/* rgsourcespane.h - Sources filter pane for PolySynaptic
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This file provides a GTK widget for filtering packages by source
 * (APT, Snap, Flatpak) with provider status indicators.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef _RGSOURCESPANE_H_
#define _RGSOURCESPANE_H_

#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <functional>
#include <map>

#include "packagesourceprovider.h"

namespace PolySynaptic {

/**
 * SourceItem - Represents a package source in the filter pane
 */
struct SourceItem {
    std::string id;
    std::string name;
    std::string iconName;
    bool enabled = true;
    bool available = true;
    int packageCount = 0;
    int installedCount = 0;
    std::string statusMessage;
};

/**
 * RGSourcesPane - GTK widget for filtering by package source
 *
 * This widget displays a list of available package sources with:
 *   - Checkboxes to enable/disable filtering
 *   - Status indicators (available, error, loading)
 *   - Package counts
 *   - Quick-filter buttons
 */
class RGSourcesPane {
public:
    RGSourcesPane();
    ~RGSourcesPane();

    /**
     * Get the main widget
     */
    GtkWidget* getWidget() { return _mainBox; }

    /**
     * Update source information
     */
    void setSources(const std::vector<SourceItem>& sources);

    /**
     * Update a single source's status
     */
    void updateSource(const std::string& id, const SourceItem& source);

    /**
     * Get enabled sources
     */
    std::vector<std::string> getEnabledSources() const;

    /**
     * Set source enabled state
     */
    void setSourceEnabled(const std::string& id, bool enabled);

    /**
     * Set all sources enabled/disabled
     */
    void setAllEnabled(bool enabled);

    /**
     * Filter change callback
     */
    using FilterChangedCallback = std::function<void(
        const std::vector<std::string>& enabledSources)>;

    void setFilterChangedCallback(FilterChangedCallback callback) {
        _filterChangedCallback = std::move(callback);
    }

    /**
     * Source settings callback (when user clicks settings button)
     */
    using SourceSettingsCallback = std::function<void(const std::string& sourceId)>;

    void setSourceSettingsCallback(SourceSettingsCallback callback) {
        _sourceSettingsCallback = std::move(callback);
    }

    /**
     * Refresh callback (when user clicks refresh)
     */
    using RefreshCallback = std::function<void(const std::string& sourceId)>;

    void setRefreshCallback(RefreshCallback callback) {
        _refreshCallback = std::move(callback);
    }

    /**
     * Set compact mode (less details, smaller size)
     */
    void setCompactMode(bool compact);

    /**
     * Show/hide package counts
     */
    void setShowCounts(bool show);

    /**
     * Set loading state for a source
     */
    void setSourceLoading(const std::string& id, bool loading);

    /**
     * Set error state for a source
     */
    void setSourceError(const std::string& id, const std::string& error);

private:
    // Widgets
    GtkWidget* _mainBox = nullptr;
    GtkWidget* _headerBox = nullptr;
    GtkWidget* _sourcesList = nullptr;
    GtkWidget* _quickFilterBox = nullptr;
    GtkWidget* _allButton = nullptr;
    GtkWidget* _noneButton = nullptr;

    // Source rows (id -> row widgets)
    struct SourceRow {
        GtkWidget* row;
        GtkWidget* checkBox;
        GtkWidget* icon;
        GtkWidget* nameLabel;
        GtkWidget* countLabel;
        GtkWidget* statusIcon;
        GtkWidget* settingsButton;
        GtkWidget* spinner;
        bool loading = false;
    };
    std::map<std::string, SourceRow> _sourceRows;

    // State
    std::vector<SourceItem> _sources;
    bool _compactMode = false;
    bool _showCounts = true;

    // Callbacks
    FilterChangedCallback _filterChangedCallback;
    SourceSettingsCallback _sourceSettingsCallback;
    RefreshCallback _refreshCallback;

    // Build UI
    void buildUI();
    void buildHeader();
    void buildSourcesList();
    void buildQuickFilters();

    // Create a source row
    GtkWidget* createSourceRow(const SourceItem& source);

    // Update row appearance
    void updateRowAppearance(const std::string& id);

    // Signal handlers
    static void onCheckToggled(GtkToggleButton* button, gpointer data);
    static void onSettingsClicked(GtkButton* button, gpointer data);
    static void onAllClicked(GtkButton* button, gpointer data);
    static void onNoneClicked(GtkButton* button, gpointer data);

    // Notify filter changed
    void notifyFilterChanged();

    // Get status icon name
    static const char* getStatusIcon(bool available, bool loading,
                                      const std::string& error);
};

/**
 * RGSourcesBadge - Small badge showing package source
 */
class RGSourcesBadge {
public:
    RGSourcesBadge(const std::string& sourceId);
    ~RGSourcesBadge();

    GtkWidget* getWidget() { return _eventBox; }

    void setSourceId(const std::string& sourceId);
    void setTooltip(const std::string& tooltip);

    // Click callback
    using ClickCallback = std::function<void(const std::string& sourceId)>;
    void setClickCallback(ClickCallback callback) {
        _clickCallback = std::move(callback);
    }

private:
    GtkWidget* _eventBox = nullptr;
    GtkWidget* _label = nullptr;
    std::string _sourceId;
    ClickCallback _clickCallback;

    void updateAppearance();

    static gboolean onButtonPress(GtkWidget* widget, GdkEventButton* event,
                                   gpointer data);

    // Get badge colors for source
    static void getBadgeColors(const std::string& sourceId,
                               std::string& bgColor,
                               std::string& fgColor);
};

/**
 * RGConfinementBadge - Badge showing confinement level
 */
class RGConfinementBadge {
public:
    RGConfinementBadge(ConfinementLevel level = ConfinementLevel::UNKNOWN);
    ~RGConfinementBadge();

    GtkWidget* getWidget() { return _box; }

    void setConfinement(ConfinementLevel level);

private:
    GtkWidget* _box = nullptr;
    GtkWidget* _icon = nullptr;
    GtkWidget* _label = nullptr;
    ConfinementLevel _level;

    void updateAppearance();

    static const char* getIconName(ConfinementLevel level);
    static const char* getLevelName(ConfinementLevel level);
    static void getLevelColors(ConfinementLevel level,
                               std::string& bgColor,
                               std::string& fgColor);
};

/**
 * RGTrustBadge - Badge showing trust level
 */
class RGTrustBadge {
public:
    RGTrustBadge(TrustLevel level = TrustLevel::UNKNOWN);
    ~RGTrustBadge();

    GtkWidget* getWidget() { return _box; }

    void setTrustLevel(TrustLevel level);

private:
    GtkWidget* _box = nullptr;
    GtkWidget* _icon = nullptr;
    GtkWidget* _label = nullptr;
    TrustLevel _level;

    void updateAppearance();

    static const char* getIconName(TrustLevel level);
    static const char* getLevelName(TrustLevel level);
    static void getLevelColors(TrustLevel level,
                               std::string& bgColor,
                               std::string& fgColor);
};

} // namespace PolySynaptic

#endif // _RGSOURCESPANE_H_

// vim:ts=4:sw=4:et
