#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <future>
#include <chrono>
#include <filesystem>
#include <cstring>
#include <atomic>

#define _WIN32
#include "windows.h"

#define private public
#include "../source/config_watcher.h"
#undef private

extern HINSTANCE g_hInst;
static int applyCalls = 0;
void ApplyConfig(HWND) { ++applyCalls; }
static HANDLE g_stopHandle = nullptr;
static bool stopSignaled = false;
int g_sleepCalls = 0;
std::atomic<bool> g_debugEnabled{false};
HANDLE (*pCreateEventW)(void*, BOOL, BOOL, LPCWSTR) = [](void*, BOOL, BOOL, LPCWSTR){
    static intptr_t next = 1;
    return reinterpret_cast<HANDLE>(next++);
};
BOOL (*pSetEvent)(HANDLE h) = [](HANDLE h){ if (h == g_stopHandle) stopSignaled = true; return TRUE; };
HANDLE (*pCreateFileW)(LPCWSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE) = [](LPCWSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE){ return reinterpret_cast<HANDLE>(1); };
DWORD (*pWaitForSingleObject)(HANDLE, DWORD) = [](HANDLE h, DWORD) -> DWORD {
    return (h == g_stopHandle && !stopSignaled) ? WAIT_TIMEOUT : WAIT_OBJECT_0;
};
static DWORD lastBytes = 0;
BOOL (*pReadDirectoryChangesW)(HANDLE, void* buffer, DWORD, BOOL, DWORD, DWORD* bytesReturned, OVERLAPPED*, void*) = [](HANDLE, void* buffer, DWORD, BOOL, DWORD, DWORD* bytesReturned, OVERLAPPED*, void*) {
    auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
    info->NextEntryOffset = 0;
    info->Action = FILE_ACTION_RENAMED_NEW_NAME;
    const wchar_t* name = L"kbdlayoutmon.config";
    info->FileNameLength = static_cast<DWORD>(wcslen(name) * sizeof(wchar_t));
    std::memcpy(info->FileName, name, info->FileNameLength);
    lastBytes = sizeof(FILE_NOTIFY_INFORMATION) + info->FileNameLength;
    if (bytesReturned) *bytesReturned = lastBytes;
    return TRUE;
};
BOOL (*pCancelIoEx)(HANDLE, OVERLAPPED*) = [](HANDLE, OVERLAPPED*) { return TRUE; };
BOOL (*pGetOverlappedResult)(HANDLE, OVERLAPPED*, DWORD* bytes, BOOL) = [](HANDLE, OVERLAPPED*, DWORD* bytes, BOOL){ if(bytes) *bytes = lastBytes; return TRUE; };
static int waitCalls = 0;
DWORD (*pWaitForMultipleObjects)(DWORD, const HANDLE*, BOOL, DWORD) = [](DWORD, const HANDLE*, BOOL, DWORD) -> DWORD {
    return waitCalls++ == 0 ? WAIT_OBJECT_0 : WAIT_OBJECT_0 + 1;
};
DWORD (*pGetModuleFileNameW)(HINSTANCE, wchar_t* buffer, DWORD) = [](HINSTANCE, wchar_t* buffer, DWORD) -> DWORD { buffer[0] = L'\0'; return 0; };

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

