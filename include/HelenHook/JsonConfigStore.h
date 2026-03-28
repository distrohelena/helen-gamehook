#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace helen
{
    /**
     * @brief Stores Helen-owned integer configuration values in a flat JSON object on disk.
     *
     * The store loads an existing file during construction and rejects malformed JSON,
     * non-object roots, and non-integer values rather than silently repairing them.
     */
    class JsonConfigStore
    {
    public:
        /**
         * @brief Opens a JSON-backed config store rooted at the given filesystem path.
         * @param path Absolute or relative path to the Helen-owned JSON file.
         */
        explicit JsonConfigStore(std::filesystem::path path);

        /**
         * @brief Returns the integer value for a key or the provided default when the key is missing.
         * @param key Flat config key stored in the JSON object.
         * @param default_value Value returned when the key is not present.
         * @return Stored integer value or the caller-provided default.
         */
        int GetInt(const std::string& key, int default_value) const;

        /**
         * @brief Updates or inserts one integer config entry in memory.
         * @param key Flat config key stored in the JSON object.
         * @param value Integer value that should be written for the key.
         */
        void SetInt(const std::string& key, int value);

        /**
         * @brief Persists the current integer config map to the JSON file path owned by this store.
         */
        void Save() const;

    private:
        /**
         * @brief Loads the existing JSON file when it is present.
         */
        void Load();

        /** @brief Filesystem path where the Helen-owned JSON document is stored. */
        std::filesystem::path path_;
        /** @brief Flat map of integer config values keyed by their string identifiers. */
        std::map<std::string, int> int_values_;
    };
}