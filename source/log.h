#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <fstream>
#include <atomic>
#include <utility>

/**
 * @brief Severity levels for log messages.
 */
enum class LogLevel {
    Info,
    Warn,
    Error
};

#ifdef _WIN32
#  include <windows.h>
#  include "handle_guard.h"
#else
using HANDLE = void*;
#endif

/**
 * @brief Threaded log writer used by the application and hook DLL.
 */
class Log {
public:
    /**
     * @brief Construct the logging subsystem.
     *
     * @param maxQueueSize Maximum number of messages that may be queued
     *        before the oldest entry is discarded. Defaults to 1000
     *        messages.
     * @param startThreads Whether worker threads should be started
     *        immediately. Tests can disable this to inspect the queue
     *        without asynchronous processing.
     */
    Log(size_t maxQueueSize = 1000, bool startThreads = true);
    /// Ensure worker threads are stopped and the log file closed.
    ~Log();

    /**
     * @brief Queue a message for asynchronous logging.
     *
     * When the queue already contains the configured maximum number of
     * entries, the oldest message is dropped to make room for the new
     * one.
     *
     * @param level   Severity of the message.
     * @param message Text to append to the log file.
     * @sideeffects Signals the worker thread to write the entry.
     */
    void write(LogLevel level, const std::wstring& message);
    void write(const std::wstring& message);

    /// Adjust the maximum number of queued messages.
    void setMaxQueueSize(size_t maxSize);

    /// Obtain the number of messages currently queued (primarily for tests).
    size_t queueSize() const;

    /// Peek at the oldest queued message (primarily for tests).
    std::wstring peekOldest() const;

    /// Flush queued messages and terminate the worker threads.
    void shutdown();

private:
    /// Background worker that writes queued messages to disk.
    void process();
    /// Listener thread that accepts messages via a named pipe.
    void pipeListener();

    std::thread m_thread;      ///< Log writer thread.
#ifdef _WIN32
    std::thread m_pipeThread; ///< Named pipe listener thread.
    HandleGuard m_stopEvent;  ///< Event to wake threads for shutdown.
#endif
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<std::pair<LogLevel, std::wstring>> m_queue;
    std::wofstream m_file;
    bool m_running = false;
    size_t m_maxQueueSize = 1000;
    size_t m_suppress = 0; ///< Internal log entries pending that should not trigger rotation.
};

/// Global log instance used by all modules.
extern Log g_log;

/**
 * @brief Write a message to the shared log.
 * @param message Null‑terminated string to append.
 */
#ifdef _WIN32
__declspec(dllexport) void WriteLog(LogLevel level, const wchar_t* message);
extern "C" __declspec(dllexport) void WriteLog(const wchar_t* message);
#else
void WriteLog(LogLevel level, const wchar_t* message);
extern "C" void WriteLog(const wchar_t* message);
#endif

// Non-inline overloads accepting std::wstring. Implemented in log.cpp.
void WriteLog(LogLevel level, const std::wstring& message);
void WriteLog(const std::wstring& message);


/**
 * @brief Enable or disable debug logging in the DLL.
 * @param enabled Set to @c true to allow log messages to be recorded.
 */
#ifdef _WIN32
extern "C" __declspec(dllexport) void SetDebugLoggingEnabled(bool enabled);
#else
extern "C" void SetDebugLoggingEnabled(bool enabled);
#endif

/// Global flag controlling verbose console output.
extern std::atomic<bool> g_verboseLogging;

/// Minimum severity that will be recorded in the log.
extern std::atomic<LogLevel> g_logLevel;
