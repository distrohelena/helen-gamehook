#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace helen
{
    /**
     * @brief Captures the current live debug state for one memory observer.
     */
    class MemoryStateObserverDebugView
    {
    public:
        /** @brief Stable observer identifier. */
        std::string Id;
        /** @brief Cached base address of the last validated state block, or zero when unresolved. */
        std::uintptr_t CachedAddress = 0;
        /** @brief Number of full scan passes performed while resolving or re-resolving the observer. */
        std::uint64_t RescanCount = 0;
        /** @brief Number of mapped updates emitted by the observer. */
        std::uint64_t UpdateCount = 0;
        /** @brief Last raw integer value accepted by the observer, when one has been seen. */
        std::optional<int> LastRawValue;
        /** @brief Last mapped integer value emitted by the observer, when one has been seen. */
        std::optional<int> LastMappedValue;
    };
}