#pragma once

#include <windows.h>

// RAII wrapper for Windows registry handles
class WinRegHandle {
public:
    WinRegHandle() noexcept : m_hKey(nullptr) {}
    explicit WinRegHandle(HKEY hKey) noexcept : m_hKey(hKey) {}
    ~WinRegHandle() { reset(); }

    WinRegHandle(const WinRegHandle&) = delete;
    WinRegHandle& operator=(const WinRegHandle&) = delete;

    WinRegHandle(WinRegHandle&& other) noexcept : m_hKey(other.m_hKey) {
        other.m_hKey = nullptr;
    }
    WinRegHandle& operator=(WinRegHandle&& other) noexcept {
        if (this != &other) {
            reset();
            m_hKey = other.m_hKey;
            other.m_hKey = nullptr;
        }
        return *this;
    }

    // Retrieve the underlying handle
    HKEY get() const noexcept { return m_hKey; }

    // Pointer to receive a handle from functions like RegOpenKeyEx
    HKEY* receive() noexcept {
        reset();
        return &m_hKey;
    }

    // Release ownership without closing
    HKEY release() noexcept {
        HKEY tmp = m_hKey;
        m_hKey = nullptr;
        return tmp;
    }

    void reset(HKEY hKey = nullptr) noexcept {
        if (m_hKey) {
            RegCloseKey(m_hKey);
        }
        m_hKey = hKey;
    }

    explicit operator bool() const noexcept { return m_hKey != nullptr; }
    operator HKEY() const noexcept { return m_hKey; }

private:
    HKEY m_hKey;
};
