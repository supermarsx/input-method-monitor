#include <catch2/catch_test_macros.hpp>
#include "../source/log.h"
#include "../source/configuration.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>

#ifndef _WIN32
using HINSTANCE = void*;
#endif

HINSTANCE g_hInst = NULL;
std::atomic<bool> g_debugEnabled{true};

TEST_CASE("Log switches files when path changes", "[log]") {
    using namespace std::chrono_literals;
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "immon_log_test";
    fs::create_directories(dir);

    fs::path first = dir / "first.log";
    fs::path second = dir / "second.log";

    g_config.setSetting(L"log_path", first.wstring());
    Log log;

    log.write(L"one");
    std::this_thread::sleep_for(200ms);

    g_config.setSetting(L"log_path", second.wstring());
    log.write(L"two");
    std::this_thread::sleep_for(200ms);

    log.shutdown();

    REQUIRE(fs::exists(first));
    REQUIRE(fs::exists(second));

    std::wifstream f1(first);
    std::wstring line1; std::getline(f1, line1);
    REQUIRE(line1.find(L"one") != std::wstring::npos);

    std::wifstream f2(second);
    std::wstring line2; std::getline(f2, line2);
    REQUIRE(line2.find(L"two") != std::wstring::npos);

    fs::remove_all(dir);
}

TEST_CASE("Log rotates file when size limit is exceeded", "[log]") {
    using namespace std::chrono_literals;
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "immon_log_rotate_test";
    fs::create_directories(dir);

    fs::path logPath = dir / "rotate.log";
    g_config.set(L"log_path", logPath.wstring());
    g_config.set(L"max_log_size_mb", L"1");
    Log log;

    // Write a large entry to exceed the configured size (1 MB)
    std::wstring big(1024 * 1024, L'a');
    log.write(big);
    std::this_thread::sleep_for(200ms);

    // Next write should trigger rotation before it is written
    log.write(L"second entry");
    std::this_thread::sleep_for(200ms);

    log.shutdown();

    REQUIRE(fs::exists(logPath));
    REQUIRE(fs::exists(logPath.wstring() + L".1"));

    fs::remove_all(dir);
}
