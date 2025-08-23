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

extern HINSTANCE g_hInst;
extern std::atomic<bool> g_debugEnabled;

TEST_CASE("Log switches files when path changes", "[log]") {
    g_debugEnabled.store(true);
    using namespace std::chrono_literals;
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "immon_log_test";
    fs::create_directories(dir);

    fs::path first = dir / "first.log";
    fs::path second = dir / "second.log";

    g_config.set(L"log_path", first.wstring());
    Log log;

    log.write(L"one");
    std::this_thread::sleep_for(200ms);

    g_config.set(L"log_path", second.wstring());
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
    g_debugEnabled.store(true);
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

TEST_CASE("Log keeps only configured number of backups", "[log]") {
    g_debugEnabled.store(true);
    using namespace std::chrono_literals;
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "immon_log_backup_limit";
    fs::create_directories(dir);

    fs::path logPath = dir / "limit.log";
    g_config.set(L"log_path", logPath.wstring());
    g_config.set(L"max_log_size_mb", L"1");
    g_config.set(L"max_log_backups", L"2");
    Log log;

    std::wstring big(1024 * 1024, L'a');
    for (int i = 0; i < 4; ++i) {
        log.write(big);
        std::this_thread::sleep_for(200ms);
        log.write(L"entry");
        std::this_thread::sleep_for(200ms);
    }

    log.shutdown();

    REQUIRE(fs::exists(logPath));
    REQUIRE(fs::exists(logPath.wstring() + L".1"));
    REQUIRE(fs::exists(logPath.wstring() + L".2"));
    REQUIRE(!fs::exists(logPath.wstring() + L".3"));

    fs::remove_all(dir);
}

TEST_CASE("Log reports error when rotation rename fails", "[log]") {
#ifndef _WIN32
    g_debugEnabled.store(true);
    using namespace std::chrono_literals;
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "immon_log_rename_fail";
    fs::remove_all(dir);
    fs::create_directories(dir);

    fs::path logPath = dir / "rotate.log";
    fs::path rotated = dir / "rotate.log.1";
    fs::create_directory(rotated);
    g_config.set(L"log_path", logPath.wstring());
    g_config.set(L"max_log_size_mb", L"1");
    Log log;

    std::wstring big(1024 * 1024, L'a');
    log.write(big);
    std::this_thread::sleep_for(200ms);

    log.write(L"after");
    std::this_thread::sleep_for(200ms);

    log.shutdown();

    REQUIRE(fs::is_directory(rotated));

    std::wifstream f(logPath);
    std::wstring content((std::istreambuf_iterator<wchar_t>(f)), std::istreambuf_iterator<wchar_t>());
    REQUIRE(content.find(L"Failed to rotate log file") != std::wstring::npos);
    REQUIRE(content.find(L"after") != std::wstring::npos);

    fs::remove_all(dir);
#endif
}

TEST_CASE("Log drops oldest messages when queue limit exceeded", "[log]") {
    g_debugEnabled.store(true);

    Log log(3, false); // small queue, do not start threads
    log.write(L"one");
    log.write(L"two");
    log.write(L"three");
    log.write(L"four");
    log.write(L"five");

    REQUIRE(log.queueSize() == 3);
    REQUIRE(log.peekOldest() == L"three");
}

#ifdef _WIN32
TEST_CASE("Pipe listener handles messages larger than buffer", "[log][pipe]") {
    g_debugEnabled.store(true);
    using namespace std::chrono_literals;
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "immon_pipe_log_test";
    fs::create_directories(dir);

    fs::path logPath = dir / "pipe.log";
    g_config.set(L"log_path", logPath.wstring());

    Log log; // starts pipe listener
    std::this_thread::sleep_for(50ms);

    // Create a message larger than the default pipe buffer (1024 wchar_t)
    std::wstring big(1500, L'x');

    const wchar_t* pipeName = L"\\.\\pipe\\kbdlayoutmon_log";
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    // Try to connect to the pipe until the listener is ready
    for (int i = 0; i < 50 && hPipe == INVALID_HANDLE_VALUE; ++i) {
        hPipe = CreateFileW(pipeName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe == INVALID_HANDLE_VALUE)
            std::this_thread::sleep_for(50ms);
    }
    REQUIRE(hPipe != INVALID_HANDLE_VALUE);

    DWORD written = 0;
    WriteFile(hPipe, big.c_str(), static_cast<DWORD>(big.size() * sizeof(wchar_t)), &written, NULL);
    CloseHandle(hPipe);

    std::this_thread::sleep_for(200ms);
    log.shutdown();

    std::wifstream f(logPath);
    std::wstring content((std::istreambuf_iterator<wchar_t>(f)), std::istreambuf_iterator<wchar_t>());
    REQUIRE(content.find(big) != std::wstring::npos);

    fs::remove_all(dir);
}
#endif
