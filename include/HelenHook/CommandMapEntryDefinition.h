#pragma once

namespace helen
{
    /**
     * @brief Declares one integer-to-double mapping candidate used by a declarative command step.
     *
     * Mapping entries are evaluated in declaration order, and command execution succeeds only when
     * one entry matches the input integer exactly.
     */
    class CommandMapEntryDefinition
    {
    public:
        /** @brief Integer input value that must match for this mapping entry to apply. */
        int Match{};
        /** @brief Double output value produced when Match equals the step input integer. */
        double Value{};
    };
}