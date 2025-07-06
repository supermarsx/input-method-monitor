// kbdlayoutmon.cpp
#include <windows.h>
#include <string>
#include <fstream>
#include <Shlwapi.h>
#include <mutex>
#include <sstream>
#include <shellapi.h>
#include "res-icon.h"  // Include the resource header
#include "constants.h"

#define TRAY_ICON_ID 1001
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_STARTUP 1002
#define ID_TRAY_TOGGLE_LANGUAGE 1003
#define ID_TRAY_TOGGLE_LAYOUT 1004
#define ID_TRAY_TEMP_ENABLE_HOTKEYS 1005
#define ID_TRAY_APP_NAME 1006
#define ID_TRAY_RESTART 1007
#define WM_UPDATE_TRAY_MENU (WM_USER + 2)

HINSTANCE g_hInst = NULL;
HMODULE g_hDll = NULL;
HANDLE g_hMutex = NULL;  // Declare the mutex handle
typedef BOOL(*InstallGlobalHookFunc)();
typedef void(*UninstallGlobalHookFunc)();
typedef void(*SetLanguageHotKeyEnabledFunc)(bool);
typedef void(*SetLayoutHotKeyEnabledFunc)(bool);
typedef bool(*GetLanguageHotKeyEnabledFunc)();
typedef bool(*GetLayoutHotKeyEnabledFunc)();

InstallGlobalHookFunc InstallGlobalHook = NULL;
UninstallGlobalHookFunc UninstallGlobalHook = NULL;
SetLanguageHotKeyEnabledFunc SetLanguageHotKeyEnabled = NULL;
SetLayoutHotKeyEnabledFunc SetLayoutHotKeyEnabled = NULL;
GetLanguageHotKeyEnabledFunc GetLanguageHotKeyEnabled = NULL;
GetLayoutHotKeyEnabledFunc GetLayoutHotKeyEnabled = NULL;

std::mutex g_mutex;
bool g_debugEnabled = false; // Global variable to control debug logging
bool g_trayIconEnabled = true; // Global variable to control tray icon
bool g_startupEnabled = false; // Global variable to track startup status
bool g_languageHotKeyEnabled = false; // Global variable to track Language HotKey status
bool g_layoutHotKeyEnabled = false; // Global variable to track Layout HotKey status
bool g_tempHotKeysEnabled = false; // Global flag to track temporary hotkeys status
NOTIFYICONDATA nid;



// Helper function to write to log file
void WriteLog(const wchar_t* message) {
    if (!g_debugEnabled) return; // Exit if debug is not enabled

    std::lock_guard<std::mutex> guard(g_mutex);
    wchar_t logPath[MAX_PATH];
    GetModuleFileName(g_hInst, logPath, MAX_PATH);
    PathRemoveFileSpec(logPath);
    PathCombine(logPath, logPath, L"kbdlayoutmon.log");

    std::wofstream logFile(logPath, std::ios::app);
    if (logFile.is_open()) {
        logFile << message << std::endl;
        logFile.close();
    } else {
        OutputDebugString(L"Failed to open log file.");
    }
}

// Function to load configuration from a file
void LoadConfiguration() {
    wchar_t configPath[MAX_PATH];
    GetModuleFileName(g_hInst, configPath, MAX_PATH);
    PathRemoveFileSpec(configPath);
    PathCombine(configPath, configPath, CONFIG_FILE);

    std::wifstream configFile(configPath);
    if (configFile.is_open()) {
        std::wstring line;
        while (std::getline(configFile, line)) {
            if (line == L"DEBUG=1") {
                g_debugEnabled = true;
            } else if (line == L"DEBUG=0") {
                g_debugEnabled = false;
            } else if (line == L"TRAY_ICON=1") {
                g_trayIconEnabled = true;
            } else if (line == L"TRAY_ICON=0") {
                g_trayIconEnabled = false;
            }
        }
        configFile.close();
    }
}

// Helper function to check if app is set to launch at startup
bool IsStartupEnabled() {
    HKEY hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey);
    if (result == ERROR_SUCCESS) {
        wchar_t value[MAX_PATH];
        DWORD value_length = MAX_PATH;
        result = RegQueryValueEx(hKey, L"kbdlayoutmon", NULL, NULL, (LPBYTE)value, &value_length);
        RegCloseKey(hKey);
        return (result == ERROR_SUCCESS);
    }
    return false;
}

// Helper function to add application to startup
void AddToStartup() {
    HKEY hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey);
    if (result == ERROR_SUCCESS) {
        wchar_t filePath[MAX_PATH];
        GetModuleFileName(NULL, filePath, MAX_PATH);
        RegSetValueEx(hKey, L"kbdlayoutmon", 0, REG_SZ, (BYTE*)filePath, (lstrlen(filePath) + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
        WriteLog(L"Added to startup.");
        g_startupEnabled = true;
    } else {
        WriteLog(L"Failed to add to startup.");
    }
}

// Helper function to remove application from startup
void RemoveFromStartup() {
    HKEY hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey);
    if (result == ERROR_SUCCESS) {
        RegDeleteValue(hKey, L"kbdlayoutmon");
        RegCloseKey(hKey);
        WriteLog(L"Removed from startup.");
        g_startupEnabled = false;
    } else {
        WriteLog(L"Failed to remove from startup.");
    }
}

// Helper function to check the status of Language HotKey
bool IsLanguageHotKeyEnabled() {
    HKEY hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, L"Keyboard Layout\\Toggle", 0, KEY_READ, &hKey);
    if (result == ERROR_SUCCESS) {
        wchar_t value[2];
        DWORD value_length = sizeof(value);
        result = RegQueryValueEx(hKey, L"Language HotKey", NULL, NULL, (LPBYTE)value, &value_length);
        RegCloseKey(hKey);

        std::wstringstream ss;
        ss << L"Language HotKey value: " << value;
        WriteLog(ss.str().c_str());

        return (result == ERROR_SUCCESS && wcscmp(value, L"3") == 0);
    } else {
        WriteLog(L"Failed to open registry key for Language HotKey.");
    }
    return false;
}

// Helper function to check the status of Layout HotKey
bool IsLayoutHotKeyEnabled() {
    HKEY hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, L"Keyboard Layout\\Toggle", 0, KEY_READ, &hKey);
    if (result == ERROR_SUCCESS) {
        wchar_t value[2];
        DWORD value_length = sizeof(value);
        result = RegQueryValueEx(hKey, L"Layout HotKey", NULL, NULL, (LPBYTE)value, &value_length);
        RegCloseKey(hKey);

        std::wstringstream ss;
        ss << L"Layout HotKey value: " << value;
        WriteLog(ss.str().c_str());

        return (result == ERROR_SUCCESS && wcscmp(value, L"3") == 0);
    } else {
        WriteLog(L"Failed to open registry key for Layout HotKey.");
    }
    return false;
}

// Helper function to toggle Language HotKey
void ToggleLanguageHotKey(HWND hwnd, bool overrideState = false, bool state = false) {
    // Detect current state before toggling
    g_languageHotKeyEnabled = IsLanguageHotKeyEnabled();

    HKEY hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, L"Keyboard Layout\\Toggle", 0, KEY_SET_VALUE, &hKey);
    if (result == ERROR_SUCCESS) {
        const wchar_t* value;
        if (overrideState) {
            value = state ? L"3" : L"1";
            g_languageHotKeyEnabled = state;
        } else {
            value = g_languageHotKeyEnabled ? L"1" : L"3";
            g_languageHotKeyEnabled = !g_languageHotKeyEnabled;
        }
        RegSetValueEx(hKey, L"Language HotKey", 0, REG_SZ, (const BYTE*)value, (lstrlen(value) + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
        //PostMessage(hwnd, WM_UPDATE_TRAY_MENU, 0, 0);

        // Update the shared memory value
        WaitForSingleObject(g_hMutex, INFINITE);
        SetLanguageHotKeyEnabled(g_languageHotKeyEnabled);
        ReleaseMutex(g_hMutex);
    }
}

// Helper function to toggle Layout HotKey
void ToggleLayoutHotKey(HWND hwnd, bool overrideState = false, bool state = false) {
    // Detect current state before toggling
    g_layoutHotKeyEnabled = IsLayoutHotKeyEnabled();

    HKEY hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, L"Keyboard Layout\\Toggle", 0, KEY_SET_VALUE, &hKey);
    if (result == ERROR_SUCCESS) {
        const wchar_t* value;
        if (overrideState) {
            value = state ? L"3" : L"2";
            g_layoutHotKeyEnabled = state;
        } else {
            value = g_layoutHotKeyEnabled ? L"2" : L"3";
            g_layoutHotKeyEnabled = !g_layoutHotKeyEnabled;
        }
        RegSetValueEx(hKey, L"Layout HotKey", 0, REG_SZ, (const BYTE*)value, (lstrlen(value) + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
        //PostMessage(hwnd, WM_UPDATE_TRAY_MENU, 0, 0);

        // Update the shared memory value
        WaitForSingleObject(g_hMutex, INFINITE);
        SetLayoutHotKeyEnabled(g_layoutHotKeyEnabled);
        ReleaseMutex(g_hMutex);
    }
}

void TemporarilyEnableHotKeys(HWND hwnd) {
    WriteLog(L"TemporarilyEnableHotKeys called.");

    if (g_tempHotKeysEnabled) {
        WriteLog(L"TemporarilyEnableHotKeys already enabled. Skipping.");
        return; // Avoid repeated enabling
    }

    g_tempHotKeysEnabled = true;

    HKEY hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, L"Keyboard Layout\\Toggle", 0, KEY_SET_VALUE, &hKey);
    if (result == ERROR_SUCCESS) {
        RegSetValueEx(hKey, L"Language HotKey", 0, REG_SZ, (const BYTE*)L"1", (lstrlen(L"1") + 1) * sizeof(wchar_t));
        RegSetValueEx(hKey, L"Layout HotKey", 0, REG_SZ, (const BYTE*)L"2", (lstrlen(L"2") + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
        WriteLog(L"Temporarily enabled hotkeys.");

        // Update the global status
        g_languageHotKeyEnabled = true;
        g_layoutHotKeyEnabled = true;

        // Update tray menu checkmarks
        //PostMessage(hwnd, WM_UPDATE_TRAY_MENU, 0, 0);

        // Update the shared memory values
        WaitForSingleObject(g_hMutex, INFINITE);
        SetLanguageHotKeyEnabled(g_languageHotKeyEnabled);
        SetLayoutHotKeyEnabled(g_layoutHotKeyEnabled);
        ReleaseMutex(g_hMutex);

        // Set a timer to revert changes after 10 seconds
        SetTimer(hwnd, 1, 10000, NULL);
    } else {
        WriteLog(L"Failed to open registry key for temporarily enabling hotkeys.");
    }
}


// Function to handle timer events and reset hotkeys
void OnTimer(HWND hwnd) {
    if (g_tempHotKeysEnabled) {
        ToggleLanguageHotKey(hwnd, true, false);
        ToggleLayoutHotKey(hwnd, true, false);

        // Reset the flag
        g_tempHotKeysEnabled = false;

        // Update tray menu checkmarks
        //PostMessage(hwnd, WM_UPDATE_TRAY_MENU, 0, 0);
    }
    KillTimer(hwnd, 1); // Kill the timer
}

// Function to add the tray icon
void AddTrayIcon(HWND hwnd) {
    if (!g_trayIconEnabled) return; // Do not add tray icon if disabled in config

    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_MYAPP)); // Use custom icon
    wcscpy_s(nid.szTip, L"kbdlayoutmon");
    Shell_NotifyIcon(NIM_ADD, &nid);
}

// Function to remove the tray icon
void RemoveTrayIcon() {
    if (g_trayIconEnabled) {
        Shell_NotifyIcon(NIM_DELETE, &nid);
    }
}

// Function to handle tray icon menu
void ShowTrayMenu(HWND hwnd) {
    if (!g_trayIconEnabled) return; // Do not show tray menu if tray icon is disabled

    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();

    // Add tray item title with the app name (disabled, non-selectable)
    MENUITEMINFO mii;
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_FTYPE | MIIM_STRING;
    mii.fType = MFT_STRING | MFT_RIGHTJUSTIFY;
    mii.dwTypeData = L"kbdlayoutmon";
    InsertMenuItem(hMenu, ID_TRAY_APP_NAME, TRUE, &mii);
    InsertMenu(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

    // Add startup option with checkmark if enabled
    InsertMenu(hMenu, 2, MF_BYPOSITION | MF_STRING | (g_startupEnabled ? MF_CHECKED : 0), ID_TRAY_STARTUP, L"Launch at startup");
    // Add toggle Language HotKey option with checkmark if enabled
    InsertMenu(hMenu, 3, MF_BYPOSITION | MF_STRING | (g_languageHotKeyEnabled ? MF_CHECKED : 0), ID_TRAY_TOGGLE_LANGUAGE, L"Toggle Language HotKey");
    // Add toggle Layout HotKey option with checkmark if enabled
    InsertMenu(hMenu, 4, MF_BYPOSITION | MF_STRING | (g_layoutHotKeyEnabled ? MF_CHECKED : 0), ID_TRAY_TOGGLE_LAYOUT, L"Toggle Layout HotKey");
    // Add option to temporarily enable hotkeys
    InsertMenu(hMenu, 5, MF_BYPOSITION | MF_STRING, ID_TRAY_TEMP_ENABLE_HOTKEYS, L"Temporarily Enable HotKeys");
    // Add separator
    InsertMenu(hMenu, 6, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    // Add restart option
    InsertMenu(hMenu, 7, MF_BYPOSITION | MF_STRING, ID_TRAY_RESTART, L"Restart");
    // Add exit option
    InsertMenu(hMenu, 8, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Quit");

    // Set the foreground window before calling TrackPopupMenu or the menu will not disappear when the user clicks outside of it
    SetForegroundWindow(hwnd);

    // Display the menu
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) {
                ShowTrayMenu(hwnd);
            }
            break;
        case WM_COMMAND:
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
                    //PostMessage(hwnd, WM_UPDATE_TRAY_MENU, 0, 0);
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
                case ID_TRAY_RESTART:
                    // Restart the application
                    ShellExecute(NULL, L"open", L"cmd.exe", L"/C taskkill /IM kbdlayoutmon.exe /F && start kbdlayoutmon.exe", NULL, SW_HIDE);
                    break;
            }
            break;
        case WM_TIMER:
            OnTimer(hwnd);
            break;
        case WM_UPDATE_TRAY_MENU:
            ShowTrayMenu(hwnd);
            break;
        case WM_DESTROY:
            RemoveTrayIcon();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInst = hInstance;

    // Load the configuration file
    LoadConfiguration();

    WriteLog(L"Executable started.");

    // Check if the app is set to launch at startup
    g_startupEnabled = IsStartupEnabled();

    // Check if Language HotKey is enabled
    g_languageHotKeyEnabled = IsLanguageHotKeyEnabled();

    // Check if Layout HotKey is enabled
    g_layoutHotKeyEnabled = IsLayoutHotKeyEnabled();

    // Register the window class
    const wchar_t CLASS_NAME[] = L"TrayIconWindowClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClass(&wc)) {
        DWORD errorCode = GetLastError();
        std::wstringstream ss;
        ss << L"Failed to register window class. Error code: 0x" << std::hex << errorCode;
        WriteLog(ss.str().c_str());
        return 1;
    }

    // Create the message-only window
    HWND hwnd = CreateWindowEx(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        NULL,                           // Window text
        0,                              // Window style
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        HWND_MESSAGE,                   // Message-only window
        NULL,                           // Parent window
        hInstance,                      // Instance handle
        NULL                            // Additional application data
    );

    if (hwnd == NULL) {
        DWORD errorCode = GetLastError();
        std::wstringstream ss;
        ss << L"Failed to create message-only window. Error code: 0x" << std::hex << errorCode;
        WriteLog(ss.str().c_str());
        return 1;
    }

    // Add the tray icon
    AddTrayIcon(hwnd);

    // Load the DLL
    g_hDll = LoadLibrary(L"kbdlayoutmonhook.dll");
    if (g_hDll == NULL) {
        DWORD errorCode = GetLastError();
        std::wstringstream ss;
        ss << L"Failed to load kbdlayoutmonhook.dll. Error code: 0x" << std::hex << errorCode;
        WriteLog(ss.str().c_str());
        return 1;
    }

    InstallGlobalHook = (InstallGlobalHookFunc)GetProcAddress(g_hDll, "InstallGlobalHook");
    UninstallGlobalHook = (UninstallGlobalHookFunc)GetProcAddress(g_hDll, "UninstallGlobalHook");
    SetLanguageHotKeyEnabled = (SetLanguageHotKeyEnabledFunc)GetProcAddress(g_hDll, "SetLanguageHotKeyEnabled");
    SetLayoutHotKeyEnabled = (SetLayoutHotKeyEnabledFunc)GetProcAddress(g_hDll, "SetLayoutHotKeyEnabled");
    GetLanguageHotKeyEnabled = (GetLanguageHotKeyEnabledFunc)GetProcAddress(g_hDll, "GetLanguageHotKeyEnabled");
    GetLayoutHotKeyEnabled = (GetLayoutHotKeyEnabledFunc)GetProcAddress(g_hDll, "GetLayoutHotKeyEnabled");

    if (!InstallGlobalHook || !UninstallGlobalHook || !SetLanguageHotKeyEnabled || !SetLayoutHotKeyEnabled || !GetLanguageHotKeyEnabled || !GetLayoutHotKeyEnabled) {
        DWORD errorCode = GetLastError();
        std::wstringstream ss;
        ss << L"Failed to get function addresses from kbdlayoutmonhook.dll. Error code: 0x" << std::hex << errorCode;
        WriteLog(ss.str().c_str());
        FreeLibrary(g_hDll);
        return 1;
    }

    if (!InstallGlobalHook()) {
        WriteLog(L"Failed to install global hook.");
        FreeLibrary(g_hDll);
        return 1;
    }

    // Initialize the mutex
    g_hMutex = CreateMutex(NULL, FALSE, L"Global\\KbdHookMutex");

    // Update the shared memory values at startup
    WaitForSingleObject(g_hMutex, INFINITE);
    SetLanguageHotKeyEnabled(g_languageHotKeyEnabled);
    SetLayoutHotKeyEnabled(g_layoutHotKeyEnabled);
    ReleaseMutex(g_hMutex);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UninstallGlobalHook();
    FreeLibrary(g_hDll);
    CloseHandle(g_hMutex); // Close the mutex handle
    WriteLog(L"Executable stopped.");
    return static_cast<int>(msg.wParam);
}
