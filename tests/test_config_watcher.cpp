#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <future>
#include <chrono>
#include <filesystem>
#include <cstring>
#include <atomic>
#include "../source/app_state.h"

#define _WIN32
#include "windows.h"

#define private public
#include "../source/config_watcher.h"
#undef private

// Delegated globals and stubs are provided in tests/stubs.cpp
extern HINSTANCE g_hInst;
extern int applyCalls;
extern int g_sleepCalls;
extern HANDLE (*pCreateEventW)(void*, BOOL, BOOL, LPCWSTR);
extern BOOL (*pSetEvent)(HANDLE);
extern HANDLE (*pCreateFileW)(LPCWSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
extern DWORD (*pWaitForSingleObject)(HANDLE, DWORD);
extern BOOL (*pReadDirectoryChangesW)(HANDLE, void* buffer, DWORD, BOOL, DWORD, DWORD* bytesReturned, OVERLAPPED*, void*);
extern BOOL (*pCancelIoEx)(HANDLE, OVERLAPPED*);
extern BOOL (*pGetOverlappedResult)(HANDLE, OVERLAPPED*, DWORD* bytes, BOOL);
extern DWORD (*pWaitForMultipleObjects)(DWORD, const HANDLE*, BOOL, DWORD);
extern DWORD (*pGetModuleFileNameW)(HINSTANCE, wchar_t* buffer, DWORD);
extern int waitCalls;
extern DWORD lastBytes;
static HANDLE g_stopHandle = nullptr;
static bool stopSignaled = false;

TEST_CASE("Renaming config file triggers reload", "[config_watcher]") {
    applyCalls = 0;
    g_sleepCalls = 0;
    {
        ConfigWatcher watcher(nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::filesystem::rename("kbdlayoutmon.config", "kbdlayoutmon.config.tmp");
        std::filesystem::rename("kbdlayoutmon.config.tmp", "kbdlayoutmon.config");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(applyCalls >= 1);
}

TEST_CASE("Watcher exits when stop event set and directory cannot be opened", "[config_watcher]") {
    auto origCreateFileW = pCreateFileW;
    pCreateFileW = [](LPCWSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE) -> HANDLE { return nullptr; };

    stopSignaled = false;
    waitCalls = 0;
    g_sleepCalls = 0;
    std::chrono::steady_clock::time_point start;
    {
        ConfigWatcher watcher(nullptr);
        g_stopHandle = watcher.m_stopEvent.get();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        SetEvent(g_stopHandle);
        start = std::chrono::steady_clock::now();
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    pCreateFileW = origCreateFileW;
    g_stopHandle = nullptr;
    stopSignaled = false;
    REQUIRE(elapsed < std::chrono::milliseconds(200));
    REQUIRE(g_sleepCalls >= 1);
}

TEST_CASE("Watcher thread stops promptly when signaled", "[config_watcher]") {
    stopSignaled = false;
    waitCalls = 0;

    auto origWaitForMultipleObjects = pWaitForMultipleObjects;
    pWaitForMultipleObjects = [](DWORD, const HANDLE*, BOOL, DWORD) -> DWORD {
        while (!stopSignaled) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return WAIT_OBJECT_0 + 1;
    };

    ConfigWatcher* watcher = new ConfigWatcher(nullptr);
    g_stopHandle = watcher->m_stopEvent.get();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    SetEvent(g_stopHandle);

    auto start = std::chrono::steady_clock::now();
    auto dtor = std::async(std::launch::async, [watcher]() { delete watcher; });
    bool finished = dtor.wait_for(std::chrono::milliseconds(200)) == std::future_status::ready;
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (finished) {
        dtor.get();
    }
    pWaitForMultipleObjects = origWaitForMultipleObjects;
    g_stopHandle = nullptr;
    stopSignaled = false;

    REQUIRE(finished);
    REQUIRE(elapsed < std::chrono::milliseconds(200));
}

