#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <chrono>
#include <stdexcept>

#define private public
#include "../source/config_watcher.h"
#undef private

extern HINSTANCE g_hInst;
void ApplyConfig(HWND) {}

HANDLE (*pCreateEventW)(void*, BOOL, BOOL, LPCWSTR) = [](void*, BOOL, BOOL, LPCWSTR){ return reinterpret_cast<HANDLE>(1); };
HANDLE (*pCreateThread)(LPVOID, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, DWORD*) =
    [](LPVOID, SIZE_T, LPTHREAD_START_ROUTINE start, LPVOID param, DWORD, DWORD*) -> HANDLE {
        start(param);
        return reinterpret_cast<HANDLE>(1);
    };
BOOL (*pSetEvent)(HANDLE) = [](HANDLE){ return TRUE; };
HANDLE (*pFindFirstChangeNotificationW)(LPCWSTR, BOOL, DWORD) = [](LPCWSTR, BOOL, DWORD){ return reinterpret_cast<HANDLE>(1); };
static int nextCalls = 0;
BOOL (*pFindNextChangeNotification)(HANDLE) = [](HANDLE){ ++nextCalls; return FALSE; };
static int closeCalls = 0;
BOOL (*pFindCloseChangeNotification)(HANDLE) = [](HANDLE){ ++closeCalls; return TRUE; };
DWORD (*pWaitForMultipleObjects)(DWORD, const HANDLE*, BOOL, DWORD) = [](DWORD, const HANDLE*, BOOL, DWORD) -> DWORD { return WAIT_OBJECT_0; };
DWORD (*pWaitForSingleObject)(HANDLE, DWORD) = [](HANDLE, DWORD) -> DWORD { return 0; };
DWORD (*pGetModuleFileNameW)(HINSTANCE, wchar_t* buffer, DWORD) = [](HINSTANCE, wchar_t* buffer, DWORD) -> DWORD { buffer[0] = L'\0'; return 0; };

TEST_CASE("Watcher exits when FindNextChangeNotification fails", "[config_watcher]") {
    nextCalls = 0;
    closeCalls = 0;
    {
        ConfigWatcher watcher(nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(nextCalls == 1);
    REQUIRE(closeCalls == 1);
}

TEST_CASE("Watcher fails gracefully when CreateEventW fails", "[config_watcher]") {
    auto origEvent = pCreateEventW;
    pCreateEventW = [](void*, BOOL, BOOL, LPCWSTR){ return static_cast<HANDLE>(nullptr); };
    REQUIRE_THROWS_AS(ConfigWatcher(nullptr), std::runtime_error);
    pCreateEventW = origEvent;
}

TEST_CASE("Watcher fails gracefully when CreateThread fails", "[config_watcher]") {
    auto origThread = pCreateThread;
    pCreateThread = [](LPVOID, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, DWORD*) -> HANDLE {
        return static_cast<HANDLE>(nullptr);
    };
    REQUIRE_THROWS_AS(ConfigWatcher(nullptr), std::runtime_error);
    pCreateThread = origThread;
}

