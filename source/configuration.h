#pragma once

#include <map>
#include <string>
#include <optional>
#include <vector>
#include <istream>
#include <mutex>

/**
 * @brief Manages configuration settings loaded from a file.
 *
 * Keys are stored in lower case to simplify lookups.  Values are kept
 * verbatim after trimming whitespace.
 */
class Configuration {
public:
    /**
     * @brief Load configuration from a file.
     *
     * If @p path is empty or @c std::nullopt, the previously loaded
     * path is reused. When no file has been loaded yet, the default
     * configuration file next to the executable is used.
     *
     * @param path Optional path to a configuration file.
     * @return void
     * @sideeffects Updates internal settings and remembers the last path.
     */
    void load(std::optional<std::wstring> path = std::nullopt);

    /**
     * @brief Retrieve the path of the last loaded configuration file.
     * @return Absolute path of the last successfully loaded file.
     */
    std::wstring getLastPath() const;

    /**
     * @brief Retrieve the value associated with @p key.
     * @param key Lower-cased configuration key to look up.
     * @return Optional containing the value if the key exists.
     */
    std::optional<std::wstring> getSetting(const std::wstring& key) const;

    /**
     * @brief Set the value for @p key.
     * @param key Lower-cased configuration key.
     * @param value Value to store.
     */
    void setSetting(const std::wstring& key, const std::wstring& value);

private:
    /// Map containing lower-cased keys from the configuration file.
    std::map<std::wstring, std::wstring> settings;

    /// Stores the path of the most recently loaded configuration file.
    std::wstring m_lastPath;

    /// Mutex guarding access to #settings and #m_lastPath.
    mutable std::mutex m_mutex;
};

/// Global configuration instance shared across modules.
extern Configuration g_config;

/// Parse configuration from individual lines.
std::map<std::wstring, std::wstring> ParseConfigLines(const std::vector<std::wstring>& lines);

/// Parse configuration from a wide character stream.
std::map<std::wstring, std::wstring> ParseConfigStream(std::wistream& stream);
