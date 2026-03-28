#pragma once

#include <string>
#include <vector>

#include <HelenHook/CommandStepDefinition.h>

namespace helen
{
    /**
     * @brief Declares one named command workflow composed from constrained runtime steps.
     */
    class CommandDefinition
    {
    public:
        /** @brief Stable command identifier referenced by hooks and UI actions. */
        std::string Id;
        /** @brief Human-readable command name shown in tooling and diagnostics. */
        std::string Name;
        /** @brief Ordered step sequence executed when the command runs. */
        std::vector<CommandStepDefinition> Steps;
    };
}