#pragma once
#include <string>
#ifdef UNIT_TEST
#include "../tests/windows_stub.h"
#else
#include <windows.h>
#endif

// Quotes and escapes a path for use on the Windows command line.
std::wstring QuotePath(const std::wstring& path);

// RAII wrapper for Win32 timers. Starts a timer on construction and
// automatically kills it on destruction.
class TimerGuard {
public:
    TimerGuard(HWND hwnd, UINT id, UINT elapse, TIMERPROC proc = nullptr)
        : m_hwnd(hwnd),
#ifdef UNIT_TEST
          // Use test-controlled timer hooks to avoid touching real Win32 timers.
          m_id(pSetTimer(hwnd, id, elapse, proc))
#else
          m_id(SetTimer(hwnd, id, elapse, proc))
#endif
    {}

    ~TimerGuard() {
        if (m_id != 0)
#ifdef UNIT_TEST
            pKillTimer(m_hwnd, m_id);
#else
            KillTimer(m_hwnd, m_id);
#endif
    }

    TimerGuard(const TimerGuard&) = delete;
    TimerGuard& operator=(const TimerGuard&) = delete;

    /// Returns true if the timer was successfully created.
    explicit operator bool() const noexcept { return m_id != 0; }

private:
    HWND m_hwnd{};
    UINT m_id{};
};
