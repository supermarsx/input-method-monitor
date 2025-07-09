// kbdlayoutmon.cpp
#include <windows.h>
#include <string>
#include <fstream>
#include <Shlwapi.h>
#include <mutex>
#include <sstream>
#include <shellapi.h>
#include <vector>
#include "res-icon.h"  // Include the resource header
#include "configuration.h"
#include "log.h"

#define TRAY_ICON_ID 1001
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_STARTUP 1002
#define ID_TRAY_TOGGLE_LANGUAGE 1003
#define ID_TRAY_TOGGLE_LAYOUT 1004
#define ID_TRAY_TEMP_ENABLE_HOTKEYS 1005
#define ID_TRAY_APP_NAME 1006
#define ID_TRAY_RESTART 1007
#define ID_TRAY_OPEN_LOG 1008
#define WM_UPDATE_TRAY_MENU (WM_USER + 2)

HINSTANCE g_hInst = NULL;
HMODULE g_hDll = NULL;
HANDLE g_hMutex = NULL;  // Declare the mutex handle
HANDLE g_hInstanceMutex = NULL; // Mutex to enforce single instance
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
DWORD g_tempHotKeyTimeout = 10000; // Timeout for temporary hotkeys in milliseconds
NOTIFYICONDATA nid;



// Helper function to write to log file
extern "C" __declspec(dllexport) void WriteLog(const wchar_t* message) {
    g_log.write(message);
}

// Retrieve version information from the executable's version resource
std::wstring GetVersionString() {
    wchar_t path[MAX_PATH] = {0};
    if (!GetModuleFileNameW(NULL, path, MAX_PATH))
        return L"";

    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(path, &handle);
    if (size == 0)
        return L"";

    std::vector<BYTE> data(size);
    if (!GetFileVersionInfoW(path, handle, size, data.data()))
        return L"";

    VS_FIXEDFILEINFO* info = nullptr;
    UINT len = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<LPVOID*>(&info), &len) || len == 0)
        return L"";

    wchar_t ver[64] = {0};
    swprintf(ver, 64, L"%u.%u.%u.%u",
             HIWORD(info->dwFileVersionMS),
             LOWORD(info->dwFileVersionMS),
             HIWORD(info->dwFileVersionLS),
             LOWORD(info->dwFileVersionLS));
    return ver;
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

        // Set a timer to revert changes after configured timeout
        SetTimer(hwnd, 1, g_tempHotKeyTimeout, NULL);
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
    // Add open log option
    InsertMenu(hMenu, 6, MF_BYPOSITION | MF_STRING, ID_TRAY_OPEN_LOG, L"Open Log File");
    // Add separator
    InsertMenu(hMenu, 7, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    // Add restart option
    InsertMenu(hMenu, 8, MF_BYPOSITION | MF_STRING, ID_TRAY_RESTART, L"Restart");
    // Add exit option
    InsertMenu(hMenu, 9, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Quit");

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
                case ID_TRAY_OPEN_LOG:
                {
                    wchar_t logPath[MAX_PATH] = {0};
                    auto it = g_config.settings.find(L"log_path");
                    if (it != g_config.settings.end() && !it->second.empty()) {
                        lstrcpynW(logPath, it->second.c_str(), MAX_PATH);
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

    // Create a named mutex to ensure a single instance
    g_hInstanceMutex = CreateMutex(NULL, TRUE, L"InputMethodMonitorSingleton");
    if (g_hInstanceMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        WriteLog(L"Another instance is already running.");
        MessageBox(NULL, L"Another instance is already running.", L"Input Method Monitor", MB_ICONEXCLAMATION | MB_OK);
        ReleaseMutex(g_hInstanceMutex);
        CloseHandle(g_hInstanceMutex);
        return 0;
    }

    // Load configuration before any logging occurs
    g_config.load();
    g_debugEnabled = g_config.settings[L"debug"] == L"1";
    if (g_config.settings.count(L"tray_icon")) {
        g_trayIconEnabled = g_config.settings[L"tray_icon"] != L"0";
    }
    if (g_config.settings.count(L"temp_hotkey_timeout")) {
        g_tempHotKeyTimeout = std::wcstoul(g_config.settings[L"temp_hotkey_timeout"].c_str(), nullptr, 10);
    }

    // Parse command line options after the config file so they override
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            if (wcscmp(argv[i], L"--no-tray") == 0) {
                g_trayIconEnabled = false;
            } else if (wcscmp(argv[i], L"--debug") == 0) {
                g_debugEnabled = true;
            } else if (wcscmp(argv[i], L"--version") == 0) {
                std::wstring version = GetVersionString();
                if (AttachConsole(ATTACH_PARENT_PROCESS)) {
                    FILE* fp = _wfopen(L"CONOUT$", L"w");
                    if (fp) {
                        fwprintf(fp, L"%s\n", version.c_str());
                        fclose(fp);
                    }
                    FreeConsole();
                } else {
                    MessageBox(NULL, version.c_str(), L"Input Method Monitor", MB_OK | MB_ICONINFORMATION);
                }
                LocalFree(argv);
                if (g_hInstanceMutex) {
                    ReleaseMutex(g_hInstanceMutex);
                    CloseHandle(g_hInstanceMutex);
                }
                return 0;
            }
        }
        LocalFree(argv);
    }

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
        if (g_hInstanceMutex) {
            ReleaseMutex(g_hInstanceMutex);
            CloseHandle(g_hInstanceMutex);
        }
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
        if (g_hInstanceMutex) {
            ReleaseMutex(g_hInstanceMutex);
            CloseHandle(g_hInstanceMutex);
        }
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
        if (g_hInstanceMutex) {
            ReleaseMutex(g_hInstanceMutex);
            CloseHandle(g_hInstanceMutex);
        }
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
        if (g_hInstanceMutex) {
            ReleaseMutex(g_hInstanceMutex);
            CloseHandle(g_hInstanceMutex);
        }
        return 1;
    }

    if (!InstallGlobalHook()) {
        WriteLog(L"Failed to install global hook.");
        FreeLibrary(g_hDll);
        if (g_hInstanceMutex) {
            ReleaseMutex(g_hInstanceMutex);
            CloseHandle(g_hInstanceMutex);
        }
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
    if (g_hInstanceMutex) {
        ReleaseMutex(g_hInstanceMutex);
        CloseHandle(g_hInstanceMutex);
    }
    WriteLog(L"Executable stopped.");
    return static_cast<int>(msg.wParam);
}
