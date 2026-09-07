#pragma once

#include <filesystem>
#include <string>

#include <HelenHook/FileReadPolicy.h>
#include <HelenHook/FileWritePolicy.h>

namespace helen {
    /**
     * @brief Describes one exact original file and the read/write policy applied to it for a session.
     *
     * The path is resolved and validated by FileWriteRoutingService::Initialize. Route declarations are exact files;
     * callers must not use a directory, wildcard, alternate stream, or path that escapes the request base directory.
     */
    struct FileWriteRoute {
        /** @brief Stable pack-provided identifier used for diagnostics and route uniqueness. */
        std::string Id;

        /** @brief Original file path that must exist as one unambiguous regular file at initialization. */
        std::filesystem::path OriginalPath;

        /** @brief Policy controlling whether write-capable opens are denied or redirected. */
        FileWritePolicy WritePolicy = FileWritePolicy::Deny;

        /** @brief Policy controlling whether read opens use the original or session copy. */
        FileReadPolicy ReadPolicy = FileReadPolicy::Original;
    };
}
