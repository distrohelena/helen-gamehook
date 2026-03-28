#pragma once

#include <cstddef>
#include <string>

#include <HelenHook/HookBlobRelocationSourceDefinition.h>

namespace helen
{
    /**
     * @brief Declares one patch-site relocation that must be resolved inside a native hook blob asset.
     */
    class HookBlobRelocationDefinition
    {
    public:
        /** @brief Byte offset within the blob asset where the relocation should be written. */
        std::size_t Offset = 0;
        /** @brief Encoding applied when materializing the relocation value into the blob bytes. */
        std::string Encoding;
        /** @brief Source operand that provides the relocation value. */
        HookBlobRelocationSourceDefinition Source;
    };
}