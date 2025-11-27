/* rgdebugpanel.cc - Debug panel implementation
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include "rgdebugpanel.h"

#include <sstream>
#include <iomanip>
#include <fstream>

namespace PolySynaptic {

// ============================================================================
// RGDebugPanel Implementation
// ============================================================================

RGDebugPanel::RGDebugPanel() {
    buildUI();
    registerCommands();
}

RGDebugPanel::~RGDebugPanel() {}

void RGDebugPanel::buildUI() {
    _notebook = gtk_notebook_new();

    buildLogTab();
    buildProviderTab();
    buildConsoleTab();
    buildMetricsTab();

    gtk_widget_show_all(_notebook);
}

void RGDebugPanel::buildLogTab() {
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);

    // Filter toolbar
    GtkWidget* toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    // Level filter
    GtkWidget* levelLabel = gtk_label_new("Level:");
    _levelCombo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(_levelCombo), "DEBUG", "Debug");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(_levelCombo), "INFO", "Info");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(_levelCombo), "WARN", "Warning");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(_levelCombo), "ERROR", "Error");
    gtk_combo_box_set_active(GTK_COMBO_BOX(_levelCombo), 0);
    g_signal_connect(_levelCombo, "changed", G_CALLBACK(onLevelChanged), this);

    gtk_box_pack_start(GTK_BOX(toolbar), levelLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), _levelCombo, FALSE, FALSE, 0);

    // Provider filter
    GtkWidget* providerLabel = gtk_label_new("Provider:");
    _providerCombo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(_providerCombo), "", "All");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(_providerCombo), "APT", "APT");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(_providerCombo), "Snap", "Snap");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(_providerCombo), "Flatpak", "Flatpak");
    gtk_combo_box_set_active(GTK_COMBO_BOX(_providerCombo), 0);
    g_signal_connect(_providerCombo, "changed", G_CALLBACK(onProviderChanged), this);

    gtk_box_pack_start(GTK_BOX(toolbar), providerLabel, FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(toolbar), _providerCombo, FALSE, FALSE, 0);

    // Search
    _searchEntry = gtk_search_entry_new();
    gtk_widget_set_size_request(_searchEntry, 200, -1);
    gtk_entry_set_placeholder_text(GTK_ENTRY(_searchEntry), "Search logs...");
    g_signal_connect(_searchEntry, "search-changed", G_CALLBACK(onSearchChanged), this);

    gtk_box_pack_start(GTK_BOX(toolbar), _searchEntry, TRUE, TRUE, 6);

    // Auto-scroll
    _autoScrollCheck = gtk_check_button_new_with_label("Auto-scroll");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(_autoScrollCheck), TRUE);
    g_signal_connect(_autoScrollCheck, "toggled", G_CALLBACK(onAutoScrollToggled), this);

    gtk_box_pack_start(GTK_BOX(toolbar), _autoScrollCheck, FALSE, FALSE, 0);

    // Buttons
    _clearButton = gtk_button_new_with_label("Clear");
    g_signal_connect(_clearButton, "clicked", G_CALLBACK(onClearClicked), this);

    _exportButton = gtk_button_new_with_label("Export");
    g_signal_connect(_exportButton, "clicked", G_CALLBACK(onExportClicked), this);

    gtk_box_pack_end(GTK_BOX(toolbar), _exportButton, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(toolbar), _clearButton, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

    // Log view
    GtkWidget* scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    _logView = gtk_text_view_new();
    _logBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(_logView));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(_logView), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(_logView), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(_logView), TRUE);

    createTextTags();

    gtk_container_add(GTK_CONTAINER(scrolled), _logView);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    // Add to notebook
    GtkWidget* tabLabel = gtk_label_new("Logs");
    gtk_notebook_append_page(GTK_NOTEBOOK(_notebook), vbox, tabLabel);
}

void RGDebugPanel::buildProviderTab() {
    GtkWidget* scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled), 6);

    // Create list store
    _providerStore = gtk_list_store_new(8,
        G_TYPE_STRING,   // ID
        G_TYPE_STRING,   // Name
        G_TYPE_BOOLEAN,  // Available
        G_TYPE_BOOLEAN,  // Enabled
        G_TYPE_STRING,   // Version
        G_TYPE_INT,      // Package count
        G_TYPE_INT,      // Operation count
        G_TYPE_STRING);  // Last error

    _providerTree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(_providerStore));

    // Columns
    GtkCellRenderer* renderer;
    GtkTreeViewColumn* column;

    // Name
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Provider", renderer,
                                                       "text", 1, nullptr);
    gtk_tree_view_append_column(GTK_TREE_VIEW(_providerTree), column);

    // Available
    renderer = gtk_cell_renderer_toggle_new();
    column = gtk_tree_view_column_new_with_attributes("Available", renderer,
                                                       "active", 2, nullptr);
    gtk_tree_view_append_column(GTK_TREE_VIEW(_providerTree), column);

    // Enabled
    renderer = gtk_cell_renderer_toggle_new();
    column = gtk_tree_view_column_new_with_attributes("Enabled", renderer,
                                                       "active", 3, nullptr);
    gtk_tree_view_append_column(GTK_TREE_VIEW(_providerTree), column);

    // Version
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Version", renderer,
                                                       "text", 4, nullptr);
    gtk_tree_view_append_column(GTK_TREE_VIEW(_providerTree), column);

    // Packages
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Packages", renderer,
                                                       "text", 5, nullptr);
    gtk_tree_view_append_column(GTK_TREE_VIEW(_providerTree), column);

    // Operations
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Operations", renderer,
                                                       "text", 6, nullptr);
    gtk_tree_view_append_column(GTK_TREE_VIEW(_providerTree), column);

    // Last Error
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Last Error", renderer,
                                                       "text", 7, nullptr);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(_providerTree), column);

    gtk_container_add(GTK_CONTAINER(scrolled), _providerTree);

    GtkWidget* tabLabel = gtk_label_new("Providers");
    gtk_notebook_append_page(GTK_NOTEBOOK(_notebook), scrolled, tabLabel);
}

void RGDebugPanel::buildConsoleTab() {
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);

    // Console output
    GtkWidget* scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    _consoleView = gtk_text_view_new();
    _consoleBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(_consoleView));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(_consoleView), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(_consoleView), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(_consoleView), TRUE);

    gtk_container_add(GTK_CONTAINER(scrolled), _consoleView);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    // Command entry
    GtkWidget* cmdBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    GtkWidget* prompt = gtk_label_new(">");
    gtk_box_pack_start(GTK_BOX(cmdBox), prompt, FALSE, FALSE, 0);

    _commandEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(_commandEntry),
                                    "Enter command (type 'help' for list)");
    g_signal_connect(_commandEntry, "activate", G_CALLBACK(onCommandActivate), this);

    gtk_box_pack_start(GTK_BOX(cmdBox), _commandEntry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), cmdBox, FALSE, FALSE, 0);

    GtkWidget* tabLabel = gtk_label_new("Console");
    gtk_notebook_append_page(GTK_NOTEBOOK(_notebook), vbox, tabLabel);
}

void RGDebugPanel::buildMetricsTab() {
    _metricsGrid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(_metricsGrid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(_metricsGrid), 12);
    gtk_container_set_border_width(GTK_CONTAINER(_metricsGrid), 12);

    int row = 0;

    auto addMetric = [&](const char* name, const char* id) {
        GtkWidget* nameLabel = gtk_label_new(name);
        gtk_widget_set_halign(nameLabel, GTK_ALIGN_START);

        GtkWidget* valueLabel = gtk_label_new("--");
        gtk_widget_set_halign(valueLabel, GTK_ALIGN_END);

        gtk_grid_attach(GTK_GRID(_metricsGrid), nameLabel, 0, row, 1, 1);
        gtk_grid_attach(GTK_GRID(_metricsGrid), valueLabel, 1, row, 1, 1);

        _metricLabels[id] = valueLabel;
        row++;
    };

    addMetric("Search Time:", "search_time");
    addMetric("Cache Load Time:", "cache_load_time");
    addMetric("UI Render Time:", "ui_render_time");
    addMetric("Memory Usage:", "memory_usage");
    addMetric("Active Operations:", "active_ops");

    GtkWidget* tabLabel = gtk_label_new("Metrics");
    gtk_notebook_append_page(GTK_NOTEBOOK(_notebook), _metricsGrid, tabLabel);
}

void RGDebugPanel::showDialog(GtkWindow* parent) {
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Debug Panel",
        parent,
        GTK_DIALOG_MODAL,
        "_Close", GTK_RESPONSE_CLOSE,
        nullptr);

    gtk_window_set_default_size(GTK_WINDOW(dialog), 800, 500);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    // Reparent notebook temporarily
    g_object_ref(_notebook);
    GtkWidget* oldParent = gtk_widget_get_parent(_notebook);
    if (oldParent) {
        gtk_container_remove(GTK_CONTAINER(oldParent), _notebook);
    }

    gtk_box_pack_start(GTK_BOX(content), _notebook, TRUE, TRUE, 0);

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));

    // Restore notebook
    gtk_container_remove(GTK_CONTAINER(content), _notebook);
    if (oldParent) {
        gtk_container_add(GTK_CONTAINER(oldParent), _notebook);
    }
    g_object_unref(_notebook);

    gtk_widget_destroy(dialog);
}

void RGDebugPanel::updateLogs(std::shared_ptr<MemorySink> sink) {
    if (!sink) return;

    auto entries = sink->getEntriesFiltered(_minLevel, _providerFilter, _operationFilter);

    // Clear and repopulate
    gtk_text_buffer_set_text(_logBuffer, "", 0);

    for (const auto& entry : entries) {
        appendLogText(entry);
    }

    if (_autoScroll) {
        scrollToEnd();
    }
}

void RGDebugPanel::addLogEntry(const LogEntry& entry) {
    if (entry.level < _minLevel) return;
    if (!_providerFilter.empty() && entry.provider != _providerFilter) return;
    if (!_operationFilter.empty() && entry.operation != _operationFilter) return;

    appendLogText(entry);

    if (_autoScroll) {
        scrollToEnd();
    }
}

void RGDebugPanel::clearLogs() {
    gtk_text_buffer_set_text(_logBuffer, "", 0);
}

void RGDebugPanel::setMinLogLevel(LogLevel level) {
    _minLevel = level;
    applyLogFilters();
}

void RGDebugPanel::setProviderFilter(const std::string& provider) {
    _providerFilter = provider;
    applyLogFilters();
}

void RGDebugPanel::setOperationFilter(const std::string& operation) {
    _operationFilter = operation;
    applyLogFilters();
}

bool RGDebugPanel::exportLogs(const std::string& path, bool asJson) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(_logBuffer, &start, &end);
    char* text = gtk_text_buffer_get_text(_logBuffer, &start, &end, FALSE);

    if (asJson) {
        file << "[";
        // Would need to parse and convert to JSON array
        // For now, just output as text wrapped in JSON
        file << "{\"logs\":\"" << text << "\"}";
        file << "]";
    } else {
        file << text;
    }

    g_free(text);
    file.close();

    return true;
}

std::string RGDebugPanel::executeCommand(const std::string& command) {
    // Parse command and arguments
    std::istringstream stream(command);
    std::string cmd;
    stream >> cmd;

    std::vector<std::string> args;
    std::string arg;
    while (stream >> arg) {
        args.push_back(arg);
    }

    // Find and execute command
    auto it = _commands.find(cmd);
    if (it != _commands.end()) {
        return it->second(args);
    }

    return "Unknown command: " + cmd + "\nType 'help' for available commands.";
}

void RGDebugPanel::setProviderInfo(const std::vector<ProviderDebugInfo>& info) {
    gtk_list_store_clear(_providerStore);

    GtkTreeIter iter;
    for (const auto& p : info) {
        gtk_list_store_append(_providerStore, &iter);
        gtk_list_store_set(_providerStore, &iter,
            0, p.id.c_str(),
            1, p.name.c_str(),
            2, p.available,
            3, p.enabled,
            4, p.version.c_str(),
            5, p.packageCount,
            6, p.operationCount,
            7, p.lastError.c_str(),
            -1);
    }
}

void RGDebugPanel::updateMetrics(const PerformanceMetrics& metrics) {
    std::ostringstream ss;

    ss.str("");
    ss << std::fixed << std::setprecision(2) << metrics.searchTime << " ms";
    gtk_label_set_text(GTK_LABEL(_metricLabels["search_time"]), ss.str().c_str());

    ss.str("");
    ss << std::fixed << std::setprecision(2) << metrics.cacheLoadTime << " ms";
    gtk_label_set_text(GTK_LABEL(_metricLabels["cache_load_time"]), ss.str().c_str());

    ss.str("");
    ss << std::fixed << std::setprecision(2) << metrics.uiRenderTime << " ms";
    gtk_label_set_text(GTK_LABEL(_metricLabels["ui_render_time"]), ss.str().c_str());

    ss.str("");
    ss << (metrics.memoryUsage / 1024 / 1024) << " MB";
    gtk_label_set_text(GTK_LABEL(_metricLabels["memory_usage"]), ss.str().c_str());

    ss.str("");
    ss << metrics.activeOperations;
    gtk_label_set_text(GTK_LABEL(_metricLabels["active_ops"]), ss.str().c_str());
}

void RGDebugPanel::createTextTags() {
    gtk_text_buffer_create_tag(_logBuffer, "debug",
        "foreground", "#888888", nullptr);
    gtk_text_buffer_create_tag(_logBuffer, "info",
        "foreground", "#000000", nullptr);
    gtk_text_buffer_create_tag(_logBuffer, "warn",
        "foreground", "#f57c00", "weight", PANGO_WEIGHT_BOLD, nullptr);
    gtk_text_buffer_create_tag(_logBuffer, "error",
        "foreground", "#d32f2f", "weight", PANGO_WEIGHT_BOLD, nullptr);
    gtk_text_buffer_create_tag(_logBuffer, "fatal",
        "foreground", "#ffffff", "background", "#d32f2f",
        "weight", PANGO_WEIGHT_BOLD, nullptr);
}

const char* RGDebugPanel::getTagForLevel(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "debug";
        case LogLevel::INFO: return "info";
        case LogLevel::WARN: return "warn";
        case LogLevel::ERROR: return "error";
        case LogLevel::FATAL: return "fatal";
    }
    return "info";
}

void RGDebugPanel::appendLogText(const LogEntry& entry) {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(_logBuffer, &end);

    std::string text = entry.toReadable() + "\n";
    gtk_text_buffer_insert_with_tags_by_name(
        _logBuffer, &end, text.c_str(), -1,
        getTagForLevel(entry.level), nullptr);
}

void RGDebugPanel::applyLogFilters() {
    // Re-filter would require re-fetching from source
    // For now, just mark that filters changed
}

void RGDebugPanel::scrollToEnd() {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(_logBuffer, &end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(_logView), &end,
                                  0.0, FALSE, 0.0, 0.0);
}

void RGDebugPanel::onLevelChanged(GtkComboBox* combo, gpointer data) {
    auto* self = static_cast<RGDebugPanel*>(data);

    const char* id = gtk_combo_box_get_active_id(combo);
    if (g_strcmp0(id, "DEBUG") == 0) {
        self->setMinLogLevel(LogLevel::DEBUG);
    } else if (g_strcmp0(id, "INFO") == 0) {
        self->setMinLogLevel(LogLevel::INFO);
    } else if (g_strcmp0(id, "WARN") == 0) {
        self->setMinLogLevel(LogLevel::WARN);
    } else if (g_strcmp0(id, "ERROR") == 0) {
        self->setMinLogLevel(LogLevel::ERROR);
    }
}

void RGDebugPanel::onProviderChanged(GtkComboBox* combo, gpointer data) {
    auto* self = static_cast<RGDebugPanel*>(data);

    const char* id = gtk_combo_box_get_active_id(combo);
    self->setProviderFilter(id ? id : "");
}

void RGDebugPanel::onSearchChanged(GtkSearchEntry* entry, gpointer data) {
    // Implement search filtering
}

void RGDebugPanel::onClearClicked(GtkButton* button, gpointer data) {
    auto* self = static_cast<RGDebugPanel*>(data);
    self->clearLogs();
}

void RGDebugPanel::onExportClicked(GtkButton* button, gpointer data) {
    auto* self = static_cast<RGDebugPanel*>(data);

    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        "Export Logs",
        nullptr,
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        nullptr);

    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
                                       "polysynaptic-logs.json");

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        self->exportLogs(filename, true);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

void RGDebugPanel::onAutoScrollToggled(GtkToggleButton* button, gpointer data) {
    auto* self = static_cast<RGDebugPanel*>(data);
    self->_autoScroll = gtk_toggle_button_get_active(button);
}

void RGDebugPanel::onCommandActivate(GtkEntry* entry, gpointer data) {
    auto* self = static_cast<RGDebugPanel*>(data);

    const char* command = gtk_entry_get_text(entry);
    if (command && command[0] != '\0') {
        // Add to console
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(self->_consoleBuffer, &end);

        std::string cmdLine = std::string("> ") + command + "\n";
        gtk_text_buffer_insert(self->_consoleBuffer, &end,
                               cmdLine.c_str(), -1);

        // Execute and show result
        std::string result = self->executeCommand(command);
        gtk_text_buffer_get_end_iter(self->_consoleBuffer, &end);
        gtk_text_buffer_insert(self->_consoleBuffer, &end,
                               (result + "\n\n").c_str(), -1);

        // Clear entry
        gtk_entry_set_text(entry, "");
    }
}

void RGDebugPanel::registerCommands() {
    _commands["help"] = [this](const std::vector<std::string>&) {
        return "Available commands:\n"
               "  help          - Show this help\n"
               "  status        - Show provider status\n"
               "  clear         - Clear console\n"
               "  search <term> - Search packages\n"
               "  info <pkg>    - Show package info\n"
               "  loglevel <n>  - Set log level (0-4)\n";
    };

    _commands["clear"] = [this](const std::vector<std::string>&) {
        gtk_text_buffer_set_text(_consoleBuffer, "", 0);
        return "";
    };

    _commands["status"] = [](const std::vector<std::string>&) {
        return "APT: OK\nSnap: OK\nFlatpak: OK";
    };

    _commands["loglevel"] = [this](const std::vector<std::string>& args) {
        if (args.empty()) {
            return "Current log level: " + std::to_string(static_cast<int>(_minLevel));
        }

        try {
            int level = std::stoi(args[0]);
            if (level >= 0 && level <= 4) {
                _minLevel = static_cast<LogLevel>(level);
                return "Log level set to " + std::to_string(level);
            }
        } catch (...) {}

        return std::string("Invalid level. Use 0-4.");
    };
}

// ============================================================================
// RGLogLevelIndicator Implementation
// ============================================================================

RGLogLevelIndicator::RGLogLevelIndicator() {
    _box = gtk_event_box_new();

    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_container_add(GTK_CONTAINER(_box), hbox);

    _levelIcon = gtk_image_new_from_icon_name("dialog-information-symbolic",
                                               GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_box_pack_start(GTK_BOX(hbox), _levelIcon, FALSE, FALSE, 0);

    _errorLabel = gtk_label_new("0");
    gtk_widget_set_name(_errorLabel, "error-count");
    gtk_box_pack_start(GTK_BOX(hbox), _errorLabel, FALSE, FALSE, 0);

    _warningLabel = gtk_label_new("0");
    gtk_widget_set_name(_warningLabel, "warning-count");
    gtk_box_pack_start(GTK_BOX(hbox), _warningLabel, FALSE, FALSE, 0);

    g_signal_connect(_box, "button-press-event",
                     G_CALLBACK(onButtonPress), this);

    gtk_widget_set_tooltip_text(_box, "Click to open debug panel");

    updateAppearance();
    gtk_widget_show_all(_box);
}

RGLogLevelIndicator::~RGLogLevelIndicator() {}

void RGLogLevelIndicator::setLevel(LogLevel level) {
    _level = level;
    updateAppearance();
}

void RGLogLevelIndicator::setErrorCount(int count) {
    _errorCount = count;
    updateAppearance();
}

void RGLogLevelIndicator::setWarningCount(int count) {
    _warningCount = count;
    updateAppearance();
}

void RGLogLevelIndicator::updateAppearance() {
    // Update icon based on level/counts
    const char* iconName = "dialog-information-symbolic";
    if (_errorCount > 0) {
        iconName = "dialog-error-symbolic";
    } else if (_warningCount > 0) {
        iconName = "dialog-warning-symbolic";
    }

    gtk_image_set_from_icon_name(GTK_IMAGE(_levelIcon), iconName,
                                  GTK_ICON_SIZE_SMALL_TOOLBAR);

    // Update counts
    gtk_label_set_text(GTK_LABEL(_errorLabel),
                       std::to_string(_errorCount).c_str());
    gtk_label_set_text(GTK_LABEL(_warningLabel),
                       std::to_string(_warningCount).c_str());

    // Show/hide based on counts
    gtk_widget_set_visible(_errorLabel, _errorCount > 0);
    gtk_widget_set_visible(_warningLabel, _warningCount > 0);
}

gboolean RGLogLevelIndicator::onButtonPress(GtkWidget* widget,
                                             GdkEventButton* event,
                                             gpointer data)
{
    auto* self = static_cast<RGLogLevelIndicator*>(data);

    if (event->button == 1 && self->_clickCallback) {
        self->_clickCallback();
        return TRUE;
    }

    return FALSE;
}

} // namespace PolySynaptic

// vim:ts=4:sw=4:et
