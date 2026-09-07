#pragma once

#include <filesystem>
#include <string>

#include <HelenHook/FileReadPolicy.h>
#include <HelenHook/FileWritePolicy.h>

namespace helen
{
    /**
     * @brief Stores one pack-declared exact file route before its filesystem root is resolved.
     *
     * Root is restricted by the build-manifest parser to the supported `game` and `documents`
     * values. Path remains relative until runtime initialization supplies the corresponding
     * installation or known-folder root.
     */
    struct FileWriteRouteDefinition
    {
        /** @brief Stable identifier used to diagnose and merge one route declaration. */
        std::string Id;

        /** @brief Supported logical root name, either `game` or `documents`. */
        std::string Root;

        /** @brief Exact UTF-8 relative file path beneath Root. */
        std::filesystem::path Path;

        /** @brief Policy applied to write-capable opens and file mutations. */
        FileWritePolicy WritePolicy = FileWritePolicy::Deny;

        /** @brief Source selected for later game reads of the route. */
        FileReadPolicy ReadPolicy = FileReadPolicy::Original;
    };
}
