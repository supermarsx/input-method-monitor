#include <string>
#include <atomic>
#include <cwchar>
#include "windows.h"
#include "../source/app_state.h"

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

// Worker thread control (may be overridden by hook implementation)
std::atomic<bool> g_workerRunning{false};

// Default Windows API function pointer stubs used by tests. Tests may override these.
HANDLE (*pCreateEventW)(void*, BOOL, BOOL, LPCWSTR) = [](void*, BOOL, BOOL, LPCWSTR){ return reinterpret_cast<HANDLE>(1); };
BOOL (*pSetEvent)(HANDLE) = [](HANDLE){ return TRUE; };
HANDLE (*pCreateFileW)(LPCWSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE) = [](LPCWSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE){ return reinterpret_cast<HANDLE>(1); };
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
