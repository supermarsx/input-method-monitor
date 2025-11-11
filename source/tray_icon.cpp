#include "tray_icon.h"
#include "res-icon.h"
#include "hotkey_registry.h"
#include "configuration.h"
#include "constants.h"
#include "log.h"
#include "utils.h"
#ifdef _WIN32
#  include <shlwapi.h>
#endif
#include "app_state.h"

#ifndef ARRAYSIZE
#define ARRAYSIZE(A) (sizeof(A) / sizeof((A)[0]))
#endif

#ifndef UNIT_TEST
// Global state defined elsewhere
extern Configuration g_config;
#endif

// Command-line overrides populated by kbdlayoutmon.cpp
extern std::wstring g_cliIconPath;
extern std::wstring g_cliTrayTooltip;

BOOL (WINAPI *pShell_NotifyIcon)(DWORD, PNOTIFYICONDATA) = ::Shell_NotifyIcon;

TrayIcon::TrayIcon(HWND hwnd) {
    if (!GetAppState().trayIconEnabled.load()) return;

    ZeroMemory(&nid_, sizeof(nid_));
    nid_.cbSize = sizeof(NOTIFYICONDATA);
    nid_.hWnd = hwnd;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAYICON;
    // Determine icon path, preferring command-line override when present
    std::wstring iconPath = g_cliIconPath;
    if (iconPath.empty()) {
        auto iconVal = g_config.get(L"icon_path");
        if (iconVal && !iconVal->empty())
            iconPath = *iconVal;
    }
    if (!iconPath.empty()) {
        nid_.hIcon = reinterpret_cast<HICON>(
            LoadImageW(nullptr, iconPath.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE));
    }
    if (!nid_.hIcon) {
        nid_.hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_MYAPP));
    }

    // Set tray tooltip from override/config or use default name
    std::wstring tip = g_cliTrayTooltip;
    if (tip.empty()) {
        auto tipVal = g_config.get(L"tray_tooltip");
        if (tipVal && !tipVal->empty())
            tip = *tipVal;
    }
    if (!tip.empty()) {
        wcscpy_s(nid_.szTip, ARRAYSIZE(nid_.szTip), tip.c_str());
    } else {
        wcscpy_s(nid_.szTip, ARRAYSIZE(nid_.szTip), L"kbdlayoutmon");
    }
    pShell_NotifyIcon(NIM_ADD, &nid_);
    added_ = true;
}

TrayIcon::~TrayIcon() {
    if (added_) {
        pShell_NotifyIcon(NIM_DELETE, &nid_);
    }
}

void TrayIcon::Update(const std::wstring& iconPath, const std::wstring& tooltip) {
    if (!added_) return;

    // Prefer CLI overrides over provided parameters
    std::wstring finalIcon = g_cliIconPath.empty() ? iconPath : g_cliIconPath;
    std::wstring finalTip = g_cliTrayTooltip.empty() ? tooltip : g_cliTrayTooltip;

    HICON hIcon = nullptr;
    if (!finalIcon.empty()) {
        hIcon = reinterpret_cast<HICON>(
            LoadImageW(nullptr, finalIcon.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE));
    }
    if (!hIcon) {
        hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_MYAPP));
    }
    nid_.hIcon = hIcon;

    if (!finalTip.empty()) {
        wcscpy_s(nid_.szTip, ARRAYSIZE(nid_.szTip), finalTip.c_str());
    } else {
        wcscpy_s(nid_.szTip, ARRAYSIZE(nid_.szTip), L"kbdlayoutmon");
    }

    pShell_NotifyIcon(NIM_MODIFY, &nid_);
}

#ifndef UNIT_TEST
void ShowTrayMenu(HWND hwnd) {
    if (!GetAppState().trayIconEnabled.load()) return;

    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();

    MENUITEMINFO mii;
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_FTYPE | MIIM_STRING;
    mii.fType = MFT_STRING | MFT_RIGHTJUSTIFY;
    mii.dwTypeData = L"kbdlayoutmon";
    InsertMenuItem(hMenu, ID_TRAY_APP_NAME, TRUE, &mii);
    InsertMenu(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

    auto& state = GetAppState();
    InsertMenu(hMenu, 2, MF_BYPOSITION | MF_STRING | (state.startupEnabled.load() ? MF_CHECKED : 0), ID_TRAY_STARTUP, L"Launch at startup");
    InsertMenu(hMenu, 3, MF_BYPOSITION | MF_STRING | (state.languageHotKeyEnabled.load() ? MF_CHECKED : 0), ID_TRAY_TOGGLE_LANGUAGE, L"Toggle Language HotKey");
    InsertMenu(hMenu, 4, MF_BYPOSITION | MF_STRING | (state.layoutHotKeyEnabled.load() ? MF_CHECKED : 0), ID_TRAY_TOGGLE_LAYOUT, L"Toggle Layout HotKey");
    InsertMenu(hMenu, 5, MF_BYPOSITION | MF_STRING, ID_TRAY_TEMP_ENABLE_HOTKEYS, L"Temporarily Enable HotKeys");
    InsertMenu(hMenu, 6, MF_BYPOSITION | MF_STRING, ID_TRAY_OPEN_LOG, L"Open Log File");
    InsertMenu(hMenu, 7, MF_BYPOSITION | MF_STRING, ID_TRAY_OPEN_CONFIG, L"Open Config File");
    InsertMenu(hMenu, 8, MF_BYPOSITION | MF_STRING | (state.debugEnabled.load() ? MF_CHECKED : 0), ID_TRAY_TOGGLE_DEBUG, L"Debug Logging");
    InsertMenu(hMenu, 9, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenu(hMenu, 10, MF_BYPOSITION | MF_STRING, ID_TRAY_RESTART, L"Restart");
    InsertMenu(hMenu, 11, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Quit");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

// Function pointers declared in main module
extern void (*SetDebugLoggingEnabledPtr)(bool);
extern HMODULE g_hDll; // used for restart? not required, ignore

void HandleTrayCommand(HWND hwnd, WPARAM wParam) {
    switch (LOWORD(wParam)) {
        case ID_TRAY_EXIT:
            PostQuitMessage(0);
            break;
        case ID_TRAY_STARTUP:
            if (GetAppState().startupEnabled.load()) {
                RemoveFromStartup();
            } else {
                AddToStartup();
            }
            break;
        case ID_TRAY_TOGGLE_LANGUAGE:
            ToggleLanguageHotKey(hwnd);
            break;
        case ID_TRAY_TOGGLE_LAYOUT:
            ToggleLayoutHotKey(hwnd);
            break;
        case ID_TRAY_TEMP_ENABLE_HOTKEYS:
            TemporarilyEnableHotKeys(hwnd);
            break;
        case ID_TRAY_OPEN_LOG:
        {
            wchar_t logPath[MAX_PATH] = {0};
            auto val = g_config.get(L"log_path");
            if (val && !val->empty()) {
                lstrcpynW(logPath, val->c_str(), MAX_PATH);
            } else if (g_hInst) {
                GetModuleFileName(g_hInst, logPath, MAX_PATH);
                PathRemoveFileSpec(logPath);
                PathCombine(logPath, logPath, L"kbdlayoutmon.log");
            } else {
                lstrcpyW(logPath, L"kbdlayoutmon.log");
            }
            ShellExecute(NULL, L"open", logPath, NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        case ID_TRAY_OPEN_CONFIG:
        {
            wchar_t cfgPath[MAX_PATH] = {0};
            if (g_hInst) {
                GetModuleFileName(g_hInst, cfgPath, MAX_PATH);
                PathRemoveFileSpec(cfgPath);
                PathCombine(cfgPath, cfgPath, configFile.c_str());
            } else {
                lstrcpynW(cfgPath, configFile.c_str(), MAX_PATH);
            }
            ShellExecute(NULL, L"open", cfgPath, NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        case ID_TRAY_TOGGLE_DEBUG:
            if (GetAppState().debugEnabled.load()) {
                WriteLog(LogLevel::Info, L"Debug logging disabled.");
                GetAppState().debugEnabled.store(false);
                if (SetDebugLoggingEnabled)
                    SetDebugLoggingEnabled(false);
            } else {
                GetAppState().debugEnabled.store(true);
                if (SetDebugLoggingEnabled)
                    SetDebugLoggingEnabled(true);
                WriteLog(LogLevel::Info, L"Debug logging enabled.");
            }
            break;
        case ID_TRAY_RESTART:
            ShellExecute(NULL, L"open", L"cmd.exe", L"/C taskkill /IM kbdlayoutmon.exe /F && start kbdlayoutmon.exe", NULL, SW_HIDE);
            break;
    }
}
#endif // UNIT_TEST
