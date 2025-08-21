#include "config_watcher.h"

#include <string>
#include <shlwapi.h>
#include <system_error>

#include "configuration.h"
#include "log.h"

// g_hInst is defined in kbdlayoutmon.cpp
extern HINSTANCE g_hInst;
// ApplyConfig is implemented in kbdlayoutmon.cpp
void ApplyConfig(HWND hwnd);

ConfigWatcher::ConfigWatcher(HWND hwnd) : m_hwnd(hwnd) {
    m_stopEvent.reset(CreateEventW(NULL, TRUE, FALSE, NULL));
    try {
        m_thread = std::thread(&ConfigWatcher::threadProc, this);
    } catch (const std::system_error&) {
        WriteLog(LogLevel::Error, L"Failed to create watcher thread.");
        throw;
    }
}

ConfigWatcher::~ConfigWatcher() {
    if (m_stopEvent) {
        SetEvent(m_stopEvent.get());
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_stopEvent.reset();
}

void ConfigWatcher::threadProc(ConfigWatcher* self) {
    wchar_t dirPath[MAX_PATH];
    std::wstring cfgPath = g_config.getLastPath();
    if (cfgPath.empty()) {
        GetModuleFileNameW(g_hInst, dirPath, MAX_PATH);
    } else {
        wcsncpy(dirPath, cfgPath.c_str(), MAX_PATH);
        dirPath[MAX_PATH - 1] = L'\0';
    }
    PathRemoveFileSpecW(dirPath);

    HandleGuard hChange(FindFirstChangeNotificationW(dirPath, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE));
    if (!hChange)
        return;

    HANDLE handles[2] = { hChange.get(), self->m_stopEvent.get() };
    for (;;) {
        DWORD wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (wait == WAIT_OBJECT_0) {
            g_config.load();
            ApplyConfig(self->m_hwnd);
            WriteLog(LogLevel::Info, L"Configuration reloaded.");
            BOOL ok = FindNextChangeNotification(hChange.get());
            if (!ok) {
                WriteLog(LogLevel::Error, L"FindNextChangeNotification failed.");
                break;
            }
        } else if (wait == WAIT_OBJECT_0 + 1) {
            break;
        } else {
            break;
        }
    }

    FindCloseChangeNotification(hChange.release());
    return;
}

