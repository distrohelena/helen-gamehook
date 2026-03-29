#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace helen
{
    /**
     * @brief Captures the portable identity of any file by combining its exact size and SHA-256 digest.
     *
     * The fingerprint is content-only and does not encode the file name. Callers can use it for
     * strict matching, build provenance checks, or any other workflow that needs a reusable file
     * hash without assuming executable-specific metadata.
     */
    class FileFingerprint
    {
    public:
        /** @brief Exact file size in bytes measured from the on-disk file contents. */
        std::uintmax_t FileSize = 0;

        /** @brief Lowercase SHA-256 hex digest of the complete file contents. */
        std::string Sha256;

        /**
         * @brief Builds one file fingerprint from the file at the supplied path.
         * @param file_path Absolute or relative file path that should be fingerprinted.
         * @return Fully populated fingerprint containing the file size and SHA-256 digest.
         * @throws std::invalid_argument Thrown when the path is empty or not a regular file.
         * @throws std::runtime_error Thrown when the file cannot be opened, sized, or hashed completely.
         */
        static FileFingerprint FromPath(const std::filesystem::path& file_path);
    };
}
