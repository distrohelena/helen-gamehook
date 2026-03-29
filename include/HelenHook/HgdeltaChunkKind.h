#pragma once

#include <cstdint>

namespace helen
{
    /**
     * @brief Describes how one `.hgdelta` chunk should be materialized at runtime.
     */
    enum class HgdeltaChunkKind : std::uint32_t
    {
        /** @brief Copy bytes from the base file at the same target offset. */
        BaseCopy = 0,

        /** @brief Read bytes directly from the delta payload. */
        DeltaBytes = 1,
    };
}
