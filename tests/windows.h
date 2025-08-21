#pragma once
#include <cstdint>
#include <cwchar>
#include <cstring>

using HANDLE = void*;
using HINSTANCE = void*;
using HWND = void*;
using HKL = void*;
using HHOOK = void*;
using HKEY = void*;
using DWORD = unsigned long;
using BOOL = int;
using LPCWSTR = const wchar_t*;
using WPARAM = uintptr_t;
using LPARAM = intptr_t;
using LRESULT = intptr_t;
using LANGID = unsigned short;
using BYTE = unsigned char;
using LONG = long;
using UINT = unsigned int;
using HOOKPROC = LRESULT(*)(int, WPARAM, LPARAM);
using TIMERPROC = void(*)(HWND, UINT, UINT, DWORD);
using LPVOID = void*;
using LPBYTE = BYTE*;

#define TRUE 1
#define FALSE 0
#define WAIT_OBJECT_0 0
#define INFINITE 0xFFFFFFFF
#define MAX_PATH 260
#define FILE_NOTIFY_CHANGE_LAST_WRITE 0x00000010
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)(-1))
#define HSHELL_LANGUAGE 0x0001
#define WH_SHELL 10
#define KL_NAMELENGTH 9
#define ERROR_SUCCESS 0L
#define REG_SZ 1
#define KEY_SET_VALUE 0x0002
#define KEY_READ 0x20019
#define GENERIC_WRITE 0x40000000
#define OPEN_EXISTING 3
#define FILE_ATTRIBUTE_NORMAL 0x80
#define WM_USER 0x0400
#define NIF_MESSAGE 0x00000001
#define NIF_ICON 0x00000002
#define NIF_TIP 0x00000004
#define NIM_ADD 0x00000000
#define NIM_DELETE 0x00000002
#ifndef ARRAYSIZE
#define ARRAYSIZE(A) (sizeof(A) / sizeof((A)[0]))
#endif
#define HWND_MESSAGE ((HWND)-3)
#define HKEY_CURRENT_USER ((HKEY)1)
#define HKEY_USERS ((HKEY)2)
#define LOWORD(l) ((LANGID)((uintptr_t)(l) & 0xFFFF))
#define UNREFERENCED_PARAMETER(x) (void)(x)
#define DLL_PROCESS_ATTACH 1
#define DLL_PROCESS_DETACH 0
#ifndef __declspec
#define __declspec(x)
#endif
#ifndef CALLBACK
#define CALLBACK
#endif
#ifndef WINAPI
#define WINAPI
#endif
#ifndef APIENTRY
#define APIENTRY
#endif

extern "C" {
    extern HANDLE (*pCreateEventW)(void*, BOOL, BOOL, LPCWSTR);
    extern BOOL (*pSetEvent)(HANDLE);
    extern HANDLE (*pFindFirstChangeNotificationW)(LPCWSTR, BOOL, DWORD);
    extern BOOL (*pFindNextChangeNotification)(HANDLE);
    extern BOOL (*pFindCloseChangeNotification)(HANDLE);
    extern DWORD (*pWaitForMultipleObjects)(DWORD, const HANDLE*, BOOL, DWORD);
    extern DWORD (*pGetModuleFileNameW)(HINSTANCE, wchar_t*, DWORD);
}

struct NOTIFYICONDATA {
    DWORD cbSize;
    HWND hWnd;
    UINT uID;
    UINT uFlags;
    UINT uCallbackMessage;
    HANDLE hIcon;
    wchar_t szTip[64];
};

using PNOTIFYICONDATA = NOTIFYICONDATA*;

inline BOOL Shell_NotifyIcon(DWORD, NOTIFYICONDATA*) { return TRUE; }
#define ZeroMemory(ptr, size) std::memset(ptr, 0, size)
#ifndef MAKEINTRESOURCE
#define MAKEINTRESOURCE(i) ((LPCWSTR)(uintptr_t)((uint16_t)(i)))
#endif
inline HANDLE LoadIcon(HINSTANCE, LPCWSTR) { return nullptr; }
inline int wcscpy_s(wchar_t* dst, size_t, const wchar_t* src) { std::wcscpy(dst, src); return 0; }

inline HANDLE CreateEventW(void* a, BOOL b, BOOL c, LPCWSTR d) { return pCreateEventW(a,b,c,d); }
inline BOOL SetEvent(HANDLE h) { return pSetEvent(h); }
inline HANDLE FindFirstChangeNotificationW(LPCWSTR a, BOOL b, DWORD c) { return pFindFirstChangeNotificationW(a,b,c); }
inline BOOL FindNextChangeNotification(HANDLE h) { return pFindNextChangeNotification(h); }
inline BOOL FindCloseChangeNotification(HANDLE h) { return pFindCloseChangeNotification(h); }
inline DWORD WaitForMultipleObjects(DWORD a, const HANDLE* b, BOOL c, DWORD d) { return pWaitForMultipleObjects(a,b,c,d); }
inline DWORD GetModuleFileNameW(HINSTANCE inst, wchar_t* buffer, DWORD size) { return pGetModuleFileNameW(inst, buffer, size); }
inline void CloseHandle(HANDLE) {}
inline HANDLE CreateFileW(LPCWSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE) { return nullptr; }
inline BOOL WriteFile(HANDLE, const void*, DWORD, DWORD*, void*) { return TRUE; }
inline LONG RegOpenKeyEx(HKEY, LPCWSTR, DWORD, DWORD, HKEY*) { return ERROR_SUCCESS; }
extern LONG g_RegSetValueExResult;
inline LONG RegSetValueEx(HKEY, LPCWSTR, DWORD, DWORD, const BYTE*, DWORD) { return g_RegSetValueExResult; }
inline LONG RegSetValueExW(HKEY, LPCWSTR, DWORD, DWORD, const BYTE*, DWORD) { return g_RegSetValueExResult; }
inline LONG RegQueryValueEx(HKEY, LPCWSTR valueName, DWORD, DWORD*, BYTE* data, DWORD* len) {
    if (valueName && data && len && *len >= sizeof(wchar_t) * 2) {
        auto wdata = reinterpret_cast<wchar_t*>(data);
        if (wcscmp(valueName, L"Language HotKey") == 0) {
            wcscpy(wdata, L"1");
        } else if (wcscmp(valueName, L"Layout HotKey") == 0) {
            wcscpy(wdata, L"2");
        } else {
            wdata[0] = L'\0';
        }
    }
    return ERROR_SUCCESS;
}
inline LONG RegDeleteValue(HKEY, LPCWSTR) { return ERROR_SUCCESS; }
inline LONG RegCloseKey(HKEY) { return ERROR_SUCCESS; }
inline HKL GetKeyboardLayout(DWORD) { return nullptr; }
inline BOOL GetKeyboardLayoutName(wchar_t*) { return TRUE; }
inline HHOOK SetWindowsHookEx(int, HOOKPROC, HINSTANCE, DWORD) { return (HHOOK)1; }
inline LRESULT CallNextHookEx(HHOOK, int, WPARAM, LPARAM) { return 0; }
inline BOOL UnhookWindowsHookEx(HHOOK) { return TRUE; }
inline DWORD GetLastError() { return 0; }
inline HANDLE CreateMutex(void*, BOOL, LPCWSTR) { return nullptr; }
inline DWORD WaitForSingleObject(HANDLE, DWORD) { return WAIT_OBJECT_0; }
inline BOOL ReleaseMutex(HANDLE) { return TRUE; }
inline void DisableThreadLibraryCalls(HINSTANCE) {}
extern UINT (*pSetTimer)(HWND, UINT, UINT, TIMERPROC);
extern BOOL (*pKillTimer)(HWND, UINT);
inline UINT SetTimer(HWND a, UINT b, UINT c, TIMERPROC d) { return pSetTimer(a, b, c, d); }
inline BOOL KillTimer(HWND a, UINT b) { return pKillTimer(a, b); }
inline int lstrlen(const wchar_t* s) { return wcslen(s); }
