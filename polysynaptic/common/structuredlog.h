/* structuredlog.h - Structured JSON logging for PolySynaptic
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This file provides a structured logging system that outputs JSON-formatted
 * logs for easy parsing and analysis. Features:
 *   - JSON output format
 *   - Log levels (DEBUG, INFO, WARN, ERROR, FATAL)
 *   - Contextual fields (provider, operation, package)
 *   - Duration tracking
 *   - Debug panel integration
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef _STRUCTUREDLOG_H_
#define _STRUCTUREDLOG_H_

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <mutex>
#include <fstream>
#include <sstream>
#include <functional>
#include <memory>
#include <iomanip>
#include <ctime>
#include <deque>

namespace PolySynaptic {

// ============================================================================
// Log Levels
// ============================================================================

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    FATAL = 4
};

inline const char* logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
    }
    return "UNKNOWN";
}

// ============================================================================
// Log Entry
// ============================================================================

/**
 * LogEntry - Single log entry with structured fields
 */
struct LogEntry {
    // Timestamp
    std::chrono::system_clock::time_point timestamp;

    // Level
    LogLevel level;

    // Message
    std::string message;

    // Contextual fields
    std::string provider;           // APT, Snap, Flatpak, etc.
    std::string operation;          // search, install, remove, etc.
    std::string packageId;          // Package being operated on
    std::string component;          // UI, Backend, etc.

    // Error details
    std::string errorCode;
    std::string rawStderr;
    int exitCode = 0;

    // Duration (for timed operations)
    std::chrono::milliseconds duration{0};

    // Arbitrary key-value data
    std::map<std::string, std::string> fields;

    // Default constructor
    LogEntry()
        : timestamp(std::chrono::system_clock::now())
        , level(LogLevel::INFO)
    {}

    // Convert to JSON string
    std::string toJson() const {
        std::ostringstream json;
        json << "{";

        // Timestamp
        auto time = std::chrono::system_clock::to_time_t(timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()) % 1000;
        json << "\"timestamp\":\"";
        json << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%S");
        json << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z\"";

        // Level
        json << ",\"level\":\"" << logLevelToString(level) << "\"";

        // Message
        json << ",\"message\":\"" << escapeJson(message) << "\"";

        // Optional fields
        if (!provider.empty())
            json << ",\"provider\":\"" << escapeJson(provider) << "\"";
        if (!operation.empty())
            json << ",\"operation\":\"" << escapeJson(operation) << "\"";
        if (!packageId.empty())
            json << ",\"packageId\":\"" << escapeJson(packageId) << "\"";
        if (!component.empty())
            json << ",\"component\":\"" << escapeJson(component) << "\"";
        if (!errorCode.empty())
            json << ",\"errorCode\":\"" << escapeJson(errorCode) << "\"";
        if (!rawStderr.empty())
            json << ",\"stderr\":\"" << escapeJson(rawStderr) << "\"";
        if (exitCode != 0)
            json << ",\"exitCode\":" << exitCode;
        if (duration.count() > 0)
            json << ",\"durationMs\":" << duration.count();

        // Custom fields
        for (const auto& [key, value] : fields) {
            json << ",\"" << escapeJson(key) << "\":\"" << escapeJson(value) << "\"";
        }

        json << "}";
        return json.str();
    }

    // Human-readable format for UI
    std::string toReadable() const {
        std::ostringstream ss;

        auto time = std::chrono::system_clock::to_time_t(timestamp);
        ss << std::put_time(std::localtime(&time), "%H:%M:%S");

        ss << " [" << logLevelToString(level) << "]";

        if (!provider.empty()) {
            ss << " [" << provider << "]";
        }

        if (!operation.empty()) {
            ss << " " << operation;
        }

        if (!packageId.empty()) {
            ss << " (" << packageId << ")";
        }

        ss << ": " << message;

        if (duration.count() > 0) {
            ss << " (" << duration.count() << "ms)";
        }

        return ss.str();
    }

private:
    static std::string escapeJson(const std::string& s) {
        std::ostringstream escaped;
        for (char c : s) {
            switch (c) {
                case '"':  escaped << "\\\""; break;
                case '\\': escaped << "\\\\"; break;
                case '\b': escaped << "\\b"; break;
                case '\f': escaped << "\\f"; break;
                case '\n': escaped << "\\n"; break;
                case '\r': escaped << "\\r"; break;
                case '\t': escaped << "\\t"; break;
                default:
                    if (c < 0x20) {
                        escaped << "\\u" << std::hex << std::setfill('0')
                                << std::setw(4) << static_cast<int>(c);
                    } else {
                        escaped << c;
                    }
            }
        }
        return escaped.str();
    }
};

// ============================================================================
// Log Sink Interface
// ============================================================================

/**
 * LogSink - Interface for log output destinations
 */
class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(const LogEntry& entry) = 0;
    virtual void flush() {}
};

/**
 * FileSink - Write logs to a file
 */
class FileSink : public LogSink {
public:
    explicit FileSink(const std::string& path) : _path(path) {
        _file.open(path, std::ios::app);
    }

    ~FileSink() override {
        if (_file.is_open()) {
            _file.close();
        }
    }

    void write(const LogEntry& entry) override {
        if (_file.is_open()) {
            _file << entry.toJson() << "\n";
        }
    }

    void flush() override {
        if (_file.is_open()) {
            _file.flush();
        }
    }

private:
    std::string _path;
    std::ofstream _file;
};

/**
 * ConsoleSink - Write logs to console
 */
class ConsoleSink : public LogSink {
public:
    void write(const LogEntry& entry) override {
        std::cerr << entry.toReadable() << std::endl;
    }
};

/**
 * MemorySink - Keep logs in memory for debug panel
 */
class MemorySink : public LogSink {
public:
    explicit MemorySink(size_t maxEntries = 1000) : _maxEntries(maxEntries) {}

    void write(const LogEntry& entry) override {
        std::lock_guard<std::mutex> lock(_mutex);
        _entries.push_back(entry);
        while (_entries.size() > _maxEntries) {
            _entries.pop_front();
        }
    }

    std::vector<LogEntry> getEntries(size_t count = 0) const {
        std::lock_guard<std::mutex> lock(_mutex);
        if (count == 0 || count >= _entries.size()) {
            return std::vector<LogEntry>(_entries.begin(), _entries.end());
        }
        auto start = _entries.end() - count;
        return std::vector<LogEntry>(start, _entries.end());
    }

    std::vector<LogEntry> getEntriesFiltered(
        LogLevel minLevel = LogLevel::DEBUG,
        const std::string& provider = "",
        const std::string& operation = "") const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<LogEntry> filtered;
        for (const auto& entry : _entries) {
            if (entry.level < minLevel) continue;
            if (!provider.empty() && entry.provider != provider) continue;
            if (!operation.empty() && entry.operation != operation) continue;
            filtered.push_back(entry);
        }
        return filtered;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(_mutex);
        _entries.clear();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _entries.size();
    }

private:
    size_t _maxEntries;
    std::deque<LogEntry> _entries;
    mutable std::mutex _mutex;
};

// ============================================================================
// Logger
// ============================================================================

/**
 * Logger - Main logging class
 *
 * Thread-safe singleton with multiple sinks.
 */
class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    // Add a sink
    void addSink(std::shared_ptr<LogSink> sink) {
        std::lock_guard<std::mutex> lock(_mutex);
        _sinks.push_back(std::move(sink));
    }

    // Set minimum log level
    void setMinLevel(LogLevel level) {
        _minLevel = level;
    }

    LogLevel getMinLevel() const {
        return _minLevel;
    }

    // Log an entry
    void log(const LogEntry& entry) {
        if (entry.level < _minLevel) return;

        std::lock_guard<std::mutex> lock(_mutex);
        for (auto& sink : _sinks) {
            sink->write(entry);
        }
    }

    // Convenience methods
    void debug(const std::string& msg) { log(LogLevel::DEBUG, msg); }
    void info(const std::string& msg) { log(LogLevel::INFO, msg); }
    void warn(const std::string& msg) { log(LogLevel::WARN, msg); }
    void error(const std::string& msg) { log(LogLevel::ERROR, msg); }
    void fatal(const std::string& msg) { log(LogLevel::FATAL, msg); }

    void log(LogLevel level, const std::string& msg) {
        LogEntry entry;
        entry.level = level;
        entry.message = msg;
        log(entry);
    }

    // Flush all sinks
    void flush() {
        std::lock_guard<std::mutex> lock(_mutex);
        for (auto& sink : _sinks) {
            sink->flush();
        }
    }

    // Get memory sink for debug panel
    std::shared_ptr<MemorySink> getMemorySink() {
        return _memorySink;
    }

private:
    Logger() : _minLevel(LogLevel::INFO) {
        // Default sinks
        _memorySink = std::make_shared<MemorySink>(1000);
        _sinks.push_back(_memorySink);
        _sinks.push_back(std::make_shared<ConsoleSink>());
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::mutex _mutex;
    std::vector<std::shared_ptr<LogSink>> _sinks;
    std::shared_ptr<MemorySink> _memorySink;
    LogLevel _minLevel;
};

// ============================================================================
// Log Builder
// ============================================================================

/**
 * LogBuilder - Fluent interface for building log entries
 */
class LogBuilder {
public:
    LogBuilder(LogLevel level) {
        _entry.level = level;
    }

    LogBuilder& message(const std::string& msg) {
        _entry.message = msg;
        return *this;
    }

    LogBuilder& provider(const std::string& p) {
        _entry.provider = p;
        return *this;
    }

    LogBuilder& operation(const std::string& op) {
        _entry.operation = op;
        return *this;
    }

    LogBuilder& package(const std::string& pkg) {
        _entry.packageId = pkg;
        return *this;
    }

    LogBuilder& component(const std::string& comp) {
        _entry.component = comp;
        return *this;
    }

    LogBuilder& errorCode(const std::string& code) {
        _entry.errorCode = code;
        return *this;
    }

    LogBuilder& stderr(const std::string& err) {
        _entry.rawStderr = err;
        return *this;
    }

    LogBuilder& exitCode(int code) {
        _entry.exitCode = code;
        return *this;
    }

    LogBuilder& duration(std::chrono::milliseconds d) {
        _entry.duration = d;
        return *this;
    }

    LogBuilder& field(const std::string& key, const std::string& value) {
        _entry.fields[key] = value;
        return *this;
    }

    void emit() {
        Logger::instance().log(_entry);
    }

    LogEntry build() const {
        return _entry;
    }

private:
    LogEntry _entry;
};

// ============================================================================
// Scoped Timer for Operations
// ============================================================================

/**
 * ScopedLogTimer - Automatically logs operation duration
 */
class ScopedLogTimer {
public:
    ScopedLogTimer(LogLevel level,
                   const std::string& operation,
                   const std::string& provider = "",
                   const std::string& packageId = "")
        : _level(level)
        , _operation(operation)
        , _provider(provider)
        , _packageId(packageId)
        , _start(std::chrono::steady_clock::now())
    {}

    ~ScopedLogTimer() {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - _start);

        LogBuilder(_level)
            .message(_operation + " completed")
            .operation(_operation)
            .provider(_provider)
            .package(_packageId)
            .duration(duration)
            .emit();
    }

    // Allow marking as failed
    void fail(const std::string& error, const std::string& stderr_ = "") {
        _failed = true;
        _error = error;
        _stderr = stderr_;
    }

private:
    LogLevel _level;
    std::string _operation;
    std::string _provider;
    std::string _packageId;
    std::chrono::steady_clock::time_point _start;
    bool _failed = false;
    std::string _error;
    std::string _stderr;
};

// ============================================================================
// Convenience Macros
// ============================================================================

#define LOG_DEBUG(msg) PolySynaptic::Logger::instance().debug(msg)
#define LOG_INFO(msg)  PolySynaptic::Logger::instance().info(msg)
#define LOG_WARN(msg)  PolySynaptic::Logger::instance().warn(msg)
#define LOG_ERROR(msg) PolySynaptic::Logger::instance().error(msg)
#define LOG_FATAL(msg) PolySynaptic::Logger::instance().fatal(msg)

#define LOG(level) PolySynaptic::LogBuilder(level)

} // namespace PolySynaptic

#endif // _STRUCTUREDLOG_H_

// vim:ts=4:sw=4:et
