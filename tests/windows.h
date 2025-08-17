#pragma once
#include <cstdint>
#include <cwchar>
#include <cstddef>

using HANDLE = void*;
using HINSTANCE = void*;
using HWND = void*;
using DWORD = unsigned long;
using BOOL = int;
using LPCWSTR = const wchar_t*;
using SIZE_T = std::size_t;
using LPVOID = void*;
using LPTHREAD_START_ROUTINE = DWORD (*)(LPVOID);

#define WINAPI

#define TRUE 1
#define FALSE 0
#define WAIT_OBJECT_0 0
#define INFINITE 0xFFFFFFFF
#define MAX_PATH 260
#define FILE_NOTIFY_CHANGE_LAST_WRITE 0x00000010
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)(-1))

extern "C" {
    extern HANDLE (*pCreateEventW)(void*, BOOL, BOOL, LPCWSTR);
    extern HANDLE (*pCreateThread)(LPVOID, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, DWORD*);
    extern BOOL (*pSetEvent)(HANDLE);
    extern HANDLE (*pFindFirstChangeNotificationW)(LPCWSTR, BOOL, DWORD);
    extern BOOL (*pFindNextChangeNotification)(HANDLE);
    extern BOOL (*pFindCloseChangeNotification)(HANDLE);
    extern DWORD (*pWaitForMultipleObjects)(DWORD, const HANDLE*, BOOL, DWORD);
    extern DWORD (*pWaitForSingleObject)(HANDLE, DWORD);
    extern DWORD (*pGetModuleFileNameW)(HINSTANCE, wchar_t*, DWORD);
}

inline HANDLE CreateEventW(void* a, BOOL b, BOOL c, LPCWSTR d) { return pCreateEventW(a,b,c,d); }
inline HANDLE CreateThread(LPVOID a, SIZE_T b, LPTHREAD_START_ROUTINE c, LPVOID d, DWORD e, DWORD* f) { return pCreateThread(a,b,c,d,e,f); }
inline BOOL SetEvent(HANDLE h) { return pSetEvent(h); }
inline HANDLE FindFirstChangeNotificationW(LPCWSTR a, BOOL b, DWORD c) { return pFindFirstChangeNotificationW(a,b,c); }
inline BOOL FindNextChangeNotification(HANDLE h) { return pFindNextChangeNotification(h); }
inline BOOL FindCloseChangeNotification(HANDLE h) { return pFindCloseChangeNotification(h); }
inline DWORD WaitForMultipleObjects(DWORD a, const HANDLE* b, BOOL c, DWORD d) { return pWaitForMultipleObjects(a,b,c,d); }
inline DWORD WaitForSingleObject(HANDLE h, DWORD d) { return pWaitForSingleObject(h,d); }
inline DWORD GetModuleFileNameW(HINSTANCE inst, wchar_t* buffer, DWORD size) { return pGetModuleFileNameW(inst, buffer, size); }
inline void CloseHandle(HANDLE) {}
