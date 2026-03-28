#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <HelenHook/MemoryStateObserverCheckDefinition.h>
#include <HelenHook/MemoryStateObserverMapEntryDefinition.h>

namespace helen
{
    /**
     * @brief Declares one bounded in-process memory observer that watches for a validated state block and maps its value into Helen config.
     */
    class MemoryStateObserverDefinition
    {
    public:
        /** @brief Stable observer identifier used by diagnostics and debug output. */
        std::string Id;
        /** @brief Inclusive start address of the bounded scan range. */
        std::uintptr_t ScanStartAddress = 0;
        /** @brief Exclusive end address of the bounded scan range. */
        std::uintptr_t ScanEndAddress = 0;
        /** @brief Byte stride applied when scanning for a candidate state block. */
        std::size_t ScanStride = 0;
        /** @brief Signed byte offset, relative to each candidate base address, that contains the observed raw integer value. */
        int ValueOffset = 0;
        /** @brief Poll interval used by the background observer thread when it is active. */
        int PollIntervalMs = 0;
        /** @brief Config key that receives the mapped observer value after a change is detected. */
        std::string TargetConfigKey;
        /** @brief Optional follow-up command executed after the config key is updated. */
        std::optional<std::string> CommandId;
        /** @brief Validation checks that a candidate state block must satisfy before it is accepted. */
        std::vector<MemoryStateObserverCheckDefinition> Checks;
        /** @brief Raw-to-config mappings that translate observed state codes into Helen-owned config values. */
        std::vector<MemoryStateObserverMapEntryDefinition> Mappings;
    };
}