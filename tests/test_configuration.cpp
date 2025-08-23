#include <catch2/catch_test_macros.hpp>
#include "../source/config_parser.h"
#include "../source/configuration.h"
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <thread>
#include <optional>
#include <limits>
#include <cstdlib>


TEST_CASE("Valid entries are parsed", "[config]") {
    std::vector<std::wstring> lines = {
        L"DEBUG=1",
        L"TRAY_ICON=0",
        L"Temp_Hotkey_TimeOut=5000",
        L"LOG_PATH=C:/tmp/log.txt",
        L"MAX_LOG_SIZE_MB=5",
        L"MAX_QUEUE_SIZE=2000",
        L"ICON_PATH=C:/icons/app.ico",
        L"TRAY_TOOLTIP=Hello"
    };
    auto settings = ParseConfigLines(lines);
    REQUIRE(settings[L"debug"] == L"1");
    REQUIRE(settings[L"tray_icon"] == L"0");
    REQUIRE(settings[L"temp_hotkey_timeout"] == L"5000");
    REQUIRE(settings[L"log_path"] == L"C:/tmp/log.txt");
    REQUIRE(settings[L"max_log_size_mb"] == L"5");
    REQUIRE(settings[L"max_queue_size"] == L"2000");
    REQUIRE(settings[L"icon_path"] == L"C:/icons/app.ico");
    REQUIRE(settings[L"tray_tooltip"] == L"Hello");
}

TEST_CASE("Malformed lines are ignored", "[config]") {
    std::vector<std::wstring> lines = {
        L"JUSTKEY",
        L"   ",
        L"NOEQUALS HERE",
        L""
    };
    auto settings = ParseConfigLines(lines);
    REQUIRE(settings.empty());
}

TEST_CASE("Whitespace handling", "[config]") {
    std::vector<std::wstring> lines = {
        L"  DEBUG = 1  ",
        L"\tTRAY_ICON\t=\t1\t",
        L"TEMP_HOTKEY_TIMEOUT = notanumber",
        L"  \tLOG_PATH\t =  path/space  ",
        L"MAX_LOG_SIZE_MB = invalid",
        L"MAX_QUEUE_SIZE = nope"
    };
    auto settings = ParseConfigLines(lines);
    REQUIRE(settings[L"debug"] == L"1");
    REQUIRE(settings[L"tray_icon"] == L"1");
    REQUIRE(settings[L"temp_hotkey_timeout"] == L"10000");
    REQUIRE(settings[L"log_path"] == L"path/space");
    REQUIRE(settings[L"max_log_size_mb"] == L"10");
    REQUIRE(settings[L"max_queue_size"] == L"1000");
}

TEST_CASE("Negative values fallback", "[config]") {
    std::vector<std::wstring> lines = {
        L"TEMP_HOTKEY_TIMEOUT=-5000",
        L"MAX_LOG_SIZE_MB=-2",
        L"MAX_QUEUE_SIZE=-1"
    };
    auto settings = ParseConfigLines(lines);
    REQUIRE(settings[L"temp_hotkey_timeout"] == L"10000");
    REQUIRE(settings[L"max_log_size_mb"] == L"10");
    REQUIRE(settings[L"max_queue_size"] == L"1000");
}

TEST_CASE("Overflow values fallback", "[config]") {
    std::wstring big = std::to_wstring(std::numeric_limits<unsigned long>::max());
    big.push_back(L'0');
    std::vector<std::wstring> lines = {
        L"TEMP_HOTKEY_TIMEOUT=" + big,
        L"MAX_LOG_SIZE_MB=" + big,
        L"MAX_QUEUE_SIZE=" + big
    };
    auto settings = ParseConfigLines(lines);
    REQUIRE(settings[L"temp_hotkey_timeout"] == L"10000");
    REQUIRE(settings[L"max_log_size_mb"] == L"10");
    REQUIRE(settings[L"max_queue_size"] == L"1000");
}

TEST_CASE("Comment lines are ignored", "[config]") {
    std::vector<std::wstring> lines = {
        L"# comment line",
        L"; another",
        L"DEBUG=1",
        L"  #indented",
        L"TRAY_ICON=1"
    };
    auto settings = ParseConfigLines(lines);
    REQUIRE(settings[L"debug"] == L"1");
    REQUIRE(settings[L"tray_icon"] == L"1");
    REQUIRE(settings.size() == 2);
}

TEST_CASE("Quoted values are preserved", "[config]") {
    std::vector<std::wstring> lines = {
        L"URL=\"https://example.com/path?x=1#frag\"",
        L"NAME='John Doe'"
    };
    auto settings = ParseConfigLines(lines);
    REQUIRE(settings[L"url"] == L"https://example.com/path?x=1#frag");
    REQUIRE(settings[L"name"] == L"John Doe");
}

TEST_CASE("Inline comments are stripped", "[config]") {
    std::vector<std::wstring> lines = {
        L"KEY=value # comment",
        L"FOO=\"bar #baz\" # another",
        L"BAZ='qux ;quux' ; comment"
    };
    auto settings = ParseConfigLines(lines);
    REQUIRE(settings[L"key"] == L"value");
    REQUIRE(settings[L"foo"] == L"bar #baz");
    REQUIRE(settings[L"baz"] == L"qux ;quux");
}

TEST_CASE("Environment variables are expanded", "[config]") {
#ifdef _WIN32
    _wputenv_s(L"IMMON_TEST_VAR", L"expanded");
    std::vector<std::wstring> lines = {L"PATH_VAR=%IMMON_TEST_VAR%"};
    auto settings = ParseConfigLines(lines);
    REQUIRE(settings[L"path_var"] == L"expanded");
#else
    setenv("IMMON_TEST_VAR", "expanded", 1);
    std::vector<std::wstring> lines = {L"PATH_VAR=$IMMON_TEST_VAR"};
    auto settings = ParseConfigLines(lines);
    REQUIRE(settings[L"path_var"] == L"expanded");
#endif
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

    {
        std::wifstream in(cfg);
        auto settings = ParseConfigStream(in);
        REQUIRE(settings[L"debug"] == L"0");
    }

    {
        std::wofstream out(cfg);
        out << L"DEBUG=1\n";
    }

    {
        std::wifstream in(cfg);
        auto updated = ParseConfigStream(in);
        REQUIRE(updated[L"debug"] == L"1");
    }

    fs::remove(cfg);
    fs::remove(dir);
}

TEST_CASE("Configuration loads default path and handles missing files", "[configuration]") {
    namespace fs = std::filesystem;
    Configuration cfg;

    fs::path dir = fs::temp_directory_path() / "immon_load";
    fs::create_directories(dir);
    auto old = fs::current_path();
    fs::current_path(dir);

    SECTION("load uses default path when file exists") {
        fs::path cfgPath = dir / fs::path{std::wstring{L"kbdlayoutmon.config"}};
        {
            std::wofstream out(cfgPath);
            out << L"DEBUG=1\n";
        }
        cfg.load();
        REQUIRE(cfg.getLastPath() == cfgPath.wstring());
        REQUIRE(cfg.get(L"debug") == std::optional<std::wstring>(L"1"));
    }

    SECTION("nonexistent file does not modify state") {
        cfg.load();
        REQUIRE(cfg.getLastPath().empty());
        REQUIRE(cfg.snapshot().empty());
        cfg.load((dir / L"missing.config").wstring());
        REQUIRE(cfg.getLastPath().empty());
        REQUIRE(cfg.snapshot().empty());
    }

    fs::current_path(old);
    fs::remove_all(dir);
}

TEST_CASE("set/get operations are thread safe", "[configuration]") {
    Configuration cfg;
    const int iterations = 1000;
    std::thread writer([&]() {
        for (int i = 0; i < iterations; ++i)
            cfg.set(L"counter", std::to_wstring(i));
    });
    std::thread reader([&]() {
        for (int i = 0; i < iterations; ++i)
            cfg.get(L"counter");
    });
    writer.join();
    reader.join();
    REQUIRE(cfg.get(L"counter") == std::optional<std::wstring>(std::to_wstring(iterations - 1)));
}

TEST_CASE("snapshot returns a consistent copy", "[configuration]") {
    Configuration cfg;
    cfg.set(L"a", L"1");
    auto snap = cfg.snapshot();
    cfg.set(L"a", L"2");
    REQUIRE(snap[L"a"] == L"1");
    auto snap2 = cfg.snapshot();
    REQUIRE(snap2[L"a"] == L"2");
}
