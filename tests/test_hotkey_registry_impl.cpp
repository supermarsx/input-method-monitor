#include "windows_stub.h"
#include <string>
#include "../source/app_state.h"
#include "../source/hotkey_registry.h"

// Minimal Log declarations to avoid including the heavy log.h which pulls in <fstream>
enum class LogLevel { Info, Warn, Error };
void WriteLog(LogLevel level, const std::wstring& message);


// Minimal test-only implementations that simulate registry operations using
// test-controlled globals (g_RegOpenKeyExResult, g_RegSetValueExResult, etc.)

void ToggleLanguageHotKey(HWND hwnd, bool overrideState, bool desiredState) {
    auto& app = GetAppState();
    bool previous = app.languageHotKeyEnabled.load();
    bool desired;
    if (overrideState) {
        desired = desiredState;
    } else {
        desired = !previous;
    }

    // Simulate RegOpenKeyEx failure
    extern LONG g_RegOpenKeyExResult;
    extern bool g_RegOpenKeyExFailOnSetValue;
    extern LONG g_RegSetValueExResult;

    if (g_RegOpenKeyExResult != ERROR_SUCCESS && g_RegOpenKeyExFailOnSetValue) {
        {
        wchar_t buf[64]; swprintf(buf, _countof(buf), L"Failed to open registry key for Language HotKey. Error: %d", g_RegOpenKeyExResult);
        WriteLog(LogLevel::Error, std::wstring(buf));
    }
        return;
    }

    // Simulate RegSetValueEx
    if (g_RegSetValueExResult != ERROR_SUCCESS) {
        {
        wchar_t buf[64]; swprintf(buf, _countof(buf), L"Failed to set registry value for Language HotKey. Error: %d", g_RegSetValueExResult);
        WriteLog(LogLevel::Error, std::wstring(buf));
    }
        app.languageHotKeyEnabled.store(previous);
        return;
    }

    app.languageHotKeyEnabled.store(desired);
    if (SetLanguageHotKeyEnabled)
        SetLanguageHotKeyEnabled(app.languageHotKeyEnabled.load());
}

void ToggleLayoutHotKey(HWND hwnd, bool overrideState, bool desiredState) {
    auto& app = GetAppState();
    bool previous = app.layoutHotKeyEnabled.load();
    bool desired = overrideState ? desiredState : !previous;

    extern LONG g_RegOpenKeyExResult;
    extern bool g_RegOpenKeyExFailOnSetValue;
    extern LONG g_RegSetValueExResult;

    if (g_RegOpenKeyExResult != ERROR_SUCCESS && g_RegOpenKeyExFailOnSetValue) {
        {
        wchar_t buf[64]; swprintf(buf, _countof(buf), L"Failed to open registry key for Layout HotKey. Error: %d", g_RegOpenKeyExResult);
        WriteLog(LogLevel::Error, std::wstring(buf));
    }
        return;
    }

    if (g_RegSetValueExResult != ERROR_SUCCESS) {
        {
        wchar_t buf[64]; swprintf(buf, _countof(buf), L"Failed to set registry value for Layout HotKey. Error: %d", g_RegSetValueExResult);
        WriteLog(LogLevel::Error, std::wstring(buf));
    }
        app.layoutHotKeyEnabled.store(previous);
        return;
    }

    app.layoutHotKeyEnabled.store(desired);
    if (SetLayoutHotKeyEnabled)
        SetLayoutHotKeyEnabled(app.layoutHotKeyEnabled.load());
}

void TemporarilyEnableHotKeys(HWND hwnd) {
    auto& state = GetAppState();

    extern LONG g_RegOpenKeyExResult;
    extern LONG g_RegSetValueExResult;

    if (g_RegOpenKeyExResult != ERROR_SUCCESS) {
        {
        wchar_t buf[64]; swprintf(buf, _countof(buf), L"Failed to open registry key for temporarily enabling hotkeys. Error: %d", g_RegOpenKeyExResult);
        WriteLog(LogLevel::Error, std::wstring(buf));
    }
        return;
    }

    if (g_RegSetValueExResult != ERROR_SUCCESS) {
        {
        wchar_t buf[64]; swprintf(buf, _countof(buf), L"Failed to set registry values for temporarily enabling hotkeys. Error: %d", g_RegSetValueExResult);
        WriteLog(LogLevel::Error, std::wstring(buf));
    }
        return;
    }

    state.tempHotKeysEnabled.store(true);
    state.languageHotKeyEnabled.store(true);
    state.layoutHotKeyEnabled.store(true);
    if (SetLanguageHotKeyEnabled) SetLanguageHotKeyEnabled(true);
    if (SetLayoutHotKeyEnabled) SetLayoutHotKeyEnabled(true);
}

bool HandleHotkeyFlag(const wchar_t* flag) {
    if (!flag) return false;
    if (wcscmp(flag, L"--disable-language-hotkey") == 0) {
        ToggleLanguageHotKey(nullptr, true, false);
        return true;
    }
    if (wcscmp(flag, L"--enable-layout-hotkey") == 0) {
        ToggleLayoutHotKey(nullptr, true, true);
        return true;
    }
    if (wcscmp(flag, L"--enable-language-hotkey") == 0) {
        ToggleLanguageHotKey(nullptr, true, true);
        return true;
    }
    if (wcscmp(flag, L"--disable-layout-hotkey") == 0) {
        ToggleLayoutHotKey(nullptr, true, false);
        return true;
    }
    return false;
}
