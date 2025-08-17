#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <chrono>

#define private public
#include "../source/config_watcher.h"
#undef private

extern HINSTANCE g_hInst;
void ApplyConfig(HWND) {}

HANDLE (*pCreateEventW)(void*, BOOL, BOOL, LPCWSTR) = [](void*, BOOL, BOOL, LPCWSTR){ return reinterpret_cast<HANDLE>(1); };
BOOL (*pSetEvent)(HANDLE) = [](HANDLE){ return TRUE; };
HANDLE (*pFindFirstChangeNotificationW)(LPCWSTR, BOOL, DWORD) = [](LPCWSTR, BOOL, DWORD){ return reinterpret_cast<HANDLE>(1); };
static int nextCalls = 0;
BOOL (*pFindNextChangeNotification)(HANDLE) = [](HANDLE){ ++nextCalls; return FALSE; };
static int closeCalls = 0;
BOOL (*pFindCloseChangeNotification)(HANDLE) = [](HANDLE){ ++closeCalls; return TRUE; };
DWORD (*pWaitForMultipleObjects)(DWORD, const HANDLE*, BOOL, DWORD) = [](DWORD, const HANDLE*, BOOL, DWORD) -> DWORD { return WAIT_OBJECT_0; };
DWORD (*pGetModuleFileNameW)(HINSTANCE, wchar_t* buffer, DWORD) = [](HINSTANCE, wchar_t* buffer, DWORD) -> DWORD { buffer[0] = L'\0'; return 0; };

TEST_CASE("Watcher exits when FindNextChangeNotification fails", "[config_watcher]") {
    {
        ConfigWatcher watcher(nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(nextCalls == 1);
    REQUIRE(closeCalls == 1);
}

