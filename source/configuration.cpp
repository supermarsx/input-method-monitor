#include "configuration.h"
#include <windows.h>
#include <shlwapi.h>
#include "constants.h"
#include "log.h"
#include <fstream>
#include <algorithm>

extern HINSTANCE g_hInst; // Provided by the executable

// Global configuration instance shared across modules
Configuration g_config;

void Configuration::load(std::optional<std::wstring> path) {
    std::wstring fullPath;

    if (path && !path->empty()) {
        m_lastPath = *path;
        fullPath = *path;
    } else if (!m_lastPath.empty()) {
        fullPath = m_lastPath;
    } else {
        wchar_t configPath[MAX_PATH];
        GetModuleFileNameW(g_hInst, configPath, MAX_PATH);
        PathRemoveFileSpecW(configPath);
        PathCombineW(configPath, configPath, configFile.c_str());
        fullPath = configPath;
        m_lastPath = fullPath;
    }

    std::wifstream file(fullPath.c_str());
    if (!file.is_open()) {
        std::wstring msg = L"Failed to open configuration file: " + fullPath;
        WriteLog(msg.c_str());
        return;
    }

    settings.clear();

    std::vector<std::wstring> lines;
    std::wstring line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    for (const std::wstring& lineRef : lines) {
        std::wstring currentLine = lineRef;
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

        std::transform(key.begin(), key.end(), key.begin(), [](wchar_t c) {
            return std::towlower(c);
        });

        if (key == L"temp_hotkey_timeout") {
            try {
                int timeout = std::stoi(value);
                settings[key] = std::to_wstring(timeout);
            } catch (...) {
                settings[key] = L"10000";
            }
        } else if (key == L"max_log_size_mb") {
            try {
                int size = std::stoi(value);
                settings[key] = std::to_wstring(size);
            } catch (...) {
                settings[key] = L"10";
            }
        } else {
            settings[key] = value;
        }
    }
}

std::wstring Configuration::getLastPath() const {
    return m_lastPath;
}
