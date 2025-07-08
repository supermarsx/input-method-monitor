#include "log.h"
#include <windows.h>
#include <fstream>
#include <Shlwapi.h>
#include <utility>

extern HINSTANCE g_hInst; // Provided by the executable or DLL
extern bool g_debugEnabled;

namespace {
std::wstring GetLogPath() {
    wchar_t logPath[MAX_PATH] = {0};
    if (g_hInst) {
        GetModuleFileName(g_hInst, logPath, MAX_PATH);
        PathRemoveFileSpec(logPath);
        PathCombine(logPath, logPath, L"kbdlayoutmon.log");
    } else {
        lstrcpyW(logPath, L"kbdlayoutmon.log");
    }
    return logPath;
}
}

Log g_log;

Log::Log() {
    m_running = true;
    m_thread = std::thread(&Log::process, this);
}

Log::~Log() {
    shutdown();
}

void Log::shutdown() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running = false;
    }
    m_cv.notify_all();
    if (m_thread.joinable())
        m_thread.join();
}

void Log::write(const std::wstring& message) {
    if (!g_debugEnabled)
        return;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(message);
    }
    m_cv.notify_one();
}

void Log::process() {
    for (;;) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_queue.empty() || !m_running; });
        while (!m_queue.empty()) {
            std::wstring msg = std::move(m_queue.front());
            m_queue.pop();
            lock.unlock();

            std::wstring path = GetLogPath();
            std::wofstream logFile(path, std::ios::app);
            if (logFile.is_open()) {
                logFile << msg << std::endl;
            } else {
                OutputDebugString(L"Failed to open log file.");
            }
            lock.lock();
        }
        if (!m_running && m_queue.empty())
            break;
    }
}

