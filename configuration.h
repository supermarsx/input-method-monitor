#pragma once

#include <map>
#include <string>

class Configuration {
public:
    // map containing lowercased keys from the config file
    std::map<std::wstring, std::wstring> settings;

    void load();
};
