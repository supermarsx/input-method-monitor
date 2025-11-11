#include <string>
#include <atomic>
#include <cwchar>
#include "windows_stub.h"
#include "../source/app_state.h"
#include "../source/hotkey_registry.h"


// CLI helpers used by some tests
bool g_cliMode = false;
std::wstring g_cliIconPath;
std::wstring g_cliTrayTooltip;

// Registry test controls
LONG g_RegOpenKeyExResult = ERROR_SUCCESS;
bool g_RegOpenKeyExFailOnSetValue = false;
LONG g_RegSetValueExResult = ERROR_SUCCESS;

// Sleep tracking for tests that simulate waits
int g_sleepCalls = 0;

// Global instance handle used by tray/icon/log code
HINSTANCE g_hInst = nullptr;

// Provide simple implementations for startup helpers used by ApplyConfig
bool IsStartupEnabled() { return GetAppState().startupEnabled.load(); }
void AddToStartup() { GetAppState().startupEnabled.store(true); }
void RemoveFromStartup() { GetAppState().startupEnabled.store(false); }


// Worker thread control (may be overridden by hook implementation)
std::atomic<bool> g_workerRunning{false};

// Default Windows API function pointer stubs used by tests. Tests may override these.
HANDLE (*pCreateEventW)(void*, BOOL, BOOL, LPCWSTR) = [](void*, BOOL, BOOL, LPCWSTR){ return reinterpret_cast<HANDLE>(1); };
BOOL (*pSetEvent)(HANDLE) = [](HANDLE){ return TRUE; };
HANDLE (*pCreateFileW)(LPCWSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE) = [](LPCWSTR path, DWORD access, DWORD share, void* sec, DWORD disp, DWORD flags, HANDLE templ) -> HANDLE {
    // If attempting to open a named pipe, simulate a client handle
    if (path && wcsncmp(path, L"\\\\.\\pipe\\", 9) == 0) {
        return reinterpret_cast<HANDLE>(3); // simulated pipe client handle
    }
    // Otherwise forward to the real kernel32 CreateFileW via GetProcAddress to avoid recursion
    HMODULE h = GetModuleHandleW(L"kernel32.dll");
    if (!h) return INVALID_HANDLE_VALUE;
    using Fn = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPVOID, DWORD, DWORD, HANDLE);
    auto real = (Fn)GetProcAddress(h, "CreateFileW");
    if (!real) return INVALID_HANDLE_VALUE;
    return real(path, access, share, nullptr, disp, flags, templ);
};

// Named pipe stubs (simulate in-process pipe handles)
HANDLE (*pCreateNamedPipeW)(LPCWSTR, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, void*) = [](LPCWSTR, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, void*) -> HANDLE { return reinterpret_cast<HANDLE>(2); };
BOOL (*pConnectNamedPipe)(HANDLE, void*) = [](HANDLE, void*) -> BOOL { return TRUE; };
BOOL (*pDisconnectNamedPipe)(HANDLE) = [](HANDLE) -> BOOL { return TRUE; };

BOOL (*pReadFile)(HANDLE, void*, DWORD, DWORD*, void*) = [](HANDLE h, void* buf, DWORD len, DWORD* read, void*) -> BOOL {
    // For regular files, forward to real ReadFile
    if (h != reinterpret_cast<HANDLE>(3)) {
        return ::ReadFile(h, buf, len, read, nullptr);
    }
    // Simulated pipe client read: no-op
    if (read) *read = 0;
    return TRUE;
};

BOOL (*pWriteFile)(HANDLE, const void*, DWORD, DWORD*, void*) = [](HANDLE h, const void* buf, DWORD len, DWORD* written, void*) -> BOOL {
    // If writing to simulated pipe client handle, forward contents to the log
    if (h == reinterpret_cast<HANDLE>(3)) {
        // assume wchar_t payload
        size_t wchars = len / sizeof(wchar_t);
        const wchar_t* wbuf = reinterpret_cast<const wchar_t*>(buf);
        std::wstring msg(wbuf, wchars);
        // Trim possible trailing nulls
        while (!msg.empty() && msg.back() == L'\0') msg.pop_back();
        WriteLog(LogLevel::Info, msg.c_str());
        if (written) *written = len;
        return TRUE;
    }
    // Otherwise forward to real WriteFile
    return ::WriteFile(h, buf, len, written, nullptr);
};
BOOL (*pReadDirectoryChangesW)(HANDLE, void*, DWORD, BOOL, DWORD, DWORD*, OVERLAPPED*, void*) = [](HANDLE, void*, DWORD, BOOL, DWORD, DWORD*, OVERLAPPED*, void*){ return TRUE; };
BOOL (*pCancelIoEx)(HANDLE, OVERLAPPED*) = [](HANDLE, OVERLAPPED*){ return TRUE; };
BOOL (*pGetOverlappedResult)(HANDLE, OVERLAPPED*, DWORD*, BOOL) = [](HANDLE, OVERLAPPED*, DWORD*, BOOL){ return TRUE; };
DWORD (*pWaitForMultipleObjects)(DWORD, const HANDLE*, BOOL, DWORD) = [](DWORD, const HANDLE*, BOOL, DWORD) -> DWORD { return WAIT_OBJECT_0; };
DWORD (*pWaitForSingleObject)(HANDLE, DWORD) = [](HANDLE, DWORD) -> DWORD { return WAIT_OBJECT_0; };
DWORD (*pGetModuleFileNameW)(HINSTANCE, wchar_t*, DWORD) = [](HINSTANCE, wchar_t* buffer, DWORD) -> DWORD { if(buffer) buffer[0]=L'\0'; return 0; };
HANDLE (*pLoadImageW)(HINSTANCE, LPCWSTR, UINT, int, int, UINT) = [](HINSTANCE, LPCWSTR, UINT, int, int, UINT){ return reinterpret_cast<HANDLE>(1); };
UINT (*pSetTimer)(HWND, UINT, UINT, TIMERPROC) = [](HWND, UINT, UINT, TIMERPROC) -> UINT { return 1; };
BOOL (*pKillTimer)(HWND, UINT) = [](HWND, UINT) -> BOOL { return TRUE; };
BOOL (WINAPI *pShell_NotifyIcon)(DWORD, PNOTIFYICONDATA) = [](DWORD, PNOTIFYICONDATA){ return TRUE; };
// Minimal defaults for other test-only globals
int applyCalls = 0;
std::atomic<bool> g_stopRequested{false};
// Additional state used by config_watcher tests
int waitCalls = 0;
DWORD lastBytes = 0;
