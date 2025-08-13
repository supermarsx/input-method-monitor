#include "config_parser.h"
#include <algorithm>
#include <cwctype>
#include <stdexcept>

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

        std::transform(key.begin(), key.end(), key.begin(), [](wchar_t c) {
            return std::towlower(c);
        });

        if (key == L"temp_hotkey_timeout") {
            try {
                if (!value.empty() && value[0] == L'-') {
                    throw std::invalid_argument("negative");
                }
                unsigned long timeout = std::stoul(value);
                result[key] = std::to_wstring(timeout);
            } catch (...) {
                result[key] = L"10000";
            }
        } else if (key == L"max_log_size_mb") {
            try {
                if (!value.empty() && value[0] == L'-') {
                    throw std::invalid_argument("negative");
                }
                unsigned long size = std::stoul(value);
                result[key] = std::to_wstring(size);
            } catch (...) {
                result[key] = L"10";
            }
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

