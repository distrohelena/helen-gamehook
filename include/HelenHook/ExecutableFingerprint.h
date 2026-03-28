#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace helen
{
    /**
     * @brief Captures the strict file identity used to match one build-specific pack variant.
     *
     * The runtime uses the executable file name, exact file size, and lowercase SHA-256 digest so
     * build activation stays deterministic across developer machines and end-user installs.
     */
    class ExecutableFingerprint
    {
    public:
        /** @brief Executable leaf file name extracted from the fingerprinted path. */
        std::string FileName;

        /** @brief Exact file size in bytes. */
        std::uintmax_t FileSize = 0;

        /** @brief Lowercase SHA-256 hex digest of the full file contents. */
        std::string Sha256;

        /**
         * @brief Builds one executable fingerprint from the file at the supplied path.
         * @param file_path Absolute or relative file path that should be fingerprinted.
         * @return Fully populated fingerprint containing the file name, byte size, and SHA-256 digest.
         * @throws std::invalid_argument Thrown when the path is empty or not a regular file.
         * @throws std::runtime_error Thrown when the file cannot be opened or hashed completely.
         */
        static ExecutableFingerprint FromPath(const std::filesystem::path& file_path);
    };
}
