#pragma once

#include <filesystem>

#include <HelenHook/BuildDefinition.h>
#include <HelenHook/PackDefinition.h>

namespace helen
{
    /**
     * @brief Couples a resolved pack/build declaration pair with the directories it was loaded from.
     */
    class LoadedBuildPack
    {
    public:
        /** @brief Directory that contains the top-level pack metadata and shared assets. */
        std::filesystem::path PackDirectory;
        /** @brief Directory that contains the selected build-specific declaration files. */
        std::filesystem::path BuildDirectory;
        /** @brief Parsed top-level pack metadata and shared declarations. */
        PackDefinition Pack;
        /** @brief Parsed build-specific declarations that matched the requested executable fingerprint. */
        BuildDefinition Build;
    };
}