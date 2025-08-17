#include "hotkey_registry.h"
#include "log.h"
#include "utils.h"
#include "winreg_handle.h"
#include "configuration.h"
#include <sstream>

// Global variable definitions
bool g_startupEnabled = false;
#ifndef UNIT_TEST
std::atomic<bool> g_languageHotKeyEnabled{false};
std::atomic<bool> g_layoutHotKeyEnabled{false};
#endif
std::atomic<bool> g_tempHotKeysEnabled{false};
DWORD g_tempHotKeyTimeout = 10000;

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
        WriteLog(ss.str().c_str());
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
        WriteLog(ss.str().c_str());
        return (result == ERROR_SUCCESS && wcscmp(value, L"3") == 0);
    } else {
        WriteLog(L"Failed to open registry key for Layout HotKey.");
    }
    return false;
}

// Generic helper to toggle a registry-backed hotkey
static void ToggleHotKey(HWND hwnd, const wchar_t* valueName,
                         std::atomic<bool>& enabledFlag,
                         const wchar_t* onValue, const wchar_t* offValue,
                         void (*updateFunc)(bool), bool overrideState = false,
                         bool state = false) {
    WinRegHandle hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, L"Keyboard Layout\\Toggle", 0,
                               KEY_SET_VALUE, hKey.receive());
    if (result == ERROR_SUCCESS) {
        const wchar_t* value;
        if (overrideState) {
            value = state ? onValue : offValue;
            enabledFlag.store(state);
        } else {
            bool cur = enabledFlag.load();
            value = cur ? offValue : onValue;
            enabledFlag.store(!cur);
        }

        RegSetValueEx(hKey.get(), valueName, 0, REG_SZ,
                      reinterpret_cast<const BYTE*>(value),
                      (lstrlen(value) + 1) * sizeof(wchar_t));

        if (updateFunc)
            updateFunc(enabledFlag.load());
    }
}

void ToggleLanguageHotKey(HWND hwnd, bool overrideState, bool state) {
    g_languageHotKeyEnabled.store(IsLanguageHotKeyEnabled());
    ToggleHotKey(hwnd, L"Language HotKey", g_languageHotKeyEnabled, L"3", L"1",
                 SetLanguageHotKeyEnabled, overrideState, state);
}

void ToggleLayoutHotKey(HWND hwnd, bool overrideState, bool state) {
    g_layoutHotKeyEnabled.store(IsLayoutHotKeyEnabled());
    ToggleHotKey(hwnd, L"Layout HotKey", g_layoutHotKeyEnabled, L"3", L"2",
                 SetLayoutHotKeyEnabled, overrideState, state);
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

        // Update the shared memory values
        SetLanguageHotKeyEnabled(g_languageHotKeyEnabled.load());
        SetLayoutHotKeyEnabled(g_layoutHotKeyEnabled.load());

        // Set a timer to revert changes after configured timeout
        SetTimer(hwnd, 1, g_tempHotKeyTimeout, NULL);
    } else {
        WriteLog(L"Failed to open registry key for temporarily enabling hotkeys.");
    }
}

void OnTimer(HWND hwnd) {
    if (g_tempHotKeysEnabled.load()) {
        ToggleLanguageHotKey(hwnd, true, false);
        ToggleLayoutHotKey(hwnd, true, false);
        g_tempHotKeysEnabled.store(false);
    }
    KillTimer(hwnd, 1);
}
