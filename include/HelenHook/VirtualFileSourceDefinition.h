#pragma once

#include <cstdint>
#include <filesystem>

#include <HelenHook/VirtualFileHashDefinition.h>
#include <HelenHook/VirtualFileSourceKind.h>

namespace helen
{
    /**
     * @brief Declares the asset metadata used to satisfy one virtual file request.
     *
     * Full-file sources only need a pack-relative asset path. Delta-backed sources also carry exact base and target
     * file metadata plus the chunk size declared by the delta container so pack parsing can preserve the source model
     * without changing serving behavior yet.
     */
    class VirtualFileSourceDefinition
    {
    public:
        /** @brief Source kind that tells the runtime whether this is a full-file or delta-backed asset. */
        VirtualFileSourceKind Kind = VirtualFileSourceKind::FullFile;

        /** @brief Pack-relative source asset path that provides the replacement bytes or delta container. */
        std::filesystem::path Path;

        /** @brief Exact base-file metadata required by delta-backed sources. */
        VirtualFileHashDefinition Base;

        /** @brief Exact target-file metadata required by delta-backed sources. */
        VirtualFileHashDefinition Target;

        /** @brief Declared delta chunk size in bytes for delta-backed sources. */
        std::uint32_t ChunkSize = 0;
    };
}
