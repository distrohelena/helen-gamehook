#include <HelenHook/HgdeltaFile.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    /**
     * @brief Fixed four-byte container signature used by Helen Game Delta v1 files.
     */
    constexpr std::string_view HgdeltaMagic = "HGDL";

    /**
     * @brief Binary format version supported by this loader.
     */
    constexpr std::uint32_t HgdeltaVersion = 1;

    /**
     * @brief Number of bytes in the fixed header.
     */
    constexpr std::size_t HgdeltaHeaderSize = 116;

    /**
     * @brief Number of bytes in one fixed-size chunk table entry.
     */
    constexpr std::size_t HgdeltaChunkEntrySize = 20;

    /**
     * @brief Number of bytes in one stored SHA-256 digest.
     */
    constexpr std::size_t HgdeltaDigestByteCount = 32;

    /**
     * @brief Returns whether the supplied path points to an existing regular file.
     * @param file_path Path whose on-disk type should be validated.
     * @return True when the path exists and names a regular file; otherwise false.
     */
    bool IsRegularFile(const std::filesystem::path& file_path)
    {
        std::error_code error_code;
        return std::filesystem::is_regular_file(file_path, error_code) && !error_code;
    }

    /**
     * @brief Loads one complete binary file into memory.
     * @param file_path Path of the file that should be read.
     * @return Entire file contents as a byte buffer.
     */
    std::vector<std::uint8_t> ReadAllBytes(const std::filesystem::path& file_path)
    {
        std::error_code error_code;
        const std::uintmax_t file_size = std::filesystem::file_size(file_path, error_code);
        if (error_code)
        {
            throw std::runtime_error("Hgdelta loading failed to read the file size.");
        }

        if (file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()))
        {
            throw std::runtime_error("Hgdelta loading failed because the file is too large.");
        }

        if (file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
        {
            throw std::runtime_error("Hgdelta loading failed because the file cannot be read in one pass.");
        }

        std::ifstream stream(file_path, std::ios::binary);
        if (!stream)
        {
            throw std::runtime_error("Hgdelta loading failed to open the file.");
        }

        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file_size), 0);
        if (!bytes.empty())
        {
            stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            if (stream.gcount() != static_cast<std::streamsize>(bytes.size()) || stream.bad())
            {
                throw std::runtime_error("Hgdelta loading failed while reading the file.");
            }
        }

        return bytes;
    }

    /**
     * @brief Reads one little-endian 32-bit unsigned integer from a byte buffer.
     * @param bytes Source byte buffer.
     * @param offset Byte offset within the source buffer.
     * @return Parsed 32-bit value.
     */
    std::uint32_t ReadUInt32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
    {
        if (offset > bytes.size() || bytes.size() - offset < sizeof(std::uint32_t))
        {
            throw std::runtime_error("Hgdelta header or chunk table is truncated.");
        }

        return static_cast<std::uint32_t>(bytes[offset]) |
            (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
            (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
            (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    }

    /**
     * @brief Reads one little-endian 64-bit unsigned integer from a byte buffer.
     * @param bytes Source byte buffer.
     * @param offset Byte offset within the source buffer.
     * @return Parsed 64-bit value.
     */
    std::uint64_t ReadUInt64(const std::vector<std::uint8_t>& bytes, std::size_t offset)
    {
        if (offset > bytes.size() || bytes.size() - offset < sizeof(std::uint64_t))
        {
            throw std::runtime_error("Hgdelta header or chunk table is truncated.");
        }

        std::uint64_t value = 0;
        for (std::size_t index = 0; index < sizeof(std::uint64_t); ++index)
        {
            value |= static_cast<std::uint64_t>(bytes[offset + index]) << (index * 8);
        }

        return value;
    }

    /**
     * @brief Converts raw digest bytes into lowercase hexadecimal text.
     * @param bytes Raw SHA-256 bytes.
     * @return Lowercase hexadecimal digest text.
     */
    std::string BytesToLowerHex(const std::uint8_t* bytes)
    {
        constexpr std::string_view LowerHexDigits = "0123456789abcdef";

        std::string text;
        text.reserve(HgdeltaDigestByteCount * 2);
        for (std::size_t index = 0; index < HgdeltaDigestByteCount; ++index)
        {
            const std::uint8_t byte = bytes[index];
            text.push_back(LowerHexDigits[(byte >> 4) & 0x0F]);
            text.push_back(LowerHexDigits[byte & 0x0F]);
        }

        return text;
    }

    /**
     * @brief Validates that one chunk table entry is internally consistent.
     * @param kind Serialized chunk kind value.
     * @param chunk_size Nominal chunk size from the delta header.
     * @param chunk_index Position of the chunk within the target file.
     * @param chunk_count Total number of chunks in the container.
     * @param base_size Exact base file size recorded in the header.
     * @param target_size Exact reconstructed target size recorded in the header.
     * @param target_chunk_size Exact size of this chunk in the chunk table.
     * @param payload_offset Byte offset within the payload block.
     * @param payload_size Exact payload byte count for the chunk.
     * @param payload_bytes Raw payload bytes copied from the container.
     */
    void ValidateChunkEntry(
        std::uint32_t kind,
        std::uint32_t chunk_size,
        std::size_t chunk_index,
        std::size_t chunk_count,
        std::uint64_t base_size,
        std::uint64_t target_size,
        std::uint32_t target_chunk_size,
        std::uint64_t payload_offset,
        std::uint32_t payload_size,
        const std::vector<std::uint8_t>& payload_bytes)
    {
        if (kind != 0 && kind != 1)
        {
            throw std::runtime_error("Hgdelta chunk table contains an unknown chunk kind.");
        }

        if (target_chunk_size == 0)
        {
            throw std::runtime_error("Hgdelta chunk table contains an empty chunk.");
        }

        const std::uint64_t target_offset = static_cast<std::uint64_t>(chunk_index) * chunk_size;
        if (target_offset > target_size || target_size - target_offset < target_chunk_size)
        {
            throw std::runtime_error("Hgdelta chunk table describes bytes outside the target file.");
        }

        if (chunk_index + 1 < chunk_count)
        {
            if (target_chunk_size != chunk_size)
            {
                throw std::runtime_error("Hgdelta chunk table contains a non-final chunk with the wrong size.");
            }
        }
        else if (target_chunk_size > chunk_size)
        {
            throw std::runtime_error("Hgdelta chunk table contains a final chunk that exceeds the nominal chunk size.");
        }

        if (kind == 0)
        {
            if (target_offset > base_size || base_size - target_offset < target_chunk_size)
            {
                throw std::runtime_error("Hgdelta base-copy chunk describes bytes outside the base file.");
            }

            if (payload_offset != 0 || payload_size != 0)
            {
                throw std::runtime_error("Hgdelta base-copy chunks must not reference payload bytes.");
            }
        }
        else if (payload_size == 0)
        {
            throw std::runtime_error("Hgdelta delta-byte chunks must reference payload bytes.");
        }
        else if (payload_size != target_chunk_size)
        {
            throw std::runtime_error("Hgdelta delta-byte chunks must store the full target chunk bytes.");
        }

        if (payload_offset > payload_bytes.size() || payload_bytes.size() - payload_offset < payload_size)
        {
            throw std::runtime_error("Hgdelta chunk payload extends beyond the payload block.");
        }
    }
}

namespace helen
{
    /**
     * @brief Loads one `.hgdelta` container from disk and validates its fixed binary layout.
     * @param file_path Absolute or relative path of the delta container to parse.
     * @return Fully parsed delta model including header data, chunk table, and payload bytes.
     * @throws std::invalid_argument Thrown when the path is empty or not a regular file.
     * @throws std::runtime_error Thrown when the file cannot be read or the header is malformed.
     */
    HgdeltaFile HgdeltaFile::Load(const std::filesystem::path& file_path)
    {
        if (file_path.empty())
        {
            throw std::invalid_argument("Hgdelta loading requires a non-empty file path.");
        }

        if (!IsRegularFile(file_path))
        {
            throw std::invalid_argument("Hgdelta loading requires an existing regular file.");
        }

        const std::vector<std::uint8_t> bytes = ReadAllBytes(file_path);
        if (bytes.size() < HgdeltaHeaderSize)
        {
            throw std::runtime_error("Hgdelta file is too small to contain the fixed header.");
        }

        if (!std::equal(HgdeltaMagic.begin(), HgdeltaMagic.end(), bytes.begin()))
        {
            throw std::runtime_error("Hgdelta file has an unrecognized magic signature.");
        }

        const std::uint32_t version = ReadUInt32(bytes, 4);
        if (version != HgdeltaVersion)
        {
            throw std::runtime_error("Hgdelta file uses an unsupported format version.");
        }

        const std::uint32_t flags = ReadUInt32(bytes, 8);
        if (flags != 0)
        {
            throw std::runtime_error("Hgdelta file contains unsupported header flags.");
        }

        const std::uint32_t chunk_size = ReadUInt32(bytes, 12);
        const std::uint64_t base_file_size = ReadUInt64(bytes, 16);
        const std::uint64_t target_file_size = ReadUInt64(bytes, 24);
        const std::string base_sha256 = BytesToLowerHex(bytes.data() + 32);
        const std::string target_sha256 = BytesToLowerHex(bytes.data() + 64);
        const std::uint32_t chunk_count = ReadUInt32(bytes, 96);
        const std::uint64_t chunk_table_offset = ReadUInt64(bytes, 100);
        const std::uint64_t payload_offset = ReadUInt64(bytes, 108);

        if (chunk_size == 0)
        {
            throw std::runtime_error("Hgdelta file contains an invalid chunk size.");
        }

        if (chunk_table_offset < HgdeltaHeaderSize)
        {
            throw std::runtime_error("Hgdelta chunk table overlaps the fixed header.");
        }

        if (payload_offset < chunk_table_offset)
        {
            throw std::runtime_error("Hgdelta payload starts before the chunk table ends.");
        }

        const std::uint64_t chunk_table_size = static_cast<std::uint64_t>(chunk_count) * HgdeltaChunkEntrySize;
        if (chunk_count != 0 && chunk_table_size / HgdeltaChunkEntrySize != chunk_count)
        {
            throw std::runtime_error("Hgdelta chunk table size overflowed.");
        }

        if (chunk_table_offset > payload_offset || payload_offset - chunk_table_offset < chunk_table_size)
        {
            throw std::runtime_error("Hgdelta payload offset does not leave room for the chunk table.");
        }

        if (payload_offset > bytes.size())
        {
            throw std::runtime_error("Hgdelta payload offset points past the end of the file.");
        }

        std::vector<std::uint8_t> payload_bytes(bytes.begin() + static_cast<std::ptrdiff_t>(payload_offset), bytes.end());
        HgdeltaFile delta;
        delta.ChunkSize = chunk_size;
        delta.BaseFileSize = base_file_size;
        delta.TargetFileSize = target_file_size;
        delta.BaseSha256 = base_sha256;
        delta.TargetSha256 = target_sha256;
        delta.PayloadBytes = std::move(payload_bytes);
        delta.Chunks.reserve(chunk_count);

        std::uint64_t reconstructed_size = 0;
        for (std::size_t chunk_index = 0; chunk_index < chunk_count; ++chunk_index)
        {
            const std::size_t entry_offset = static_cast<std::size_t>(chunk_table_offset) + (chunk_index * HgdeltaChunkEntrySize);
            const std::uint32_t kind = ReadUInt32(bytes, entry_offset);
            const std::uint32_t target_chunk_size = ReadUInt32(bytes, entry_offset + 4);
            const std::uint64_t entry_payload_offset = ReadUInt64(bytes, entry_offset + 8);
            const std::uint32_t entry_payload_size = ReadUInt32(bytes, entry_offset + 16);

            ValidateChunkEntry(
                kind,
                chunk_size,
                chunk_index,
                chunk_count,
                base_file_size,
                target_file_size,
                target_chunk_size,
                entry_payload_offset,
                entry_payload_size,
                delta.PayloadBytes);

            HgdeltaChunkDefinition chunk;
            chunk.Kind = static_cast<HgdeltaChunkKind>(kind);
            chunk.TargetOffset = reconstructed_size;
            chunk.TargetSize = target_chunk_size;
            chunk.PayloadOffset = entry_payload_offset;
            chunk.PayloadSize = entry_payload_size;
            delta.Chunks.push_back(chunk);

            reconstructed_size += target_chunk_size;
        }

        if (reconstructed_size != target_file_size)
        {
            throw std::runtime_error("Hgdelta chunks do not add up to the target file size.");
        }

        return delta;
    }
}
