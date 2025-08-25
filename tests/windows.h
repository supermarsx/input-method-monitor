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
using HMODULE = void*;
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
using FARPROC = void*;

using ATOM = unsigned short;
using HICON = HANDLE;
using HCURSOR = HANDLE;
using HBRUSH = HANDLE;
using LPSTR = char*;
using LPWSTR = wchar_t*;
using WCHAR = wchar_t;

typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);

struct WNDCLASS {
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCWSTR lpszMenuName;
    LPCWSTR lpszClassName;
};

struct MSG {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    LONG pt;
};

using ULONG_PTR = uintptr_t;

struct OVERLAPPED {
    ULONG_PTR Internal;
    ULONG_PTR InternalHigh;
    union {
        struct {
            DWORD Offset;
            DWORD OffsetHigh;
        };
        void* Pointer;
    };
    HANDLE hEvent;
};

struct FILE_NOTIFY_INFORMATION {
    DWORD NextEntryOffset;
    DWORD Action;
    DWORD FileNameLength;
    WCHAR FileName[1];
};

#define TRUE 1
#define FALSE 0
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define INFINITE 0xFFFFFFFF
#define MAX_PATH 260
#define FILE_NOTIFY_CHANGE_LAST_WRITE 0x00000010
#define FILE_NOTIFY_CHANGE_FILE_NAME 0x00000001
#define FILE_LIST_DIRECTORY 0x0001
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#define FILE_SHARE_DELETE 0x00000004
#define FILE_FLAG_BACKUP_SEMANTICS 0x02000000
#define FILE_FLAG_OVERLAPPED 0x40000000
#define FILE_ACTION_ADDED 0x00000001
#define FILE_ACTION_REMOVED 0x00000002
#define FILE_ACTION_MODIFIED 0x00000003
#define FILE_ACTION_RENAMED_OLD_NAME 0x00000004
#define FILE_ACTION_RENAMED_NEW_NAME 0x00000005
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
#define NIM_MODIFY 0x00000001
#define NIM_DELETE 0x00000002
#define IMAGE_ICON 1
#define LR_LOADFROMFILE 0x00000010
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

#define CW_USEDEFAULT 0

extern "C" {
    extern HANDLE (*pCreateEventW)(void*, BOOL, BOOL, LPCWSTR);
    extern BOOL (*pSetEvent)(HANDLE);
    extern HANDLE (*pCreateFileW)(LPCWSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
    extern BOOL (*pReadDirectoryChangesW)(HANDLE, void*, DWORD, BOOL, DWORD, DWORD*, OVERLAPPED*, void*);
    extern BOOL (*pCancelIoEx)(HANDLE, OVERLAPPED*);
    extern BOOL (*pGetOverlappedResult)(HANDLE, OVERLAPPED*, DWORD*, BOOL);
    extern DWORD (*pWaitForMultipleObjects)(DWORD, const HANDLE*, BOOL, DWORD);
    extern DWORD (*pGetModuleFileNameW)(HINSTANCE, wchar_t*, DWORD);
    extern HANDLE (*pLoadImageW)(HINSTANCE, LPCWSTR, UINT, int, int, UINT);

    ATOM RegisterClass(const WNDCLASS*);
    HWND CreateWindowEx(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HANDLE, HINSTANCE, LPVOID);
    LRESULT DefWindowProc(HWND, UINT, WPARAM, LPARAM);
    BOOL DestroyWindow(HWND);
    void PostQuitMessage(int);
    LPWSTR* CommandLineToArgvW(LPCWSTR, int*);
    void LocalFree(void*);
    BOOL GetMessage(MSG*, HWND, UINT, UINT);
    BOOL TranslateMessage(const MSG*);
    LRESULT DispatchMessage(const MSG*);
    HMODULE LoadLibrary(LPCWSTR);
    FARPROC GetProcAddress(HMODULE, const char*);
    BOOL FreeLibrary(HMODULE);
    int MessageBox(HWND, LPCWSTR, LPCWSTR, UINT);
    BOOL AttachConsole(DWORD);
    BOOL FreeConsole(void);
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
inline HANDLE CreateFileW(LPCWSTR a, DWORD b, DWORD c, void* d, DWORD e, DWORD f, HANDLE g) { return pCreateFileW(a,b,c,d,e,f,g); }
inline BOOL ReadDirectoryChangesW(HANDLE a, void* b, DWORD c, BOOL d, DWORD e, DWORD* f, OVERLAPPED* g, void* h) { return pReadDirectoryChangesW(a,b,c,d,e,f,g,h); }
inline BOOL CancelIoEx(HANDLE a, OVERLAPPED* b) { return pCancelIoEx(a,b); }
inline BOOL GetOverlappedResult(HANDLE a, OVERLAPPED* b, DWORD* c, BOOL d) { return pGetOverlappedResult(a,b,c,d); }
inline DWORD WaitForMultipleObjects(DWORD a, const HANDLE* b, BOOL c, DWORD d) { return pWaitForMultipleObjects(a,b,c,d); }
inline DWORD GetModuleFileNameW(HINSTANCE inst, wchar_t* buffer, DWORD size) { return pGetModuleFileNameW(inst, buffer, size); }
inline HANDLE LoadImageW(HINSTANCE a, LPCWSTR b, UINT c, int d, int e, UINT f) { return pLoadImageW(a,b,c,d,e,f); }
inline void CloseHandle(HANDLE) {}
inline BOOL WriteFile(HANDLE, const void*, DWORD, DWORD*, void*) { return TRUE; }
extern LONG g_RegOpenKeyExResult;
extern bool g_RegOpenKeyExFailOnSetValue;
inline LONG RegOpenKeyEx(HKEY, LPCWSTR, DWORD, DWORD samDesired, HKEY*) {
    if (g_RegOpenKeyExFailOnSetValue && (samDesired & KEY_SET_VALUE)) {
        return g_RegOpenKeyExResult;
    }
    return ERROR_SUCCESS;
}
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
extern DWORD (*pWaitForSingleObject)(HANDLE, DWORD);
inline DWORD WaitForSingleObject(HANDLE a, DWORD b) { return pWaitForSingleObject(a, b); }
inline BOOL ReleaseMutex(HANDLE) { return TRUE; }
inline void DisableThreadLibraryCalls(HINSTANCE) {}
extern UINT (*pSetTimer)(HWND, UINT, UINT, TIMERPROC);
extern BOOL (*pKillTimer)(HWND, UINT);
inline UINT SetTimer(HWND a, UINT b, UINT c, TIMERPROC d) { return pSetTimer(a, b, c, d); }
inline BOOL KillTimer(HWND a, UINT b) { return pKillTimer(a, b); }
inline int lstrlen(const wchar_t* s) { return wcslen(s); }
extern int g_sleepCalls;
inline void Sleep(DWORD) { ++g_sleepCalls; }
