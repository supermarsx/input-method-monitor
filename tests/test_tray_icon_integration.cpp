#include <catch2/catch_test_macros.hpp>
#include "../source/tray_icon.h"
#include "../source/configuration.h"
#include <set>
#include <atomic>
#include <memory>
#include <vector>
#include <string>

// Globals expected by tray_icon and kbdlayoutmon
extern HINSTANCE g_hInst;
extern std::atomic<bool> g_trayIconEnabled;
extern std::atomic<bool> g_debugEnabled;
std::unique_ptr<TrayIcon> g_trayIcon;
extern std::wstring g_cliIconPath;
extern std::wstring g_cliTrayTooltip;
extern HANDLE (*pLoadImageW)(HINSTANCE, LPCWSTR, UINT, int, int, UINT);

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

// Stubs for CLI override testing
static std::wstring g_loadedPath3;
static std::wstring g_lastTip3;
static HANDLE DummyLoadImage3(HINSTANCE, LPCWSTR path, UINT, int, int, UINT) {
    g_loadedPath3 = path ? path : L"";
    return reinterpret_cast<HANDLE>(1);
}
static BOOL FakeShellNotifyIcon3(DWORD msg, PNOTIFYICONDATA data) {
    if (msg == NIM_ADD) {
        g_lastTip3 = data->szTip;
    }
    return TRUE;
}

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

// Minimal ApplyConfig used for override testing
static void ApplyConfigTest(HWND hwnd) {
    if (!g_trayIconEnabled.load()) return;
    auto iconVal = g_config.get(L"icon_path");
    auto tipVal = g_config.get(L"tray_tooltip");
    std::wstring icon = iconVal ? *iconVal : L"";
    std::wstring tip = tipVal ? *tipVal : L"";
    if (!g_trayIcon) {
        g_trayIcon = std::make_unique<TrayIcon>(hwnd);
    } else {
        g_trayIcon->Update(icon, tip);
    }
}

TEST_CASE("CLI overrides applied to new tray icons") {
    g_loadedPath3.clear();
    g_lastTip3.clear();
    g_trayIcon.reset();
    g_cliIconPath.clear();
    g_cliTrayTooltip.clear();
    pShell_NotifyIcon = FakeShellNotifyIcon3;
    pLoadImageW = DummyLoadImage3;

    // Simulate parsing command line with overrides
    std::vector<std::wstring> argv = {L"app", L"--icon-path", L"cli.ico", L"--tray-tooltip", L"CliTip"};
    for (size_t i = 1; i < argv.size(); ++i) {
        if (argv[i] == L"--icon-path" && i + 1 < argv.size()) {
            g_config.set(L"icon_path", argv[i + 1]);
            g_cliIconPath = argv[i + 1];
            ++i;
        } else if (argv[i] == L"--tray-tooltip" && i + 1 < argv.size()) {
            g_config.set(L"tray_tooltip", argv[i + 1]);
            g_cliTrayTooltip = argv[i + 1];
            ++i;
        }
    }

    // Simulate config reload dropping entries but overrides persist
    g_config.set(L"icon_path", L"");
    g_config.set(L"tray_tooltip", L"");

    ApplyConfigTest(reinterpret_cast<HWND>(1));
    REQUIRE(g_loadedPath3 == L"cli.ico");
    REQUIRE(g_lastTip3 == L"CliTip");

    g_trayIcon.reset();
    g_cliIconPath.clear();
    g_cliTrayTooltip.clear();
}
