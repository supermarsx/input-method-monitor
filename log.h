#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

class Log {
public:
    Log();
    ~Log();

    void write(const std::wstring& message);
    void shutdown();

private:
    void process();

    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<std::wstring> m_queue;
    bool m_running = false;
};

extern Log g_log;
