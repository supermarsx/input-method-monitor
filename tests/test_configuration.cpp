#include <catch2/catch_test_macros.hpp>
#include "config_parser.h"
#include <string>
#include <vector>

TEST_CASE("Valid entries are parsed", "[config]") {
    std::vector<std::wstring> lines = {
        L"DEBUG=1",
        L"TRAY_ICON=0",
        L"Temp_Hotkey_TimeOut=5000",
        L"LOG_PATH=C:/tmp/log.txt",
        L"MAX_LOG_SIZE_MB=5"
    };
    auto settings = parse_config_lines(lines);
    REQUIRE(settings[L"debug"] == L"1");
    REQUIRE(settings[L"tray_icon"] == L"0");
    REQUIRE(settings[L"temp_hotkey_timeout"] == L"5000");
    REQUIRE(settings[L"log_path"] == L"C:/tmp/log.txt");
    REQUIRE(settings[L"max_log_size_mb"] == L"5");
}

TEST_CASE("Malformed lines are ignored", "[config]") {
    std::vector<std::wstring> lines = {
        L"JUSTKEY",
        L"   ",
        L"NOEQUALS HERE",
        L""
    };
    auto settings = parse_config_lines(lines);
    REQUIRE(settings.empty());
}

TEST_CASE("Whitespace handling", "[config]") {
    std::vector<std::wstring> lines = {
        L"  DEBUG = 1  ",
        L"\tTRAY_ICON\t=\t1\t",
        L"TEMP_HOTKEY_TIMEOUT = notanumber",
        L"  \tLOG_PATH\t =  path/space  ",
        L"MAX_LOG_SIZE_MB = invalid"
    };
    auto settings = parse_config_lines(lines);
    REQUIRE(settings[L"debug"] == L"1");
    REQUIRE(settings[L"tray_icon"] == L"1");
    REQUIRE(settings[L"temp_hotkey_timeout"] == L"10000");
    REQUIRE(settings[L"log_path"] == L"path/space");
    REQUIRE(settings[L"max_log_size_mb"] == L"10");
}
