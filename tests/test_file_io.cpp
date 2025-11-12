#include "test_file_io.h"
#include "windows_stub.h"
#include <vector>

std::wstring read_file_wstring_win(const std::filesystem::path& path) {
    std::wstring result;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return result;
    DWORD sizeLow = GetFileSize(h, nullptr);
    if (sizeLow == INVALID_FILE_SIZE) { CloseHandle(h); return result; }
    DWORD bytes = sizeLow;
    std::vector<unsigned char> buf(bytes + 4);
    DWORD read = 0;
    if (ReadFile(h, buf.data(), bytes, &read, nullptr) && read > 0) {
        // Detect BOM (UTF-16 LE) first
        if (read >= 2 && buf[0] == 0xFF && buf[1] == 0xFE) {
            size_t wchars = (read - 2) / sizeof(wchar_t);
            result.resize(wchars);
            memcpy(&result[0], reinterpret_cast<const char*>(buf.data() + 2), wchars * sizeof(wchar_t));
        } else {
            // Heuristic: if many zero bytes at odd indices, assume UTF-16 LE without BOM
            size_t check = std::min<size_t>(read, 256);
            size_t zeroCount = 0;
            size_t samples = 0;
            for (size_t i = 1; i + 1 < check; i += 2) {
                ++samples;
                if (buf[i] == 0x00) ++zeroCount;
            }
            if (samples > 0 && (zeroCount * 100 / samples) > 60 && (read % 2) == 0) {
                size_t wchars = read / sizeof(wchar_t);
                result.resize(wchars);
                memcpy(&result[0], reinterpret_cast<const char*>(buf.data()), wchars * sizeof(wchar_t));
            } else {
                // fallback: convert from UTF-8 to wide
                int needed = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(buf.data()), (int)read, nullptr, 0);
                if (needed > 0) {
                    result.resize(needed);
                    MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(buf.data()), (int)read, &result[0], needed);
                }
            }
        }
    }
    CloseHandle(h);
    return result;
}
