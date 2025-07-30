#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <fstream>

class Log {
public:
    Log();
    ~Log();

    void write(const std::wstring& message);
    void shutdown();

private:
    void process();
    void pipeListener();

    std::thread m_thread;
    std::thread m_pipeThread;
    HANDLE m_stopEvent = NULL;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<std::wstring> m_queue;
    std::wofstream m_file;
    bool m_running = false;
};

extern Log g_log;

// Verbose logging flag shared across modules
extern std::atomic<bool> g_verbose;

// Exported helper for modules that share the logging system
extern "C" __declspec(dllexport) void WriteLog(const wchar_t* message);
// Exported helper to toggle debug logging state in the DLL
extern "C" __declspec(dllexport) void SetDebugLoggingEnabled(bool enabled);
