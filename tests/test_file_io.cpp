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
    std::vector<char> buf(bytes + 2);
    DWORD read = 0;
    if (ReadFile(h, buf.data(), bytes, &read, nullptr) && read > 0) {
        // assume UTF-16 LE file (wchar_t)
        if (read >= 2 && reinterpret_cast<unsigned char*>(buf.data())[0] == 0xFF && reinterpret_cast<unsigned char*>(buf.data())[1] == 0xFE) {
            // has BOM
            size_t wchars = (read - 2) / sizeof(wchar_t);
            result.resize(wchars);
            memcpy(&result[0], buf.data() + 2, wchars * sizeof(wchar_t));
        } else {
            // fallback: convert from narrow to wide
            int needed = MultiByteToWideChar(CP_UTF8, 0, buf.data(), (int)read, nullptr, 0);
            if (needed > 0) {
                result.resize(needed);
                MultiByteToWideChar(CP_UTF8, 0, buf.data(), (int)read, &result[0], needed);
            }
        }
    }
    CloseHandle(h);
    return result;
}
