#pragma once

#include <string>

namespace helen
{
    /**
     * @brief Declares one config entry required by a split pack.
     */
    class ConfigEntryDefinition
    {
    public:
        /** @brief Stable config key persisted by the runtime config store. */
        std::string Key;
        /** @brief Declared config value type such as int or bool. */
        std::string Type;
        /** @brief Default integer value used before the runtime saves a user-specific override. */
        int DefaultValue{};
    };
}