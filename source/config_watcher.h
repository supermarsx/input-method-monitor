#pragma once

#include <windows.h>
#include "unique_handle.h"

/**
 * @brief RAII helper that watches the configuration file for changes.
 *
 * The watcher starts monitoring in the constructor and terminates the
 * background thread when destroyed.
 */
class ConfigWatcher {
public:
    /// Begin watching for configuration changes affecting @p hwnd.
    explicit ConfigWatcher(HWND hwnd);
    /// Stop the watcher thread and clean up resources.
    ~ConfigWatcher();

    ConfigWatcher(const ConfigWatcher&) = delete;
    ConfigWatcher& operator=(const ConfigWatcher&) = delete;

private:
    static DWORD WINAPI threadProc(LPVOID param);

    HWND m_hwnd;            ///< Window receiving update notifications.
    UniqueHandle m_thread;  ///< Background thread handle.
    UniqueHandle m_stopEvent; ///< Event used to signal thread shutdown.
};

