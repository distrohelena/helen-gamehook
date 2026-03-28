#pragma once

#include <cstddef>
#include <filesystem>
#include <vector>

#include <HelenHook/HookBlobRelocationDefinition.h>

namespace helen
{
    /**
     * @brief Declares the native blob asset injected by a blob-backed hook action.
     */
    class HookBlobDefinition
    {
    public:
        /** @brief Pack-relative asset path that contains the native blob bytes. */
        std::filesystem::path AssetPath;
        /** @brief Entry byte offset within the asset where execution should begin. */
        std::size_t EntryOffset = 0;
        /** @brief Relocation writes that must be applied before the blob can execute. */
        std::vector<HookBlobRelocationDefinition> Relocations;
    };
}