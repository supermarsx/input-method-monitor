#pragma once

#include <map>
#include <string>

class Configuration {
public:
    // map containing lowercased keys from the config file
    std::map<std::wstring, std::wstring> settings;

    // Load configuration from the given path. If no path is provided,
    // the previously loaded path is used. When no path has been loaded
    // yet, the default configuration file next to the executable is used.
    void load(const std::wstring& path = L"");

private:
    std::wstring m_lastPath; // remembers the path of the last loaded config
};

extern Configuration g_config; // Shared configuration instance
