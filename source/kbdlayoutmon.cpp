// kbdlayoutmon.cpp
#include <windows.h>
#include <string>
#include <fstream>
#include <shlwapi.h>
#include <atomic>
#include <sstream>
#include <shellapi.h>
#include <vector>
#include "res-icon.h"  // Include the resource header
#include "configuration.h"
#include "constants.h"
#include "log.h"
#include "winreg_handle.h"
#include "utils.h"

// Forward declarations
void ApplyConfig(HWND hwnd);
DWORD WINAPI ConfigWatchThread(LPVOID param);
void StartConfigWatcher(HWND hwnd);
void StopConfigWatcher();

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
#define ID_TRAY_TOGGLE_DEBUG 1009
#define ID_TRAY_OPEN_CONFIG 1010
#define WM_UPDATE_TRAY_MENU (WM_USER + 2)

HINSTANCE g_hInst = NULL;
HMODULE g_hDll = NULL;
HANDLE g_hInstanceMutex = NULL; // Mutex to enforce single instance
typedef BOOL(*InstallGlobalHookFunc)();
typedef void(*UninstallGlobalHookFunc)();
typedef void(*SetLanguageHotKeyEnabledFunc)(bool);
typedef void(*SetLayoutHotKeyEnabledFunc)(bool);
typedef bool(*GetLanguageHotKeyEnabledFunc)();
typedef bool(*GetLayoutHotKeyEnabledFunc)();
typedef void(*SetDebugLoggingEnabledFunc)(bool);
typedef BOOL(*InitHookModuleFunc)();
typedef void(*CleanupHookModuleFunc)();

InstallGlobalHookFunc InstallGlobalHook = NULL;
UninstallGlobalHookFunc UninstallGlobalHook = NULL;
SetLanguageHotKeyEnabledFunc SetLanguageHotKeyEnabled = NULL;
SetLayoutHotKeyEnabledFunc SetLayoutHotKeyEnabled = NULL;
GetLanguageHotKeyEnabledFunc GetLanguageHotKeyEnabled = NULL;
GetLayoutHotKeyEnabledFunc GetLayoutHotKeyEnabled = NULL;
SetDebugLoggingEnabledFunc SetDebugLoggingEnabled = NULL;
InitHookModuleFunc InitHookModule = NULL;
CleanupHookModuleFunc CleanupHookModule = NULL;

std::atomic<bool> g_debugEnabled{false}; // Global variable to control debug logging
std::atomic<bool> g_trayIconEnabled{true}; // Global variable to control tray icon
bool g_startupEnabled = false; // Global variable to track startup status
std::atomic<bool> g_languageHotKeyEnabled{false}; // Global variable to track Language HotKey status
std::atomic<bool> g_layoutHotKeyEnabled{false}; // Global variable to track Layout HotKey status
std::atomic<bool> g_tempHotKeysEnabled{false}; // Global flag to track temporary hotkeys status
DWORD g_tempHotKeyTimeout = 10000; // Timeout for temporary hotkeys in milliseconds
bool g_cliMode = false;                     // Suppress GUI/tray behavior
NOTIFYICONDATA nid;
HWND g_hwnd = NULL;                // Handle to our message window
HANDLE g_hConfigThread = NULL;     // Thread monitoring config file
HANDLE g_hStopConfigEvent = NULL;  // Event to stop config watcher
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

// Return a short usage string describing supported options
std::wstring GetUsageString() {
    return
        L"Usage: kbdlayoutmon [options]\n\n"
        L"Options:\n"
        L"  --config <path>  Load configuration from the specified file\n"
        L"  --no-tray    Run without the system tray icon\n"
        L"  --debug      Enable debug logging\n"
        L"  --verbose    Increase logging verbosity\n"
        L"  --cli        Run in CLI mode without GUI/tray icon\n"
        L"  --tray-icon <0|1>        Override tray icon setting\n"
        L"  --temp-hotkey-timeout <ms>  Override temporary hotkey timeout\n"
        L"  --log-path <path>          Override log file location\n"
        L"  --max-log-size-mb <num>    Override max log size\n"
        L"  --version    Print the application version and exit\n"
        L"  --help       Show this help message and exit";
}



// Helper function to check if app is set to launch at startup
bool IsStartupEnabled() {
    WinRegHandle hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER,
                               L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                               0, KEY_READ, hKey.receive());
    if (result == ERROR_SUCCESS) {
        wchar_t value[MAX_PATH];
        DWORD value_length = MAX_PATH;
        result = RegQueryValueEx(hKey.get(), L"kbdlayoutmon", NULL, NULL,
                                 (LPBYTE)value, &value_length);
        return (result == ERROR_SUCCESS);
    }
    return false;
}

// Helper function to add application to startup
void AddToStartup() {
    WinRegHandle hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER,
                               L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                               0, KEY_SET_VALUE, hKey.receive());
    if (result == ERROR_SUCCESS) {
        wchar_t filePath[MAX_PATH];
        GetModuleFileNameW(NULL, filePath, MAX_PATH);
        std::wstring quotedPath = QuotePath(filePath);
        RegSetValueExW(hKey.get(), L"kbdlayoutmon", 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(quotedPath.c_str()),
                       (quotedPath.size() + 1) * sizeof(wchar_t));
        WriteLog(L"Added to startup.");
        g_startupEnabled = true;
    } else {
        WriteLog(L"Failed to add to startup.");
    }
}

// Helper function to remove application from startup
void RemoveFromStartup() {
    WinRegHandle hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER,
                               L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                               0, KEY_SET_VALUE, hKey.receive());
    if (result == ERROR_SUCCESS) {
        RegDeleteValue(hKey.get(), L"kbdlayoutmon");
        WriteLog(L"Removed from startup.");
        g_startupEnabled = false;
    } else {
        WriteLog(L"Failed to remove from startup.");
    }
}

// Helper function to check the status of Language HotKey
bool IsLanguageHotKeyEnabled() {
    WinRegHandle hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, L"Keyboard Layout\\Toggle", 0,
                               KEY_READ, hKey.receive());
    if (result == ERROR_SUCCESS) {
        wchar_t value[2];
        DWORD value_length = sizeof(value);
        result = RegQueryValueEx(hKey.get(), L"Language HotKey", NULL, NULL,
                                 (LPBYTE)value, &value_length);

        std::wstringstream ss;
        ss << L"Language HotKey value: " << value;
        WriteLog(ss.str());

        return (result == ERROR_SUCCESS && wcscmp(value, L"3") == 0);
    } else {
        WriteLog(L"Failed to open registry key for Language HotKey.");
    }
    return false;
}

// Helper function to check the status of Layout HotKey
bool IsLayoutHotKeyEnabled() {
    WinRegHandle hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, L"Keyboard Layout\\Toggle", 0,
                               KEY_READ, hKey.receive());
    if (result == ERROR_SUCCESS) {
        wchar_t value[2];
        DWORD value_length = sizeof(value);
        result = RegQueryValueEx(hKey.get(), L"Layout HotKey", NULL, NULL,
                                 (LPBYTE)value, &value_length);

        std::wstringstream ss;
        ss << L"Layout HotKey value: " << value;
        WriteLog(ss.str());

        return (result == ERROR_SUCCESS && wcscmp(value, L"3") == 0);
    } else {
        WriteLog(L"Failed to open registry key for Layout HotKey.");
    }
    return false;
}

// Helper function to toggle Language HotKey
void ToggleLanguageHotKey(HWND hwnd, bool overrideState = false, bool state = false) {
    // Detect current state before toggling
    g_languageHotKeyEnabled.store(IsLanguageHotKeyEnabled());

    WinRegHandle hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, L"Keyboard Layout\\Toggle", 0,
                               KEY_SET_VALUE, hKey.receive());
    if (result == ERROR_SUCCESS) {
        const wchar_t* value;
        if (overrideState) {
            value = state ? L"3" : L"1";
            g_languageHotKeyEnabled.store(state);
        } else {
            bool cur = g_languageHotKeyEnabled.load();
            value = cur ? L"1" : L"3";
            g_languageHotKeyEnabled.store(!cur);
        }
        RegSetValueEx(hKey.get(), L"Language HotKey", 0, REG_SZ,
                      reinterpret_cast<const BYTE*>(value),
                      (lstrlen(value) + 1) * sizeof(wchar_t));
        //PostMessage(hwnd, WM_UPDATE_TRAY_MENU, 0, 0);

        // Update the shared memory value
        SetLanguageHotKeyEnabled(g_languageHotKeyEnabled.load());
    }
}

// Helper function to toggle Layout HotKey
void ToggleLayoutHotKey(HWND hwnd, bool overrideState = false, bool state = false) {
    // Detect current state before toggling
    g_layoutHotKeyEnabled.store(IsLayoutHotKeyEnabled());

    WinRegHandle hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, L"Keyboard Layout\\Toggle", 0,
                               KEY_SET_VALUE, hKey.receive());
    if (result == ERROR_SUCCESS) {
        const wchar_t* value;
        if (overrideState) {
            value = state ? L"3" : L"2";
            g_layoutHotKeyEnabled.store(state);
        } else {
            bool cur = g_layoutHotKeyEnabled.load();
            value = cur ? L"2" : L"3";
            g_layoutHotKeyEnabled.store(!cur);
        }
        RegSetValueEx(hKey.get(), L"Layout HotKey", 0, REG_SZ,
                      reinterpret_cast<const BYTE*>(value),
                      (lstrlen(value) + 1) * sizeof(wchar_t));
        //PostMessage(hwnd, WM_UPDATE_TRAY_MENU, 0, 0);

        // Update the shared memory value
        SetLayoutHotKeyEnabled(g_layoutHotKeyEnabled.load());
    }
}

void TemporarilyEnableHotKeys(HWND hwnd) {
    WriteLog(L"TemporarilyEnableHotKeys called.");

    if (g_tempHotKeysEnabled.load()) {
        WriteLog(L"TemporarilyEnableHotKeys already enabled. Skipping.");
        return; // Avoid repeated enabling
    }

    g_tempHotKeysEnabled.store(true);

    WinRegHandle hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, L"Keyboard Layout\\Toggle", 0,
                               KEY_SET_VALUE, hKey.receive());
    if (result == ERROR_SUCCESS) {
        RegSetValueEx(hKey.get(), L"Language HotKey", 0, REG_SZ, reinterpret_cast<const BYTE*>(L"1"),
                      (lstrlen(L"1") + 1) * sizeof(wchar_t));
        RegSetValueEx(hKey.get(), L"Layout HotKey", 0, REG_SZ, reinterpret_cast<const BYTE*>(L"2"),
                      (lstrlen(L"2") + 1) * sizeof(wchar_t));
        WriteLog(L"Temporarily enabled hotkeys.");

        // Update the global status
        g_languageHotKeyEnabled.store(true);
        g_layoutHotKeyEnabled.store(true);

        // Update tray menu checkmarks
        //PostMessage(hwnd, WM_UPDATE_TRAY_MENU, 0, 0);

        // Update the shared memory values
        SetLanguageHotKeyEnabled(g_languageHotKeyEnabled.load());
        SetLayoutHotKeyEnabled(g_layoutHotKeyEnabled.load());

        // Set a timer to revert changes after configured timeout
        SetTimer(hwnd, 1, g_tempHotKeyTimeout, NULL);
    } else {
        WriteLog(L"Failed to open registry key for temporarily enabling hotkeys.");
    }
}


// Function to handle timer events and reset hotkeys
void OnTimer(HWND hwnd) {
    if (g_tempHotKeysEnabled.load()) {
        ToggleLanguageHotKey(hwnd, true, false);
        ToggleLayoutHotKey(hwnd, true, false);

        // Reset the flag
        g_tempHotKeysEnabled.store(false);

        // Update tray menu checkmarks
        //PostMessage(hwnd, WM_UPDATE_TRAY_MENU, 0, 0);
    }
    KillTimer(hwnd, 1); // Kill the timer
}

// Function to add the tray icon
void AddTrayIcon(HWND hwnd) {
    if (!g_trayIconEnabled.load()) return; // Do not add tray icon if disabled in config

    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_MYAPP)); // Use custom icon
    wcscpy_s(nid.szTip, ARRAYSIZE(nid.szTip), L"kbdlayoutmon");
    Shell_NotifyIcon(NIM_ADD, &nid);
}

// Function to remove the tray icon
void RemoveTrayIcon() {
    if (g_trayIconEnabled.load()) {
        Shell_NotifyIcon(NIM_DELETE, &nid);
    }
}

// Apply configuration values to runtime settings
void ApplyConfig(HWND hwnd) {
    auto debugVal = g_config.getSetting(L"debug");
    bool newDebug = (debugVal && *debugVal == L"1");
    if (newDebug != g_debugEnabled.load()) {
        g_debugEnabled.store(newDebug);
        if (SetDebugLoggingEnabled)
            SetDebugLoggingEnabled(g_debugEnabled.load());
    }

    bool tray = true;
    auto trayVal = g_config.getSetting(L"tray_icon");
    if (trayVal)
        tray = *trayVal != L"0";
    if (tray != g_trayIconEnabled.load()) {
        if (tray) {
            g_trayIconEnabled.store(true);
            if (hwnd)
                AddTrayIcon(hwnd);
        } else {
            if (hwnd)
                RemoveTrayIcon();
            g_trayIconEnabled.store(false);
        }
    }

    auto timeoutVal = g_config.getSetting(L"temp_hotkey_timeout");
    if (timeoutVal) {
        g_tempHotKeyTimeout =
            std::wcstoul(timeoutVal->c_str(), nullptr, 10);
    }
}

// Thread procedure to watch for configuration file changes
DWORD WINAPI ConfigWatchThread(LPVOID param) {
    HWND hwnd = (HWND)param;
    wchar_t dirPath[MAX_PATH];
    std::wstring cfgPath = g_config.getLastPath();
    if (cfgPath.empty()) {
        GetModuleFileNameW(g_hInst, dirPath, MAX_PATH);
    } else {
        wcsncpy(dirPath, cfgPath.c_str(), MAX_PATH);
        dirPath[MAX_PATH - 1] = L'\0';
    }
    PathRemoveFileSpecW(dirPath);

    HANDLE hChange = FindFirstChangeNotificationW(dirPath, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
    if (hChange == INVALID_HANDLE_VALUE)
        return 0;

    HANDLE handles[2] = { hChange, g_hStopConfigEvent };
    for (;;) {
        DWORD wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (wait == WAIT_OBJECT_0) {
            if (g_config.load()) {
                ApplyConfig(hwnd);
                WriteLog(L"Configuration reloaded.");
            } else {
                WriteLog(L"Failed to reload configuration.");
            }
            FindNextChangeNotification(hChange);
        } else if (wait == WAIT_OBJECT_0 + 1) {
            break;
        } else {
            break;
        }
    }

    FindCloseChangeNotification(hChange);
    return 0;
}

void StartConfigWatcher(HWND hwnd) {
    g_hStopConfigEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    g_hConfigThread = CreateThread(NULL, 0, ConfigWatchThread, hwnd, 0, NULL);
}

void StopConfigWatcher() {
    if (g_hStopConfigEvent) {
        SetEvent(g_hStopConfigEvent);
    }
    if (g_hConfigThread) {
        WaitForSingleObject(g_hConfigThread, INFINITE);
        CloseHandle(g_hConfigThread);
        g_hConfigThread = NULL;
    }
    if (g_hStopConfigEvent) {
        CloseHandle(g_hStopConfigEvent);
        g_hStopConfigEvent = NULL;
    }
}

// Function to handle tray icon menu
void ShowTrayMenu(HWND hwnd) {
    if (!g_trayIconEnabled.load()) return; // Do not show tray menu if tray icon is disabled

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
    InsertMenu(hMenu, 3, MF_BYPOSITION | MF_STRING | (g_languageHotKeyEnabled.load() ? MF_CHECKED : 0), ID_TRAY_TOGGLE_LANGUAGE, L"Toggle Language HotKey");
    // Add toggle Layout HotKey option with checkmark if enabled
    InsertMenu(hMenu, 4, MF_BYPOSITION | MF_STRING | (g_layoutHotKeyEnabled.load() ? MF_CHECKED : 0), ID_TRAY_TOGGLE_LAYOUT, L"Toggle Layout HotKey");
    // Add option to temporarily enable hotkeys
    InsertMenu(hMenu, 5, MF_BYPOSITION | MF_STRING, ID_TRAY_TEMP_ENABLE_HOTKEYS, L"Temporarily Enable HotKeys");
    // Add open log option
    InsertMenu(hMenu, 6, MF_BYPOSITION | MF_STRING, ID_TRAY_OPEN_LOG, L"Open Log File");
    // Add open config option
    InsertMenu(hMenu, 7, MF_BYPOSITION | MF_STRING, ID_TRAY_OPEN_CONFIG, L"Open Config File");
    // Add debug toggle option with checkmark if enabled
    InsertMenu(hMenu, 8, MF_BYPOSITION | MF_STRING | (g_debugEnabled.load() ? MF_CHECKED : 0), ID_TRAY_TOGGLE_DEBUG, L"Debug Logging");
    // Add separator
    InsertMenu(hMenu, 9, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    // Add restart option
    InsertMenu(hMenu, 10, MF_BYPOSITION | MF_STRING, ID_TRAY_RESTART, L"Restart");
    // Add exit option
    InsertMenu(hMenu, 11, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Quit");

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
                    auto val = g_config.getSetting(L"log_path");
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

/**
 * @brief Application entry point.
 *
 * Initializes the tray icon, loads the hook DLL and enters the message loop.
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInst = hInstance;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring customConfigPath;
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            if (wcscmp(argv[i], L"--config") == 0 && i + 1 < argc) {
                customConfigPath = argv[i + 1];
                ++i;
            } else if (wcscmp(argv[i], L"--cli") == 0 || wcscmp(argv[i], L"--cli-mode") == 0) {
                g_cliMode = true;
            }
        }
    }

    // Create a named mutex to ensure a single instance
    g_hInstanceMutex = CreateMutex(NULL, TRUE, L"InputMethodMonitorSingleton");
    if (g_hInstanceMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        WriteLog(L"Another instance is already running.");
        if (g_cliMode || AttachConsole(ATTACH_PARENT_PROCESS)) {
            FILE* fp = _wfopen(L"CONOUT$", L"w");
            if (fp) {
                fwprintf(fp, L"Another instance is already running.\n");
                fclose(fp);
            }
            if (!g_cliMode)
                FreeConsole();
        } else {
            MessageBox(NULL, L"Another instance is already running.", L"Input Method Monitor", MB_ICONEXCLAMATION | MB_OK);
        }
        ReleaseMutex(g_hInstanceMutex);
        CloseHandle(g_hInstanceMutex);
        return 0;
    }

    // Load configuration before any logging occurs
    if (!g_config.load(customConfigPath)) {
        WriteLog(L"Using default configuration values.");
    }
    ApplyConfig(NULL);

    // Parse command line options after the config file so they override
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            if (wcscmp(argv[i], L"--config") == 0 && i + 1 < argc) {
                ++i; // skip the path argument
            } else if (wcscmp(argv[i], L"--no-tray") == 0) {
                g_trayIconEnabled.store(false);
            } else if (wcscmp(argv[i], L"--tray-icon") == 0 && i + 1 < argc) {
                g_config.setSetting(L"tray_icon", argv[i + 1]);
                g_trayIconEnabled.store(wcscmp(argv[i + 1], L"0") != 0);
                ++i;
            } else if (wcscmp(argv[i], L"--temp-hotkey-timeout") == 0 && i + 1 < argc) {
                g_config.setSetting(L"temp_hotkey_timeout", argv[i + 1]);
                g_tempHotKeyTimeout = std::wcstoul(argv[i + 1], nullptr, 10);
                ++i;
            } else if (wcscmp(argv[i], L"--log-path") == 0 && i + 1 < argc) {
                g_config.setSetting(L"log_path", argv[i + 1]);
                ++i;
            } else if (wcscmp(argv[i], L"--max-log-size-mb") == 0 && i + 1 < argc) {
                g_config.setSetting(L"max_log_size_mb", argv[i + 1]);
                ++i;
            } else if (wcscmp(argv[i], L"--cli") == 0 || wcscmp(argv[i], L"--cli-mode") == 0) {
                g_cliMode = true;
                g_trayIconEnabled.store(false);
            } else if (wcscmp(argv[i], L"--verbose") == 0) {
                g_verboseLogging = true;
                if (!g_debugEnabled.load()) {
                    g_debugEnabled.store(true);
                    if (SetDebugLoggingEnabled)
                        SetDebugLoggingEnabled(true);
                }
            } else if (wcscmp(argv[i], L"--debug") == 0) {
                if (!g_debugEnabled.load()) {
                    g_debugEnabled.store(true);
                    if (SetDebugLoggingEnabled)
                        SetDebugLoggingEnabled(true);
                }
            } else if (wcscmp(argv[i], L"--version") == 0) {
                std::wstring version = GetVersionString();
                if (g_cliMode || AttachConsole(ATTACH_PARENT_PROCESS)) {
                    FILE* fp = _wfopen(L"CONOUT$", L"w");
                    if (fp) {
                        fwprintf(fp, L"%s\n", version.c_str());
                        fclose(fp);
                    }
                    if (!g_cliMode)
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
            } else if (wcscmp(argv[i], L"--help") == 0) {
                std::wstring usage = GetUsageString();
                if (g_cliMode || AttachConsole(ATTACH_PARENT_PROCESS)) {
                    FILE* fp = _wfopen(L"CONOUT$", L"w");
                    if (fp) {
                        fwprintf(fp, L"%s\n", usage.c_str());
                        fclose(fp);
                    }
                    if (!g_cliMode)
                        FreeConsole();
                } else {
                    MessageBox(NULL, usage.c_str(), L"Input Method Monitor", MB_OK | MB_ICONINFORMATION);
                }
                LocalFree(argv);
                if (g_hInstanceMutex) {
                    ReleaseMutex(g_hInstanceMutex);
                    CloseHandle(g_hInstanceMutex);
                }
                return 0;
            }
        }
        ApplyConfig(NULL);
        LocalFree(argv);
    }

    WriteLog(L"Executable started.");

    // Check if the app is set to launch at startup
    g_startupEnabled = IsStartupEnabled();

    // Check if Language HotKey is enabled
    g_languageHotKeyEnabled.store(IsLanguageHotKeyEnabled());

    // Check if Layout HotKey is enabled
    g_layoutHotKeyEnabled.store(IsLayoutHotKeyEnabled());

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
        WriteLog(ss.str());
        if (g_hInstanceMutex) {
            ReleaseMutex(g_hInstanceMutex);
            CloseHandle(g_hInstanceMutex);
        }
        StopConfigWatcher();
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
        WriteLog(ss.str());
        if (g_hInstanceMutex) {
            ReleaseMutex(g_hInstanceMutex);
            CloseHandle(g_hInstanceMutex);
        }
        return 1;
    }

    g_hwnd = hwnd;

    // Add the tray icon
    AddTrayIcon(hwnd);
    StartConfigWatcher(hwnd);

    // Load the DLL
    g_hDll = LoadLibrary(L"kbdlayoutmonhook.dll");
    if (g_hDll == NULL) {
        DWORD errorCode = GetLastError();
        std::wstringstream ss;
        ss << L"Failed to load kbdlayoutmonhook.dll. Error code: 0x" << std::hex << errorCode;
        WriteLog(ss.str());
        if (g_hInstanceMutex) {
            ReleaseMutex(g_hInstanceMutex);
            CloseHandle(g_hInstanceMutex);
        }
        StopConfigWatcher();
        return 1;
    }

    InstallGlobalHook = (InstallGlobalHookFunc)GetProcAddress(g_hDll, "InstallGlobalHook");
    UninstallGlobalHook = (UninstallGlobalHookFunc)GetProcAddress(g_hDll, "UninstallGlobalHook");
    SetLanguageHotKeyEnabled = (SetLanguageHotKeyEnabledFunc)GetProcAddress(g_hDll, "SetLanguageHotKeyEnabled");
    SetLayoutHotKeyEnabled = (SetLayoutHotKeyEnabledFunc)GetProcAddress(g_hDll, "SetLayoutHotKeyEnabled");
    GetLanguageHotKeyEnabled = (GetLanguageHotKeyEnabledFunc)GetProcAddress(g_hDll, "GetLanguageHotKeyEnabled");
    GetLayoutHotKeyEnabled = (GetLayoutHotKeyEnabledFunc)GetProcAddress(g_hDll, "GetLayoutHotKeyEnabled");
    SetDebugLoggingEnabled = (SetDebugLoggingEnabledFunc)GetProcAddress(g_hDll, "SetDebugLoggingEnabled");
    InitHookModule = (InitHookModuleFunc)GetProcAddress(g_hDll, "InitHookModule");
    CleanupHookModule = (CleanupHookModuleFunc)GetProcAddress(g_hDll, "CleanupHookModule");

    if (!InstallGlobalHook || !UninstallGlobalHook || !SetLanguageHotKeyEnabled || !SetLayoutHotKeyEnabled ||
        !GetLanguageHotKeyEnabled || !GetLayoutHotKeyEnabled || !SetDebugLoggingEnabled ||
        !InitHookModule || !CleanupHookModule) {
        DWORD errorCode = GetLastError();
        std::wstringstream ss;
        ss << L"Failed to get function addresses from kbdlayoutmonhook.dll. Error code: 0x" << std::hex << errorCode;
        WriteLog(ss.str());
        FreeLibrary(g_hDll);
        if (g_hInstanceMutex) {
            ReleaseMutex(g_hInstanceMutex);
            CloseHandle(g_hInstanceMutex);
        }
        StopConfigWatcher();
        return 1;
    }

    // Initialize the hook module now that it's loaded
    if (!InitHookModule()) {
        WriteLog(L"Failed to initialize hook module.");
        CleanupHookModule();
        FreeLibrary(g_hDll);
        if (g_hInstanceMutex) {
            ReleaseMutex(g_hInstanceMutex);
            CloseHandle(g_hInstanceMutex);
        }
        StopConfigWatcher();
        return 1;
    }

    // Propagate current debug logging state to the DLL
    SetDebugLoggingEnabled(g_debugEnabled.load());

    if (!InstallGlobalHook()) {
        WriteLog(L"Failed to install global hook.");
        CleanupHookModule();
        FreeLibrary(g_hDll);
        if (g_hInstanceMutex) {
            ReleaseMutex(g_hInstanceMutex);
            CloseHandle(g_hInstanceMutex);
        }
        StopConfigWatcher();
        return 1;
    }

    // Update the shared memory values at startup
    SetLanguageHotKeyEnabled(g_languageHotKeyEnabled.load());
    SetLayoutHotKeyEnabled(g_layoutHotKeyEnabled.load());

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    StopConfigWatcher();
    UninstallGlobalHook();
    CleanupHookModule();
    FreeLibrary(g_hDll);
    if (g_hInstanceMutex) {
        ReleaseMutex(g_hInstanceMutex);
        CloseHandle(g_hInstanceMutex);
    }
    WriteLog(L"Executable stopped.");
    return static_cast<int>(msg.wParam);
}
