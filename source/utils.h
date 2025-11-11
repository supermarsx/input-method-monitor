#pragma once
#include <string>
#include <windows.h>

// Quotes and escapes a path for use on the Windows command line.
std::wstring QuotePath(const std::wstring& path);

// RAII wrapper for Win32 timers. Starts a timer on construction and
// automatically kills it on destruction.
class TimerGuard {
public:
    TimerGuard(HWND hwnd, UINT id, UINT elapse, TIMERPROC proc = nullptr)
        : m_hwnd(hwnd), m_id(SetTimer(hwnd, id, elapse, proc)) {}

    ~TimerGuard() {
        if (m_id != 0)
            KillTimer(m_hwnd, m_id);
    }

    TimerGuard(const TimerGuard&) = delete;
    TimerGuard& operator=(const TimerGuard&) = delete;

    /// Returns true if the timer was successfully created.
    explicit operator bool() const noexcept { return m_id != 0; }

private:
    HWND m_hwnd{};
    UINT m_id{};
};

