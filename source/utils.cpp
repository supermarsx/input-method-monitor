#include "utils.h"

std::wstring QuotePath(const std::wstring& path) {
    // Windows command line parsing requires that any backslashes preceding a
    // quote be doubled and the quote itself escaped. Additionally, any
    // trailing backslashes must be doubled before the closing quote. This
    // function performs those transformations and then surrounds the path in
    // quotes.

    std::wstring result;
    result.reserve(path.size() * 2 + 2);
    result.push_back(L'"');

    std::size_t backslashCount = 0;
    for (wchar_t ch : path) {
        if (ch == L'\\') {
            ++backslashCount;
        } else if (ch == L'"') {
            // Double the backslashes and escape the quote
            result.append(backslashCount * 2, L'\\');
            result.push_back(L'\\');
            result.push_back(L'"');
            backslashCount = 0;
        } else {
            // Emit any accumulated backslashes and the current character
            result.append(backslashCount, L'\\');
            result.push_back(ch);
            backslashCount = 0;
        }
    }

    // Double any trailing backslashes before appending the closing quote
    result.append(backslashCount * 2, L'\\');
    result.push_back(L'"');

    return result;
}
