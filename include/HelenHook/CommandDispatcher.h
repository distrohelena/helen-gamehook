#pragma once

#include <map>
#include <optional>
#include <string>

namespace helen
{
    class JsonConfigStore;

    /**
     * @brief Tracks registered integer config keys and optionally persists them through a JSON config store.
     *
     * The dispatcher owns only typed key registration and lookup for now. Command parsing remains outside
     * this scaffold until later tasks add concrete command definitions.
     */
    class CommandDispatcher
    {
    public:
        /**
         * @brief Creates an in-memory dispatcher without persistent backing.
         */
        CommandDispatcher();

        /**
         * @brief Creates a dispatcher that mirrors registered integer values into a JSON config store.
         * @param config_store Store used to load defaults and persist later updates.
         */
        explicit CommandDispatcher(JsonConfigStore& config_store);

        /**
         * @brief Registers one integer config key and initializes its current value.
         * @param key Flat config key exposed through the typed command surface.
         * @param default_value Default value used when no persisted value exists yet.
         */
        void RegisterConfigInt(const std::string& key, int default_value);

        /**
         * @brief Updates a registered integer config key.
         * @param key Registered config key that should be updated.
         * @param value New integer value for the key.
         * @return True when the key exists and was updated; otherwise false.
         */
        bool TrySetInt(const std::string& key, int value);

        /**
         * @brief Returns the current value for a registered integer config key.
         * @param key Registered config key to query.
         * @return Stored value when the key exists; otherwise no value.
         */
        std::optional<int> TryGetInt(const std::string& key) const;

    private:
        /** @brief Optional JSON config store used for persisted integer values. */
        JsonConfigStore* config_store_{};
        /** @brief Registered integer config values keyed by their exported string names. */
        std::map<std::string, int> int_values_;
    };
}