#include <catch2/catch_test_macros.hpp>
#include "../source/tray_icon.h"
#include "../source/configuration.h"
#include "../source/app_state.h"
#include <set>
#include <stdexcept>
#include <atomic>
#include <string>

// Provide definitions for globals expected by tray_icon (delegated to tests/stubs.cpp)
extern HINSTANCE g_hInst;
// Stub for LoadImageW used by tray icon (per-test instance)
static std::wstring g_loadedPath;
static HANDLE DummyLoadImage(HINSTANCE, LPCWSTR path, UINT, int, int, UINT) {
    g_loadedPath = path ? path : L"";
    return reinterpret_cast<HANDLE>(1);
}
// Use the shared pointer stub, tests may override it
extern HANDLE (*pLoadImageW)(HINSTANCE, LPCWSTR, UINT, int, int, UINT);

// Track icon IDs added and removed
static std::set<UINT> g_icons;
static std::wstring g_lastTip;

BOOL FakeShellNotifyIcon(DWORD msg, PNOTIFYICONDATA data) {
    if (msg == NIM_ADD) {
        g_icons.insert(data->uID);
        g_lastTip = data->szTip;
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

TEST_CASE("TrayIcon uses configured icon and tooltip") {
    g_icons.clear();
    g_lastTip.clear();
    g_loadedPath.clear();
    pShell_NotifyIcon = FakeShellNotifyIcon;
    g_config.set(L"icon_path", L"custom.ico");
    g_config.set(L"tray_tooltip", L"CustomTip");
    TrayIcon icon(reinterpret_cast<HWND>(1));
    REQUIRE(g_loadedPath == L"custom.ico");
    REQUIRE(g_lastTip == L"CustomTip");
}
