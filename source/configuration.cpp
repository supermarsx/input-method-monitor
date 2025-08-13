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
#include "config_parser.h"
#include <fstream>

#ifndef _WIN32
using HINSTANCE = void*;
#endif
extern HINSTANCE g_hInst; // Provided by the executable

/// Global configuration instance shared across modules.
Configuration g_config;

void Configuration::load(std::optional<std::wstring> path) {
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
        return;
    }

    settings = ParseConfigStream(file);
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
