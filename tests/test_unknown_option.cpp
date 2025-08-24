#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <atomic>
#include "../source/cli_utils.h"
#include "../source/configuration.h"
#include "../source/app_state.h"

extern bool g_cliMode;

TEST_CASE("WarnUnrecognizedOption logs error") {
    using namespace std::chrono_literals;
    namespace fs = std::filesystem;

    GetAppState().debugEnabled.store(true);
    g_cliMode = true;

    fs::path logPath = fs::temp_directory_path() / "unknown_option.log";
    g_config.set(L"log_path", logPath.wstring());

    WarnUnrecognizedOption(L"--bogus-flag");
    std::this_thread::sleep_for(200ms);

    std::wifstream file(logPath);
    std::wstring content((std::istreambuf_iterator<wchar_t>(file)),
                         std::istreambuf_iterator<wchar_t>());
    REQUIRE(content.find(L"Unrecognized option: --bogus-flag") != std::wstring::npos);

    g_config.set(L"log_path", L"");
    fs::remove(logPath);
}
