#pragma once

#include <optional>
#include <string>

namespace helen
{
    /**
     * @brief Declares one validation rule applied to a candidate observed memory state block.
     */
    class MemoryStateObserverCheckDefinition
    {
    public:
        /** @brief Comparison mode used by the check such as `equals-constant` or `equals-value-at-offset`. */
        std::string Comparison;
        /** @brief Signed byte offset, relative to the observer base address, whose value participates in the comparison. */
        int Offset = 0;
        /** @brief Constant integer value required by `equals-constant` comparisons. */
        std::optional<int> ExpectedValue;
        /** @brief Relative byte offset whose integer value must match when `equals-value-at-offset` is used. */
        std::optional<int> CompareOffset;
    };
}