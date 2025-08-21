#include "log.h"
#ifdef _WIN32
#  include <windows.h>
#  include <shlwapi.h>
#  include "handle_guard.h"
#else
#  include <filesystem>
#  include <chrono>
#  include <ctime>
#  include <cwchar>
#  define MAX_PATH 260
#endif
#ifndef _WIN32
using HINSTANCE = void*;
inline void lstrcpyW(wchar_t* dst, const wchar_t* src) { std::wcscpy(dst, src); }
#endif
#include <fstream>
#include <iostream>
#include <utility>
#include <atomic>
#include <vector>
#include "configuration.h"

extern HINSTANCE g_hInst; // Provided by the executable or DLL
std::atomic<bool> g_verboseLogging{false};

namespace {
std::wstring GetLogPath() {
    auto val = g_config.get(L"log_path");
    if (val && !val->empty()) {
        return *val;
    }

    wchar_t logPath[MAX_PATH] = {0};
#ifdef _WIN32
    if (g_hInst) {
        GetModuleFileNameW(g_hInst, logPath, MAX_PATH);
        PathRemoveFileSpecW(logPath);
        PathCombineW(logPath, logPath, L"kbdlayoutmon.log");
    } else {
        lstrcpyW(logPath, L"kbdlayoutmon.log");
    }
    return logPath;
#else
    (void)g_hInst;
    lstrcpyW(logPath, L"kbdlayoutmon.log");
    return logPath;
#endif
}
}

/// Global log instance used by the executable and DLL.
Log g_log;

extern std::atomic<bool> g_debugEnabled;
extern "C" void SetDebugLoggingEnabled(bool enabled) {
    g_debugEnabled.store(enabled);
}

extern "C" void WriteLog(const wchar_t* message) {
    g_log.write(message);
    if (g_verboseLogging) {
#ifdef _WIN32
        OutputDebugStringW(message);
        OutputDebugStringW(L"\n");
#else
        std::wcerr << message << std::endl;
#endif
    }
}

Log::Log(size_t maxQueueSize, bool startThreads) : m_maxQueueSize(maxQueueSize) {
    if (auto val = g_config.get(L"max_queue_size")) {
        try {
            m_maxQueueSize = std::stoul(*val);
        } catch (...) {
            // Ignore parse errors and keep provided default
        }
    }
    m_running = true;
    if (startThreads) {
#ifdef _WIN32
        m_stopEvent.reset(CreateEventW(NULL, TRUE, FALSE, NULL));
        m_thread = std::thread(&Log::process, this);
        m_pipeThread = std::thread(&Log::pipeListener, this);
#else
        m_thread = std::thread(&Log::process, this);
#endif
    }
}

Log::~Log() {
    shutdown();
}

void Log::shutdown() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running = false;
    }
#ifdef _WIN32
    if (m_stopEvent)
        SetEvent(m_stopEvent.get());
#endif
    m_cv.notify_all();
    if (m_thread.joinable())
        m_thread.join();
#ifdef _WIN32
    if (m_pipeThread.joinable())
        m_pipeThread.join();
    m_stopEvent.reset();
#endif
}

void Log::write(const std::wstring& message) {
    if (!g_debugEnabled.load())
        return;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.size() >= m_maxQueueSize) {
            m_queue.pop();
        }
        m_queue.push(message);
    }
    m_cv.notify_one();
}

void Log::setMaxQueueSize(size_t maxSize) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxQueueSize = maxSize ? maxSize : 1; // avoid zero
    while (m_queue.size() > m_maxQueueSize) {
        m_queue.pop();
    }
}

size_t Log::queueSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}

std::wstring Log::peekOldest() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_queue.empty())
        return std::wstring{};
    return m_queue.front();
}

void Log::process() {
    std::wstring path = GetLogPath();
#ifdef _WIN32
    m_file.open(path.c_str(), std::ios::app);
#else
    m_file.open(std::filesystem::path(path), std::ios::app);
#endif
    if (!m_file.is_open()) {
#ifdef _WIN32
        OutputDebugString(L"Failed to open log file.");
#else
        std::wcerr << L"Failed to open log file." << std::endl;
#endif
    }

    auto logError = [this](const wchar_t* msg) {
        ++m_suppress;
        bool prev = g_debugEnabled.exchange(true);
        this->write(msg);
        g_debugEnabled.store(prev);
    };
    for (;;) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_queue.empty() || !m_running; });
        while (!m_queue.empty()) {
            std::wstring msg = std::move(m_queue.front());
            m_queue.pop();
            lock.unlock();

            bool suppress = false;
            if (m_suppress > 0) {
                suppress = true;
                --m_suppress;
            }

            std::wstring cfgPath = GetLogPath();
            if (cfgPath != path || !m_file.is_open()) {
                if (m_file.is_open())
                    m_file.close();
                path = cfgPath;
#ifdef _WIN32
                m_file.open(path.c_str(), std::ios::app);
#else
                m_file.open(std::filesystem::path(path), std::ios::app);
#endif
                if (!m_file.is_open()) {
                    if (!suppress)
                        logError(L"Failed to open log file.");
                    lock.lock();
                    continue;
                }
            }

            if (m_file.is_open()) {
                // Check log file size
                size_t maxMb = 10;
                auto val = g_config.get(L"max_log_size_mb");
                if (val) {
                    try {
                        maxMb = std::stoul(*val);
                    } catch (...) {
                        maxMb = 10;
                    }
                }
                unsigned long long maxBytes = static_cast<unsigned long long>(maxMb) * 1024 * 1024ULL;

#ifdef _WIN32
                WIN32_FILE_ATTRIBUTE_DATA fad;
                if (!suppress && GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) {
                    unsigned long long size = (static_cast<unsigned long long>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
                    if (size > maxBytes) {
                        m_file.close();
                        std::wstring rotated = path + L".1";
                        if (!MoveFileExW(path.c_str(), rotated.c_str(), MOVEFILE_REPLACE_EXISTING)) {
                            logError(L"Failed to rotate log file.");
                            m_file.open(path.c_str(), std::ios::app);
                            if (!m_file.is_open()) {
                                logError(L"Failed to reopen log file after failed rotation.");
                                lock.lock();
                                continue;
                            }
                        } else {
                            m_file.open(path.c_str(), std::ios::out | std::ios::trunc);
                            if (!m_file.is_open()) {
                                logError(L"Failed to reopen log file after rotation.");
                                lock.lock();
                                continue;
                            }
                        }
                    }
                }
#else
                namespace fs = std::filesystem;
                try {
                    if (!suppress && fs::exists(path)) {
                        auto size = fs::file_size(path);
                        if (size > maxBytes) {
                            m_file.close();
                            try {
                                fs::rename(path, path + L".1");
                                m_file.open(fs::path(path), std::ios::out | std::ios::trunc);
                                if (!m_file.is_open()) {
                                    logError(L"Failed to reopen log file after rotation.");
                                    lock.lock();
                                    continue;
                                }
                            } catch (...) {
                                logError(L"Failed to rotate log file.");
                                m_file.open(fs::path(path), std::ios::app);
                                if (!m_file.is_open()) {
                                    logError(L"Failed to reopen log file after failed rotation.");
                                    lock.lock();
                                    continue;
                                }
                            }
                        }
                    }
                } catch (...) {
                }
#endif

                wchar_t ts[32] = {0};
#ifdef _WIN32
                SYSTEMTIME st{};
                GetLocalTime(&st);
                swprintf(ts, 32, L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
#else
                std::time_t t = std::time(nullptr);
                std::tm tm{};
                localtime_r(&t, &tm);
                swprintf(ts, 32, L"%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
#endif
                m_file << ts << L" " << msg << std::endl;
            } else {
#ifdef _WIN32
                OutputDebugString(L"Failed to write to log file.");
#else
                std::wcerr << L"Failed to write to log file." << std::endl;
#endif
            }
            lock.lock();
        }
        if (!m_running && m_queue.empty())
            break;
    }
    if (m_file.is_open())
        m_file.close();
}

#ifdef _WIN32
void Log::pipeListener() {
    const wchar_t* pipeName = L"\\\\.\\pipe\\kbdlayoutmon_log";
    OVERLAPPED ov{};
    HandleGuard event(CreateEventW(NULL, TRUE, FALSE, NULL));
    ov.hEvent = event.get();
    if (!ov.hEvent)
        return;

    while (m_running) {
        HandleGuard pipe(CreateNamedPipeW(pipeName,
                                         PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                                         PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                         PIPE_UNLIMITED_INSTANCES,
                                         0, 0, 0, NULL));
        if (pipe.get() == INVALID_HANDLE_VALUE) {
            Sleep(1000);
            continue;
        }

        ResetEvent(event.get());
        BOOL connected = ConnectNamedPipe(pipe.get(), &ov);
        DWORD err = GetLastError();
        if (!connected) {
            if (err == ERROR_PIPE_CONNECTED) {
                SetEvent(event.get());
            } else if (err != ERROR_IO_PENDING) {
                continue;
            }
        }

        HANDLE events[2] = { ov.hEvent, m_stopEvent.get() };
        DWORD wait = WaitForMultipleObjects(2, events, FALSE, INFINITE);
        if (wait == WAIT_OBJECT_0 + 1) {
            break;
        } else if (wait == WAIT_OBJECT_0) {
            std::wstring message;
            std::vector<wchar_t> buffer(1024);
            DWORD bytesRead = 0;
            BOOL success = FALSE;
            do {
                success = ReadFile(pipe.get(), buffer.data(),
                                   static_cast<DWORD>(buffer.size() * sizeof(wchar_t)),
                                   &bytesRead, NULL);
                if (bytesRead > 0) {
                    message.append(buffer.data(), bytesRead / sizeof(wchar_t));
                }
            } while (!success && GetLastError() == ERROR_MORE_DATA);

            if (success && !message.empty()) {
                write(message);
            }
        }

        DisconnectNamedPipe(pipe.get());
    }
}
#else
void Log::pipeListener() {}
#endif

