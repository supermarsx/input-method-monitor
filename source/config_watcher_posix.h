#pragma once

#ifndef _WIN32
#include <thread>
#include <atomic>

// Dummy HWND type for non-Windows builds
using HWND = void*;

class ConfigWatcher {
public:
    explicit ConfigWatcher(HWND hwnd);
    ~ConfigWatcher();

    ConfigWatcher(const ConfigWatcher&) = delete;
    ConfigWatcher& operator=(const ConfigWatcher&) = delete;

private:
    void threadProc();

    HWND m_hwnd;
    std::thread m_thread;
    int m_fd{-1};
    int m_wd{-1};
    std::atomic<bool> m_stop{false};
};

#endif // !_WIN32
