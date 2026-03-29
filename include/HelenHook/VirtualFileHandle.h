#pragma once

#include <cstdint>
#include <memory>

namespace helen
{
    class VirtualFileSource;
}

namespace helen
{
    /**
     * @brief Stores one opened virtual-file source instance and its current read cursor.
     *
     * The runtime keeps one handle instance per opened synthetic file so each caller gets an independent read position
     * over the same shared source object.
     */
    class VirtualFileHandle
    {
    public:
        /** @brief Shared source object that serves reads and mappings for this opened virtual handle. */
        std::shared_ptr<VirtualFileSource> Source;

        /** @brief Zero-based read cursor within Source. */
        std::uint64_t ReadPosition{ 0 };
    };
}
