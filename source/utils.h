#pragma once
#include <string>

inline std::wstring QuotePath(const std::wstring& path) {
    return L"\"" + path + L"\"";
}
