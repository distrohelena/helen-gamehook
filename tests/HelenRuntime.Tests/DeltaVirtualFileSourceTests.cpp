#include <HelenHook/DeltaVirtualFileSource.h>

#include <HelenHook/FileFingerprint.h>
#include <HelenHook/PackAssetResolver.h>
#include <HelenHook/VirtualFileDefinition.h>
#include <HelenHook/VirtualFileSourceKind.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sstream>
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
     * @brief Writes one exact binary payload to disk for delta-source test scenarios.
     * @param path Destination file path that should be created or replaced.
     * @param bytes Binary payload written into the file.
     */
    void WriteAllBytes(const std::filesystem::path& path, std::string_view bytes)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::runtime_error("Failed to create a delta virtual file test asset.");
        }

        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!stream)
        {
            throw std::runtime_error("Failed to write a delta virtual file test asset.");
        }
    }

    /**
     * @brief Appends one 32-bit little-endian integer to a byte buffer.
     * @param bytes Destination buffer that receives the encoded value.
     * @param value Integer value that should be serialized.
     */
    void AppendUInt32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
    {
        bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
        bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
        bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
        bytes.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    }

    /**
     * @brief Appends one 64-bit little-endian integer to a byte buffer.
     * @param bytes Destination buffer that receives the encoded value.
     * @param value Integer value that should be serialized.
     */
    void AppendUInt64(std::vector<std::uint8_t>& bytes, std::uint64_t value)
    {
        for (int index = 0; index < 8; ++index)
        {
            bytes.push_back(static_cast<std::uint8_t>((value >> (index * 8)) & 0xFF));
        }
    }

    /**
     * @brief Appends one exact ASCII string to a byte buffer without a terminator.
     * @param bytes Destination buffer that receives the text bytes.
     * @param text ASCII text that should be serialized.
     */
    void AppendAscii(std::vector<std::uint8_t>& bytes, std::string_view text)
    {
        bytes.insert(bytes.end(), text.begin(), text.end());
    }

    /**
     * @brief Decodes one hexadecimal nibble into its numeric value.
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

        throw std::runtime_error("Delta virtual file test digest contained a non-hexadecimal character.");
    }

    /**
     * @brief Appends one 64-character SHA-256 digest string as 32 raw bytes.
     * @param bytes Destination buffer that receives the encoded hash bytes.
     * @param hex_digest Lowercase or uppercase hexadecimal digest string.
     */
    void AppendHexDigest(std::vector<std::uint8_t>& bytes, std::string_view hex_digest)
    {
        if (hex_digest.size() != 64)
        {
            throw std::runtime_error("Delta virtual file test digest must contain 64 hexadecimal characters.");
        }

        for (std::size_t index = 0; index < hex_digest.size(); index += 2)
        {
            const std::uint8_t value = static_cast<std::uint8_t>((DecodeHexNibble(hex_digest[index]) << 4) | DecodeHexNibble(hex_digest[index + 1]));
            bytes.push_back(value);
        }
    }

    /**
     * @brief Builds one valid two-chunk hgdelta container that replaces the second chunk with `WXYZ`.
     * @param base_sha256 Exact SHA-256 digest of the expected base file.
     * @param target_sha256 Exact SHA-256 digest of the reconstructed target file.
     * @return Complete hgdelta container bytes for the test scenario.
     */
    std::vector<std::uint8_t> BuildSampleHgdeltaBytes(std::string_view base_sha256, std::string_view target_sha256)
    {
        std::vector<std::uint8_t> bytes;
        bytes.reserve(192);

        AppendAscii(bytes, "HGDL");
        AppendUInt32(bytes, 1);
        AppendUInt32(bytes, 0);
        AppendUInt32(bytes, 4);
        AppendUInt64(bytes, 8);
        AppendUInt64(bytes, 8);
        AppendHexDigest(bytes, base_sha256);
        AppendHexDigest(bytes, target_sha256);
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

        AppendAscii(bytes, "WXYZ");
        return bytes;
    }

    /**
     * @brief Builds one delta-backed virtual-file definition for the supplied source metadata.
     * @param game_path Declared game-relative path used for registration.
     * @param source_path Pack-relative hgdelta asset path.
     * @param base_fingerprint Exact expected base file fingerprint.
     * @param target_fingerprint Exact expected target file fingerprint.
     * @return Fully populated delta-backed virtual-file definition.
     */
    helen::VirtualFileDefinition CreateDeltaVirtualFileDefinition(
        const char* file_id,
        const char* game_path,
        const char* source_path,
        const helen::FileFingerprint& base_fingerprint,
        const helen::FileFingerprint& target_fingerprint)
    {
        helen::VirtualFileDefinition definition;
        definition.Id = file_id;
        definition.GamePath = game_path;
        definition.Mode = "delta-on-read";
        definition.Source.Kind = helen::VirtualFileSourceKind::DeltaFile;
        definition.Source.Path = source_path;
        definition.Source.Base.FileSize = base_fingerprint.FileSize;
        definition.Source.Base.Sha256 = base_fingerprint.Sha256;
        definition.Source.Target.FileSize = target_fingerprint.FileSize;
        definition.Source.Target.Sha256 = target_fingerprint.Sha256;
        definition.Source.ChunkSize = 4;
        return definition;
    }

    /**
     * @brief Counts regular files beneath one directory tree for resolved-cache assertions.
     * @param root Directory tree that should be counted recursively.
     * @return Number of regular files found beneath the supplied root.
     */
    std::size_t CountRegularFilesRecursive(const std::filesystem::path& root)
    {
        if (!std::filesystem::exists(root))
        {
            return 0;
        }

        std::size_t count = 0;
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root))
        {
            if (entry.is_regular_file())
            {
                ++count;
            }
        }

        return count;
    }

    /**
     * @brief Throws a runtime_error that appends the current Win32 last-error code to the supplied message.
     * @param message Failure prefix that explains the operation that failed.
     */
    [[noreturn]] void ThrowLastError(const char* message)
    {
        std::ostringstream stream;
        stream << message << " (GetLastError=" << GetLastError() << ")";
        throw std::runtime_error(stream.str());
    }
}

/**
 * @brief Verifies that a delta-backed source reconstructs bytes from the exact base file and rejects a mismatched base hash.
 */
void RunDeltaVirtualFileSourceTests()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "HHG" / "DVS";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    try
    {
        const std::filesystem::path game_root = root / "game";
        const std::filesystem::path pack_root = root / "pack";
        const std::filesystem::path build_root = pack_root / "builds" / "steam-goty-1.0";
        const std::filesystem::path second_pack_root = root / "pack-alt";
        const std::filesystem::path second_build_root = second_pack_root / "builds" / "steam-goty-1.0-alt";
        const std::filesystem::path base_file_path = game_root / "BmGame" / "CookedPC" / "BmGame.u";
        const std::filesystem::path target_file_path = root / "expected-target.u";
        const std::filesystem::path delta_file_path = build_root / "assets" / "deltas" / "BmGame.hgdelta";
        const std::filesystem::path second_delta_file_path = second_build_root / "assets" / "deltas" / "BmGame.hgdelta";
        const std::filesystem::path cache_directory = root / "helengamehook" / "cache";

        std::filesystem::create_directories(base_file_path.parent_path());
        std::filesystem::create_directories(delta_file_path.parent_path());
        std::filesystem::create_directories(second_delta_file_path.parent_path());
        WriteAllBytes(base_file_path, "ABCDEFGH");
        WriteAllBytes(target_file_path, "ABCDWXYZ");

        const helen::FileFingerprint base_fingerprint = helen::FileFingerprint::FromPath(base_file_path);
        const helen::FileFingerprint target_fingerprint = helen::FileFingerprint::FromPath(target_file_path);
        const std::vector<std::uint8_t> delta_bytes = BuildSampleHgdeltaBytes(base_fingerprint.Sha256, target_fingerprint.Sha256);
        std::ofstream delta_stream(delta_file_path, std::ios::binary | std::ios::trunc);
        delta_stream.write(reinterpret_cast<const char*>(delta_bytes.data()), static_cast<std::streamsize>(delta_bytes.size()));
        delta_stream.close();
        std::ofstream second_delta_stream(second_delta_file_path, std::ios::binary | std::ios::trunc);
        second_delta_stream.write(reinterpret_cast<const char*>(delta_bytes.data()), static_cast<std::streamsize>(delta_bytes.size()));
        second_delta_stream.close();

        const helen::VirtualFileDefinition definition = CreateDeltaVirtualFileDefinition(
            "bmgameGameplayPackage",
            "BmGame/CookedPC/BmGame.u",
            "assets/deltas/BmGame.hgdelta",
            base_fingerprint,
            target_fingerprint);

        const helen::PackAssetResolver resolver(pack_root, build_root);
        helen::DeltaVirtualFileSource source(resolver, cache_directory, base_file_path, definition);
        const std::string resolved_path_text = source.ResolvedFilePath.generic_string();
        Expect(resolved_path_text.find("/resolved/") != std::string::npos, "Resolved cache path did not stay beneath the expected resolved-cache root.");
        Expect(source.ResolvedFilePath.extension() == ".bin", "Resolved cache path did not preserve the expected binary cache-file extension.");

        std::array<char, 8> bytes{};
        std::size_t bytes_read = 0;
        Expect(source.Read(0, bytes.data(), bytes.size(), bytes_read), "Expected delta source read to succeed.");
        Expect(bytes_read == 8, "Delta source read byte count mismatch.");
        Expect(std::string_view(bytes.data(), 8) == "ABCDWXYZ", "Delta source payload mismatch.");

        Expect(CountRegularFilesRecursive(cache_directory / "resolved") == 0, "Resolved-cache directory unexpectedly contained files before the first mapping.");

        const std::optional<HANDLE> mapping_handle = source.CreateFileMapping(PAGE_READONLY, 0, 0);
        if (!mapping_handle.has_value())
        {
            ThrowLastError("Expected CreateFileMapping to materialize and map the reconstructed delta target.");
        }

        void* const mapping_view = MapViewOfFile(*mapping_handle, FILE_MAP_READ, 0, 0, 8);
        Expect(mapping_view != nullptr, "Expected MapViewOfFile to succeed for the materialized delta target.");
        Expect(std::string_view(static_cast<const char*>(mapping_view), 8) == "ABCDWXYZ", "Mapped delta target payload mismatch.");
        UnmapViewOfFile(mapping_view);
        CloseHandle(*mapping_handle);

        const std::optional<HANDLE> writable_mapping_handle = source.CreateFileMapping(PAGE_READWRITE, 0, 0);
        if (!writable_mapping_handle.has_value())
        {
            ThrowLastError("Expected CreateFileMapping to support writable mappings for the resolved delta target.");
        }

        void* const writable_mapping_view = MapViewOfFile(*writable_mapping_handle, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 8);
        Expect(writable_mapping_view != nullptr, "Expected MapViewOfFile to succeed for a writable resolved delta target mapping.");
        UnmapViewOfFile(writable_mapping_view);
        CloseHandle(*writable_mapping_handle);

        Expect(CountRegularFilesRecursive(cache_directory / "resolved") == 1, "Expected one resolved cache file after the first delta mapping.");

        const std::optional<HANDLE> second_mapping_handle = source.CreateFileMapping(PAGE_READONLY, 0, 0);
        Expect(second_mapping_handle.has_value(), "Expected CreateFileMapping to reuse the resolved cache file on a second call.");
        CloseHandle(*second_mapping_handle);
        Expect(CountRegularFilesRecursive(cache_directory / "resolved") == 1, "Resolved cache unexpectedly created duplicate files for the same delta source.");

        const helen::VirtualFileDefinition second_definition = CreateDeltaVirtualFileDefinition(
            "bmgameGameplayPackageAlternate",
            "BmGame/CookedPC/BmGame.u",
            "assets/deltas/BmGame.hgdelta",
            base_fingerprint,
            target_fingerprint);
        helen::DeltaVirtualFileSource second_source(resolver, cache_directory, base_file_path, second_definition);
        Expect(second_source.ResolvedFilePath != source.ResolvedFilePath, "Resolved cache path unexpectedly ignored the virtual file identity.");
        const std::optional<HANDLE> separated_mapping_handle = second_source.CreateFileMapping(PAGE_READONLY, 0, 0);
        if (!separated_mapping_handle.has_value())
        {
            ThrowLastError("Expected CreateFileMapping to materialize a separate resolved cache file for a different virtual file identifier.");
        }
        CloseHandle(*separated_mapping_handle);
        Expect(CountRegularFilesRecursive(cache_directory / "resolved") == 2, "Resolved cache unexpectedly reused one file across different virtual file identifiers.");

        const helen::PackAssetResolver second_resolver(second_pack_root, second_build_root);
        helen::DeltaVirtualFileSource second_build_source(second_resolver, cache_directory, base_file_path, definition);
        Expect(second_build_source.ResolvedFilePath != source.ResolvedFilePath, "Resolved cache path unexpectedly ignored the pack/build identity.");
        const std::optional<HANDLE> separated_build_mapping_handle = second_build_source.CreateFileMapping(PAGE_READONLY, 0, 0);
        if (!separated_build_mapping_handle.has_value())
        {
            ThrowLastError("Expected CreateFileMapping to materialize a separate resolved cache file for a different pack/build identity.");
        }
        CloseHandle(*separated_build_mapping_handle);
        Expect(CountRegularFilesRecursive(cache_directory / "resolved") == 3, "Resolved cache unexpectedly reused one file across different pack/build identities.");

        WriteAllBytes(base_file_path, "ZZZZZZZZ");
        bool constructor_threw = false;
        try
        {
            helen::DeltaVirtualFileSource mismatched_source(resolver, cache_directory, base_file_path, definition);
            static_cast<void>(mismatched_source);
        }
        catch (const std::runtime_error&)
        {
            constructor_threw = true;
        }

        Expect(constructor_threw, "Delta source unexpectedly accepted a mismatched base file fingerprint.");
    }
    catch (...)
    {
        std::filesystem::remove_all(root);
        throw;
    }

    std::filesystem::remove_all(root);
}
