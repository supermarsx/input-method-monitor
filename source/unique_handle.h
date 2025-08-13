#pragma once

#include <windows.h>

/**
 * @brief RAII wrapper for Windows HANDLE objects.
 */
class UniqueHandle {
public:
    /// Construct an empty wrapper that owns no handle.
    UniqueHandle() noexcept : m_handle(nullptr) {}
    /// Take ownership of an existing handle.
    explicit UniqueHandle(HANDLE h) noexcept : m_handle(h) {}
    /// Close the wrapped handle on destruction.
    ~UniqueHandle() { reset(); }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    /// Move constructor transfers ownership.
    UniqueHandle(UniqueHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }
    /// Move assignment transfers ownership of the handle.
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            reset();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    /// Retrieve the underlying handle without transferring ownership.
    HANDLE get() const noexcept { return m_handle; }

    /// Pointer to receive a handle from APIs like CreateEvent.
    HANDLE* receive() noexcept {
        reset();
        return &m_handle;
    }

    /// Release ownership without closing the handle.
    HANDLE release() noexcept {
        HANDLE tmp = m_handle;
        m_handle = nullptr;
        return tmp;
    }

    /// Close the current handle and optionally replace it.
    void reset(HANDLE h = nullptr) noexcept {
        if (m_handle && m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
        }
        m_handle = h;
    }

    /// True if a valid handle is held.
    explicit operator bool() const noexcept {
        return m_handle && m_handle != INVALID_HANDLE_VALUE;
    }

    /// Implicit conversion to the raw handle type.
    operator HANDLE() const noexcept { return m_handle; }

private:
    HANDLE m_handle;
};

