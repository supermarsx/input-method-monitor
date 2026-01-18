#include "windows_stub.h"
#include "../source/config_watcher.h"
#include <atomic>
#include <chrono>
#include <thread>

// Minimal in-test implementation of ConfigWatcher for Windows unit tests.
// This avoids linking source/config_watcher.cpp which depends on platform APIs.

extern int applyCalls;
extern int g_sleepCalls;
extern std::atomic<bool> g_stopRequested;

ConfigWatcher::ConfigWatcher(HWND hwnd) : m_hwnd(hwnd), m_stopEvent(nullptr) {
    g_stopRequested.store(false);
    m_stopEvent.reset(CreateEventW(NULL, TRUE, FALSE, NULL));
    m_thread = std::thread(&ConfigWatcher::threadProc, this);
}

ConfigWatcher::~ConfigWatcher() {
    g_stopRequested.store(true);
    if (m_stopEvent) {
        SetEvent(m_stopEvent.get());
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_stopEvent.reset();
}

void ConfigWatcher::threadProc(ConfigWatcher* self) {
    UNREFERENCED_PARAMETER(self);
    HANDLE hDir = pCreateFileW(
        L".",
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL);

    if (!hDir || hDir == INVALID_HANDLE_VALUE) {
        ++g_sleepCalls;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return;
    }

    if (!g_stopRequested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ++applyCalls;
    }

    CloseHandle(hDir);
}
