#include "hotkey_cli.h"
#include "hotkey_registry.h"

bool HandleHotkeyFlag(const wchar_t* flag) {
    if (wcscmp(flag, L"--enable-language-hotkey") == 0) {
        ToggleLanguageHotKey(nullptr, true, true);
        return true;
    }
    if (wcscmp(flag, L"--disable-language-hotkey") == 0) {
        ToggleLanguageHotKey(nullptr, true, false);
        return true;
    }
    if (wcscmp(flag, L"--enable-layout-hotkey") == 0) {
        ToggleLayoutHotKey(nullptr, true, true);
        return true;
    }
    if (wcscmp(flag, L"--disable-layout-hotkey") == 0) {
        ToggleLayoutHotKey(nullptr, true, false);
        return true;
    }
    return false;
}
