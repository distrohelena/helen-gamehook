#pragma once

#include <cstdint>
#include <string>

namespace helen
{
    /**
     * @brief Describes the exact size and SHA-256 digest for one file version referenced by a virtual-file source.
     *
     * The parser uses this structure for both the installed base asset and the reconstructed target asset of a
     * delta-backed virtual file. The file size is tracked as an exact unsigned byte count, and the digest preserves
     * the declared hexadecimal SHA-256 text without inventing normalization rules here.
     */
    class VirtualFileHashDefinition
    {
    public:
        /** @brief Exact file size in bytes for the referenced asset. */
        std::uintmax_t FileSize = 0;

        /** @brief Declared SHA-256 digest for the referenced asset. */
        std::string Sha256;
    };
}
