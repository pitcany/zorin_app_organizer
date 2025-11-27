/* rgsourcespane.cc - Sources filter pane implementation
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include "rgsourcespane.h"

#include <sstream>

namespace PolySynaptic {

// ============================================================================
// RGSourcesPane Implementation
// ============================================================================

RGSourcesPane::RGSourcesPane() {
    buildUI();
}

RGSourcesPane::~RGSourcesPane() {
    // GTK widgets are ref-counted and will be destroyed with parent
}

void RGSourcesPane::buildUI() {
    _mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(_mainBox, 6);
    gtk_widget_set_margin_end(_mainBox, 6);
    gtk_widget_set_margin_top(_mainBox, 6);
    gtk_widget_set_margin_bottom(_mainBox, 6);

    buildHeader();
    buildSourcesList();
    buildQuickFilters();

    gtk_widget_show_all(_mainBox);
}

void RGSourcesPane::buildHeader() {
    _headerBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    GtkWidget* titleLabel = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(titleLabel), "<b>Package Sources</b>");
    gtk_widget_set_halign(titleLabel, GTK_ALIGN_START);

    gtk_box_pack_start(GTK_BOX(_headerBox), titleLabel, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(_mainBox), _headerBox, FALSE, FALSE, 0);

    // Separator
    GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(_mainBox), sep, FALSE, FALSE, 0);
}

void RGSourcesPane::buildSourcesList() {
    _sourcesList = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(_mainBox), _sourcesList, TRUE, TRUE, 0);
}

void RGSourcesPane::buildQuickFilters() {
    _quickFilterBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_top(_quickFilterBox, 6);

    _allButton = gtk_button_new_with_label("All");
    gtk_widget_set_tooltip_text(_allButton, "Enable all sources");
    g_signal_connect(_allButton, "clicked", G_CALLBACK(onAllClicked), this);

    _noneButton = gtk_button_new_with_label("None");
    gtk_widget_set_tooltip_text(_noneButton, "Disable all sources");
    g_signal_connect(_noneButton, "clicked", G_CALLBACK(onNoneClicked), this);

    gtk_box_pack_start(GTK_BOX(_quickFilterBox), _allButton, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(_quickFilterBox), _noneButton, TRUE, TRUE, 0);

    gtk_box_pack_end(GTK_BOX(_mainBox), _quickFilterBox, FALSE, FALSE, 0);
}

void RGSourcesPane::setSources(const std::vector<SourceItem>& sources) {
    _sources = sources;

    // Clear existing rows
    GList* children = gtk_container_get_children(GTK_CONTAINER(_sourcesList));
    for (GList* iter = children; iter != nullptr; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);

    _sourceRows.clear();

    // Create new rows
    for (const auto& source : sources) {
        GtkWidget* row = createSourceRow(source);
        gtk_box_pack_start(GTK_BOX(_sourcesList), row, FALSE, FALSE, 0);
    }

    gtk_widget_show_all(_sourcesList);
}

void RGSourcesPane::updateSource(const std::string& id, const SourceItem& source) {
    // Find and update the source
    for (auto& s : _sources) {
        if (s.id == id) {
            s = source;
            break;
        }
    }

    updateRowAppearance(id);
}

std::vector<std::string> RGSourcesPane::getEnabledSources() const {
    std::vector<std::string> enabled;

    for (const auto& source : _sources) {
        if (source.enabled) {
            enabled.push_back(source.id);
        }
    }

    return enabled;
}

void RGSourcesPane::setSourceEnabled(const std::string& id, bool enabled) {
    for (auto& source : _sources) {
        if (source.id == id) {
            source.enabled = enabled;
            break;
        }
    }

    auto it = _sourceRows.find(id);
    if (it != _sourceRows.end()) {
        gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(it->second.checkBox), enabled);
    }

    notifyFilterChanged();
}

void RGSourcesPane::setAllEnabled(bool enabled) {
    for (auto& source : _sources) {
        source.enabled = enabled;
    }

    for (auto& [id, row] : _sourceRows) {
        gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(row.checkBox), enabled);
    }

    notifyFilterChanged();
}

void RGSourcesPane::setCompactMode(bool compact) {
    _compactMode = compact;

    for (const auto& source : _sources) {
        updateRowAppearance(source.id);
    }
}

void RGSourcesPane::setShowCounts(bool show) {
    _showCounts = show;

    for (auto& [id, row] : _sourceRows) {
        gtk_widget_set_visible(row.countLabel, show);
    }
}

void RGSourcesPane::setSourceLoading(const std::string& id, bool loading) {
    auto it = _sourceRows.find(id);
    if (it != _sourceRows.end()) {
        it->second.loading = loading;

        if (loading) {
            gtk_spinner_start(GTK_SPINNER(it->second.spinner));
            gtk_widget_show(it->second.spinner);
            gtk_widget_hide(it->second.statusIcon);
        } else {
            gtk_spinner_stop(GTK_SPINNER(it->second.spinner));
            gtk_widget_hide(it->second.spinner);
            gtk_widget_show(it->second.statusIcon);
        }
    }
}

void RGSourcesPane::setSourceError(const std::string& id, const std::string& error) {
    for (auto& source : _sources) {
        if (source.id == id) {
            source.statusMessage = error;
            source.available = error.empty();
            break;
        }
    }

    updateRowAppearance(id);
}

GtkWidget* RGSourcesPane::createSourceRow(const SourceItem& source) {
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_top(row, 2);
    gtk_widget_set_margin_bottom(row, 2);

    SourceRow rowData;
    rowData.row = row;

    // Checkbox
    rowData.checkBox = gtk_check_button_new();
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(rowData.checkBox), source.enabled);
    gtk_widget_set_sensitive(rowData.checkBox, source.available);

    // Store source ID for callback
    g_object_set_data_full(G_OBJECT(rowData.checkBox), "source-id",
                           g_strdup(source.id.c_str()), g_free);
    g_signal_connect(rowData.checkBox, "toggled",
                     G_CALLBACK(onCheckToggled), this);

    gtk_box_pack_start(GTK_BOX(row), rowData.checkBox, FALSE, FALSE, 0);

    // Icon
    rowData.icon = gtk_image_new_from_icon_name(
        source.iconName.c_str(), GTK_ICON_SIZE_MENU);
    gtk_box_pack_start(GTK_BOX(row), rowData.icon, FALSE, FALSE, 0);

    // Name
    rowData.nameLabel = gtk_label_new(source.name.c_str());
    gtk_widget_set_halign(rowData.nameLabel, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(row), rowData.nameLabel, TRUE, TRUE, 0);

    // Count label
    std::ostringstream countText;
    countText << "(" << source.installedCount << "/" << source.packageCount << ")";
    rowData.countLabel = gtk_label_new(countText.str().c_str());
    gtk_widget_set_opacity(rowData.countLabel, 0.6);
    gtk_box_pack_start(GTK_BOX(row), rowData.countLabel, FALSE, FALSE, 0);

    if (!_showCounts) {
        gtk_widget_hide(rowData.countLabel);
    }

    // Spinner (for loading state)
    rowData.spinner = gtk_spinner_new();
    gtk_box_pack_start(GTK_BOX(row), rowData.spinner, FALSE, FALSE, 0);
    gtk_widget_set_no_show_all(rowData.spinner, TRUE);

    // Status icon
    const char* statusIcon = getStatusIcon(source.available, false,
                                            source.statusMessage);
    rowData.statusIcon = gtk_image_new_from_icon_name(
        statusIcon, GTK_ICON_SIZE_MENU);

    if (!source.statusMessage.empty()) {
        gtk_widget_set_tooltip_text(rowData.statusIcon,
                                     source.statusMessage.c_str());
    }

    gtk_box_pack_start(GTK_BOX(row), rowData.statusIcon, FALSE, FALSE, 0);

    // Settings button
    rowData.settingsButton = gtk_button_new_from_icon_name(
        "preferences-system-symbolic", GTK_ICON_SIZE_MENU);
    gtk_button_set_relief(GTK_BUTTON(rowData.settingsButton), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(rowData.settingsButton, "Configure source");

    g_object_set_data_full(G_OBJECT(rowData.settingsButton), "source-id",
                           g_strdup(source.id.c_str()), g_free);
    g_signal_connect(rowData.settingsButton, "clicked",
                     G_CALLBACK(onSettingsClicked), this);

    gtk_box_pack_start(GTK_BOX(row), rowData.settingsButton, FALSE, FALSE, 0);

    _sourceRows[source.id] = rowData;

    return row;
}

void RGSourcesPane::updateRowAppearance(const std::string& id) {
    auto rowIt = _sourceRows.find(id);
    if (rowIt == _sourceRows.end()) return;

    const SourceItem* source = nullptr;
    for (const auto& s : _sources) {
        if (s.id == id) {
            source = &s;
            break;
        }
    }

    if (!source) return;

    auto& row = rowIt->second;

    // Update enabled state
    gtk_widget_set_sensitive(row.checkBox, source->available);
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(row.checkBox), source->enabled);

    // Update count
    std::ostringstream countText;
    countText << "(" << source->installedCount << "/" << source->packageCount << ")";
    gtk_label_set_text(GTK_LABEL(row.countLabel), countText.str().c_str());

    // Update status icon
    const char* statusIcon = getStatusIcon(source->available, row.loading,
                                            source->statusMessage);
    gtk_image_set_from_icon_name(GTK_IMAGE(row.statusIcon),
                                  statusIcon, GTK_ICON_SIZE_MENU);

    if (!source->statusMessage.empty()) {
        gtk_widget_set_tooltip_text(row.statusIcon,
                                     source->statusMessage.c_str());
    } else {
        gtk_widget_set_tooltip_text(row.statusIcon, nullptr);
    }
}

void RGSourcesPane::onCheckToggled(GtkToggleButton* button, gpointer data) {
    auto* self = static_cast<RGSourcesPane*>(data);
    const char* sourceId = static_cast<const char*>(
        g_object_get_data(G_OBJECT(button), "source-id"));

    if (!sourceId) return;

    bool enabled = gtk_toggle_button_get_active(button);

    for (auto& source : self->_sources) {
        if (source.id == sourceId) {
            source.enabled = enabled;
            break;
        }
    }

    self->notifyFilterChanged();
}

void RGSourcesPane::onSettingsClicked(GtkButton* button, gpointer data) {
    auto* self = static_cast<RGSourcesPane*>(data);
    const char* sourceId = static_cast<const char*>(
        g_object_get_data(G_OBJECT(button), "source-id"));

    if (!sourceId) return;

    if (self->_sourceSettingsCallback) {
        self->_sourceSettingsCallback(sourceId);
    }
}

void RGSourcesPane::onAllClicked(GtkButton* button, gpointer data) {
    auto* self = static_cast<RGSourcesPane*>(data);
    self->setAllEnabled(true);
}

void RGSourcesPane::onNoneClicked(GtkButton* button, gpointer data) {
    auto* self = static_cast<RGSourcesPane*>(data);
    self->setAllEnabled(false);
}

void RGSourcesPane::notifyFilterChanged() {
    if (_filterChangedCallback) {
        _filterChangedCallback(getEnabledSources());
    }
}

const char* RGSourcesPane::getStatusIcon(bool available, bool loading,
                                          const std::string& error)
{
    if (loading) {
        return "emblem-synchronizing-symbolic";
    }

    if (!error.empty()) {
        return "dialog-error-symbolic";
    }

    if (available) {
        return "emblem-ok-symbolic";
    }

    return "dialog-warning-symbolic";
}

// ============================================================================
// RGSourcesBadge Implementation
// ============================================================================

RGSourcesBadge::RGSourcesBadge(const std::string& sourceId)
    : _sourceId(sourceId)
{
    _eventBox = gtk_event_box_new();

    _label = gtk_label_new(nullptr);
    gtk_container_add(GTK_CONTAINER(_eventBox), _label);

    g_signal_connect(_eventBox, "button-press-event",
                     G_CALLBACK(onButtonPress), this);

    updateAppearance();
    gtk_widget_show_all(_eventBox);
}

RGSourcesBadge::~RGSourcesBadge() {}

void RGSourcesBadge::setSourceId(const std::string& sourceId) {
    _sourceId = sourceId;
    updateAppearance();
}

void RGSourcesBadge::setTooltip(const std::string& tooltip) {
    gtk_widget_set_tooltip_text(_eventBox, tooltip.c_str());
}

void RGSourcesBadge::updateAppearance() {
    std::string bgColor, fgColor;
    getBadgeColors(_sourceId, bgColor, fgColor);

    // Badge text
    std::string badgeText;
    if (_sourceId == "apt") {
        badgeText = "deb";
    } else if (_sourceId == "snap") {
        badgeText = "snap";
    } else if (_sourceId == "flatpak") {
        badgeText = "flatpak";
    } else {
        badgeText = _sourceId;
    }

    // Apply CSS styling
    std::ostringstream markup;
    markup << "<span background='" << bgColor << "' foreground='" << fgColor
           << "' font_size='small'> " << badgeText << " </span>";

    gtk_label_set_markup(GTK_LABEL(_label), markup.str().c_str());
}

gboolean RGSourcesBadge::onButtonPress(GtkWidget* widget,
                                        GdkEventButton* event,
                                        gpointer data)
{
    auto* self = static_cast<RGSourcesBadge*>(data);

    if (event->button == 1 && self->_clickCallback) {
        self->_clickCallback(self->_sourceId);
        return TRUE;
    }

    return FALSE;
}

void RGSourcesBadge::getBadgeColors(const std::string& sourceId,
                                     std::string& bgColor,
                                     std::string& fgColor)
{
    if (sourceId == "apt") {
        bgColor = "#a40000";  // Dark red
        fgColor = "#ffffff";
    } else if (sourceId == "snap") {
        bgColor = "#e95420";  // Ubuntu orange
        fgColor = "#ffffff";
    } else if (sourceId == "flatpak") {
        bgColor = "#4a86cf";  // Blue
        fgColor = "#ffffff";
    } else {
        bgColor = "#555555";  // Grey
        fgColor = "#ffffff";
    }
}

// ============================================================================
// RGConfinementBadge Implementation
// ============================================================================

RGConfinementBadge::RGConfinementBadge(ConfinementLevel level)
    : _level(level)
{
    _box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

    _icon = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(_box), _icon, FALSE, FALSE, 0);

    _label = gtk_label_new(nullptr);
    gtk_box_pack_start(GTK_BOX(_box), _label, FALSE, FALSE, 0);

    updateAppearance();
    gtk_widget_show_all(_box);
}

RGConfinementBadge::~RGConfinementBadge() {}

void RGConfinementBadge::setConfinement(ConfinementLevel level) {
    _level = level;
    updateAppearance();
}

void RGConfinementBadge::updateAppearance() {
    gtk_image_set_from_icon_name(GTK_IMAGE(_icon), getIconName(_level),
                                  GTK_ICON_SIZE_SMALL_TOOLBAR);

    std::string bgColor, fgColor;
    getLevelColors(_level, bgColor, fgColor);

    std::ostringstream markup;
    markup << "<span background='" << bgColor << "' foreground='" << fgColor
           << "'> " << getLevelName(_level) << " </span>";

    gtk_label_set_markup(GTK_LABEL(_label), markup.str().c_str());

    // Tooltip
    std::string tooltip;
    switch (_level) {
        case ConfinementLevel::STRICT:
            tooltip = "Runs in a secure sandbox with limited system access";
            break;
        case ConfinementLevel::CLASSIC:
            tooltip = "Has full system access (not sandboxed)";
            break;
        case ConfinementLevel::DEVMODE:
            tooltip = "Development mode - security features disabled";
            break;
        case ConfinementLevel::UNCONFINED:
            tooltip = "Traditional package with full system access";
            break;
        default:
            tooltip = "Unknown confinement level";
    }
    gtk_widget_set_tooltip_text(_box, tooltip.c_str());
}

const char* RGConfinementBadge::getIconName(ConfinementLevel level) {
    switch (level) {
        case ConfinementLevel::STRICT:
            return "security-high-symbolic";
        case ConfinementLevel::CLASSIC:
            return "security-low-symbolic";
        case ConfinementLevel::DEVMODE:
            return "dialog-warning-symbolic";
        case ConfinementLevel::UNCONFINED:
            return "security-medium-symbolic";
        default:
            return "dialog-question-symbolic";
    }
}

const char* RGConfinementBadge::getLevelName(ConfinementLevel level) {
    switch (level) {
        case ConfinementLevel::STRICT:
            return "Sandboxed";
        case ConfinementLevel::CLASSIC:
            return "Classic";
        case ConfinementLevel::DEVMODE:
            return "Dev Mode";
        case ConfinementLevel::UNCONFINED:
            return "Full Access";
        default:
            return "Unknown";
    }
}

void RGConfinementBadge::getLevelColors(ConfinementLevel level,
                                         std::string& bgColor,
                                         std::string& fgColor)
{
    switch (level) {
        case ConfinementLevel::STRICT:
            bgColor = "#2e7d32";  // Green
            fgColor = "#ffffff";
            break;
        case ConfinementLevel::CLASSIC:
            bgColor = "#f9a825";  // Yellow
            fgColor = "#000000";
            break;
        case ConfinementLevel::DEVMODE:
            bgColor = "#c62828";  // Red
            fgColor = "#ffffff";
            break;
        case ConfinementLevel::UNCONFINED:
            bgColor = "#757575";  // Grey
            fgColor = "#ffffff";
            break;
        default:
            bgColor = "#9e9e9e";
            fgColor = "#000000";
    }
}

// ============================================================================
// RGTrustBadge Implementation
// ============================================================================

RGTrustBadge::RGTrustBadge(TrustLevel level)
    : _level(level)
{
    _box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

    _icon = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(_box), _icon, FALSE, FALSE, 0);

    _label = gtk_label_new(nullptr);
    gtk_box_pack_start(GTK_BOX(_box), _label, FALSE, FALSE, 0);

    updateAppearance();
    gtk_widget_show_all(_box);
}

RGTrustBadge::~RGTrustBadge() {}

void RGTrustBadge::setTrustLevel(TrustLevel level) {
    _level = level;
    updateAppearance();
}

void RGTrustBadge::updateAppearance() {
    gtk_image_set_from_icon_name(GTK_IMAGE(_icon), getIconName(_level),
                                  GTK_ICON_SIZE_SMALL_TOOLBAR);

    std::string bgColor, fgColor;
    getLevelColors(_level, bgColor, fgColor);

    std::ostringstream markup;
    markup << "<span background='" << bgColor << "' foreground='" << fgColor
           << "'> " << getLevelName(_level) << " </span>";

    gtk_label_set_markup(GTK_LABEL(_label), markup.str().c_str());

    // Tooltip
    std::string tooltip;
    switch (_level) {
        case TrustLevel::OFFICIAL:
            tooltip = "Official package from distribution repositories";
            break;
        case TrustLevel::VERIFIED:
            tooltip = "Verified publisher";
            break;
        case TrustLevel::COMMUNITY:
            tooltip = "Community-maintained package";
            break;
        case TrustLevel::THIRD_PARTY:
            tooltip = "Third-party source";
            break;
        default:
            tooltip = "Unknown trust level";
    }
    gtk_widget_set_tooltip_text(_box, tooltip.c_str());
}

const char* RGTrustBadge::getIconName(TrustLevel level) {
    switch (level) {
        case TrustLevel::OFFICIAL:
            return "emblem-default-symbolic";
        case TrustLevel::VERIFIED:
            return "emblem-ok-symbolic";
        case TrustLevel::COMMUNITY:
            return "system-users-symbolic";
        case TrustLevel::THIRD_PARTY:
            return "dialog-warning-symbolic";
        default:
            return "dialog-question-symbolic";
    }
}

const char* RGTrustBadge::getLevelName(TrustLevel level) {
    switch (level) {
        case TrustLevel::OFFICIAL:
            return "Official";
        case TrustLevel::VERIFIED:
            return "Verified";
        case TrustLevel::COMMUNITY:
            return "Community";
        case TrustLevel::THIRD_PARTY:
            return "Third-party";
        default:
            return "Unknown";
    }
}

void RGTrustBadge::getLevelColors(TrustLevel level,
                                   std::string& bgColor,
                                   std::string& fgColor)
{
    switch (level) {
        case TrustLevel::OFFICIAL:
            bgColor = "#1565c0";  // Blue
            fgColor = "#ffffff";
            break;
        case TrustLevel::VERIFIED:
            bgColor = "#2e7d32";  // Green
            fgColor = "#ffffff";
            break;
        case TrustLevel::COMMUNITY:
            bgColor = "#7b1fa2";  // Purple
            fgColor = "#ffffff";
            break;
        case TrustLevel::THIRD_PARTY:
            bgColor = "#ef6c00";  // Orange
            fgColor = "#ffffff";
            break;
        default:
            bgColor = "#9e9e9e";
            fgColor = "#000000";
    }
}

} // namespace PolySynaptic

// vim:ts=4:sw=4:et
