#pragma once

#include <string>

namespace helen
{
    /**
     * @brief Declares one live runtime value slot that native hook blobs may read or write.
     */
    class RuntimeSlotDefinition
    {
    public:
        /** @brief Stable slot identifier used by relocations and runtime tooling. */
        std::string Id;
        /** @brief Declarative slot storage type. Task 1 currently supports only float32. */
        std::string Type;
        /** @brief Initial numeric value assigned before any runtime command mutates the slot. */
        double InitialValue = 0.0;
    };
}