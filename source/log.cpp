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
#include "app_state.h"

#ifdef UNIT_TEST
#include "../tests/windows_stub.h"
#endif


extern HINSTANCE g_hInst; // Provided by the executable or DLL
std::atomic<bool> g_verboseLogging{false};
std::atomic<LogLevel> g_logLevel{LogLevel::Info};

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
#ifdef UNIT_TEST
// During unit tests avoid starting global log worker threads to prevent
// interfering with test-local Log instances and locking log files.
Log g_log(1000, false);
#else
Log g_log;
#endif

extern "C" void SetDebugLoggingEnabled(bool enabled) {
    GetAppState().debugEnabled.store(enabled);
}

namespace {
const wchar_t* LevelPrefix(LogLevel level) {
    switch (level) {
    case LogLevel::Warn:
        return L"WARN";
    case LogLevel::Error:
        return L"ERROR";
    default:
        return L"INFO";
    }
}
}

void WriteLog(LogLevel level, const wchar_t* message) {
#ifdef UNIT_TEST
    // In unit tests write synchronously to the configured log path so tests
    // observe entries immediately without relying on background threads.
    if (!GetAppState().debugEnabled.load()) return;
    if (level < g_logLevel.load()) return;

    auto append_with_bom_and_sharing = [](const std::wstring& path, const std::wstring& payload) {
#ifdef _WIN32
        // Open with share flags that allow other processes to delete the file while it is open.
        HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE)
            return false;

        // Instrumentation for unit tests: report handle opens/closes for debugging
        OutputDebugStringW(L"UNIT_TEST: Opened log file handle\n");

        // If file was just created (size 0) write UTF-16 LE BOM
        LARGE_INTEGER li{};
        if (GetFileSizeEx(h, &li) && li.QuadPart == 0) {
            unsigned char bom[2] = { 0xFF, 0xFE };
            DWORD wrote = 0;
            WriteFile(h, bom, 2, &wrote, NULL);
        }

        // Move to end and write payload as UTF-16 LE
        // Note: WriteFile on Windows writes bytes; wchar_t is UTF-16 on Windows
        DWORD bytes = static_cast<DWORD>(payload.size() * sizeof(wchar_t));
        DWORD written = 0;
        // Ensure we append by setting file pointer to end
        SetFilePointer(h, 0, NULL, FILE_END);
        BOOL ok = WriteFile(h, payload.c_str(), bytes, &written, NULL);
        CloseHandle(h);
        OutputDebugStringW(L"UNIT_TEST: Closed log file handle\n");
        return ok == TRUE;
#else
        // Fallback to wofstream for non-Windows (shouldn't be used in these tests)
        try {
            std::wofstream ofs(std::filesystem::path(path), std::ios::app);
            if (!ofs.is_open()) return false;
            ofs << payload;
            ofs.close();
            return true;
        } catch (...) {
            return false;
        }
#endif
    };

    std::wstring path = GetLogPath();
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
    std::wstring line = std::wstring(ts) + L" [" + LevelPrefix(level) + L"] " + message + L"\r\n";

    // In UNIT_TEST ensure rotation semantics are honored prior to writing
#ifdef _WIN32
    // Determine max size and backups from configuration
    unsigned long long maxBytes = 10ULL * 1024ULL * 1024ULL;
    if (auto val = g_config.get(L"max_log_size_mb")) {
        try { maxBytes = static_cast<unsigned long long>(std::stoul(*val)) * 1024ULL * 1024ULL; } catch(...) { }
    }
    size_t maxBackups = 5;
    if (auto b = g_config.get(L"max_log_backups")) {
        try { maxBackups = std::stoul(*b); } catch(...) { }
    }

    WIN32_FILE_ATTRIBUTE_DATA fad;
    bool exists = GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad) != 0;
    auto rotate_file = [](const std::wstring& src, const std::wstring& dest) {
        if (MoveFileExW(src.c_str(), dest.c_str(), MOVEFILE_REPLACE_EXISTING))
            return true;
        if (CopyFileW(src.c_str(), dest.c_str(), FALSE)) {
            // Copy is sufficient for tests even if delete fails due to open handles.
            DeleteFileW(src.c_str());
            return true;
        }
        HANDLE hSrc = CreateFileW(src.c_str(), GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hSrc == INVALID_HANDLE_VALUE)
            hSrc = nullptr;
        HANDLE hDest = CreateFileW(dest.c_str(), GENERIC_WRITE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hDest == INVALID_HANDLE_VALUE) {
            if (hSrc)
                CloseHandle(hSrc);
            return false;
        }
        if (hSrc) {
            std::vector<char> buf(64 * 1024);
            DWORD read = 0;
            bool ok = true;
            while (ReadFile(hSrc, buf.data(), static_cast<DWORD>(buf.size()), &read, NULL) && read > 0) {
                DWORD written = 0;
                if (!WriteFile(hDest, buf.data(), read, &written, NULL) || written != read) {
                    ok = false;
                    break;
                }
            }
            CloseHandle(hSrc);
            CloseHandle(hDest);
            return ok;
        }
        CloseHandle(hDest);
        return true;
    };

    if (exists) {
        unsigned long long size = (static_cast<unsigned long long>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
        if (size > maxBytes) {
            // Rotate backups
            for (size_t i = maxBackups; i > 0; --i) {
                std::wstring src = path + L"." + std::to_wstring(i);
                DWORD attrs = GetFileAttributesW(src.c_str());
                if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                    if (i == maxBackups) {
                        DeleteFileW(src.c_str());
                    } else {
                        std::wstring dest = path + L"." + std::to_wstring(i + 1);
                        rotate_file(src, dest);
                    }
                }
            }
            std::wstring rotated = path + L".1";
            if (!rotate_file(path, rotated)) {
                // Failed to rotate - write an error log entry directly to path
                append_with_bom_and_sharing(path, std::wstring(ts) + L" [ERROR] Failed to rotate log file.\r\n");
            }
        }
    }
#endif

    append_with_bom_and_sharing(path, line);

    if (g_verboseLogging) {
        std::wstring out = std::wstring(L"[") + LevelPrefix(level) + L"] " + message;
#ifdef _WIN32
        OutputDebugStringW(out.c_str());
        OutputDebugStringW(L"\n");
#else
        std::wcerr << out << std::endl;
#endif
    }
#else
    g_log.write(level, message);
    if (g_verboseLogging) {
        std::wstring out = std::wstring(L"[") + LevelPrefix(level) + L"] " + message;
#ifdef _WIN32
        OutputDebugStringW(out.c_str());
        OutputDebugStringW(L"\n");
#else
        std::wcerr << out << std::endl;
#endif
    }
#endif
}

extern "C" void WriteLog(const wchar_t* message) {
    WriteLog(LogLevel::Info, message);
}

// std::wstring overloads centralized here to avoid inline emissions across TUs
void WriteLog(LogLevel level, const std::wstring& message) {
    WriteLog(level, message.c_str());
}

void WriteLog(const std::wstring& message) {
    WriteLog(message.c_str());
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

void Log::write(LogLevel level, const std::wstring& message) {
    if (!GetAppState().debugEnabled.load())
        return;
    if (level < g_logLevel.load())
        return;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.size() >= m_maxQueueSize) {
            m_queue.pop();
        }
        m_queue.push({level, message});
    }
    m_cv.notify_one();
}

void Log::write(const std::wstring& message) {
    write(LogLevel::Info, message);
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
    return m_queue.front().second;
}

void Log::process() {
    std::wstring path = GetLogPath();
#ifndef UNIT_TEST
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
#endif


    auto logError = [this](const wchar_t* msg) {
        ++m_suppress;
        bool prev = GetAppState().debugEnabled.exchange(true);
        this->write(LogLevel::Error, msg);
        GetAppState().debugEnabled.store(prev);
    };
    for (;;) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_queue.empty() || !m_running; });
        while (!m_queue.empty()) {
            auto entry = std::move(m_queue.front());
            m_queue.pop();
            lock.unlock();
            LogLevel level = entry.first;
            std::wstring msg = std::move(entry.second);

            bool suppress = false;
            if (m_suppress > 0) {
                suppress = true;
                --m_suppress;
            }

#ifndef UNIT_TEST
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
#endif

#ifdef UNIT_TEST
                // In unit tests, forward to WriteLog to reuse BOM, sharing and rotation logic.
                WriteLog(level, msg.c_str());
                lock.lock();
#else
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

                size_t maxBackups = 5;
                if (auto backups = g_config.get(L"max_log_backups")) {
                    try {
                        maxBackups = std::stoul(*backups);
                    } catch (...) {
                        maxBackups = 5;
                    }
                }

#ifdef _WIN32
                WIN32_FILE_ATTRIBUTE_DATA fad;
                if (!suppress && GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) {
                    unsigned long long size = (static_cast<unsigned long long>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
                    if (size > maxBytes) {
                        m_file.close();

                        for (size_t i = maxBackups; i > 0; --i) {
                            std::wstring src = path + L"." + std::to_wstring(i);
                            DWORD attrs = GetFileAttributesW(src.c_str());
                            if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                                if (i == maxBackups) {
                                    DeleteFileW(src.c_str());
                                } else {
                                    std::wstring dest = path + L"." + std::to_wstring(i + 1);
                                    MoveFileExW(src.c_str(), dest.c_str(), MOVEFILE_REPLACE_EXISTING);
                                }
                            }
                        }

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
                                for (size_t i = maxBackups; i > 0; --i) {
                                    fs::path src = path + L"." + std::to_wstring(i);
                                    if (fs::exists(src) && fs::is_regular_file(src)) {
                                        if (i == maxBackups) {
                                            fs::remove(src);
                                        } else {
                                            fs::rename(src, path + L"." + std::to_wstring(i + 1));
                                        }
                                    }
                                }
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
                m_file << ts << L" [" << LevelPrefix(level) << L"] " << msg << std::endl;
            } else {
#ifdef _WIN32
                OutputDebugString(L"Failed to write to log file.");
#else
                std::wcerr << L"Failed to write to log file." << std::endl;
#endif
            }
            lock.lock();
#endif
        }
        if (!m_running && m_queue.empty())
            break;
    }
    if (m_file.is_open())
        m_file.close();
}

#ifdef _WIN32
void Log::pipeListener() {
    std::wstring loc = GetLogPath();
    OutputDebugStringW(L"pipeListener: starting\n");
    OutputDebugStringW(loc.c_str());
    OutputDebugStringW(L"\n");
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
                LogLevel level = LogLevel::Info;
                if (message.size() > 2 && message[0] == L'[') {
                    auto pos = message.find(L"] ");
                    if (pos != std::wstring::npos) {
                        std::wstring lvl = message.substr(1, pos - 1);
                        if (lvl == L"WARN")
                            level = LogLevel::Warn;
                        else if (lvl == L"ERROR")
                            level = LogLevel::Error;
                        message = message.substr(pos + 2);
                    }
                }
                write(level, message);
            }
        }

        DisconnectNamedPipe(pipe.get());
    }
}
#else
void Log::pipeListener() {}
#endif

