#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

#include <windows.h>

#include <HelenHook/FileFingerprint.h>
#include <HelenHook/HgdeltaFile.h>
#include <HelenHook/PackAssetResolver.h>
#include <HelenHook/VirtualFileDefinition.h>
#include <HelenHook/VirtualFileSource.h>

namespace helen
{
    /**
     * @brief Reconstructs one virtual file by mixing bytes from the installed base file and a parsed hgdelta container.
     *
     * The source validates the exact base fingerprint up front, keeps the parsed delta container in memory, and serves
     * offset-based reads without rebuilding the whole target file.
     */
    class DeltaVirtualFileSource final : public VirtualFileSource
    {
    public:
        /** @brief Exact fingerprint measured from the installed base file used by this source. */
        FileFingerprint BaseFingerprint;

        /** @brief Exact fingerprint measured from the declared hgdelta asset used to key resolved cache files. */
        FileFingerprint DeltaFingerprint;

        /** @brief Parsed hgdelta container that describes how to reconstruct the target file. */
        HgdeltaFile Delta;

        /** @brief Absolute path to the installed base file read by this source. */
        std::filesystem::path BaseFilePath;

        /** @brief Absolute path to the declared hgdelta asset used to reconstruct the target file. */
        std::filesystem::path DeltaFilePath;

        /** @brief Absolute cache root beneath helengamehook where resolved delta targets are materialized. */
        std::filesystem::path CacheDirectory;

        /** @brief Absolute resolved-cache file path derived from the pack/build/file identity and exact fingerprints. */
        std::filesystem::path ResolvedFilePath;

        /**
         * @brief Creates one delta-backed source from the supplied pack metadata and opened base file path.
         * @param asset_resolver Resolver used to locate the declared hgdelta container.
         * @param cache_directory Writable helengamehook cache directory used for resolved file materialization.
         * @param base_file_path Actual installed base file path opened by the game.
         * @param definition Delta-backed virtual-file definition that declares the expected metadata.
         * @throws std::invalid_argument Thrown when the definition is not delta-backed, the cache directory is invalid, or the base file path is invalid.
         * @throws std::runtime_error Thrown when the delta container or installed base file does not match the declared metadata.
         */
        DeltaVirtualFileSource(
            const PackAssetResolver& asset_resolver,
            const std::filesystem::path& cache_directory,
            const std::filesystem::path& base_file_path,
            const VirtualFileDefinition& definition);

        /**
         * @brief Returns the exact reconstructed target size declared by the loaded hgdelta container.
         * @return Target payload size in bytes.
         */
        std::uint64_t GetSize() const override;

        /**
         * @brief Reconstructs bytes from the base file and delta payload starting at one absolute target offset.
         * @param offset Zero-based target offset where the read should begin.
         * @param buffer Destination buffer that receives reconstructed bytes.
         * @param bytes_to_read Maximum byte count requested by the caller.
         * @param bytes_read Receives the actual number of bytes copied into buffer.
         * @return True when the read completed successfully; otherwise false.
         */
        bool Read(
            std::uint64_t offset,
            void* buffer,
            std::size_t bytes_to_read,
            std::size_t& bytes_read) override;

        /**
         * @brief Materializes a validated reconstructed target into the resolved cache and maps that file through Win32 APIs.
         * @param protection Requested mapping protection flags from CreateFileMapping.
         * @param maximum_size_high High-order requested mapping size.
         * @param maximum_size_low Low-order requested mapping size.
         * @return Real file-mapping handle when the resolved cache file exists or was materialized successfully; otherwise no value.
         */
        std::optional<HANDLE> CreateFileMapping(
            DWORD protection,
            DWORD maximum_size_high,
            DWORD maximum_size_low) override;
    };
}
