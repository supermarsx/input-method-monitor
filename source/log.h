#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <fstream>

/**
 * @brief Threaded log writer used by the application and hook DLL.
 */
class Log {
public:
    /// Construct the logging subsystem and start worker threads.
    Log();
    /// Ensure worker threads are stopped and the log file closed.
    ~Log();

    /**
     * @brief Queue a message for asynchronous logging.
     * @param message Text to append to the log file.
     * @sideeffects Signals the worker thread to write the entry.
     */
    void write(const std::wstring& message);

    /// Flush queued messages and terminate the worker threads.
    void shutdown();

private:
    /// Background worker that writes queued messages to disk.
    void process();
    /// Listener thread that accepts messages via a named pipe.
    void pipeListener();

    std::thread m_thread;      ///< Log writer thread.
    std::thread m_pipeThread;  ///< Named pipe listener thread.
    HANDLE m_stopEvent = NULL; ///< Event to wake threads for shutdown.
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<std::wstring> m_queue;
    std::wofstream m_file;
    bool m_running = false;
};

/// Global log instance used by all modules.
extern Log g_log;

/**
 * @brief Write a message to the shared log.
 * @param message Null‑terminated string to append.
 */
extern "C" __declspec(dllexport) void WriteLog(const wchar_t* message);

/**
 * @brief Enable or disable debug logging in the DLL.
 * @param enabled Set to @c true to allow log messages to be recorded.
 */
extern "C" __declspec(dllexport) void SetDebugLoggingEnabled(bool enabled);
