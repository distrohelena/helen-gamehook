#include <HelenHook/HgdeltaChunkKind.h>
#include <HelenHook/HgdeltaFile.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    /**
     * @brief Throws when one required boolean condition is false so the shared test harness stops immediately.
     * @param condition Boolean condition under test.
     * @param message Failure message reported by the shared test runner.
     */
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Appends one 32-bit little-endian value to the supplied byte buffer.
     * @param bytes Destination byte buffer that receives the encoded integer.
     * @param value Unsigned 32-bit value that should be serialized.
     */
    void AppendUInt32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
    {
        bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
        bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
        bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
        bytes.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    }

    /**
     * @brief Appends one 64-bit little-endian value to the supplied byte buffer.
     * @param bytes Destination byte buffer that receives the encoded integer.
     * @param value Unsigned 64-bit value that should be serialized.
     */
    void AppendUInt64(std::vector<std::uint8_t>& bytes, std::uint64_t value)
    {
        for (int index = 0; index < 8; ++index)
        {
            bytes.push_back(static_cast<std::uint8_t>((value >> (index * 8)) & 0xFF));
        }
    }

    /**
     * @brief Appends one ASCII string to the supplied byte buffer without a terminator.
     * @param bytes Destination byte buffer that receives the encoded string.
     * @param text ASCII text that should be serialized verbatim.
     */
    void AppendAscii(std::vector<std::uint8_t>& bytes, std::string_view text)
    {
        bytes.insert(bytes.end(), text.begin(), text.end());
    }

    /**
     * @brief Decodes one hexadecimal digit into its numeric nibble value.
     * @param digit ASCII hexadecimal character.
     * @return Unsigned nibble value in the range 0 through 15.
     */
    std::uint8_t DecodeHexNibble(char digit)
    {
        if (digit >= '0' && digit <= '9')
        {
            return static_cast<std::uint8_t>(digit - '0');
        }

        if (digit >= 'a' && digit <= 'f')
        {
            return static_cast<std::uint8_t>(10 + digit - 'a');
        }

        if (digit >= 'A' && digit <= 'F')
        {
            return static_cast<std::uint8_t>(10 + digit - 'A');
        }

        throw std::runtime_error("Sample hgdelta digest contained a non-hexadecimal character.");
    }

    /**
     * @brief Appends one lowercase SHA-256 digest string as 32 raw bytes.
     * @param bytes Destination byte buffer that receives the encoded hash.
     * @param hex_digest Lowercase or uppercase 64-character hexadecimal digest string.
     */
    void AppendHexDigest(std::vector<std::uint8_t>& bytes, std::string_view hex_digest)
    {
        if (hex_digest.size() != 64)
        {
            throw std::runtime_error("Sample hgdelta digest must contain 64 hexadecimal characters.");
        }

        for (std::size_t index = 0; index < hex_digest.size(); index += 2)
        {
            const std::uint8_t value = static_cast<std::uint8_t>((DecodeHexNibble(hex_digest[index]) << 4) | DecodeHexNibble(hex_digest[index + 1]));
            bytes.push_back(value);
        }
    }

    /**
     * @brief Appends one fixed-size hgdelta chunk table entry to the supplied byte buffer.
     * @param bytes Destination byte buffer that receives the encoded chunk entry.
     * @param kind Serialized chunk kind value.
     * @param target_size Exact target byte count for the chunk.
     * @param payload_offset Byte offset of the chunk payload within the payload block.
     * @param payload_size Exact payload byte count referenced by the chunk.
     */
    void AppendChunkEntry(
        std::vector<std::uint8_t>& bytes,
        std::uint32_t kind,
        std::uint32_t target_size,
        std::uint64_t payload_offset,
        std::uint32_t payload_size)
    {
        AppendUInt32(bytes, kind);
        AppendUInt32(bytes, target_size);
        AppendUInt64(bytes, payload_offset);
        AppendUInt32(bytes, payload_size);
    }

    /**
     * @brief Appends one hgdelta header using the fixed Task 1 container layout.
     * @param bytes Destination byte buffer that receives the encoded header.
     * @param chunk_size Nominal chunk size recorded in the header.
     * @param base_file_size Declared base file size recorded in the header.
     * @param target_file_size Declared target file size recorded in the header.
     * @param base_sha256 Declared lowercase base SHA-256 digest string.
     * @param target_sha256 Declared lowercase target SHA-256 digest string.
     * @param chunk_count Number of fixed-size chunk entries in the chunk table.
     * @param chunk_table_offset Byte offset of the chunk table from the start of the file.
     * @param payload_offset Byte offset of the payload block from the start of the file.
     */
    void AppendHeader(
        std::vector<std::uint8_t>& bytes,
        std::uint32_t chunk_size,
        std::uint64_t base_file_size,
        std::uint64_t target_file_size,
        std::string_view base_sha256,
        std::string_view target_sha256,
        std::uint32_t chunk_count,
        std::uint64_t chunk_table_offset,
        std::uint64_t payload_offset)
    {
        AppendAscii(bytes, "HGDL");
        AppendUInt32(bytes, 1);
        AppendUInt32(bytes, 0);
        AppendUInt32(bytes, chunk_size);
        AppendUInt64(bytes, base_file_size);
        AppendUInt64(bytes, target_file_size);
        AppendHexDigest(bytes, base_sha256);
        AppendHexDigest(bytes, target_sha256);
        AppendUInt32(bytes, chunk_count);
        AppendUInt64(bytes, chunk_table_offset);
        AppendUInt64(bytes, payload_offset);
    }

    /**
     * @brief Builds one valid synthetic `.hgdelta` payload for parser coverage.
     * @return Complete binary container for one two-chunk delta file.
     */
    std::vector<std::uint8_t> BuildSampleHgdeltaBytes()
    {
        std::vector<std::uint8_t> bytes;
        bytes.reserve(200);

        AppendHeader(
            bytes,
            4,
            4,
            8,
            "e12e115acf4552b2568b55e93cbd39394c4ef81c82447fafc997882a02d23677",
            "9ac2197d9258257b1ae8463e4214e4cd0a578bc1517f2415928b91be4283fc48",
            2,
            116,
            156);

        AppendChunkEntry(bytes, 0, 4, 0, 0);
        AppendChunkEntry(bytes, 1, 4, 0, 4);

        AppendAscii(bytes, "EFGH");
        return bytes;
    }

    /**
     * @brief Builds one malformed `.hgdelta` payload whose base-copy chunk extends past the declared base file size.
     * @return Complete binary container that should be rejected by the parser.
     */
    std::vector<std::uint8_t> BuildBaseCopyPastBaseFileHgdeltaBytes()
    {
        std::vector<std::uint8_t> bytes;
        bytes.reserve(160);

        AppendHeader(
            bytes,
            4,
            3,
            4,
            "0000000000000000000000000000000000000000000000000000000000000000",
            "1111111111111111111111111111111111111111111111111111111111111111",
            1,
            116,
            136);
        AppendChunkEntry(bytes, 0, 4, 0, 0);
        return bytes;
    }

    /**
     * @brief Builds one malformed `.hgdelta` payload whose delta-byte chunk cannot reconstruct the declared target size.
     * @return Complete binary container that should be rejected by the parser.
     */
    std::vector<std::uint8_t> BuildDeltaBytesSizeMismatchHgdeltaBytes()
    {
        std::vector<std::uint8_t> bytes;
        bytes.reserve(160);

        AppendHeader(
            bytes,
            4,
            0,
            4,
            "2222222222222222222222222222222222222222222222222222222222222222",
            "3333333333333333333333333333333333333333333333333333333333333333",
            1,
            116,
            136);
        AppendChunkEntry(bytes, 1, 4, 0, 3);
        AppendAscii(bytes, "XYZ");
        return bytes;
    }

    /**
     * @brief Builds one valid zero-length `.hgdelta` payload with no chunks and no payload bytes.
     * @return Complete binary container for an empty reconstructed target file.
     */
    std::vector<std::uint8_t> BuildZeroLengthHgdeltaBytes()
    {
        std::vector<std::uint8_t> bytes;
        bytes.reserve(116);

        AppendHeader(
            bytes,
            4,
            0,
            0,
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            0,
            116,
            116);
        return bytes;
    }

    /**
     * @brief Writes one exact binary payload to disk for temporary test assets.
     * @param path Destination file path that should be created or replaced.
     * @param bytes Binary payload written into the file.
     */
    void WriteAllBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::runtime_error("Failed to create an hgdelta test asset.");
        }

        stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!stream)
        {
            throw std::runtime_error("Failed to write an hgdelta test asset.");
        }
    }
}

/**
 * @brief Verifies that the hgdelta loader parses header metadata, chunk definitions, and payload bytes from a valid sample.
 */
void RunHgdeltaFileTests()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "HelenRuntimeTests" / "HgdeltaFile";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    try
    {
        const std::filesystem::path delta_path = root / "sample.hgdelta";
        WriteAllBytes(delta_path, BuildSampleHgdeltaBytes());

        const helen::HgdeltaFile delta = helen::HgdeltaFile::Load(delta_path);
        Expect(delta.ChunkSize == 4, "Chunk size mismatch.");
        Expect(delta.BaseFileSize == 4, "Base file size mismatch.");
        Expect(delta.TargetFileSize == 8, "Target file size mismatch.");
        Expect(
            delta.BaseSha256 == "e12e115acf4552b2568b55e93cbd39394c4ef81c82447fafc997882a02d23677",
            "Base SHA-256 mismatch.");
        Expect(
            delta.TargetSha256 == "9ac2197d9258257b1ae8463e4214e4cd0a578bc1517f2415928b91be4283fc48",
            "Target SHA-256 mismatch.");
        Expect(delta.Chunks.size() == 2, "Chunk count mismatch.");
        Expect(delta.Chunks[0].Kind == helen::HgdeltaChunkKind::BaseCopy, "First chunk kind mismatch.");
        Expect(delta.Chunks[0].TargetOffset == 0, "First chunk target offset mismatch.");
        Expect(delta.Chunks[0].TargetSize == 4, "First chunk target size mismatch.");
        Expect(delta.Chunks[0].PayloadOffset == 0, "First chunk payload offset mismatch.");
        Expect(delta.Chunks[0].PayloadSize == 0, "First chunk payload size mismatch.");
        Expect(delta.Chunks[1].Kind == helen::HgdeltaChunkKind::DeltaBytes, "Second chunk kind mismatch.");
        Expect(delta.Chunks[1].TargetOffset == 4, "Second chunk target offset mismatch.");
        Expect(delta.Chunks[1].TargetSize == 4, "Second chunk target size mismatch.");
        Expect(delta.Chunks[1].PayloadOffset == 0, "Second chunk payload offset mismatch.");
        Expect(delta.Chunks[1].PayloadSize == 4, "Second chunk payload size mismatch.");
        Expect(delta.PayloadBytes.size() == 4, "Payload byte count mismatch.");
        Expect(std::string_view(reinterpret_cast<const char*>(delta.PayloadBytes.data()), delta.PayloadBytes.size()) == "EFGH", "Payload bytes mismatch.");

        WriteAllBytes(delta_path, BuildBaseCopyPastBaseFileHgdeltaBytes());
        bool threw = false;
        try
        {
            static_cast<void>(helen::HgdeltaFile::Load(delta_path));
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }

        Expect(threw, "Base-copy chunk that exceeds the declared base file size unexpectedly loaded.");

        WriteAllBytes(delta_path, BuildDeltaBytesSizeMismatchHgdeltaBytes());
        threw = false;
        try
        {
            static_cast<void>(helen::HgdeltaFile::Load(delta_path));
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }

        Expect(threw, "Delta-byte chunk whose payload size differs from its target size unexpectedly loaded.");

        WriteAllBytes(delta_path, BuildZeroLengthHgdeltaBytes());
        const helen::HgdeltaFile empty_delta = helen::HgdeltaFile::Load(delta_path);
        Expect(empty_delta.ChunkSize == 4, "Zero-length delta chunk size mismatch.");
        Expect(empty_delta.BaseFileSize == 0, "Zero-length delta base file size mismatch.");
        Expect(empty_delta.TargetFileSize == 0, "Zero-length delta target file size mismatch.");
        Expect(empty_delta.Chunks.empty(), "Zero-length delta unexpectedly produced chunks.");
        Expect(empty_delta.PayloadBytes.empty(), "Zero-length delta unexpectedly produced payload bytes.");

        WriteAllBytes(delta_path, std::vector<std::uint8_t>{'B', 'A', 'D', '!'});
        threw = false;
        try
        {
            static_cast<void>(helen::HgdeltaFile::Load(delta_path));
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }

        Expect(threw, "Malformed hgdelta header unexpectedly loaded.");
    }
    catch (...)
    {
        std::filesystem::remove_all(root);
        throw;
    }

    std::filesystem::remove_all(root);
}
