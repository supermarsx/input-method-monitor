// kbdlayoutmon.cpp
#include <windows.h>
#include <string>
#include <fstream>
#include <shlwapi.h>
#include <atomic>
#include <sstream>
#include <shellapi.h>
#include <vector>
#include <memory>
#include "configuration.h"
#include "constants.h"
#include "log.h"
#include "winreg_handle.h"
#include "handle_guard.h"
#include "utils.h"
#include "config_watcher.h"
#include "tray_icon.h"
#include "hotkey_registry.h"

// Forward declarations
void ApplyConfig(HWND hwnd);


HINSTANCE g_hInst = NULL;
HMODULE g_hDll = NULL;
HandleGuard g_hInstanceMutex; // Mutex to enforce single instance
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
bool g_cliMode = false;                     // Suppress GUI/tray behavior
HWND g_hwnd = NULL;                // Handle to our message window
std::unique_ptr<TrayIcon> g_trayIcon;
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
        L"  --max-queue-size <num>     Override log queue length\n"
        L"  --version    Print the application version and exit\n"
        L"  --help       Show this help message and exit";
}



// Apply configuration values to runtime settings
void ApplyConfig(HWND hwnd) {
    auto debugVal = g_config.get(L"debug");
    bool newDebug = (debugVal && *debugVal == L"1");
    if (newDebug != g_debugEnabled.load()) {
        g_debugEnabled.store(newDebug);
        if (SetDebugLoggingEnabled)
            SetDebugLoggingEnabled(g_debugEnabled.load());
    }

    bool tray = true;
    auto trayVal = g_config.get(L"tray_icon");
    if (trayVal)
        tray = *trayVal != L"0";
    if (tray != g_trayIconEnabled.load()) {
        g_trayIconEnabled.store(tray);
        if (tray) {
            if (hwnd)
                g_trayIcon = std::make_unique<TrayIcon>(hwnd);
        } else {
            g_trayIcon.reset();
        }
    } else if (tray && hwnd && !g_trayIcon) {
        g_trayIcon = std::make_unique<TrayIcon>(hwnd);
    }

    auto timeoutVal = g_config.get(L"temp_hotkey_timeout");
    if (timeoutVal) {
        g_tempHotKeyTimeout =
            std::wcstoul(timeoutVal->c_str(), nullptr, 10);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) {
                ShowTrayMenu(hwnd);
            }
            break;
        case WM_COMMAND:
            HandleTrayCommand(hwnd, wParam);
            break;
        case WM_TIMER:
            OnTimer(hwnd);
            break;
        case WM_UPDATE_TRAY_MENU:
            ShowTrayMenu(hwnd);
            break;
        case WM_DESTROY:
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
    g_hInstanceMutex.reset(CreateMutex(NULL, TRUE, L"InputMethodMonitorSingleton"));
    if (g_hInstanceMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        WriteLog(LogLevel::Error, L"Another instance is already running.");
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
        ReleaseMutex(g_hInstanceMutex.get());
        g_hInstanceMutex.reset();
        return 0;
    }

    // Load configuration before any logging occurs
    g_config.load(customConfigPath);
    ApplyConfig(NULL);

    // Parse command line options after the config file so they override
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            if (wcscmp(argv[i], L"--config") == 0 && i + 1 < argc) {
                ++i; // skip the path argument
            } else if (wcscmp(argv[i], L"--no-tray") == 0) {
                g_trayIconEnabled.store(false);
            } else if (wcscmp(argv[i], L"--tray-icon") == 0 && i + 1 < argc) {
                g_config.set(L"tray_icon", argv[i + 1]);
                g_trayIconEnabled.store(wcscmp(argv[i + 1], L"0") != 0);
                ++i;
            } else if (wcscmp(argv[i], L"--temp-hotkey-timeout") == 0 && i + 1 < argc) {
                g_config.set(L"temp_hotkey_timeout", argv[i + 1]);
                g_tempHotKeyTimeout = std::wcstoul(argv[i + 1], nullptr, 10);
                ++i;
            } else if (wcscmp(argv[i], L"--log-path") == 0 && i + 1 < argc) {
                g_config.set(L"log_path", argv[i + 1]);
                ++i;
            } else if (wcscmp(argv[i], L"--max-log-size-mb") == 0 && i + 1 < argc) {
                g_config.set(L"max_log_size_mb", argv[i + 1]);
                ++i;
            } else if (wcscmp(argv[i], L"--max-queue-size") == 0 && i + 1 < argc) {
                g_config.set(L"max_queue_size", argv[i + 1]);
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
                    ReleaseMutex(g_hInstanceMutex.get());
                    g_hInstanceMutex.reset();
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
                    ReleaseMutex(g_hInstanceMutex.get());
                    g_hInstanceMutex.reset();
                }
                return 0;
            }
        }
        ApplyConfig(NULL);
        if (auto qval = g_config.get(L"max_queue_size")) {
            try {
                g_log.setMaxQueueSize(std::stoul(*qval));
            } catch (...) {
            }
        }
        LocalFree(argv);
    }

    WriteLog(LogLevel::Info, L"Executable started.");

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
        WriteLog(LogLevel::Error, ss.str());
        if (g_hInstanceMutex) {
            ReleaseMutex(g_hInstanceMutex.get());
            g_hInstanceMutex.reset();
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
        WriteLog(LogLevel::Error, ss.str());
        if (g_hInstanceMutex) {
            ReleaseMutex(g_hInstanceMutex.get());
            g_hInstanceMutex.reset();
        }
        return 1;
    }

    g_hwnd = hwnd;

    // Ensure tray icon is cleaned up on any exit path
    struct TrayIconGuard {
        ~TrayIconGuard() { g_trayIcon.reset(); }
    } trayGuard;

    // Initialize tray icon based on current configuration
    ApplyConfig(hwnd);

    MSG msg;
    {
        ConfigWatcher configWatcher(hwnd);

        // Load the DLL
        g_hDll = LoadLibrary(L"kbdlayoutmonhook.dll");
        if (g_hDll == NULL) {
            DWORD errorCode = GetLastError();
            std::wstringstream ss;
            ss << L"Failed to load kbdlayoutmonhook.dll. Error code: 0x" << std::hex << errorCode;
            WriteLog(LogLevel::Error, ss.str());
            if (g_hInstanceMutex) {
                ReleaseMutex(g_hInstanceMutex.get());
                g_hInstanceMutex.reset();
            }
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
            WriteLog(LogLevel::Error, ss.str());
            FreeLibrary(g_hDll);
            if (g_hInstanceMutex) {
                ReleaseMutex(g_hInstanceMutex.get());
                g_hInstanceMutex.reset();
            }
            return 1;
        }

    // Initialize the hook module now that it's loaded
        if (!InitHookModule()) {
            WriteLog(LogLevel::Error, L"Failed to initialize hook module.");
            CleanupHookModule();
            FreeLibrary(g_hDll);
            if (g_hInstanceMutex) {
                ReleaseMutex(g_hInstanceMutex.get());
                g_hInstanceMutex.reset();
            }
            return 1;
        }

    // Propagate current debug logging state to the DLL
    SetDebugLoggingEnabled(g_debugEnabled.load());

        if (!InstallGlobalHook()) {
            WriteLog(LogLevel::Error, L"Failed to install global hook.");
            CleanupHookModule();
            FreeLibrary(g_hDll);
            if (g_hInstanceMutex) {
                ReleaseMutex(g_hInstanceMutex.get());
                g_hInstanceMutex.reset();
            }
            return 1;
        }

    // Update the shared memory values at startup
    SetLanguageHotKeyEnabled(g_languageHotKeyEnabled.load());
    SetLayoutHotKeyEnabled(g_layoutHotKeyEnabled.load());

        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    UninstallGlobalHook();
    CleanupHookModule();
    FreeLibrary(g_hDll);
    if (g_hInstanceMutex) {
        ReleaseMutex(g_hInstanceMutex.get());
        g_hInstanceMutex.reset();
    }
    WriteLog(LogLevel::Info, L"Executable stopped.");
    return static_cast<int>(msg.wParam);
}
