#pragma once

#include <optional>
#include <string>

namespace helen
{
    /**
     * @brief Describes one observed state change that should be applied to Helen config and optional follow-up commands.
     */
    class MemoryStateObserverUpdate
    {
    public:
        /** @brief Stable observer identifier that detected the change. */
        std::string ObserverId;
        /** @brief Config key that should receive the mapped value. */
        std::string ConfigKey;
        /** @brief Raw integer value observed in the validated memory state block. */
        int RawValue = 0;
        /** @brief Integer value produced by the observer's declarative mapping table. */
        int MappedValue = 0;
        /** @brief Optional follow-up command that should run after the config update succeeds. */
        std::optional<std::string> CommandId;
    };
}