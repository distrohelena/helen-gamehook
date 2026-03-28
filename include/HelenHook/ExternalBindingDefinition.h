#pragma once

#include <string>

namespace helen
{
    /**
     * @brief Declares one typed callback binding exposed to patched external UI code.
     *
     * External bindings keep gameplay assets declarative by mapping stable callback names such as
     * `Helen_GetInt` or `Helen_RunCommand` onto validated runtime operations instead of arbitrary
     * native code execution.
     */
    class ExternalBindingDefinition
    {
    public:
        /** @brief Stable binding identifier used by diagnostics and tooling. */
        std::string Id;

        /** @brief External callback name invoked by the patched asset. */
        std::string ExternalName;

        /** @brief Binding mode such as `get-int`, `set-int`, or `run-command`. */
        std::string Mode;

        /** @brief Config key resolved by `get-int` and `set-int` bindings. */
        std::string ConfigKey;

        /** @brief Command identifier executed by `run-command` bindings and optional `set-int` follow-up actions. */
        std::string CommandId;
    };
}
