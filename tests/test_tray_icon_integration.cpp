#include <catch2/catch_test_macros.hpp>
#include "../source/tray_icon.h"
#include <set>
#include <atomic>
#include <memory>

// Globals expected by tray_icon and kbdlayoutmon
extern HINSTANCE g_hInst;
extern std::atomic<bool> g_trayIconEnabled;
extern std::atomic<bool> g_debugEnabled;
std::unique_ptr<TrayIcon> g_trayIcon;

// Track icon handles via mock Shell_NotifyIcon
static std::set<UINT> g_icons2;
BOOL FakeShellNotifyIcon2(DWORD msg, PNOTIFYICONDATA data) {
    if (msg == NIM_ADD) {
        g_icons2.insert(data->uID);
    } else if (msg == NIM_DELETE) {
        g_icons2.erase(data->uID);
    }
    return TRUE;
}
extern BOOL (WINAPI *pShell_NotifyIcon)(DWORD, PNOTIFYICONDATA);

struct TrayIconGuard {
    ~TrayIconGuard() { g_trayIcon.reset(); }
};

// Function that mimics early exit after tray icon creation
static void SimulateEarlyExit() {
    TrayIconGuard guard;
    g_trayIcon = std::make_unique<TrayIcon>(reinterpret_cast<HWND>(1));
    return; // exiting without explicit removal
}

TEST_CASE("Tray icon removed on early exit") {
    g_icons2.clear();
    pShell_NotifyIcon = FakeShellNotifyIcon2;
    SimulateEarlyExit();
    REQUIRE(g_icons2.empty());
}
