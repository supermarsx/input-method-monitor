#pragma once
#include <string>
#include <windows.h>

inline std::wstring QuotePath(const std::wstring& path) {
    // Windows command line parsing requires that any backslashes preceding a
    // quote be doubled and the quote itself escaped. Additionally, any
    // trailing backslashes must be doubled before the closing quote. This
    // function performs those transformations and then surrounds the path in
    // quotes.

    std::wstring result;
    result.reserve(path.size() * 2 + 2);
    result.push_back(L'"');

    std::size_t backslashCount = 0;
    for (wchar_t ch : path) {
        if (ch == L'\\') {
            ++backslashCount;
        } else if (ch == L'"') {
            // Double the backslashes and escape the quote
            result.append(backslashCount * 2, L'\\');
            result.push_back(L'\\');
            result.push_back(L'"');
            backslashCount = 0;
        } else {
            // Emit any accumulated backslashes and the current character
            result.append(backslashCount, L'\\');
            result.push_back(ch);
            backslashCount = 0;
        }
    }

    // Double any trailing backslashes before appending the closing quote
    result.append(backslashCount * 2, L'\\');
    result.push_back(L'"');

    return result;
}

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
