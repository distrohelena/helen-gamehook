#pragma once

namespace helen
{
    /**
     * @brief Maps one observed raw integer value to the persisted integer value written by the runtime.
     */
    class MemoryStateObserverMapEntryDefinition
    {
    public:
        /** @brief Raw integer value read from the observed memory block. */
        int Match = 0;
        /** @brief Integer value written to Helen config when Match is observed. */
        int Value = 0;
    };
}