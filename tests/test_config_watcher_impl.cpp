#include "windows_stub.h"
#include "../source/config_watcher.h"

// Minimal in-test implementation of ConfigWatcher for Windows unit tests.
// This avoids linking source/config_watcher.cpp which depends on platform APIs.

ConfigWatcher::ConfigWatcher(HWND hwnd) : m_hwnd(hwnd), m_stopEvent(nullptr) {
    // Do not start a background thread in tests; keep it minimal.
}

ConfigWatcher::~ConfigWatcher() {
    // No-op
}
