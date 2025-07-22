#pragma once
#include <map>
#include <string>
#include <algorithm>
#include <cwctype>
#include <vector>
#include <fstream>
#include <filesystem>

inline std::map<std::wstring, std::wstring> parse_config_lines(const std::vector<std::wstring>& lines) {
    std::map<std::wstring, std::wstring> settings;
    for (const std::wstring& line : lines) {
        std::wstring currentLine = line;
        size_t start = currentLine.find_first_not_of(L" \t\r\n");
        if (start == std::wstring::npos)
            continue;
        size_t end = currentLine.find_last_not_of(L" \t\r\n");
        currentLine = currentLine.substr(start, end - start + 1);
        if (currentLine.empty())
            continue;

        size_t eqPos = currentLine.find(L'=');
        if (eqPos == std::wstring::npos)
            continue;

        std::wstring key = currentLine.substr(0, eqPos);
        std::wstring value = currentLine.substr(eqPos + 1);

        auto trim = [](std::wstring &s) {
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

        std::transform(key.begin(), key.end(), key.begin(), [](wchar_t c){
            return std::towlower(c);
        });

        if (key == L"temp_hotkey_timeout") {
            try {
                if (!value.empty() && value[0] == L'-') {
                    throw std::invalid_argument("negative");
                }
                unsigned long timeout = std::stoul(value);
                settings[key] = std::to_wstring(timeout);
            } catch (...) {
                settings[key] = L"10000";
            }
        } else if (key == L"max_log_size_mb") {
            try {
                if (!value.empty() && value[0] == L'-') {
                    throw std::invalid_argument("negative");
                }
                unsigned long size = std::stoul(value);
                settings[key] = std::to_wstring(size);
            } catch (...) {
                settings[key] = L"10";
            }
        } else {
            settings[key] = value;
        }
    }
    return settings;
}

inline std::map<std::wstring, std::wstring> parse_config_file(const std::wstring& path) {
    std::ifstream file{std::filesystem::path(path)};
    std::vector<std::wstring> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(std::wstring(line.begin(), line.end()));
    }
    return parse_config_lines(lines);
}
