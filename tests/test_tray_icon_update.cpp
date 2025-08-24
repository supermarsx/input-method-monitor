#include <catch2/catch_test_macros.hpp>
#include "../source/tray_icon.h"
#include "../source/configuration.h"
#include "../source/app_state.h"
#include <atomic>
#include <memory>
#include <set>
#include <string>

// Globals provided elsewhere
extern HINSTANCE g_hInst;
extern std::unique_ptr<TrayIcon> g_trayIcon;

// Stub LoadImageW to capture icon path
static std::wstring g_loadedPath;
static HANDLE DummyLoadImage2(HINSTANCE, LPCWSTR path, UINT, int, int, UINT) {
    g_loadedPath = path ? path : L"";
    return reinterpret_cast<HANDLE>(1);
}
extern HANDLE (*pLoadImageW)(HINSTANCE, LPCWSTR, UINT, int, int, UINT);

// Track tooltip and message type
static std::wstring g_lastTip;
static DWORD g_lastMsg;
static BOOL FakeShellNotifyIcon2(DWORD msg, PNOTIFYICONDATA data) {
    g_lastMsg = msg;
    if (msg == NIM_ADD || msg == NIM_MODIFY) {
        g_lastTip = data->szTip;
    }
    return TRUE;
}
extern BOOL (WINAPI *pShell_NotifyIcon)(DWORD, PNOTIFYICONDATA);

// Minimal ApplyConfig implementation used for testing
void ApplyConfig(HWND hwnd) {
    static std::wstring lastIcon;
    static std::wstring lastTip;
    auto iconVal = g_config.get(L"icon_path");
    auto tipVal = g_config.get(L"tray_tooltip");
    std::wstring icon = iconVal ? *iconVal : L"";
    std::wstring tip = tipVal ? *tipVal : L"";
    if (g_trayIcon && (icon != lastIcon || tip != lastTip)) {
        g_trayIcon->Update(icon, tip);
        lastIcon = icon;
        lastTip = tip;
    }
}

TEST_CASE("ApplyConfig updates tray icon on config change") {
    g_loadedPath.clear();
    g_lastTip.clear();
    g_lastMsg = 0;
    pShell_NotifyIcon = FakeShellNotifyIcon2;
    pLoadImageW = DummyLoadImage2;

    g_config.set(L"icon_path", L"icon1.ico");
    g_config.set(L"tray_tooltip", L"Tip1");
    g_trayIcon = std::make_unique<TrayIcon>(reinterpret_cast<HWND>(1));
    REQUIRE(g_loadedPath == L"icon1.ico");
    REQUIRE(g_lastTip == L"Tip1");

    g_config.set(L"icon_path", L"icon2.ico");
    g_config.set(L"tray_tooltip", L"Tip2");
    ApplyConfig(reinterpret_cast<HWND>(1));
    REQUIRE(g_loadedPath == L"icon2.ico");
    REQUIRE(g_lastTip == L"Tip2");
    REQUIRE(g_lastMsg == NIM_MODIFY);

    g_trayIcon.reset();
}
