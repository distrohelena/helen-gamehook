#include <HelenHook/DeltaVirtualFileSource.h>

#include <HelenHook/HgdeltaChunkKind.h>

#include <windows.h>
#include <wincrypt.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>

#pragma comment(lib, "advapi32.lib")

namespace
{
    /** @brief Hexadecimal digits used to materialize lowercase digest text. */
    constexpr std::string_view LowerHexDigits = "0123456789abcdef";

    /** @brief Serializes resolved-cache materialization so two callers do not race on the same cache path. */
    std::mutex ResolvedCacheMaterializationMutex;

    /**
     * @brief Converts ASCII text into lowercase so exact SHA-256 comparisons remain case-insensitive for manifests.
     * @param text ASCII text that should be normalized.
     * @return Lowercase copy of the supplied text.
     */
    std::string ToLowerAscii(std::string text)
    {
        for (char& character : text)
        {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }

        return text;
    }

    /**
     * @brief Returns whether the supplied file fingerprint matches the declared exact size and SHA-256 metadata.
     * @param fingerprint Measured file fingerprint.
     * @param expected Expected exact file metadata.
     * @return True when both size and SHA-256 digest match exactly; otherwise false.
     */
    bool MatchesFingerprint(const helen::FileFingerprint& fingerprint, const helen::VirtualFileHashDefinition& expected)
    {
        return fingerprint.FileSize == expected.FileSize && fingerprint.Sha256 == ToLowerAscii(expected.Sha256);
    }

    /**
     * @brief Returns whether the supplied fingerprint matches the declared hgdelta target fingerprint exactly.
     * @param fingerprint Measured fingerprint of one resolved target file.
     * @param delta Parsed hgdelta container whose target fingerprint should match the resolved file.
     * @return True when both size and SHA-256 digest match exactly; otherwise false.
     */
    bool MatchesTargetFingerprint(const helen::FileFingerprint& fingerprint, const helen::HgdeltaFile& delta)
    {
        return fingerprint.FileSize == delta.TargetFileSize && fingerprint.Sha256 == delta.TargetSha256;
    }

    /**
     * @brief Returns whether one Win32 file-mapping protection mode requires write access to the backing file handle.
     * @param protection Requested mapping protection flags from CreateFileMapping.
     * @return True when the mapping may write through the returned handle; otherwise false.
     */
    bool RequiresWritableBackingHandle(DWORD protection)
    {
        const DWORD base_protection = protection & 0xFF;
        if (base_protection == PAGE_READWRITE ||
            base_protection == PAGE_WRITECOPY ||
            base_protection == PAGE_EXECUTE_READWRITE ||
            base_protection == PAGE_EXECUTE_WRITECOPY)
        {
            return true;
        }

        return false;
    }

    /**
     * @brief Opens one transient Win32 cryptographic provider for SHA-256 hashing.
     * @param provider Receives the opened provider handle on success.
     * @return True when the provider opens successfully; otherwise false.
     */
    bool TryOpenHashProvider(HCRYPTPROV& provider)
    {
        provider = 0;
        return CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) == TRUE;
    }

    /**
     * @brief Opens one SHA-256 hash object from the supplied provider.
     * @param provider Open cryptographic provider handle.
     * @param hash Receives the opened hash handle on success.
     * @return True when the hash object opens successfully; otherwise false.
     */
    bool TryOpenSha256Hash(HCRYPTPROV provider, HCRYPTHASH& hash)
    {
        hash = 0;
        return CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash) == TRUE;
    }

    /**
     * @brief Finalizes one Win32 SHA-256 hash object and returns its lowercase hexadecimal digest.
     * @param hash Open hash handle whose current digest should be materialized.
     * @param digest Receives the lowercase hexadecimal digest when hashing succeeds.
     * @return True when the digest is produced successfully; otherwise false.
     */
    bool TryFinalizeHash(HCRYPTHASH hash, std::string& digest)
    {
        DWORD hash_length = 0;
        DWORD hash_length_size = sizeof(hash_length);
        if (CryptGetHashParam(hash, HP_HASHSIZE, reinterpret_cast<BYTE*>(&hash_length), &hash_length_size, 0) != TRUE)
        {
            return false;
        }

        std::vector<std::uint8_t> hash_bytes(hash_length, 0);
        DWORD requested_length = hash_length;
        if (CryptGetHashParam(hash, HP_HASHVAL, reinterpret_cast<BYTE*>(hash_bytes.data()), &requested_length, 0) != TRUE)
        {
            return false;
        }

        if (requested_length != hash_length)
        {
            return false;
        }

        digest.clear();
        digest.reserve(hash_bytes.size() * 2);
        for (const std::uint8_t byte : hash_bytes)
        {
            digest.push_back(LowerHexDigits[(byte >> 4) & 0x0F]);
            digest.push_back(LowerHexDigits[byte & 0x0F]);
        }

        return true;
    }

    /**
     * @brief Computes one lowercase SHA-256 digest over an in-memory identity payload.
     * @param text UTF-8 identity payload that should be hashed.
     * @param digest Receives the lowercase hexadecimal digest on success.
     * @return True when the identity payload is hashed successfully; otherwise false.
     */
    bool TryComputeSha256Text(std::string_view text, std::string& digest)
    {
        HCRYPTPROV provider = 0;
        if (!TryOpenHashProvider(provider))
        {
            return false;
        }

        HCRYPTHASH hash = 0;
        if (!TryOpenSha256Hash(provider, hash))
        {
            CryptReleaseContext(provider, 0);
            return false;
        }

        bool succeeded = CryptHashData(
            hash,
            reinterpret_cast<const BYTE*>(text.data()),
            static_cast<DWORD>(text.size()),
            0) == TRUE;
        if (succeeded)
        {
            succeeded = TryFinalizeHash(hash, digest);
        }

        CryptDestroyHash(hash);
        CryptReleaseContext(provider, 0);
        return succeeded;
    }

    /**
     * @brief Computes the stable cache-key digest for one pack/build/file identity triple.
     * @param pack_root Normalized top-level pack directory that owns the delta asset.
     * @param build_root Normalized active build directory that selected the virtual file.
     * @param file_id Stable virtual-file identifier declared by the pack.
     * @return Lowercase SHA-256 digest that keys the identity portion of the resolved cache path.
     */
    std::string ComputeIdentityDigest(
        const std::filesystem::path& pack_root,
        const std::filesystem::path& build_root,
        std::string_view file_id,
        const helen::FileFingerprint& base_fingerprint,
        const helen::FileFingerprint& delta_fingerprint)
    {
        const std::string identity_text =
            pack_root.generic_string() + "\n" +
            build_root.generic_string() + "\n" +
            std::string(file_id) + "\n" +
            base_fingerprint.Sha256 + "\n" +
            delta_fingerprint.Sha256;

        std::string digest;
        if (!TryComputeSha256Text(identity_text, digest))
        {
            throw std::runtime_error("Delta virtual file source failed to hash the resolved-cache identity.");
        }

        return digest;
    }

    /**
     * @brief Builds the resolved cache file path for one delta-backed virtual file.
     * @param cache_directory Writable helengamehook cache directory.
     * @param pack_root Normalized top-level pack directory that owns the delta asset.
     * @param build_root Normalized active build directory that selected the virtual file.
     * @param file_id Stable virtual-file identifier declared by the pack.
     * @param base_fingerprint Exact measured base-file fingerprint.
     * @param delta_fingerprint Exact measured hgdelta asset fingerprint.
     * @return Absolute resolved cache file path used for materialized delta mappings.
     */
    std::filesystem::path BuildResolvedFilePath(
        const std::filesystem::path& cache_directory,
        const std::filesystem::path& pack_root,
        const std::filesystem::path& build_root,
        const std::string& file_id,
        const helen::FileFingerprint& base_fingerprint,
        const helen::FileFingerprint& delta_fingerprint)
    {
        const std::string identity_digest = ComputeIdentityDigest(
            pack_root,
            build_root,
            file_id,
            base_fingerprint,
            delta_fingerprint);
        const std::string shard = identity_digest.substr(0, 2);
        return cache_directory / "resolved" / shard / (identity_digest + ".bin");
    }

    /**
     * @brief Ensures one directory exists before writing a resolved cache file beneath it.
     * @param directory_path Directory that should exist.
     * @return True when the directory exists or was created successfully; otherwise false.
     */
    bool EnsureDirectoryExists(const std::filesystem::path& directory_path)
    {
        std::error_code error_code;
        std::filesystem::create_directories(directory_path, error_code);
        if (error_code)
        {
            SetLastError(ERROR_PATH_NOT_FOUND);
        }

        return !error_code;
    }

    /**
     * @brief Builds the temporary file path used while materializing one resolved cache file.
     * @param resolved_path Final resolved cache file path that should be produced atomically.
     * @return Temporary sibling path used for staged output before the final rename.
     */
    std::filesystem::path BuildTemporaryResolvedPath(const std::filesystem::path& resolved_path)
    {
        std::filesystem::path temporary_path = resolved_path;
        temporary_path += L".tmp";
        return temporary_path;
    }

    /**
     * @brief Removes one file path when it exists and ignores missing-file cases.
     * @param file_path File path that should be removed.
     */
    void RemoveFileIfPresent(const std::filesystem::path& file_path)
    {
        std::error_code error_code;
        std::filesystem::remove(file_path, error_code);
    }

    /**
     * @brief Returns whether one resolved cache file already matches the declared target fingerprint exactly.
     * @param file_path Resolved cache file path that should be validated.
     * @param delta Parsed hgdelta container whose target fingerprint should match the resolved file.
     * @return True when the file exists and matches the exact declared target size and SHA-256; otherwise false.
     */
    bool IsResolvedFileValid(const std::filesystem::path& file_path, const helen::HgdeltaFile& delta)
    {
        std::error_code error_code;
        if (!std::filesystem::exists(file_path, error_code) || error_code)
        {
            return false;
        }

        try
        {
            const helen::FileFingerprint fingerprint = helen::FileFingerprint::FromPath(file_path);
            return MatchesTargetFingerprint(fingerprint, delta);
        }
        catch (...)
        {
            SetLastError(ERROR_INVALID_DATA);
            return false;
        }
    }

    /**
     * @brief Reads one contiguous byte range from the supplied base file stream.
     * @param stream Open base file stream positioned arbitrarily.
     * @param offset Zero-based byte offset where the read should begin.
     * @param buffer Destination buffer that receives the requested bytes.
     * @param byte_count Exact number of bytes that must be read.
     * @return True when the requested bytes were read completely; otherwise false.
     */
    bool ReadBaseBytes(std::ifstream& stream, std::uint64_t offset, void* buffer, std::size_t byte_count)
    {
        if (byte_count == 0)
        {
            return true;
        }

        if (offset > static_cast<std::uint64_t>((std::numeric_limits<std::streamoff>::max)()))
        {
            return false;
        }

        stream.clear();
        stream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        if (!stream)
        {
            return false;
        }

        stream.read(static_cast<char*>(buffer), static_cast<std::streamsize>(byte_count));
        if (stream.gcount() != static_cast<std::streamsize>(byte_count))
        {
            return false;
        }

        return !stream.bad();
    }

    /**
     * @brief Writes one contiguous byte range from the supplied base file stream into a resolved target stream.
     * @param stream Open base file stream positioned arbitrarily.
     * @param offset Zero-based byte offset where copying should begin.
     * @param output Open resolved target stream that receives copied bytes.
     * @param byte_count Exact number of bytes that must be written.
     * @return True when the requested bytes were copied completely; otherwise false.
     */
    bool WriteBaseBytesToStream(std::ifstream& stream, std::uint64_t offset, std::ofstream& output, std::uint64_t byte_count)
    {
        if (byte_count == 0)
        {
            return true;
        }

        if (offset > static_cast<std::uint64_t>((std::numeric_limits<std::streamoff>::max)()))
        {
            return false;
        }

        std::array<char, 65536> buffer{};
        std::uint64_t remaining = byte_count;
        std::uint64_t current_offset = offset;
        while (remaining > 0)
        {
            const std::size_t chunk_size = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, buffer.size()));
            stream.clear();
            stream.seekg(static_cast<std::streamoff>(current_offset), std::ios::beg);
            if (!stream)
            {
                return false;
            }

            stream.read(buffer.data(), static_cast<std::streamsize>(chunk_size));
            if (stream.gcount() != static_cast<std::streamsize>(chunk_size) || stream.bad())
            {
                return false;
            }

            output.write(buffer.data(), static_cast<std::streamsize>(chunk_size));
            if (!output)
            {
                return false;
            }

            current_offset += chunk_size;
            remaining -= chunk_size;
        }

        return true;
    }

    /**
     * @brief Materializes one resolved target file from the base file and hgdelta chunk table.
     * @param resolved_path Absolute target path that should receive the reconstructed file.
     * @param base_file_path Absolute installed base file path that supplies unchanged chunks.
     * @param delta Parsed hgdelta container that defines the reconstructed target bytes.
     * @return True when the target file was materialized and validated successfully; otherwise false.
     */
    bool MaterializeResolvedTarget(
        const std::filesystem::path& resolved_path,
        const std::filesystem::path& base_file_path,
        const helen::HgdeltaFile& delta)
    {
        if (!EnsureDirectoryExists(resolved_path.parent_path()))
        {
            return false;
        }

        std::ifstream base_stream(base_file_path, std::ios::binary);
        if (!base_stream)
        {
            SetLastError(ERROR_FILE_NOT_FOUND);
            return false;
        }

        const std::filesystem::path temporary_path = BuildTemporaryResolvedPath(resolved_path);
        RemoveFileIfPresent(temporary_path);
        std::ofstream output_stream(temporary_path, std::ios::binary | std::ios::trunc);
        if (!output_stream)
        {
            SetLastError(ERROR_OPEN_FAILED);
            return false;
        }

        for (const helen::HgdeltaChunkDefinition& chunk : delta.Chunks)
        {
            if (chunk.Kind == helen::HgdeltaChunkKind::BaseCopy)
            {
                if (!WriteBaseBytesToStream(base_stream, chunk.TargetOffset, output_stream, chunk.TargetSize))
                {
                    SetLastError(ERROR_READ_FAULT);
                    output_stream.close();
                    RemoveFileIfPresent(temporary_path);
                    return false;
                }
            }
            else if (chunk.Kind == helen::HgdeltaChunkKind::DeltaBytes)
            {
                if (chunk.PayloadOffset > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()) ||
                    chunk.PayloadOffset + chunk.PayloadSize > delta.PayloadBytes.size())
                {
                    SetLastError(ERROR_INVALID_DATA);
                    output_stream.close();
                    RemoveFileIfPresent(temporary_path);
                    return false;
                }

                output_stream.write(
                    reinterpret_cast<const char*>(delta.PayloadBytes.data() + static_cast<std::size_t>(chunk.PayloadOffset)),
                    static_cast<std::streamsize>(chunk.PayloadSize));
                if (!output_stream)
                {
                    SetLastError(ERROR_WRITE_FAULT);
                    output_stream.close();
                    RemoveFileIfPresent(temporary_path);
                    return false;
                }
            }
            else
            {
                SetLastError(ERROR_INVALID_DATA);
                output_stream.close();
                RemoveFileIfPresent(temporary_path);
                return false;
            }
        }

        output_stream.close();
        if (!output_stream)
        {
            SetLastError(ERROR_WRITE_FAULT);
            RemoveFileIfPresent(temporary_path);
            return false;
        }

        try
        {
            const helen::FileFingerprint resolved_fingerprint = helen::FileFingerprint::FromPath(temporary_path);
            if (!MatchesTargetFingerprint(resolved_fingerprint, delta))
            {
                SetLastError(ERROR_CRC);
                RemoveFileIfPresent(temporary_path);
                return false;
            }
        }
        catch (...)
        {
            SetLastError(ERROR_INVALID_DATA);
            RemoveFileIfPresent(temporary_path);
            return false;
        }

        RemoveFileIfPresent(resolved_path);
        std::error_code error_code;
        std::filesystem::rename(temporary_path, resolved_path, error_code);
        if (error_code)
        {
            SetLastError(static_cast<DWORD>(error_code.value()));
            RemoveFileIfPresent(temporary_path);
            return false;
        }

        return true;
    }
}

namespace helen
{
    DeltaVirtualFileSource::DeltaVirtualFileSource(
        const PackAssetResolver& asset_resolver,
        const std::filesystem::path& cache_directory,
        const std::filesystem::path& base_file_path,
        const VirtualFileDefinition& definition)
    {
        if (cache_directory.empty())
        {
            throw std::invalid_argument("Delta virtual file source requires a non-empty cache directory.");
        }

        if (base_file_path.empty())
        {
            throw std::invalid_argument("Delta virtual file source requires a non-empty base file path.");
        }

        if (definition.Mode != "delta-on-read" || definition.Source.Kind != VirtualFileSourceKind::DeltaFile)
        {
            throw std::invalid_argument("Delta virtual file source requires a delta-on-read delta-file definition.");
        }

        const std::optional<std::filesystem::path> delta_file_path = asset_resolver.Resolve(definition.Source.Path);
        if (!delta_file_path.has_value())
        {
            throw std::runtime_error("Delta virtual file source could not resolve the declared delta asset.");
        }

        CacheDirectory = cache_directory;
        BaseFilePath = base_file_path;
        DeltaFilePath = *delta_file_path;
        BaseFingerprint = FileFingerprint::FromPath(BaseFilePath);
        DeltaFingerprint = FileFingerprint::FromPath(DeltaFilePath);
        Delta = HgdeltaFile::Load(DeltaFilePath);

        if (!MatchesFingerprint(BaseFingerprint, definition.Source.Base))
        {
            throw std::runtime_error("Delta virtual file source base file does not match the declared manifest fingerprint.");
        }

        if (Delta.ChunkSize != definition.Source.ChunkSize ||
            Delta.BaseFileSize != definition.Source.Base.FileSize ||
            Delta.TargetFileSize != definition.Source.Target.FileSize ||
            Delta.BaseSha256 != ToLowerAscii(definition.Source.Base.Sha256) ||
            Delta.TargetSha256 != ToLowerAscii(definition.Source.Target.Sha256))
        {
            throw std::runtime_error("Delta virtual file source metadata does not match the declared manifest metadata.");
        }

        if (Delta.BaseFileSize != BaseFingerprint.FileSize || Delta.BaseSha256 != BaseFingerprint.Sha256)
        {
            throw std::runtime_error("Delta virtual file source base file does not match the delta container fingerprint.");
        }

        ResolvedFilePath = BuildResolvedFilePath(
            CacheDirectory,
            asset_resolver.GetPackRoot(),
            asset_resolver.GetBuildRoot(),
            definition.Id,
            BaseFingerprint,
            DeltaFingerprint);
    }

    std::uint64_t DeltaVirtualFileSource::GetSize() const
    {
        return Delta.TargetFileSize;
    }

    bool DeltaVirtualFileSource::Read(
        std::uint64_t offset,
        void* buffer,
        std::size_t bytes_to_read,
        std::size_t& bytes_read)
    {
        bytes_read = 0;
        if (bytes_to_read == 0)
        {
            return true;
        }

        if (buffer == nullptr)
        {
            return false;
        }

        const std::uint64_t target_size = GetSize();
        if (offset >= target_size)
        {
            return true;
        }

        std::ifstream base_stream(BaseFilePath, std::ios::binary);
        if (!base_stream)
        {
            return false;
        }

        std::uint8_t* destination = static_cast<std::uint8_t*>(buffer);
        std::uint64_t current_offset = offset;
        std::size_t remaining = static_cast<std::size_t>(std::min<std::uint64_t>(target_size - offset, bytes_to_read));
        while (remaining > 0)
        {
            const std::uint64_t chunk_index_value = current_offset / Delta.ChunkSize;
            if (chunk_index_value > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()) ||
                chunk_index_value >= Delta.Chunks.size())
            {
                return false;
            }

            const HgdeltaChunkDefinition& chunk = Delta.Chunks[static_cast<std::size_t>(chunk_index_value)];
            if (current_offset < chunk.TargetOffset || current_offset >= chunk.TargetOffset + chunk.TargetSize)
            {
                return false;
            }

            const std::uint64_t chunk_relative_offset = current_offset - chunk.TargetOffset;
            const std::size_t available_in_chunk = static_cast<std::size_t>(chunk.TargetSize - chunk_relative_offset);
            const std::size_t copy_size = std::min(remaining, available_in_chunk);

            if (chunk.Kind == HgdeltaChunkKind::BaseCopy)
            {
                if (!ReadBaseBytes(
                        base_stream,
                        chunk.TargetOffset + chunk_relative_offset,
                        destination + bytes_read,
                        copy_size))
                {
                    return false;
                }
            }
            else if (chunk.Kind == HgdeltaChunkKind::DeltaBytes)
            {
                const std::uint64_t payload_offset = chunk.PayloadOffset + chunk_relative_offset;
                if (payload_offset > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()) ||
                    payload_offset + copy_size > Delta.PayloadBytes.size())
                {
                    return false;
                }

                std::memcpy(
                    destination + bytes_read,
                    Delta.PayloadBytes.data() + static_cast<std::size_t>(payload_offset),
                    copy_size);
            }
            else
            {
                return false;
            }

            current_offset += copy_size;
            bytes_read += copy_size;
            remaining -= copy_size;
        }

        return true;
    }

    std::optional<HANDLE> DeltaVirtualFileSource::CreateFileMapping(
        DWORD protection,
        DWORD maximum_size_high,
        DWORD maximum_size_low)
    {
        {
            std::lock_guard<std::mutex> lock(ResolvedCacheMaterializationMutex);
            if (!IsResolvedFileValid(ResolvedFilePath, Delta) &&
                !MaterializeResolvedTarget(ResolvedFilePath, BaseFilePath, Delta))
            {
                if (GetLastError() == ERROR_SUCCESS)
                {
                    SetLastError(ERROR_INVALID_DATA);
                }

                return std::nullopt;
            }
        }

        const bool requires_writable_backing_handle = RequiresWritableBackingHandle(protection);
        DWORD desired_access = GENERIC_READ;
        DWORD share_mode = FILE_SHARE_READ;
        if (requires_writable_backing_handle)
        {
            desired_access |= GENERIC_WRITE;
            share_mode |= FILE_SHARE_WRITE;
        }

        const HANDLE file_handle = ::CreateFileW(
            ResolvedFilePath.c_str(),
            desired_access,
            share_mode,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file_handle == INVALID_HANDLE_VALUE)
        {
            return std::nullopt;
        }

        const HANDLE mapping_handle = ::CreateFileMappingW(
            file_handle,
            nullptr,
            protection,
            maximum_size_high,
            maximum_size_low,
            nullptr);
        const DWORD last_error = GetLastError();
        CloseHandle(file_handle);
        if (mapping_handle == nullptr)
        {
            SetLastError(last_error);
            return std::nullopt;
        }

        return mapping_handle;
    }
}
