#include "log.h"
#include <windows.h>
#include <fstream>
#include <shlwapi.h>
#include <utility>
#include "configuration.h"

extern HINSTANCE g_hInst; // Provided by the executable or DLL
extern bool g_debugEnabled;

namespace {
std::wstring GetLogPath() {
    auto it = g_config.settings.find(L"log_path");
    if (it != g_config.settings.end() && !it->second.empty()) {
        return it->second;
    }

    wchar_t logPath[MAX_PATH] = {0};
    if (g_hInst) {
        GetModuleFileName(g_hInst, logPath, MAX_PATH);
        PathRemoveFileSpec(logPath);
        PathCombine(logPath, logPath, L"kbdlayoutmon.log");
    } else {
        lstrcpyW(logPath, L"kbdlayoutmon.log");
    }
    return logPath;
}
}

Log g_log;

Log::Log() {
    m_running = true;
    m_thread = std::thread(&Log::process, this);
    m_pipeThread = std::thread(&Log::pipeListener, this);
}

Log::~Log() {
    shutdown();
}

void Log::shutdown() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running = false;
    }
    m_cv.notify_all();
    if (m_thread.joinable())
        m_thread.join();
    if (m_pipeThread.joinable())
        m_pipeThread.join();
}

void Log::write(const std::wstring& message) {
    if (!g_debugEnabled)
        return;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(message);
    }
    m_cv.notify_one();
}

void Log::process() {
    std::wstring path = GetLogPath();
    m_file.open(path, std::ios::app);
    if (!m_file.is_open()) {
        OutputDebugString(L"Failed to open log file.");
    }
    for (;;) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_queue.empty() || !m_running; });
        while (!m_queue.empty()) {
            std::wstring msg = std::move(m_queue.front());
            m_queue.pop();
            lock.unlock();

            if (m_file.is_open()) {
                // Check log file size
                size_t maxMb = 10;
                auto it = g_config.settings.find(L"max_log_size_mb");
                if (it != g_config.settings.end()) {
                    try {
                        maxMb = std::stoul(it->second);
                    } catch (...) {
                        maxMb = 10;
                    }
                }
                ULONGLONG maxBytes = static_cast<ULONGLONG>(maxMb) * 1024 * 1024ULL;

                WIN32_FILE_ATTRIBUTE_DATA fad;
                if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) {
                    ULONGLONG size = (static_cast<ULONGLONG>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
                    if (size > maxBytes) {
                        m_file.close();
                        std::wstring rotated = path + L".1";
                        MoveFileExW(path.c_str(), rotated.c_str(), MOVEFILE_REPLACE_EXISTING);
                        m_file.open(path, std::ios::out | std::ios::trunc);
                    }
                }

                SYSTEMTIME st{};
                GetLocalTime(&st);
                wchar_t ts[32] = {0};
                swprintf(ts, 32, L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
                m_file << ts << L" " << msg << std::endl;
            } else {
                OutputDebugString(L"Failed to write to log file.");
            }
            lock.lock();
        }
        if (!m_running && m_queue.empty())
            break;
    }
    if (m_file.is_open())
        m_file.close();
}

void Log::pipeListener() {
    const wchar_t* pipeName = L"\\\\.\\pipe\\kbdlayoutmon_log";
    while (m_running) {
        HANDLE hPipe = CreateNamedPipeW(pipeName,
                                       PIPE_ACCESS_INBOUND,
                                       PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                       PIPE_UNLIMITED_INSTANCES,
                                       0, 0, 0, NULL);
        if (hPipe == INVALID_HANDLE_VALUE) {
            Sleep(1000);
            continue;
        }

        BOOL connected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (connected) {
            wchar_t buffer[1024];
            DWORD bytesRead = 0;
            while (ReadFile(hPipe, buffer, sizeof(buffer) - sizeof(wchar_t), &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead / sizeof(wchar_t)] = L'\0';
                write(buffer);
            }
        }
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
}

