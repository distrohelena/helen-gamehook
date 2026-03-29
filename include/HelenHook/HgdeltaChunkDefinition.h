#pragma once

#include <cstdint>

#include <HelenHook/HgdeltaChunkKind.h>

namespace helen
{
    /**
     * @brief Describes one reconstructed target chunk inside a parsed `.hgdelta` file.
     *
     * The runtime uses the target offset and size to map a positional chunk back to the source
     * material. Base-copy chunks read from the installed base file, while delta-byte chunks read
     * from the payload block stored in the delta container.
     */
    class HgdeltaChunkDefinition
    {
    public:
        /** @brief Chunk materialization strategy used by the runtime. */
        HgdeltaChunkKind Kind = HgdeltaChunkKind::BaseCopy;

        /** @brief Byte offset of this chunk within the reconstructed target file. */
        std::uint64_t TargetOffset = 0;

        /** @brief Exact byte length of this chunk in the reconstructed target file. */
        std::uint32_t TargetSize = 0;

        /** @brief Byte offset of this chunk within the delta payload block. */
        std::uint64_t PayloadOffset = 0;

        /** @brief Exact byte length of this chunk payload within the delta payload block. */
        std::uint32_t PayloadSize = 0;
    };
}
