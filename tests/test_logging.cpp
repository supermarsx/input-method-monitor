#include <catch2/catch_test_macros.hpp>
#include <string>

static bool g_debugEnabled = false;
static int openCount = 0;

// Stubs for Windows types
using HANDLE = void*;
using DWORD = unsigned long;
#define INVALID_HANDLE_VALUE ((HANDLE)(-1))

static HANDLE CreateFileW(const wchar_t*, DWORD, DWORD, void*, DWORD, DWORD, void*) {
    ++openCount;
    return (HANDLE)1; // dummy handle
}
static bool WriteFile(HANDLE, const void*, DWORD, DWORD*, void*) { return true; }
static bool CloseHandle(HANDLE) { return true; }

static void WriteLog(const std::wstring& message) {
    if (!g_debugEnabled)
        return;
    HANDLE pipe = CreateFileW(L"\\\\.\\pipe\\kbdlayoutmon_log", 0, 0, nullptr, 0, 0, nullptr);
    if (pipe != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten = 0;
        WriteFile(pipe, message.c_str(), (DWORD)((message.size() + 1) * sizeof(wchar_t)), &bytesWritten, nullptr);
        CloseHandle(pipe);
    }
}

TEST_CASE("WriteLog stops when debug disabled", "[logging]") {
    openCount = 0;
    g_debugEnabled = true;
    WriteLog(L"first");
    REQUIRE(openCount == 1);

    g_debugEnabled = false;
    WriteLog(L"second");
    REQUIRE(openCount == 1);
}
