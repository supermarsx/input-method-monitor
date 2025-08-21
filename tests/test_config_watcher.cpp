#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <chrono>
#include <filesystem>
#include <cstring>

#define private public
#include "../source/config_watcher.h"
#undef private

extern HINSTANCE g_hInst;
static int applyCalls = 0;
void ApplyConfig(HWND) { ++applyCalls; }

HANDLE (*pCreateEventW)(void*, BOOL, BOOL, LPCWSTR) = [](void*, BOOL, BOOL, LPCWSTR){ return reinterpret_cast<HANDLE>(1); };
BOOL (*pSetEvent)(HANDLE) = [](HANDLE){ return TRUE; };
HANDLE (*pCreateFileW)(LPCWSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE) = [](LPCWSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE){ return reinterpret_cast<HANDLE>(1); };
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
    {
        ConfigWatcher watcher(nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::filesystem::rename("kbdlayoutmon.config", "kbdlayoutmon.config.tmp");
        std::filesystem::rename("kbdlayoutmon.config.tmp", "kbdlayoutmon.config");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(applyCalls >= 1);
}

