#pragma once
#include <windows.h>
#include <atomic>
#include <string>

// Extern declarations for shared state
extern HINSTANCE g_hInst;
extern std::atomic<bool> g_trayIconEnabled;
extern std::atomic<bool> g_debugEnabled;

// Message and menu identifiers
constexpr UINT WM_TRAYICON = WM_USER + 1;
constexpr UINT WM_UPDATE_TRAY_MENU = WM_USER + 2;

enum TrayMenuId {
    ID_TRAY_EXIT = 1001,
    ID_TRAY_STARTUP = 1002,
    ID_TRAY_TOGGLE_LANGUAGE = 1003,
    ID_TRAY_TOGGLE_LAYOUT = 1004,
    ID_TRAY_TEMP_ENABLE_HOTKEYS = 1005,
    ID_TRAY_APP_NAME = 1006,
    ID_TRAY_RESTART = 1007,
    ID_TRAY_OPEN_LOG = 1008,
    ID_TRAY_TOGGLE_DEBUG = 1009,
    ID_TRAY_OPEN_CONFIG = 1010
};

class TrayIcon {
public:
    explicit TrayIcon(HWND hwnd);
    ~TrayIcon();

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    // Reload the tray icon and tooltip based on provided values.
    void Update(const std::wstring& iconPath, const std::wstring& tooltip);

private:
    NOTIFYICONDATA nid_{};
    bool added_ = false;
};

extern BOOL (WINAPI *pShell_NotifyIcon)(DWORD, PNOTIFYICONDATA);

void ShowTrayMenu(HWND hwnd);
void HandleTrayCommand(HWND hwnd, WPARAM wParam);
