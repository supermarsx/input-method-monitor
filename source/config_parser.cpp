#include "config_parser.h"
#include <algorithm>
#include <cwctype>
#include <stdexcept>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
std::wstring ParseUnsignedOrDefault(const std::wstring& value, unsigned long def) {
    try {
        if (!value.empty() && value[0] == L'-') {
            throw std::invalid_argument("negative");
        }
        unsigned long parsed = std::stoul(value);
        return std::to_wstring(parsed);
    } catch (...) {
        return std::to_wstring(def);
    }
}

std::wstring ExpandEnvVars(const std::wstring& input) {
#ifdef _WIN32
    // Use the Windows API to expand %VAR% style references.
    DWORD needed = ExpandEnvironmentStringsW(input.c_str(), nullptr, 0);
    if (needed == 0)
        return input;
    std::wstring buffer(needed, L'\0');
    DWORD written = ExpandEnvironmentStringsW(input.c_str(), buffer.data(), needed);
    if (written == 0)
        return input;
    if (!buffer.empty() && buffer.back() == L'\0')
        buffer.pop_back();
    return buffer;
#else
    // Manual expansion supporting $VAR and ${VAR} as well as %VAR%.
    std::wstring result;
    result.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        wchar_t c = input[i];
        if (c == L'%') {
            size_t end = input.find(L'%', i + 1);
            if (end != std::wstring::npos) {
                std::wstring name = input.substr(i + 1, end - i - 1);
                std::string narrow(name.begin(), name.end());
                const char* val = std::getenv(narrow.c_str());
                if (val) {
                    std::string sval(val);
                    result.append(sval.begin(), sval.end());
                }
                i = end;
                continue;
            }
        } else if (c == L'$') {
            size_t start = i + 1;
            size_t end = start;
            bool brace = false;
            if (start < input.size() && input[start] == L'{') {
                brace = true;
                start++;
                end = start;
                while (end < input.size() && input[end] != L'}')
                    ++end;
                if (end == input.size()) {
                    // Unmatched '{', treat literally
                    result.push_back(c);
                    continue;
                }
            } else {
                while (end < input.size() &&
                       (std::iswalnum(input[end]) || input[end] == L'_'))
                    ++end;
            }

            if (end > start) {
                std::wstring name = input.substr(start, end - start);
                std::string narrow(name.begin(), name.end());
                const char* val = std::getenv(narrow.c_str());
                if (val) {
                    std::string sval(val);
                    result.append(sval.begin(), sval.end());
                }
                i = brace ? end : end - 1;
                continue;
            }
        }
        result.push_back(c);
    }
    return result;
#endif
}
} // namespace

std::wstring ParseBoolOrDefault(const std::wstring& value, bool def) {
    std::wstring lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) {
        return std::towlower(c);
    });
    if (lower == L"1" || lower == L"true" || lower == L"yes" || lower == L"on")
        return L"1";
    if (lower == L"0" || lower == L"false" || lower == L"no" || lower == L"off")
        return L"0";
    return def ? L"1" : L"0";
}

std::map<std::wstring, std::wstring> ParseConfigLines(const std::vector<std::wstring>& lines) {
    std::map<std::wstring, std::wstring> result;
    for (const std::wstring& line : lines) {
        std::wstring currentLine = line;
        size_t start = currentLine.find_first_not_of(L" \t\r\n");
        if (start == std::wstring::npos)
            continue;
        size_t end = currentLine.find_last_not_of(L" \t\r\n");
        currentLine = currentLine.substr(start, end - start + 1);
        if (currentLine.empty())
            continue;

        if (currentLine[0] == L'#' || currentLine[0] == L';')
            continue;

        size_t eqPos = currentLine.find(L'=');
        if (eqPos == std::wstring::npos)
            continue;

        std::wstring key = currentLine.substr(0, eqPos);
        std::wstring value = currentLine.substr(eqPos + 1);

        // Strip inline comments while respecting quoted sections
        bool inQuotes = false;
        wchar_t quoteChar = 0;
        size_t commentPos = std::wstring::npos;
        for (size_t i = 0; i < value.size(); ++i) {
            wchar_t c = value[i];
            if (inQuotes) {
                if (c == quoteChar)
                    inQuotes = false;
            } else {
                if (c == L'"' || c == L'\'') {
                    inQuotes = true;
                    quoteChar = c;
                } else if (c == L'#' || c == L';') {
                    commentPos = i;
                    break;
                }
            }
        }
        if (commentPos != std::wstring::npos)
            value = value.substr(0, commentPos);

        auto trim = [](std::wstring& s) {
            size_t start = s.find_first_not_of(L" \t\r\n");
            size_t end = s.find_last_not_of(L" \t\r\n");
            if (start == std::wstring::npos) {
                s.clear();
            } else {
                s = s.substr(start, end - start + 1);
            }
        };

        trim(key);
        trim(value);

        // Remove surrounding quotes if present
        if (value.size() >= 2 &&
            ((value.front() == L'"' && value.back() == L'"') ||
             (value.front() == L'\'' && value.back() == L'\''))) {
            value = value.substr(1, value.size() - 2);
        }

        // Expand environment variables such as %VAR% or $VAR
        value = ExpandEnvVars(value);

        std::transform(key.begin(), key.end(), key.begin(), [](wchar_t c) {
            return std::towlower(c);
        });

        if (key == L"temp_hotkey_timeout") {
            result[key] = ParseUnsignedOrDefault(value, 10000);
        } else if (key == L"max_log_size_mb") {
            result[key] = ParseUnsignedOrDefault(value, 10);
        } else if (key == L"max_queue_size") {
            result[key] = ParseUnsignedOrDefault(value, 1000);
        } else if (key == L"startup" || key == L"language_hotkey" || key == L"layout_hotkey") {
            result[key] = ParseBoolOrDefault(value, false);
        } else if (key == L"icon_path" || key == L"tray_tooltip") {
            // String values used by the tray icon subsystem
            result[key] = value;
        } else {
            result[key] = value;
        }
    }
    return result;
}

std::map<std::wstring, std::wstring> ParseConfigStream(std::wistream& stream) {
    std::vector<std::wstring> lines;
    std::wstring line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return ParseConfigLines(lines);
}

