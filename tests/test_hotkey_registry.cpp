#include <catch2/catch_test_macros.hpp>
#include "../source/hotkey_registry.h"
#include "../source/configuration.h"
#include "../source/hotkey_cli.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <iterator>

LONG g_RegSetValueExResult = ERROR_SUCCESS;
extern std::atomic<bool> g_debugEnabled;
extern UINT (*pSetTimer)(HWND, UINT, UINT, TIMERPROC);
extern BOOL (*pKillTimer)(HWND, UINT);
std::atomic<bool> g_languageHotKeyEnabled{false};
std::atomic<bool> g_layoutHotKeyEnabled{false};
SetLanguageHotKeyEnabledFunc SetLanguageHotKeyEnabled = nullptr;
SetLayoutHotKeyEnabledFunc SetLayoutHotKeyEnabled = nullptr;
DWORD (*pGetModuleFileNameW)(HINSTANCE, wchar_t*, DWORD) = [](HINSTANCE, wchar_t*, DWORD) -> DWORD { return 0; };

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

TEST_CASE("DisableLanguageHotKey sets flag to disabled") {
    g_RegSetValueExResult = ERROR_SUCCESS;
    ToggleLanguageHotKey(nullptr, true, true);
    REQUIRE(g_languageHotKeyEnabled.load());
    ToggleLanguageHotKey(nullptr, true, false);
    REQUIRE_FALSE(g_languageHotKeyEnabled.load());
}

TEST_CASE("EnableLayoutHotKey sets flag to enabled") {
    g_RegSetValueExResult = ERROR_SUCCESS;
    ToggleLayoutHotKey(nullptr, true, false);
    REQUIRE_FALSE(g_layoutHotKeyEnabled.load());
    ToggleLayoutHotKey(nullptr, true, true);
    REQUIRE(g_layoutHotKeyEnabled.load());
}

TEST_CASE("Command line flag disables Language hotkey") {
    g_RegSetValueExResult = ERROR_SUCCESS;
    g_languageHotKeyEnabled.store(true);
    REQUIRE(HandleHotkeyFlag(L"--disable-language-hotkey"));
    REQUIRE_FALSE(g_languageHotKeyEnabled.load());
}

TEST_CASE("Command line flag enables Layout hotkey") {
    g_RegSetValueExResult = ERROR_SUCCESS;
    g_layoutHotKeyEnabled.store(false);
    REQUIRE(HandleHotkeyFlag(L"--enable-layout-hotkey"));
    REQUIRE(g_layoutHotKeyEnabled.load());
}

