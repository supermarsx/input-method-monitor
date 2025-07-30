#pragma once

#include <windows.h>

/**
 * @brief RAII wrapper for Windows registry handles.
 */
class WinRegHandle {
public:
    /// Construct an empty wrapper that owns no handle.
    WinRegHandle() noexcept : m_hKey(nullptr) {}
    /// Take ownership of an existing handle.
    explicit WinRegHandle(HKEY hKey) noexcept : m_hKey(hKey) {}
    /// Close the wrapped handle on destruction.
    ~WinRegHandle() { reset(); }

    WinRegHandle(const WinRegHandle&) = delete;
    WinRegHandle& operator=(const WinRegHandle&) = delete;

    /// Move constructor transfers ownership.
    WinRegHandle(WinRegHandle&& other) noexcept : m_hKey(other.m_hKey) {
        other.m_hKey = nullptr;
    }
    /// Move assignment transfers ownership of the handle.
    WinRegHandle& operator=(WinRegHandle&& other) noexcept {
        if (this != &other) {
            reset();
            m_hKey = other.m_hKey;
            other.m_hKey = nullptr;
        }
        return *this;
    }

    /// Retrieve the underlying handle without transferring ownership.
    HKEY get() const noexcept { return m_hKey; }

    /// Pointer to receive a handle from APIs like RegOpenKeyEx.
    HKEY* receive() noexcept {
        reset();
        return &m_hKey;
    }

    /// Release ownership without closing the handle.
    HKEY release() noexcept {
        HKEY tmp = m_hKey;
        m_hKey = nullptr;
        return tmp;
    }

    /// Close the current handle and optionally replace it.
    void reset(HKEY hKey = nullptr) noexcept {
        if (m_hKey) {
            RegCloseKey(m_hKey);
        }
        m_hKey = hKey;
    }

    /// True if a valid handle is held.
    explicit operator bool() const noexcept { return m_hKey != nullptr; }
    /// Implicit conversion to the raw handle type.
    operator HKEY() const noexcept { return m_hKey; }

private:
    HKEY m_hKey;
};
