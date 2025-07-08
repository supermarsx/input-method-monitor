// kbdlayoutmonhook.cpp
#include <windows.h>
#include <msctf.h>
#include <string>
#include <fstream>
#include <Shlwapi.h>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <combaseapi.h>

HINSTANCE g_hInst = NULL;
HHOOK g_hHook = NULL;
std::mutex g_mutex;
bool g_debugEnabled = false; // Global variable to control debug logging

#pragma data_seg(".shared")
HKL g_lastHKL = NULL; // Declare g_lastHKL in the shared memory segment
LONG g_refCount = 0;
bool g_languageHotKeyEnabled = false; // Shared variable for Language HotKey status
bool g_layoutHotKeyEnabled = false; // Shared variable for Layout HotKey status
#pragma data_seg()
#pragma comment(linker, "/SECTION:.shared,RWS")

HANDLE g_hMutex = NULL;

// Helper function to write to log file
void WriteLog(const std::wstring& message) {
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
    PathCombine(configPath, configPath, L"kbdlayoutmon.config");

    std::wifstream configFile(configPath);
    if (configFile.is_open()) {
        std::wstring line;
        while (std::getline(configFile, line)) {
            if (line == L"DEBUG=1") {
                g_debugEnabled = true;
            } else if (line == L"DEBUG=0") {
                g_debugEnabled = false;
            }
        }
        configFile.close();
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
    WaitForSingleObject(g_hMutex, INFINITE); // Lock mutex
    if (!g_languageHotKeyEnabled && !g_layoutHotKeyEnabled) {
        WriteLog(L"HotKeys are disabled. Skipping registry update.");
        ReleaseMutex(g_hMutex); // Release mutex
        return;
    }

    // Update Keyboard Layout Preload
    HKEY hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, L"Keyboard Layout\\Preload", 0, KEY_SET_VALUE, &hKey);
    if (result == ERROR_SUCCESS) {
        result = RegSetValueEx(hKey, L"1", 0, REG_SZ, (const BYTE*)klid.c_str(), (DWORD)((klid.size() + 1) * sizeof(wchar_t)));
        if (result == ERROR_SUCCESS) {
            std::wstringstream ss;
            ss << L"Set default input method in registry (Preload) to Locale ID: " << localeID << L", KLID: " << klid;
            WriteLog(ss.str().c_str());
        } else {
            std::wstringstream ss;
            ss << L"Failed to set default input method in registry (Preload). Error code: " << result;
            WriteLog(ss.str().c_str());
        }
        RegCloseKey(hKey);
    } else {
        std::wstringstream ss;
        ss << L"Failed to open registry key (Preload). Error code: " << result;
        WriteLog(ss.str().c_str());
    }

    result = RegOpenKeyEx(HKEY_USERS, L".DEFAULT\\Keyboard Layout\\Preload", 0, KEY_SET_VALUE, &hKey);
    if (result == ERROR_SUCCESS) {
        result = RegSetValueEx(hKey, L"1", 0, REG_SZ, (const BYTE*)klid.c_str(), (DWORD)((klid.size() + 1) * sizeof(wchar_t)));
        if (result == ERROR_SUCCESS) {
            std::wstringstream ss;
            ss << L"Set default input method in registry (DEFAULT Preload) to Locale ID: " << localeID << L", KLID: " << klid;
            WriteLog(ss.str().c_str());
        } else {
            std::wstringstream ss;
            ss << L"Failed to set default input method in registry (DEFAULT Preload). Error code: " << result;
            WriteLog(ss.str().c_str());
        }
        RegCloseKey(hKey);
    } else {
        std::wstringstream ss;
        ss << L"Failed to open registry key (Preload). Error code: " << result;
        WriteLog(ss.str().c_str());
    }

    // Update Control Panel International User Profile
    result = RegOpenKeyEx(HKEY_CURRENT_USER, L"Control Panel\\International\\User Profile", 0, KEY_SET_VALUE, &hKey);
    if (result == ERROR_SUCCESS) {
        std::wstring value = localeID + L":" + klid;
        result = RegSetValueEx(hKey, L"InputMethodOverride", 0, REG_SZ, (const BYTE*)value.c_str(), (DWORD)((value.size() + 1) * sizeof(wchar_t)));
        if (result == ERROR_SUCCESS) {
            std::wstringstream ss;
            ss << L"Set default input method in registry (InputMethodOverride) to " << value;
            WriteLog(ss.str().c_str());
        } else {
            std::wstringstream ss;
            ss << L"Failed to set default input method in registry (InputMethodOverride). Error code: " << result;
            WriteLog(ss.str().c_str());
        }
        RegCloseKey(hKey);
    } else {
        std::wstringstream ss;
        ss << L"Failed to open registry key (User Profile). Error code: " << result;
        WriteLog(ss.str().c_str());
    }
    ReleaseMutex(g_hMutex); // Release mutex
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
            WriteLog(ss.str().c_str());
            SetDefaultInputMethodInRegistry(localeID, klid);
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

// Function to install the global hook
extern "C" __declspec(dllexport) BOOL InstallGlobalHook() {
    LoadConfiguration();
    if (g_lastHKL == NULL) {
        g_lastHKL = GetKeyboardLayout(0);
    }
    WriteLog(L"DLL loaded.");

    g_hHook = SetWindowsHookEx(WH_SHELL, ShellProc, g_hInst, 0);
    if (g_hHook == NULL) {
        DWORD errorCode = GetLastError();
        std::wstringstream ss;
        ss << L"Failed to install global hook. Error code: 0x" << std::hex << errorCode;
        WriteLog(ss.str().c_str());
        return FALSE;
    }
    IncrementRefCount();
    WriteLog(L"Global hook installed successfully.");
    return TRUE;
}

// Function to uninstall the global hook
extern "C" __declspec(dllexport) void UninstallGlobalHook() {
    if (g_hHook != NULL) {
        if (UnhookWindowsHookEx(g_hHook)) {
            WriteLog(L"Global hook uninstalled successfully.");
        } else {
            DWORD errorCode = GetLastError();
            std::wstringstream ss;
            ss << L"Failed to uninstall global hook. Error code: 0x" << std::hex << errorCode;
            WriteLog(ss.str().c_str());
        }
        g_hHook = NULL;
    }
    DecrementRefCount();
    WriteLog(L"DLL unloaded.");
}

// Increment reference count
void IncrementRefCount() {
    WaitForSingleObject(g_hMutex, INFINITE);
    g_refCount++;
    std::wstringstream ss;
    ss << L"Reference count incremented to " << g_refCount;
    WriteLog(ss.str().c_str());
    ReleaseMutex(g_hMutex);
}

// Decrement reference count and check if cleanup is needed
void DecrementRefCount() {
    WaitForSingleObject(g_hMutex, INFINITE);
    g_refCount--;
    std::wstringstream ss;
    ss << L"Reference count decremented to " << g_refCount;
    WriteLog(ss.str().c_str());
    ReleaseMutex(g_hMutex);
}

// Function to get the Language HotKey enabled state
extern "C" __declspec(dllexport) bool GetLanguageHotKeyEnabled() {
    WaitForSingleObject(g_hMutex, INFINITE);
    bool enabled = g_languageHotKeyEnabled;
    ReleaseMutex(g_hMutex);
    return enabled;
}

// Function to get the Layout HotKey enabled state
extern "C" __declspec(dllexport) bool GetLayoutHotKeyEnabled() {
    WaitForSingleObject(g_hMutex, INFINITE);
    bool enabled = g_layoutHotKeyEnabled;
    ReleaseMutex(g_hMutex);
    return enabled;
}

// Function to set the Language HotKey enabled state
extern "C" __declspec(dllexport) void SetLanguageHotKeyEnabled(bool enabled) {
    WaitForSingleObject(g_hMutex, INFINITE);
    g_languageHotKeyEnabled = enabled;
    ReleaseMutex(g_hMutex);
}

// Function to set the Layout HotKey enabled state
extern "C" __declspec(dllexport) void SetLayoutHotKeyEnabled(bool enabled) {
    WaitForSingleObject(g_hMutex, INFINITE);
    g_layoutHotKeyEnabled = enabled;
    ReleaseMutex(g_hMutex);
}

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    UNREFERENCED_PARAMETER(lpReserved);
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            g_hInst = hinstDLL;
            g_hMutex = CreateMutex(NULL, FALSE, L"Global\\KbdHookMutex");
            break;
        case DLL_PROCESS_DETACH:
            if (g_hMutex)
                CloseHandle(g_hMutex);
            break;
    }
    return TRUE;
}
