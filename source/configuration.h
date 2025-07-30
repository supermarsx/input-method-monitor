#pragma once

#include <map>
#include <string>
#include <optional>

/**
 * @brief Manages configuration settings loaded from a file.
 *
 * Keys are stored in lower case to simplify lookups.  Values are kept
 * verbatim after trimming whitespace.
 */
class Configuration {
public:
    /// Map containing lower‑cased keys from the configuration file.
    std::map<std::wstring, std::wstring> settings;

    /**
     * @brief Load configuration from a file.
     *
     * If @p path is empty or @c std::nullopt, the previously loaded
     * path is reused. When no file has been loaded yet, the default
     * configuration file next to the executable is used.
     *
     * @param path Optional path to a configuration file.
     * @return void
     * @sideeffects Updates #settings and remembers the last path.
     */
    void load(std::optional<std::wstring> path = std::nullopt);

    /**
     * @brief Retrieve the path of the last loaded configuration file.
     * @return Absolute path of the last successfully loaded file.
     */
    std::wstring getLastPath() const;

private:
    /// Stores the path of the most recently loaded configuration file.
    std::wstring m_lastPath;
};

/// Global configuration instance shared across modules.
extern Configuration g_config;
