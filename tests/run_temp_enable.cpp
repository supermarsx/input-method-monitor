#define UNIT_TEST
#include "windows_stub.h"
#include "../source/hotkey_registry.h"
#include "../source/app_state.h"
#include "../source/log.h"

int main() {
    GetAppState().debugEnabled.store(true);
    g_RegSetValueExResult = 5;
    std::wstring path = std::filesystem::temp_directory_path() / L"temp_enable_debug.log";
    g_config.set(L"log_path", path);
    TemporarilyEnableHotKeys(nullptr);
    return 0;
}
