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
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<std::wstring> m_queue;
    std::wofstream m_file;
    bool m_running = false;
};

extern Log g_log;

// Exported helper for modules that share the logging system
extern "C" __declspec(dllexport) void WriteLog(const wchar_t* message);
