#include <catch2/catch_test_macros.hpp>
#include "../source/hotkey_registry.h"
#include "../source/configuration.h"
#include "../source/hotkey_cli.h"
#include "../source/app_state.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <iterator>

LONG g_RegOpenKeyExResult = ERROR_SUCCESS;
bool g_RegOpenKeyExFailOnSetValue = false;
LONG g_RegSetValueExResult = ERROR_SUCCESS;
SetLanguageHotKeyEnabledFunc SetLanguageHotKeyEnabled = nullptr;
SetLayoutHotKeyEnabledFunc SetLayoutHotKeyEnabled = nullptr;
DWORD (*pGetModuleFileNameW)(HINSTANCE, wchar_t*, DWORD) = [](HINSTANCE, wchar_t*, DWORD) -> DWORD { return 0; };

TEST_CASE("Startup registry flag toggles") {
    auto& state = GetAppState();
    state.startupEnabled.store(false);
    AddToStartup();
    REQUIRE(state.startupEnabled.load());
    RemoveFromStartup();
    REQUIRE_FALSE(state.startupEnabled.load());
}

TEST_CASE("ToggleLanguageHotKey logs error and preserves state on RegSetValueEx failure") {
    using namespace std::chrono_literals;
    namespace fs = std::filesystem;

    auto& state = GetAppState();
    state.debugEnabled.store(true);
    state.languageHotKeyEnabled.store(false);

    fs::path logPath = fs::temp_directory_path() / "toggle_fail.log";
    g_config.set(L"log_path", logPath.wstring());

    g_RegSetValueExResult = 5; // simulate failure
    ToggleLanguageHotKey(nullptr);

    std::this_thread::sleep_for(200ms);

    std::wifstream file(logPath);
    std::wstring content((std::istreambuf_iterator<wchar_t>(file)), std::istreambuf_iterator<wchar_t>());
    REQUIRE(content.find(L"Error: 5") != std::wstring::npos);
    REQUIRE_FALSE(state.languageHotKeyEnabled.load());

    g_RegSetValueExResult = ERROR_SUCCESS;
    g_config.set(L"log_path", L"");
    fs::remove(logPath);
}

TEST_CASE("ToggleLanguageHotKey logs error and preserves state on RegOpenKeyEx failure") {
    using namespace std::chrono_literals;
    namespace fs = std::filesystem;

    auto& state = GetAppState();
    state.debugEnabled.store(true);
    state.languageHotKeyEnabled.store(false);

    fs::path logPath = fs::temp_directory_path() / "toggle_open_fail.log";
    g_config.set(L"log_path", logPath.wstring());

    g_RegOpenKeyExResult = 5; // simulate failure
    g_RegOpenKeyExFailOnSetValue = true;
    ToggleLanguageHotKey(nullptr);

    std::this_thread::sleep_for(200ms);

    std::wifstream file(logPath);
    std::wstring content((std::istreambuf_iterator<wchar_t>(file)), std::istreambuf_iterator<wchar_t>());
    REQUIRE(content.find(L"Error: 5") != std::wstring::npos);
    REQUIRE_FALSE(state.languageHotKeyEnabled.load());

    g_RegOpenKeyExResult = ERROR_SUCCESS;
    g_RegOpenKeyExFailOnSetValue = false;
    g_config.set(L"log_path", L"");
    fs::remove(logPath);
}

TEST_CASE("TemporarilyEnableHotKeys logs error and preserves state on RegSetValueEx failure") {
    using namespace std::chrono_literals;
    namespace fs = std::filesystem;

    auto& state = GetAppState();
    state.debugEnabled.store(true);
    state.tempHotKeysEnabled.store(false);
    state.languageHotKeyEnabled.store(false);
    state.layoutHotKeyEnabled.store(false);

    fs::path logPath = fs::temp_directory_path() / "temp_enable_fail.log";
    g_config.set(L"log_path", logPath.wstring());

    g_RegSetValueExResult = 5; // simulate failure
    TemporarilyEnableHotKeys(nullptr);

    std::this_thread::sleep_for(200ms);

    std::wifstream file(logPath);
    std::wstring content((std::istreambuf_iterator<wchar_t>(file)), std::istreambuf_iterator<wchar_t>());
    REQUIRE(content.find(L"Error: 5") != std::wstring::npos);
    REQUIRE_FALSE(state.tempHotKeysEnabled.load());
    REQUIRE_FALSE(state.languageHotKeyEnabled.load());
    REQUIRE_FALSE(state.layoutHotKeyEnabled.load());

    g_RegSetValueExResult = ERROR_SUCCESS;
    g_config.set(L"log_path", L"");
    fs::remove(logPath);
}

TEST_CASE("DisableLanguageHotKey sets flag to disabled") {
    g_RegSetValueExResult = ERROR_SUCCESS;
    ToggleLanguageHotKey(nullptr, true, true);
    auto& state = GetAppState();
    REQUIRE(state.languageHotKeyEnabled.load());
    ToggleLanguageHotKey(nullptr, true, false);
    REQUIRE_FALSE(state.languageHotKeyEnabled.load());
}

TEST_CASE("EnableLayoutHotKey sets flag to enabled") {
    g_RegSetValueExResult = ERROR_SUCCESS;
    ToggleLayoutHotKey(nullptr, true, false);
    auto& state = GetAppState();
    REQUIRE_FALSE(state.layoutHotKeyEnabled.load());
    ToggleLayoutHotKey(nullptr, true, true);
    REQUIRE(state.layoutHotKeyEnabled.load());
}

TEST_CASE("Command line flag disables Language hotkey") {
    g_RegSetValueExResult = ERROR_SUCCESS;
    auto& state = GetAppState();
    state.languageHotKeyEnabled.store(true);
    REQUIRE(HandleHotkeyFlag(L"--disable-language-hotkey"));
    REQUIRE_FALSE(state.languageHotKeyEnabled.load());
}

TEST_CASE("Command line flag enables Layout hotkey") {
    g_RegSetValueExResult = ERROR_SUCCESS;
    auto& state = GetAppState();
    state.layoutHotKeyEnabled.store(false);
    REQUIRE(HandleHotkeyFlag(L"--enable-layout-hotkey"));
    REQUIRE(state.layoutHotKeyEnabled.load());
}

