#include <catch2/catch_test_macros.hpp>
#include "../source/hotkey_registry.h"
#include "../source/configuration.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <iterator>

LONG g_RegSetValueExResult = ERROR_SUCCESS;
extern std::atomic<bool> g_debugEnabled;
UINT (*pSetTimer)(HWND, UINT, UINT, TIMERPROC) = [](HWND, UINT, UINT, TIMERPROC) -> UINT { return 1; };
BOOL (*pKillTimer)(HWND, UINT) = [](HWND, UINT) -> BOOL { return TRUE; };

TEST_CASE("Startup registry flag toggles") {
    g_startupEnabled = false;
    AddToStartup();
    REQUIRE(g_startupEnabled);
    RemoveFromStartup();
    REQUIRE_FALSE(g_startupEnabled);
}

TEST_CASE("ToggleLanguageHotKey logs error and preserves state on RegSetValueEx failure") {
    using namespace std::chrono_literals;
    namespace fs = std::filesystem;

    g_debugEnabled.store(true);
    g_languageHotKeyEnabled.store(false);

    fs::path logPath = fs::temp_directory_path() / "toggle_fail.log";
    g_config.set(L"log_path", logPath.wstring());

    g_RegSetValueExResult = 5; // simulate failure
    ToggleLanguageHotKey(nullptr);

    std::this_thread::sleep_for(200ms);

    std::wifstream file(logPath);
    std::wstring content((std::istreambuf_iterator<wchar_t>(file)), std::istreambuf_iterator<wchar_t>());
    REQUIRE(content.find(L"Error: 5") != std::wstring::npos);
    REQUIRE_FALSE(g_languageHotKeyEnabled.load());

    g_RegSetValueExResult = ERROR_SUCCESS;
    g_config.set(L"log_path", L"");
    fs::remove(logPath);
}

TEST_CASE("TemporarilyEnableHotKeys logs error and preserves state on RegSetValueEx failure") {
    using namespace std::chrono_literals;
    namespace fs = std::filesystem;

    g_debugEnabled.store(true);
    g_tempHotKeysEnabled.store(false);
    g_languageHotKeyEnabled.store(false);
    g_layoutHotKeyEnabled.store(false);

    fs::path logPath = fs::temp_directory_path() / "temp_enable_fail.log";
    g_config.set(L"log_path", logPath.wstring());

    g_RegSetValueExResult = 5; // simulate failure
    TemporarilyEnableHotKeys(nullptr);

    std::this_thread::sleep_for(200ms);

    std::wifstream file(logPath);
    std::wstring content((std::istreambuf_iterator<wchar_t>(file)), std::istreambuf_iterator<wchar_t>());
    REQUIRE(content.find(L"Error: 5") != std::wstring::npos);
    REQUIRE_FALSE(g_tempHotKeysEnabled.load());
    REQUIRE_FALSE(g_languageHotKeyEnabled.load());
    REQUIRE_FALSE(g_layoutHotKeyEnabled.load());

    g_RegSetValueExResult = ERROR_SUCCESS;
    g_config.set(L"log_path", L"");
    fs::remove(logPath);
}

