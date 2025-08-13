#pragma once
#include <string>

inline std::wstring QuotePath(const std::wstring& path) {
    std::wstring escaped;
    escaped.reserve(path.size());
    for (wchar_t ch : path) {
        if (ch == L'"') {
            escaped += L"\\\""; // escape inner quotes
        } else {
            escaped += ch;
        }
    }
    return L"\"" + escaped + L"\"";
}
