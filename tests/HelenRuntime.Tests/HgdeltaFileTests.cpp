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
     * @brief Builds one valid synthetic `.hgdelta` payload for parser coverage.
     * @return Complete binary container for one two-chunk delta file.
     */
    std::vector<std::uint8_t> BuildSampleHgdeltaBytes()
    {
        std::vector<std::uint8_t> bytes;
        bytes.reserve(200);

        AppendAscii(bytes, "HGDL");
        AppendUInt32(bytes, 1);
        AppendUInt32(bytes, 0);
        AppendUInt32(bytes, 4);
        AppendUInt64(bytes, 4);
        AppendUInt64(bytes, 8);
        AppendHexDigest(bytes, "e12e115acf4552b2568b55e93cbd39394c4ef81c82447fafc997882a02d23677");
        AppendHexDigest(bytes, "9ac2197d9258257b1ae8463e4214e4cd0a578bc1517f2415928b91be4283fc48");
        AppendUInt32(bytes, 2);
        AppendUInt64(bytes, 116);
        AppendUInt64(bytes, 156);

        AppendUInt32(bytes, 0);
        AppendUInt32(bytes, 4);
        AppendUInt64(bytes, 0);
        AppendUInt32(bytes, 0);

        AppendUInt32(bytes, 1);
        AppendUInt32(bytes, 4);
        AppendUInt64(bytes, 0);
        AppendUInt32(bytes, 4);

        AppendAscii(bytes, "EFGH");
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

        WriteAllBytes(delta_path, std::vector<std::uint8_t>{'B', 'A', 'D', '!'});
        bool threw = false;
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
