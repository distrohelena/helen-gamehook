#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <HelenHook/HgdeltaChunkDefinition.h>

namespace helen
{
    /**
     * @brief Captures one parsed `.hgdelta` container and its validated header metadata.
     *
     * The loader keeps the parsed header values, chunk table, and payload bytes in memory so later
     * runtime layers can serve positional reads without re-reading the container from disk.
     */
    class HgdeltaFile
    {
    public:
        /** @brief Nominal chunk size used by the delta container. */
        std::uint32_t ChunkSize = 0;

        /** @brief Exact base file size recorded in the delta header. */
        std::uint64_t BaseFileSize = 0;

        /** @brief Exact reconstructed target file size recorded in the delta header. */
        std::uint64_t TargetFileSize = 0;

        /** @brief Lowercase SHA-256 hex digest of the expected base file. */
        std::string BaseSha256;

        /** @brief Lowercase SHA-256 hex digest of the reconstructed target file. */
        std::string TargetSha256;

        /** @brief Positional chunk table describing how the target file is rebuilt. */
        std::vector<HgdeltaChunkDefinition> Chunks;

        /** @brief Raw payload bytes referenced by delta-byte chunks. */
        std::vector<std::uint8_t> PayloadBytes;

        /**
         * @brief Loads one `.hgdelta` container from disk and validates its fixed binary layout.
         * @param file_path Absolute or relative path of the delta container to parse.
         * @return Fully parsed delta model including header data, chunk table, and payload bytes.
         * @throws std::invalid_argument Thrown when the path is empty or not a regular file.
         * @throws std::runtime_error Thrown when the file cannot be read or the header is malformed.
         */
        static HgdeltaFile Load(const std::filesystem::path& file_path);
    };
}
