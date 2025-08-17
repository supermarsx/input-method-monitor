#include "tray_icon.h"
#include "res-icon.h"
#include "hotkey_registry.h"
#include "configuration.h"
#include "constants.h"
#include "log.h"
#include "utils.h"

// Global state defined elsewhere
extern Configuration g_config;

// Static tray icon data
static NOTIFYICONDATA nid;

void AddTrayIcon(HWND hwnd) {
    if (!g_trayIconEnabled.load()) return;

    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_MYAPP));
    wcscpy_s(nid.szTip, ARRAYSIZE(nid.szTip), L"kbdlayoutmon");
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void RemoveTrayIcon() {
    if (g_trayIconEnabled.load()) {
        Shell_NotifyIcon(NIM_DELETE, &nid);
    }
}

void ShowTrayMenu(HWND hwnd) {
    if (!g_trayIconEnabled.load()) return;

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

    InsertMenu(hMenu, 2, MF_BYPOSITION | MF_STRING | (g_startupEnabled ? MF_CHECKED : 0), ID_TRAY_STARTUP, L"Launch at startup");
    InsertMenu(hMenu, 3, MF_BYPOSITION | MF_STRING | (g_languageHotKeyEnabled.load() ? MF_CHECKED : 0), ID_TRAY_TOGGLE_LANGUAGE, L"Toggle Language HotKey");
    InsertMenu(hMenu, 4, MF_BYPOSITION | MF_STRING | (g_layoutHotKeyEnabled.load() ? MF_CHECKED : 0), ID_TRAY_TOGGLE_LAYOUT, L"Toggle Layout HotKey");
    InsertMenu(hMenu, 5, MF_BYPOSITION | MF_STRING, ID_TRAY_TEMP_ENABLE_HOTKEYS, L"Temporarily Enable HotKeys");
    InsertMenu(hMenu, 6, MF_BYPOSITION | MF_STRING, ID_TRAY_OPEN_LOG, L"Open Log File");
    InsertMenu(hMenu, 7, MF_BYPOSITION | MF_STRING, ID_TRAY_OPEN_CONFIG, L"Open Config File");
    InsertMenu(hMenu, 8, MF_BYPOSITION | MF_STRING | (g_debugEnabled.load() ? MF_CHECKED : 0), ID_TRAY_TOGGLE_DEBUG, L"Debug Logging");
    InsertMenu(hMenu, 9, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenu(hMenu, 10, MF_BYPOSITION | MF_STRING, ID_TRAY_RESTART, L"Restart");
    InsertMenu(hMenu, 11, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Quit");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

// Function pointers declared in main module
extern void (*SetDebugLoggingEnabled)(bool);
extern HMODULE g_hDll; // used for restart? not required, ignore

void HandleTrayCommand(HWND hwnd, WPARAM wParam) {
    switch (LOWORD(wParam)) {
        case ID_TRAY_EXIT:
            PostQuitMessage(0);
            break;
        case ID_TRAY_STARTUP:
            if (g_startupEnabled) {
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
            if (g_debugEnabled.load()) {
                WriteLog(L"Debug logging disabled.");
                g_debugEnabled.store(false);
                if (SetDebugLoggingEnabled)
                    SetDebugLoggingEnabled(false);
            } else {
                g_debugEnabled.store(true);
                if (SetDebugLoggingEnabled)
                    SetDebugLoggingEnabled(true);
                WriteLog(L"Debug logging enabled.");
            }
            break;
        case ID_TRAY_RESTART:
            ShellExecute(NULL, L"open", L"cmd.exe", L"/C taskkill /IM kbdlayoutmon.exe /F && start kbdlayoutmon.exe", NULL, SW_HIDE);
            break;
    }
}
