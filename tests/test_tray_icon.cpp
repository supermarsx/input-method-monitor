#include <catch2/catch_test_macros.hpp>
#include "../source/tray_icon.h"
#include <set>
#include <stdexcept>
#include <atomic>

// Provide definitions for globals expected by tray_icon
HINSTANCE g_hInst = nullptr;
std::atomic<bool> g_trayIconEnabled{true};

// Track icon IDs added and removed
static std::set<UINT> g_icons;

BOOL FakeShellNotifyIcon(DWORD msg, PNOTIFYICONDATA data) {
    if (msg == NIM_ADD) {
        g_icons.insert(data->uID);
    } else if (msg == NIM_DELETE) {
        g_icons.erase(data->uID);
    }
    return TRUE;
}

TEST_CASE("TrayIcon removes icon on destruction during exception") {
    g_icons.clear();
    pShell_NotifyIcon = FakeShellNotifyIcon;
    try {
        TrayIcon icon(reinterpret_cast<HWND>(1));
        throw std::runtime_error("boom");
    } catch (const std::exception&) {
        // swallow
    }
    REQUIRE(g_icons.empty());
}
