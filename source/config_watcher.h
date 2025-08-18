#pragma once

#include <windows.h>
#include <thread>
#include "handle_guard.h"

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
    static void threadProc(ConfigWatcher* self);

    HWND m_hwnd;            ///< Window receiving update notifications.
    std::thread m_thread;   ///< Background thread.
    HandleGuard m_stopEvent; ///< Event used to signal thread shutdown.
};

