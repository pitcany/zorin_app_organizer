/* rgdebugpanel.h - Debug panel for PolySynaptic
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This file provides a GTK widget for viewing structured logs,
 * debugging backend operations, and monitoring system state.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef _RGDEBUGPANEL_H_
#define _RGDEBUGPANEL_H_

#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>

#include "structuredlog.h"

namespace PolySynaptic {

/**
 * RGDebugPanel - GTK widget for debugging and log viewing
 *
 * Features:
 *   - Real-time log viewer with filtering
 *   - Provider status monitoring
 *   - Command execution console
 *   - Performance metrics
 */
class RGDebugPanel {
public:
    RGDebugPanel();
    ~RGDebugPanel();

    /**
     * Get the main widget
     */
    GtkWidget* getWidget() { return _notebook; }

    /**
     * Show the panel in a dialog
     */
    void showDialog(GtkWindow* parent);

    /**
     * Update log display from memory sink
     */
    void updateLogs(std::shared_ptr<MemorySink> sink);

    /**
     * Add a log entry manually
     */
    void addLogEntry(const LogEntry& entry);

    /**
     * Clear all logs
     */
    void clearLogs();

    /**
     * Set log level filter
     */
    void setMinLogLevel(LogLevel level);

    /**
     * Filter by provider
     */
    void setProviderFilter(const std::string& provider);

    /**
     * Filter by operation
     */
    void setOperationFilter(const std::string& operation);

    /**
     * Export logs to file
     */
    bool exportLogs(const std::string& path, bool asJson = true);

    /**
     * Execute a debug command
     */
    std::string executeCommand(const std::string& command);

    /**
     * Set provider status information
     */
    struct ProviderDebugInfo {
        std::string id;
        std::string name;
        bool available;
        bool enabled;
        std::string version;
        int packageCount;
        int operationCount;
        double avgOperationTime;  // milliseconds
        std::string lastError;
    };
    void setProviderInfo(const std::vector<ProviderDebugInfo>& info);

    /**
     * Update performance metrics
     */
    struct PerformanceMetrics {
        double searchTime;      // ms
        double cacheLoadTime;   // ms
        double uiRenderTime;    // ms
        size_t memoryUsage;     // bytes
        int activeOperations;
    };
    void updateMetrics(const PerformanceMetrics& metrics);

private:
    // Main notebook with tabs
    GtkWidget* _notebook = nullptr;

    // Log viewer tab
    GtkWidget* _logView = nullptr;
    GtkTextBuffer* _logBuffer = nullptr;
    GtkWidget* _levelCombo = nullptr;
    GtkWidget* _providerCombo = nullptr;
    GtkWidget* _searchEntry = nullptr;
    GtkWidget* _clearButton = nullptr;
    GtkWidget* _exportButton = nullptr;
    GtkWidget* _autoScrollCheck = nullptr;

    // Provider status tab
    GtkWidget* _providerTree = nullptr;
    GtkListStore* _providerStore = nullptr;

    // Console tab
    GtkWidget* _consoleView = nullptr;
    GtkTextBuffer* _consoleBuffer = nullptr;
    GtkWidget* _commandEntry = nullptr;

    // Metrics tab
    GtkWidget* _metricsGrid = nullptr;
    std::map<std::string, GtkWidget*> _metricLabels;

    // State
    LogLevel _minLevel = LogLevel::DEBUG;
    std::string _providerFilter;
    std::string _operationFilter;
    bool _autoScroll = true;

    // Build UI
    void buildUI();
    void buildLogTab();
    void buildProviderTab();
    void buildConsoleTab();
    void buildMetricsTab();

    // Log display helpers
    void appendLogText(const LogEntry& entry);
    void applyLogFilters();
    void scrollToEnd();

    // Text tags for log levels
    void createTextTags();
    const char* getTagForLevel(LogLevel level);

    // Signal handlers
    static void onLevelChanged(GtkComboBox* combo, gpointer data);
    static void onProviderChanged(GtkComboBox* combo, gpointer data);
    static void onSearchChanged(GtkSearchEntry* entry, gpointer data);
    static void onClearClicked(GtkButton* button, gpointer data);
    static void onExportClicked(GtkButton* button, gpointer data);
    static void onAutoScrollToggled(GtkToggleButton* button, gpointer data);
    static void onCommandActivate(GtkEntry* entry, gpointer data);

    // Console commands
    std::map<std::string, std::function<std::string(const std::vector<std::string>&)>>
        _commands;
    void registerCommands();
};

/**
 * RGLogLevelIndicator - Small widget showing current log level
 */
class RGLogLevelIndicator {
public:
    RGLogLevelIndicator();
    ~RGLogLevelIndicator();

    GtkWidget* getWidget() { return _box; }

    void setLevel(LogLevel level);
    void setErrorCount(int count);
    void setWarningCount(int count);

    // Click callback to open debug panel
    using ClickCallback = std::function<void()>;
    void setClickCallback(ClickCallback callback) {
        _clickCallback = std::move(callback);
    }

private:
    GtkWidget* _box = nullptr;
    GtkWidget* _levelIcon = nullptr;
    GtkWidget* _errorLabel = nullptr;
    GtkWidget* _warningLabel = nullptr;

    LogLevel _level = LogLevel::INFO;
    int _errorCount = 0;
    int _warningCount = 0;
    ClickCallback _clickCallback;

    void updateAppearance();

    static gboolean onButtonPress(GtkWidget* widget, GdkEventButton* event,
                                   gpointer data);
};

} // namespace PolySynaptic

#endif // _RGDEBUGPANEL_H_

// vim:ts=4:sw=4:et
