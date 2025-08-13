#pragma once

// RAII wrapper for Windows HANDLE objects.
// On Windows, closes the handle in the destructor.
// On other platforms, acts as a minimal placeholder.
#ifdef _WIN32
#  include <windows.h>
#endif

class HandleGuard {
public:
#ifdef _WIN32
    using native_handle_type = HANDLE;
#else
    using native_handle_type = void*;
#endif
    /// Construct without owning a handle.
    HandleGuard(native_handle_type h = nullptr) noexcept : m_handle(h) {}
    /// Close the owned handle on destruction.
    ~HandleGuard() { reset(); }

    HandleGuard(const HandleGuard&) = delete;
    HandleGuard& operator=(const HandleGuard&) = delete;

    /// Move constructor transfers ownership.
    HandleGuard(HandleGuard&& other) noexcept : m_handle(other.release()) {}
    /// Move assignment transfers ownership.
    HandleGuard& operator=(HandleGuard&& other) noexcept {
        if (this != &other) {
            reset();
            m_handle = other.release();
        }
        return *this;
    }

    /// Obtain the underlying handle.
    native_handle_type get() const noexcept { return m_handle; }

    /// Pointer suitable for functions that output a handle.
    native_handle_type* receive() noexcept {
        reset();
        return &m_handle;
    }

    /// Release ownership of the handle without closing it.
    native_handle_type release() noexcept {
        native_handle_type tmp = m_handle;
        m_handle = nullptr;
        return tmp;
    }

    /// Replace the current handle, closing the existing one.
    void reset(native_handle_type h = nullptr) noexcept {
#ifdef _WIN32
        if (m_handle && m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
        }
#endif
        m_handle = h;
    }

    /// True if a valid handle is held.
    explicit operator bool() const noexcept {
#ifdef _WIN32
        return m_handle && m_handle != INVALID_HANDLE_VALUE;
#else
        return m_handle != nullptr;
#endif
    }

#ifdef _WIN32
    /// Implicit conversion to the raw handle type.
    operator HANDLE() const noexcept { return m_handle; }
#endif

private:
    native_handle_type m_handle;
};

