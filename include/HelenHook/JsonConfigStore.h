#pragma once

#include <filesystem>
#include <cstddef>
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
         * @brief Restores an already-present integer entry without allocating or throwing.
         * @param key Existing flat config key whose value should be restored.
         * @param value Integer value assigned to the existing entry.
         * @return True when the key existed and was restored; otherwise false.
         */
        bool TrySetExistingInt(const std::string& key, int value) noexcept;

        /**
         * @brief Persists the current integer config map to the JSON file path owned by this store.
         */
        void Save() const;

        /**
         * @brief Returns how many persistence attempts this store has made.
         * @return Number of calls to Save, including attempts that failed before replacing the target file.
         */
        std::size_t GetSaveCount() const noexcept;

    private:
        /**
         * @brief Loads the existing JSON file when it is present.
         */
        void Load();

        /** @brief Filesystem path where the Helen-owned JSON document is stored. */
        std::filesystem::path path_;
        /** @brief Flat map of integer config values keyed by their string identifiers. */
        std::map<std::string, int> int_values_;
        /** @brief Number of Save calls made by this store, used to make transaction cardinality observable. */
        mutable std::size_t save_count_{};
    };
}
