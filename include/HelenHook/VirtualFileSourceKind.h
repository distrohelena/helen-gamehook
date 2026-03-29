#pragma once

#include <cstdint>

namespace helen
{
    /**
     * @brief Identifies the kind of source asset declared for one virtual file.
     */
    enum class VirtualFileSourceKind : std::uint32_t
    {
        /** @brief Replacement bytes are provided by a complete full-file asset. */
        FullFile = 0,

        /** @brief Replacement bytes are reconstructed from a delta container and its exact base and target metadata. */
        DeltaFile = 1,
    };
}
