#include "configuration.h"
#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#else
#include <filesystem>
#include <cwchar>
#endif
#include "constants.h"
#include "log.h"
#include <fstream>
#include <algorithm>
#include <cwctype>

#ifndef _WIN32
using HINSTANCE = void*;
#endif
extern HINSTANCE g_hInst; // Provided by the executable

/// Global configuration instance shared across modules.
Configuration g_config;

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

bool Configuration::load(std::optional<std::wstring> path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::wstring fullPath;

    if (path && !path->empty()) {
        m_lastPath = *path;
        fullPath = *path;
    } else {
        if (!m_lastPath.empty()) {
            fullPath = m_lastPath;
        }
        if (fullPath.empty()) {
#ifdef _WIN32
        wchar_t configPath[MAX_PATH];
        GetModuleFileNameW(g_hInst, configPath, MAX_PATH);
        PathRemoveFileSpecW(configPath);
        PathCombineW(configPath, configPath, configFile.c_str());
        fullPath = configPath;
#else
        std::filesystem::path cfg = std::filesystem::current_path() / configFile;
        fullPath = cfg.wstring();
#endif
            m_lastPath = fullPath;
        }
    }

#ifdef _WIN32
    std::wifstream file(fullPath.c_str());
#else
    std::wifstream file{std::filesystem::path(fullPath)};
#endif
    if (!file.is_open()) {
        std::wstring msg = L"Failed to open configuration file: " + fullPath;
        WriteLog(msg.c_str());
        return false;
    }

    settings = ParseConfigStream(file);
    return true;
}

std::wstring Configuration::getLastPath() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastPath;
}

std::optional<std::wstring> Configuration::getSetting(const std::wstring& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = settings.find(key);
    if (it != settings.end())
        return it->second;
    return std::nullopt;
}

void Configuration::setSetting(const std::wstring& key, const std::wstring& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    settings[key] = value;
}
