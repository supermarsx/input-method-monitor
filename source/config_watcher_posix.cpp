#include "config_watcher_posix.h"

#ifndef _WIN32
#include "configuration.h"
#include "log.h"

#include <sys/inotify.h>
#include <unistd.h>
#include <filesystem>
#include <vector>
#include <chrono>
#include <string>
#include <limits.h>

void ApplyConfig(HWND);
// Optional hook for tests to observe configuration reloads.
void (*g_testApplyConfig)(HWND) = nullptr;

ConfigWatcher::ConfigWatcher(HWND hwnd) : m_hwnd(hwnd) {
    m_thread = std::thread(&ConfigWatcher::threadProc, this);
}

ConfigWatcher::~ConfigWatcher() {
    m_stop = true;
    if (m_wd >= 0)
        inotify_rm_watch(m_fd, m_wd);
    if (m_fd >= 0)
        close(m_fd);
    if (m_thread.joinable())
        m_thread.join();
}

void ConfigWatcher::threadProc() {
    namespace fs = std::filesystem;
    std::wstring cfgPathW = g_config.getLastPath();
    std::string cfgPath(cfgPathW.begin(), cfgPathW.end());
    if (cfgPath.empty())
        cfgPath = "kbdlayoutmon.config";
    std::string dir = fs::path(cfgPath).parent_path().string();
    if (dir.empty())
        dir = ".";

    m_fd = inotify_init1(0);
    if (m_fd < 0)
        return;

    m_wd = inotify_add_watch(m_fd, dir.c_str(), IN_CLOSE_WRITE | IN_MOVED_TO | IN_MOVED_FROM);
    if (m_wd < 0)
        return;

    std::vector<char> buffer(sizeof(struct inotify_event) + NAME_MAX + 1);
    while (!m_stop) {
        ssize_t len = read(m_fd, buffer.data(), buffer.size());
        if (len <= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        for (char* ptr = buffer.data(); ptr < buffer.data() + len; ) {
            auto* ev = reinterpret_cast<struct inotify_event*>(ptr);
            if (ev->mask & (IN_CLOSE_WRITE | IN_MOVED_TO | IN_MOVED_FROM)) {
                g_config.load();
                ApplyConfig(m_hwnd);
                if (g_testApplyConfig)
                    g_testApplyConfig(m_hwnd);
                WriteLog(LogLevel::Info, L"Configuration reloaded.");
            }
            ptr += sizeof(struct inotify_event) + ev->len;
        }
    }
}

#endif // !_WIN32
