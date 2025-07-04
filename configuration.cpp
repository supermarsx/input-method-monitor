#include <windows.h>
#include <Shlwapi.h>
#include <constants.h>
#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>

class Configuration {
public:
    // Map to store settings (key:wstring, value:wstring)
    std::map<std::wstring, std::wstring> settings;

    void load() {
        wchar_t configPath[MAX_PATH];
        GetModuleFileNameW(g_hInst, configPath, MAX_PATH);
        PathRemoveFileSpecW(configPath);
        PathCombineW(configPath, configPath, configFile.c_str());

        std::wifstream configFile(configPath);
        if (configFile.is_open()) {
            std::wstring line;
            while (std::getline(configFile, line)) {
                // Trim leading and trailing spaces
                size_t start = line.find_first_not_of(L" \t\r\n");
                size_t end = line.find_last_not_of(L" \t\r\n");
                if (start == std::wstring::npos) {
                    continue; // skip empty line
                }
                line = line.substr(start, end - start + 1);
                if (line.empty()) continue;

                // Split on '=' character
                size_t eqPos = line.find(L'=');
                if (eqPos != std::wstring::npos) {
                    std::wstring key = line.substr(0, eqPos);
                    std::wstring value = line.substr(eqPos + 1);

                    // Convert key to lowercase
                    std::transform(key.begin(), key.end(), key.begin(), [](wchar_t c) { return std::towlower(c); });

                    settings[key] = value;
                }
            }
            configFile.close();
        }
    }
};
