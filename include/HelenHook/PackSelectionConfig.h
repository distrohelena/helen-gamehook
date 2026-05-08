#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace helen
{
    /**
     * @brief Loads the explicit enabled-pack selection used to choose multiple packs per executable.
     *
     * The config file is Helen-owned JSON and preserves the declared pack order for each executable.
     * Malformed JSON, duplicate pack ids within one executable entry, and invalid structural types are
     * treated as hard failures during construction.
     */
    class PackSelectionConfig
    {
    public:
        /**
         * @brief Opens and validates one JSON-backed pack-selection config file.
         * @param path Filesystem path to `packs.json`.
         */
        explicit PackSelectionConfig(std::filesystem::path path);

        /**
         * @brief Returns the explicitly enabled pack ids for one executable when configured.
         * @param executable_name Executable file name that should be looked up.
         * @return Ordered enabled-pack list when the executable has an explicit entry; otherwise no value.
         */
        std::optional<std::vector<std::string>> TryGetEnabledPacks(const std::string& executable_name) const;

    private:
        /**
         * @brief Loads and validates the existing JSON file when it is present.
         */
        void Load();

        /** @brief Filesystem path to the Helen-owned `packs.json` file. */
        std::filesystem::path path_;

        /** @brief Ordered enabled-pack ids keyed by executable file name. */
        std::map<std::string, std::vector<std::string>> enabled_packs_by_executable_;
    };
}
