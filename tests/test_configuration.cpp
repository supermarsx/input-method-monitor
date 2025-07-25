#include <catch2/catch_test_macros.hpp>
#include "config_parser.h"
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

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

TEST_CASE("Negative values fallback", "[config]") {
    std::vector<std::wstring> lines = {
        L"TEMP_HOTKEY_TIMEOUT=-5000",
        L"MAX_LOG_SIZE_MB=-2"
    };
    auto settings = parse_config_lines(lines);
    REQUIRE(settings[L"temp_hotkey_timeout"] == L"10000");
    REQUIRE(settings[L"max_log_size_mb"] == L"10");
}

TEST_CASE("Reload custom config file", "[config]") {
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "immon_test";
    fs::create_directories(dir);
    fs::path cfg = dir / "kbdlayoutmon.config";

    {
        std::wofstream out(cfg);
        out << L"DEBUG=0\n";
    }

    auto settings = parse_config_file(cfg.wstring());
    REQUIRE(settings[L"debug"] == L"0");

    {
        std::wofstream out(cfg);
        out << L"DEBUG=1\n";
    }

    auto updated = parse_config_file(cfg.wstring());
    REQUIRE(updated[L"debug"] == L"1");

    fs::remove(cfg);
    fs::remove(dir);
}
