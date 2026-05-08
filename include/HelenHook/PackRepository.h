#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <HelenHook/LoadedBuildPack.h>
#include <HelenHook/LoadedBuildPackSet.h>

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

        /**
         * @brief Loads one ordered set of explicitly enabled pack builds for the requested executable fingerprint.
         * @param packs_directory Root directory that contains one subdirectory per pack.
         * @param executable_name Executable file name reported by the host process.
         * @param executable_size Exact executable file size used for strict build matching.
         * @param executable_sha256 Lowercase SHA-256 hex digest used for strict build matching.
         * @param enabled_pack_ids Ordered enabled pack identifiers that must all resolve successfully.
         * @return A resolved ordered pack set when every requested pack id matches the executable fingerprint; otherwise no value.
         */
        std::optional<LoadedBuildPackSet> LoadPackSetForExecutable(
            const std::filesystem::path& packs_directory,
            const std::string& executable_name,
            std::uintmax_t executable_size,
            const std::string& executable_sha256,
            const std::vector<std::string>& enabled_pack_ids) const;
    };
}
