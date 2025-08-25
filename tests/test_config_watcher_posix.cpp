#include <catch2/catch_test_macros.hpp>
#ifndef _WIN32
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

#include "../source/config_watcher_posix.h"
#include "../source/configuration.h"
#include "../source/app_state.h"
#include "../source/log.h"

extern void (*g_testApplyConfig)(HWND);

namespace {
int applyCalls = 0;
void RecordApply(HWND) { ++applyCalls; }
}

TEST_CASE("ConfigWatcher reloads on file changes", "[config_watcher][posix]") {
    using namespace std::chrono_literals;
    namespace fs = std::filesystem;

    bool prevDebug = GetAppState().debugEnabled.load();
    auto prevLevel = g_logLevel.load();
    GetAppState().debugEnabled.store(true);
    g_logLevel.store(LogLevel::Info);

    fs::path dir = fs::temp_directory_path() / "immon_cfgwatch_posix";
    fs::create_directories(dir);
    fs::path cfg = dir / "kbdlayoutmon.config";
    fs::path logPath = dir / "watch.log";

    {
        std::wofstream f(cfg);
        f << L"log_path=" << logPath.wstring() << L"\n";
        f << L"key=value\n";
    }

    g_config.load(cfg.wstring());

    applyCalls = 0;
    g_testApplyConfig = RecordApply;

    {
        ConfigWatcher watcher(nullptr);
        std::this_thread::sleep_for(100ms);

        {
            std::wofstream f(cfg);
            f << L"log_path=" << logPath.wstring() << L"\n";
            f << L"key=updated\n";
        }
        std::this_thread::sleep_for(100ms);

        fs::path tmp = dir / "kbdlayoutmon.tmp";
        fs::rename(cfg, tmp);
        fs::rename(tmp, cfg);
        std::this_thread::sleep_for(200ms);

        // Trigger a final write but do not wait so the watcher thread sees the
        // event after destruction and can exit promptly.
        {
            std::wofstream f(cfg);
            f << L"log_path=" << logPath.wstring() << L"\n";
            f << L"key=final\n";
        }
    }

    g_testApplyConfig = nullptr;
    std::this_thread::sleep_for(200ms);

    REQUIRE(applyCalls >= 2);

    std::wifstream logFile(logPath);
    std::wstring line;
    size_t count = 0;
    while (std::getline(logFile, line)) {
        if (line.find(L"Configuration reloaded.") != std::wstring::npos)
            ++count;
    }
    REQUIRE(count >= 2);

    fs::remove_all(dir);
    g_config.set(L"log_path", L"");
    GetAppState().debugEnabled.store(prevDebug);
    g_logLevel.store(prevLevel);
}
#endif
