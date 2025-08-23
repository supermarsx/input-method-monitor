#ifdef _WIN32
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

    const DWORD kMaxBackoff = 1000;
    DWORD backoff = 50;
    // Continuously attempt to monitor the directory until the stop event is signalled.
    for (;;) {
        HandleGuard hDir(CreateFileW(
            dirPath, FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL));

        if (!hDir) {
            WriteLog(LogLevel::Error, L"CreateFileW failed for configuration directory.");
            if (WaitForSingleObject(self->m_stopEvent.get(), 0) == WAIT_OBJECT_0)
                break;
            Sleep(backoff);
            if (backoff < kMaxBackoff) {
                backoff *= 2;
                if (backoff > kMaxBackoff)
                    backoff = kMaxBackoff;
            }
            continue;
        }

        backoff = 50;

        BYTE buffer[1024];
        OVERLAPPED ov{};
        HandleGuard hEvent(CreateEventW(NULL, FALSE, FALSE, NULL));
        if (!hEvent) {
            WriteLog(LogLevel::Error, L"Failed to create directory watch event.");
            break;
        }
        ov.hEvent = hEvent.get();

        HANDLE handles[2] = { hEvent.get(), self->m_stopEvent.get() };
        bool restart = false;
        while (!restart) {
            DWORD bytesReturned = 0;
            BOOL ok = ReadDirectoryChangesW(
                hDir.get(), buffer, sizeof(buffer), FALSE,
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
                &bytesReturned, &ov, NULL);

            if (!ok) {
                WriteLog(LogLevel::Error, L"ReadDirectoryChangesW failed.");
                restart = true;
                break;
            }

            DWORD wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
            if (wait == WAIT_OBJECT_0 + 1) {
                CancelIoEx(hDir.get(), &ov);
                WaitForSingleObject(hEvent.get(), INFINITE);
                return;
            }
            if (wait != WAIT_OBJECT_0) {
                WriteLog(LogLevel::Error, L"WaitForMultipleObjects failed.");
                restart = true;
                break;
            }

            bytesReturned = 0;
            if (!GetOverlappedResult(hDir.get(), &ov, &bytesReturned, FALSE)) {
                WriteLog(LogLevel::Error, L"GetOverlappedResult failed.");
                restart = true;
                break;
            }

            bool renamed = false;
            BYTE* ptr = buffer;
            while (bytesReturned > 0) {
                auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(ptr);
                if (info->Action == FILE_ACTION_RENAMED_OLD_NAME ||
                    info->Action == FILE_ACTION_RENAMED_NEW_NAME) {
                    renamed = true;
                }
                if (info->NextEntryOffset == 0)
                    break;
                bytesReturned -= info->NextEntryOffset;
                ptr += info->NextEntryOffset;
            }

            g_config.load();
            ApplyConfig(self->m_hwnd);
            WriteLog(LogLevel::Info, L"Configuration reloaded.");

            if (renamed)
                restart = true;
        }
    }
}
#endif
