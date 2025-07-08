#include "configuration.h"
#include <windows.h>
#include <Shlwapi.h>
#include "constants.h"
#include <fstream>
#include <algorithm>

extern HINSTANCE g_hInst; // Provided by the executable

void Configuration::load() {
    wchar_t configPath[MAX_PATH];
    GetModuleFileNameW(g_hInst, configPath, MAX_PATH);
    PathRemoveFileSpecW(configPath);
    PathCombineW(configPath, configPath, configFile.c_str());

    std::wifstream file(configPath);
    if (!file.is_open()) {
        return;
    }

    std::wstring line;
    while (std::getline(file, line)) {
        size_t start = line.find_first_not_of(L" \t\r\n");
        if (start == std::wstring::npos)
            continue;
        size_t end = line.find_last_not_of(L" \t\r\n");
        line = line.substr(start, end - start + 1);
        if (line.empty())
            continue;

        size_t eqPos = line.find(L'=');
        if (eqPos == std::wstring::npos)
            continue;

        std::wstring key = line.substr(0, eqPos);
        std::wstring value = line.substr(eqPos + 1);

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
        settings[key] = value;
    }
}
