#include <constants.h>
#include <map>
#include <string>
#include <sstream>
#include <fstream>

class Configuration {
public:
    // Map to store settings (key:wstring, value:wstring)
    std::map<std::wstring, std::wstring> settings;

    void load() {
        std::wstring configPath;
        GetModuleFileName(g_hInst, configPath, MAX_PATH);
        PathRemoveFileSpec(configPath);
        PathCombine(configPath, configPath, configFile);

        std::wifstream configFile(configPath);
        if (configFile.is_open()) {
            std::wstring line;
            while (std::getline(configFile, line)) {
                // Remove leading/trailing whitespace
                std::wostringstream ss(line);
                ss << std::ws; // consume leading whitespace
                ss << std::ws; // consume trailing whitespace
                std::string lineStr = std::string(ss.rdbuf());
                
                // Split on first "="
                size_t eqPos = lineStr.find(L"=");
                if (eqPos != std::wstring::npos) {
                    std::wstring key = lineStr.substr(0, eqPos);
                    std::wstring value = lineStr.substr(eqPos + 1);
                    
                    // Convert to lowercase for case-insensitive matching
                    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                    
                    settings[key] = value;
                }
            }
            configFile.close();
        }
    }
};
