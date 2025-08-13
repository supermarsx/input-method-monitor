// kbdlayoutmonhook.cpp
#include <windows.h>
#include <msctf.h>
#include <string>
#include <shlwapi.h>
#include <mutex>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <combaseapi.h>
#include <thread>
#include <condition_variable>
#include <queue>
#include "configuration.h"
#include "winreg_handle.h"
#include "handle_guard.h"

std::atomic<bool> g_debugEnabled{false};
HINSTANCE g_hInst = NULL;
HHOOK g_hHook = NULL;

#pragma data_seg(".shared")
HKL g_lastHKL = NULL; // Declare g_lastHKL in the shared memory segment
LONG g_refCount = 0;
std::atomic<bool> g_languageHotKeyEnabled{false}; // Shared variable for Language HotKey status
std::atomic<bool> g_layoutHotKeyEnabled{false}; // Shared variable for Layout HotKey status
#pragma data_seg()
#pragma comment(linker, "/SECTION:.shared,RWS")

HandleGuard g_hMutex;

std::thread g_workerThread;
std::mutex g_queueMutex;
std::condition_variable g_queueCV;
std::queue<std::pair<std::wstring, std::wstring>> g_taskQueue;
bool g_workerRunning = false;

void IncrementRefCount();
void DecrementRefCount();

// Helper function to write to log file via named pipe
void WriteLog(const std::wstring& message) {
    HandleGuard pipe(CreateFileW(L"\\\\.\\pipe\\kbdlayoutmon_log", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
    if (pipe.get() != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten = 0;
        WriteFile(pipe.get(), message.c_str(), (DWORD)((message.size() + 1) * sizeof(wchar_t)), &bytesWritten, NULL);
    }
}


// Function to get the KLID in the format "00000409"
std::wstring GetKLID(HKL hkl) {
    wchar_t klid[KL_NAMELENGTH];
    if (GetKeyboardLayoutName(klid)) {
        return std::wstring(klid);
    }
    return L"Unknown";
}

// Function to get the locale ID in the format "0409"
std::wstring GetLocaleID(HKL hkl) {
    LANGID langID = LOWORD(hkl);
    std::wstringstream ss;
    ss << std::setw(4) << std::setfill(L'0') << std::hex << langID;
    return ss.str();
}

// Function to set the default input method in the registry
void SetDefaultInputMethodInRegistry(const std::wstring& localeID, const std::wstring& klid) {
    if (!g_languageHotKeyEnabled.load() && !g_layoutHotKeyEnabled.load()) {
        WriteLog(L"HotKeys are disabled. Skipping registry update.");
        return;
    }

    // Update Keyboard Layout Preload
    WinRegHandle hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, L"Keyboard Layout\\Preload", 0,
                               KEY_SET_VALUE, hKey.receive());
    if (result == ERROR_SUCCESS) {
        result = RegSetValueEx(hKey.get(), L"1", 0, REG_SZ,
                              reinterpret_cast<const BYTE*>(klid.c_str()),
                              (DWORD)((klid.size() + 1) * sizeof(wchar_t)));
        if (result == ERROR_SUCCESS) {
            std::wstringstream ss;
            ss << L"Set default input method in registry (Preload) to Locale ID: " << localeID << L", KLID: " << klid;
            WriteLog(ss.str());
        } else {
            std::wstringstream ss;
            ss << L"Failed to set default input method in registry (Preload). Error code: " << result;
            WriteLog(ss.str());
        }
    } else {
        std::wstringstream ss;
        ss << L"Failed to open registry key (Preload). Error code: " << result;
        WriteLog(ss.str());
    }

    result = RegOpenKeyEx(HKEY_USERS, L".DEFAULT\\Keyboard Layout\\Preload", 0,
                          KEY_SET_VALUE, hKey.receive());
    if (result == ERROR_SUCCESS) {
        result = RegSetValueEx(hKey.get(), L"1", 0, REG_SZ,
                              reinterpret_cast<const BYTE*>(klid.c_str()),
                              (DWORD)((klid.size() + 1) * sizeof(wchar_t)));
        if (result == ERROR_SUCCESS) {
            std::wstringstream ss;
            ss << L"Set default input method in registry (DEFAULT Preload) to Locale ID: " << localeID << L", KLID: " << klid;
            WriteLog(ss.str());
        } else {
            std::wstringstream ss;
            ss << L"Failed to set default input method in registry (DEFAULT Preload). Error code: " << result;
            WriteLog(ss.str());
        }
    } else {
        std::wstringstream ss;
        ss << L"Failed to open registry key (Preload). Error code: " << result;
        WriteLog(ss.str());
    }

    // Update Control Panel International User Profile
    result = RegOpenKeyEx(HKEY_CURRENT_USER,
                          L"Control Panel\\International\\User Profile", 0,
                          KEY_SET_VALUE, hKey.receive());
    if (result == ERROR_SUCCESS) {
        std::wstring value = localeID + L":" + klid;
        result = RegSetValueEx(hKey.get(), L"InputMethodOverride", 0, REG_SZ,
                               reinterpret_cast<const BYTE*>(value.c_str()),
                               (DWORD)((value.size() + 1) * sizeof(wchar_t)));
        if (result == ERROR_SUCCESS) {
            std::wstringstream ss;
            ss << L"Set default input method in registry (InputMethodOverride) to " << value;
            WriteLog(ss.str());
        } else {
            std::wstringstream ss;
            ss << L"Failed to set default input method in registry (InputMethodOverride). Error code: " << result;
            WriteLog(ss.str());
        }
    } else {
        std::wstringstream ss;
        ss << L"Failed to open registry key (User Profile). Error code: " << result;
        WriteLog(ss.str());
    }
}

void WorkerThread() {
    for (;;) {
        std::unique_lock<std::mutex> lock(g_queueMutex);
        g_queueCV.wait(lock, [] { return !g_taskQueue.empty() || !g_workerRunning; });
        if (!g_workerRunning && g_taskQueue.empty())
            break;
        auto task = std::move(g_taskQueue.front());
        g_taskQueue.pop();
        lock.unlock();

        SetDefaultInputMethodInRegistry(task.first, task.second);
    }
}

void StartWorkerThread() {
    g_workerRunning = true;
    g_workerThread = std::thread(WorkerThread);
}

void StopWorkerThread() {
    {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        g_workerRunning = false;
    }
    g_queueCV.notify_all();
    if (g_workerThread.joinable())
        g_workerThread.join();
}

/**
 * @brief Initialize global state after loading the DLL.
 * @return TRUE on success.
 */
extern "C" __declspec(dllexport) BOOL InitHookModule() {
    g_hMutex.reset(CreateMutex(NULL, FALSE, L"Global\\KbdHookMutex"));
    g_config.load();
    auto it = g_config.settings.find(L"debug");
    g_debugEnabled.store(it != g_config.settings.end() && it->second == L"1");
    StartWorkerThread();
    return TRUE;
}

/**
 * @brief Cleanup state before the DLL is unloaded.
 */
extern "C" __declspec(dllexport) void CleanupHookModule() {
    StopWorkerThread();
    g_hMutex.reset();
}

// Hook procedure to monitor system-wide messages
LRESULT CALLBACK ShellProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HSHELL_LANGUAGE) {
        HKL hkl = GetKeyboardLayout(0);
        if (hkl != g_lastHKL) {
            g_lastHKL = hkl;
            std::wstring localeID = GetLocaleID(hkl);
            std::wstring klid = GetKLID(hkl);
            std::wstringstream ss;
            ss << L"Keyboard layout changed. Locale ID: " << localeID << L", KLID: " << klid;
            WriteLog(ss.str());

            if (!g_workerRunning)
                StartWorkerThread();

            {
                std::lock_guard<std::mutex> lock(g_queueMutex);
                g_taskQueue.emplace(localeID, klid);
            }
            g_queueCV.notify_one();
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

/**
 * @brief Install the shell hook used to monitor layout changes.
 * @return TRUE if the hook was successfully installed.
 */
extern "C" __declspec(dllexport) BOOL InstallGlobalHook() {
    if (g_lastHKL == NULL) {
        g_lastHKL = GetKeyboardLayout(0);
    }
    WriteLog(L"DLL loaded.");

    g_hHook = SetWindowsHookEx(WH_SHELL, ShellProc, g_hInst, 0);
    if (g_hHook == NULL) {
        DWORD errorCode = GetLastError();
        std::wstringstream ss;
        ss << L"Failed to install global hook. Error code: 0x" << std::hex << errorCode;
        WriteLog(ss.str());
        return FALSE;
    }
    IncrementRefCount();
    WriteLog(L"Global hook installed successfully.");
    return TRUE;
}

/**
 * @brief Remove the previously installed shell hook.
 */
extern "C" __declspec(dllexport) void UninstallGlobalHook() {
    if (g_hHook != NULL) {
        if (UnhookWindowsHookEx(g_hHook)) {
            WriteLog(L"Global hook uninstalled successfully.");
        } else {
            DWORD errorCode = GetLastError();
            std::wstringstream ss;
            ss << L"Failed to uninstall global hook. Error code: 0x" << std::hex << errorCode;
            WriteLog(ss.str());
        }
        g_hHook = NULL;
    }
    DecrementRefCount();
    WriteLog(L"DLL unloaded.");
}

// Increment reference count
void IncrementRefCount() {
    WaitForSingleObject(g_hMutex.get(), INFINITE);
    g_refCount++;
    std::wstringstream ss;
    ss << L"Reference count incremented to " << g_refCount;
    WriteLog(ss.str());
    ReleaseMutex(g_hMutex.get());
}

// Decrement reference count and check if cleanup is needed
void DecrementRefCount() {
    WaitForSingleObject(g_hMutex.get(), INFINITE);
    g_refCount--;
    std::wstringstream ss;
    ss << L"Reference count decremented to " << g_refCount;
    WriteLog(ss.str());
    ReleaseMutex(g_hMutex.get());
}

/**
 * @brief Query whether the Windows "Language" hotkey is enabled.
 */
extern "C" __declspec(dllexport) bool GetLanguageHotKeyEnabled() {
    return g_languageHotKeyEnabled.load();
}

/**
 * @brief Query whether the Windows "Layout" hotkey is enabled.
 */
extern "C" __declspec(dllexport) bool GetLayoutHotKeyEnabled() {
    return g_layoutHotKeyEnabled.load();
}

/**
 * @brief Set the Language hotkey state shared with the executable.
 * @param enabled New enabled flag.
 */
extern "C" __declspec(dllexport) void SetLanguageHotKeyEnabled(bool enabled) {
    g_languageHotKeyEnabled.store(enabled);
}

/**
 * @brief Set the Layout hotkey state shared with the executable.
 * @param enabled New enabled flag.
 */
extern "C" __declspec(dllexport) void SetLayoutHotKeyEnabled(bool enabled) {
    g_layoutHotKeyEnabled.store(enabled);
}

/**
 * @brief Update debug logging state for the DLL.
 * @param enabled When true, log messages are written.
 */
extern "C" __declspec(dllexport) void SetDebugLoggingEnabled(bool enabled) {
    g_debugEnabled.store(enabled);
}

/**
 * @brief Standard DLL entry point called by the loader.
 */
BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    UNREFERENCED_PARAMETER(lpReserved);
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            g_hInst = hinstDLL;
            DisableThreadLibraryCalls(hinstDLL);
            break;
        case DLL_PROCESS_DETACH:
            g_hInst = NULL;
            break;
    }
    return TRUE;
}
