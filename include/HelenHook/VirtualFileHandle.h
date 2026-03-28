#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace helen
{
    /**
     * @brief Stores one RAM-backed replacement payload and its current read cursor.
     *
     * The runtime keeps one handle instance per opened synthetic file so each caller gets an
     * independent read position over the same replacement bytes.
     */
    class VirtualFileHandle
    {
    public:
        /** @brief Shared immutable replacement bytes copied from the declared pack asset. */
        std::shared_ptr<const std::vector<std::uint8_t>> ReplacementBytes;

        /** @brief Zero-based read cursor within ReplacementBytes. */
        std::uint64_t ReadPosition{ 0 };
    };
}