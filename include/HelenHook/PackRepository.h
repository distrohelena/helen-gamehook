#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include <HelenHook/LoadedBuildPack.h>

namespace helen
{
    /**
     * @brief Discovers split packs on disk and resolves the first build that matches an executable fingerprint.
     */
    class PackRepository
    {
    public:
        /**
         * @brief Loads the first pack build whose executable name and fingerprint match the requested executable.
         * @param packs_directory Root directory that contains one subdirectory per pack.
         * @param executable_name Executable file name reported by the host process.
         * @param executable_size Exact executable file size used for strict build matching.
         * @param executable_sha256 Lowercase SHA-256 hex digest used for strict build matching.
         * @return A resolved loaded pack when one build matches; otherwise no value.
         */
        std::optional<LoadedBuildPack> LoadForExecutable(
            const std::filesystem::path& packs_directory,
            const std::string& executable_name,
            std::uintmax_t executable_size,
            const std::string& executable_sha256) const;
    };
}