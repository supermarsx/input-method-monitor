#pragma once
#include <cwchar>
#include <cstddef>
using HRESULT = long;
inline HRESULT StringCchCopyW(wchar_t* dst, size_t dstSize, const wchar_t* src) {
    if (!dst || dstSize == 0) return 1;
    if (src) {
        std::wcsncpy(dst, src, dstSize - 1);
        dst[dstSize - 1] = L'\0';
    } else {
        dst[0] = L'\0';
    }
    return 0;
}
