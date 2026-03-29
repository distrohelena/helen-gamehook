#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace helen
{
    /**
     * @brief Captures the executable-specific identity used to match one build-specific pack variant.
     *
     * The executable fingerprint layers the file name over the reusable generic file fingerprint so
     * content hashing stays centralized while build matching still preserves the exact binary name.
     *
     * The runtime uses the executable file name, exact file size, and lowercase SHA-256 digest so
     * build activation stays deterministic across developer machines and end-user installs.
     */
    class ExecutableFingerprint
    {
    public:
        /** @brief Executable leaf file name extracted from the fingerprinted path. */
        std::string FileName;

        /** @brief Exact file size in bytes carried over from the generic file fingerprint. */
        std::uintmax_t FileSize = 0;

        /** @brief Lowercase SHA-256 hex digest of the full file contents carried over from the generic file fingerprint. */
        std::string Sha256;

        /**
         * @brief Builds one executable fingerprint from the file at the supplied path.
         *
         * The method reuses the generic file fingerprint path for the size and hash, then layers on
         * the executable leaf file name so callers keep the existing pack-matching contract.
         * @param file_path Absolute or relative file path that should be fingerprinted.
         * @return Fully populated fingerprint containing the file name, byte size, and SHA-256 digest.
         * @throws std::invalid_argument Thrown when the path is empty or not a regular file.
         * @throws std::runtime_error Thrown when the file cannot be opened or hashed completely.
         */
        static ExecutableFingerprint FromPath(const std::filesystem::path& file_path);
    };
}
