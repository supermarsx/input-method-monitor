#include <string>
#include <atomic>
#include <cwchar>
#include <iostream>
#include "windows_stub.h"
#include "../source/app_state.h"
#include "../source/hotkey_registry.h"
#include "../source/log.h"




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
    if (path) {
        std::wcout << L"pCreateFileW called with path: " << path << std::endl;
        // Print first few codepoints for debugging
        std::wcout << L"path codes: ";
        for (int i = 0; i < 12 && path[i] != L'\0'; ++i) {
            std::wcout << std::hex << (int)path[i] << L" ";
        }
        std::wcout << std::dec << std::endl;
        // Robust prefix checks for named pipe forms used by tests
        const wchar_t* prefix1 = L"\\\\.\\pipe\\"; // "\\.\pipe\"
        const wchar_t* prefix2 = L"\\\\?\\pipe\\"; // "\\?\pipe\"
        const wchar_t* prefixSingle = L"\\.\\pipe\\"; // "\.\pipe\"
        size_t p1len = wcslen(prefix1);
        size_t p2len = wcslen(prefix2);
        size_t pslen = wcslen(prefixSingle);
        if ((wcslen(path) >= p1len && wcsncmp(path, prefix1, p1len) == 0) ||
            (wcslen(path) >= p2len && wcsncmp(path, prefix2, p2len) == 0) ||
            (wcslen(path) >= pslen && wcsncmp(path, prefixSingle, pslen) == 0)) {
            std::wcout << L"pCreateFileW: matched pipe prefix, returning simulated handle" << std::endl;
            return reinterpret_cast<HANDLE>(3); // simulated pipe client handle
        }
        if (wcsstr(path, L"kbdlayoutmon_log") != nullptr) {
            // Allow any pipe path variant to keep pipe tests deterministic.
            std::wcout << L"pCreateFileW: matched log pipe name, returning simulated handle" << std::endl;
            return reinterpret_cast<HANDLE>(3);
        }
        // Fallback: if the path contains "\\pipe\\" anywhere, treat it as a pipe
        if (wcsstr(path, L"\\\\pipe\\") != nullptr) {
            std::wcout << L"pCreateFileW: matched pipe substring, returning simulated handle" << std::endl;
            return reinterpret_cast<HANDLE>(3);
        }
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
HANDLE (*pCreateNamedPipeW)(LPCWSTR, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, void*) = [](LPCWSTR name, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, void*) -> HANDLE {
    if (name) std::wcout << L"pCreateNamedPipeW called with name: " << name << std::endl;
    std::wcout << L"pCreateNamedPipeW: returning simulated server handle" << std::endl;
    return reinterpret_cast<HANDLE>(2);
};
BOOL (*pConnectNamedPipe)(HANDLE h, void*) = [](HANDLE h, void*) -> BOOL {
    std::wcout << L"pConnectNamedPipe called for handle=" << h << std::endl;
    return TRUE;
};
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
        std::wcout << L"pWriteFile: received message length=" << msg.size() << L" first10='" << msg.substr(0,10) << L"'" << std::endl;
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
// Minimal defaults for other test-only globals
int applyCalls = 0;
std::atomic<bool> g_stopRequested{false};
// Additional state used by config_watcher tests
int waitCalls = 0;
DWORD lastBytes = 0;
