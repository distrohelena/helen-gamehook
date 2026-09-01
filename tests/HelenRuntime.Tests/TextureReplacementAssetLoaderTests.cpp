#include <cstdint>
#include <d3d9.h>

#include <filesystem>
#include <fstream>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <HelenHook/PackAssetResolver.h>
#include <HelenHook/PackScopedTextureReplacementDefinition.h>
#include <HelenHook/TextureReplacementDefinition.h>
#include <HelenHook/TextureReplacementAssetLoader.h>

namespace
{
    /**
     * @brief Throws when a required boolean condition is false so the shared test harness stops at the first failure.
     * @param condition Boolean condition under test.
     * @param message Failure text reported by the shared test harness.
     */
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Writes a small DDS file to disk for the replacement asset loader test.
     * @param path Output path for the DDS file.
     * @param four_cc DDS compression FourCC written into the pixel-format header.
     * @param block_size Byte count for one compressed 4x4 pixel block.
     */
    void WriteTestDds(const std::filesystem::path& path, std::uint32_t four_cc, std::uint32_t block_size)
    {
        const std::uint32_t width = 1024u;
        const std::uint32_t height = 1024u;
        const std::uint32_t blocks_wide = width / 4u;
        const std::uint32_t blocks_high = height / 4u;
        const std::uint32_t payload_size = blocks_wide * blocks_high * block_size;

        std::vector<std::uint8_t> bytes(128u + payload_size, 0);
        const auto write_u32 = [&bytes](std::size_t offset, std::uint32_t value)
        {
            std::memcpy(bytes.data() + offset, &value, sizeof(value));
        };
        bytes[0] = 'D';
        bytes[1] = 'D';
        bytes[2] = 'S';
        bytes[3] = ' ';
        write_u32(4, 124u);
        write_u32(8, 0x00081007u);
        write_u32(12, height);
        write_u32(16, width);
        write_u32(20, payload_size);
        write_u32(76, 32u);
        write_u32(80, 0x00000004u);
        write_u32(84, four_cc);
        write_u32(108, 0x00001000u);

        if (block_size == 16u)
        {
            for (std::uint32_t block_index = 0; block_index < blocks_wide * blocks_high; ++block_index)
            {
                const std::size_t offset = 128u + static_cast<std::size_t>(block_index) * 16u;
                bytes[offset + 0] = 0xFF;
                bytes[offset + 1] = 0x00;
                bytes[offset + 8] = 0x00;
                bytes[offset + 9] = 0xF8;
            }
        }

        std::filesystem::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    /**
     * @brief Resolves the checked-in Batman high-resolution subtitle DDS from this test source location.
     * @return Absolute path to the replacement asset shipped by the Batman subtitle pack.
     */
    std::filesystem::path GetBatmanSubtitleReplacementDdsPath()
    {
        const std::filesystem::path source_path(__FILE__);
        return source_path.parent_path().parent_path().parent_path() /
            "games" /
            "HelenBatmanAA" /
            "helengamehook" /
            "packs" /
            "batman-aa-subtitles" /
            "builds" /
            "steam-goty-1.0" /
            "assets" /
            "textures" /
            "subtitle_scaled.dds";
    }

    /**
     * @brief Copies one DDS file while replacing its top-level header flags for malformed-input validation.
     * @param source_path Valid DDS file used as the byte-for-byte source.
     * @param destination_path Output path that receives the modified DDS file.
     * @param header_flags Replacement value written into the DDS header flags field.
     */
    void WriteDdsWithHeaderFlags(
        const std::filesystem::path& source_path,
        const std::filesystem::path& destination_path,
        std::uint32_t header_flags)
    {
        std::filesystem::copy_file(
            source_path,
            destination_path,
            std::filesystem::copy_options::overwrite_existing);

        std::fstream stream(destination_path, std::ios::binary | std::ios::in | std::ios::out);
        stream.seekp(8, std::ios::beg);
        stream.write(reinterpret_cast<const char*>(&header_flags), sizeof(header_flags));
    }

    /**
     * @brief Copies one DDS file while replacing one 32-bit header field for validation tests.
     * @param source_path Valid DDS file used as the byte-for-byte source.
     * @param destination_path Output path that receives the modified DDS file.
     * @param field_offset Byte offset of the 32-bit field within the complete DDS file.
     * @param field_value Replacement value written into the selected header field.
     */
    void WriteDdsWithHeaderField(
        const std::filesystem::path& source_path,
        const std::filesystem::path& destination_path,
        std::streamoff field_offset,
        std::uint32_t field_value)
    {
        std::filesystem::copy_file(
            source_path,
            destination_path,
            std::filesystem::copy_options::overwrite_existing);

        std::fstream stream(destination_path, std::ios::binary | std::ios::in | std::ios::out);
        stream.seekp(field_offset, std::ios::beg);
        stream.write(reinterpret_cast<const char*>(&field_value), sizeof(field_value));
    }

    /**
     * @brief Copies one DDS file while replacing its dimensions and declared level-0 payload size.
     * @param source_path Valid DDS file used as the byte-for-byte source.
     * @param destination_path Output path that receives the modified DDS file.
     * @param width Replacement width written into the DDS header.
     * @param height Replacement height written into the DDS header.
     * @param payload_size Replacement linear size and physical payload byte count.
     */
    void WriteDdsWithDimensionsAndPayloadSize(
        const std::filesystem::path& source_path,
        const std::filesystem::path& destination_path,
        std::uint32_t width,
        std::uint32_t height,
        std::uint32_t payload_size)
    {
        std::filesystem::copy_file(
            source_path,
            destination_path,
            std::filesystem::copy_options::overwrite_existing);

        std::fstream stream(destination_path, std::ios::binary | std::ios::in | std::ios::out);
        stream.seekp(12, std::ios::beg);
        stream.write(reinterpret_cast<const char*>(&height), sizeof(height));
        stream.write(reinterpret_cast<const char*>(&width), sizeof(width));
        stream.write(reinterpret_cast<const char*>(&payload_size), sizeof(payload_size));
        stream.close();
        std::filesystem::resize_file(destination_path, 128u + static_cast<std::uintmax_t>(payload_size));
    }
}

/**
 * @brief Verifies that the replacement asset loader accepts a larger DDS replacement texture.
 */
void RunTextureReplacementAssetLoaderTests()
{
    const std::filesystem::path test_directory = std::filesystem::temp_directory_path() / "helenhook-texture-replacement-loader-tests";
    const std::filesystem::path dds_path = test_directory / "replacement-1024.dds";

    WriteTestDds(dds_path, 0x35545844u, 16u);

    helen::TextureReplacementAsset asset{};
    HRESULT failure_result = E_FAIL;
    const bool succeeded = helen::TextureReplacementAssetLoader::TryLoadDds(dds_path, asset, failure_result);
    Expect(succeeded, "Expected a 1024x1024 DDS replacement asset to load successfully.");
    Expect(failure_result == S_OK, "Expected a successful HRESULT for the loaded replacement asset.");
    Expect(asset.Width == 1024u, "Replacement asset width mismatch.");
    Expect(asset.Height == 1024u, "Replacement asset height mismatch.");
    Expect(asset.Format == D3DFMT_DXT5, "Replacement asset format mismatch.");
    Expect(asset.Level0Bytes.size() == 1024u * 1024u, "Replacement asset payload size mismatch.");

    const std::filesystem::path dxt1_dds_path = test_directory / "replacement-dxt1.dds";
    WriteTestDds(dxt1_dds_path, 0x31545844u, 8u);
    helen::TextureReplacementAsset dxt1_asset{};
    failure_result = E_FAIL;
    const bool dxt1_succeeded = helen::TextureReplacementAssetLoader::TryLoadDds(
        dxt1_dds_path,
        dxt1_asset,
        failure_result);
    Expect(dxt1_succeeded, "Expected a valid DXT1 DDS replacement asset to load successfully.");
    Expect(dxt1_asset.Format == D3DFMT_DXT1, "DXT1 replacement asset format mismatch.");

    const std::filesystem::path dxt3_dds_path = test_directory / "replacement-dxt3.dds";
    WriteTestDds(dxt3_dds_path, 0x33545844u, 16u);
    helen::TextureReplacementAsset dxt3_asset{};
    failure_result = E_FAIL;
    const bool dxt3_succeeded = helen::TextureReplacementAssetLoader::TryLoadDds(
        dxt3_dds_path,
        dxt3_asset,
        failure_result);
    Expect(dxt3_succeeded, "Expected a valid DXT3 DDS replacement asset to load successfully.");
    Expect(dxt3_asset.Format == D3DFMT_DXT3, "DXT3 replacement asset format mismatch.");

    const std::filesystem::path missing_linear_size_dds_path = test_directory / "replacement-missing-linear-size-flag.dds";
    WriteDdsWithHeaderFlags(dds_path, missing_linear_size_dds_path, 0x00001007u);
    helen::TextureReplacementAsset missing_linear_size_asset{};
    failure_result = S_OK;
    const bool missing_linear_size_succeeded = helen::TextureReplacementAssetLoader::TryLoadDds(
        missing_linear_size_dds_path,
        missing_linear_size_asset,
        failure_result);
    Expect(!missing_linear_size_succeeded, "Expected a compressed DDS missing DDSD_LINEARSIZE to be rejected.");
    Expect(failure_result == E_FAIL, "Expected a compressed DDS missing DDSD_LINEARSIZE to report E_FAIL.");

    const std::filesystem::path overflowing_dimensions_dds_path = test_directory / "replacement-overflowing-dimensions.dds";
    WriteDdsWithDimensionsAndPayloadSize(
        dds_path,
        overflowing_dimensions_dds_path,
        (std::numeric_limits<std::uint32_t>::max)(),
        1u,
        16u);
    helen::TextureReplacementAsset overflowing_dimensions_asset{};
    failure_result = S_OK;
    const bool overflowing_dimensions_succeeded = helen::TextureReplacementAssetLoader::TryLoadDds(
        overflowing_dimensions_dds_path,
        overflowing_dimensions_asset,
        failure_result);
    Expect(!overflowing_dimensions_succeeded, "Expected compressed DDS dimensions that overflow block layout to be rejected.");
    Expect(failure_result == E_FAIL, "Expected overflowing compressed DDS dimensions to report E_FAIL.");

    const std::filesystem::path batman_dds_path = GetBatmanSubtitleReplacementDdsPath();
    helen::TextureReplacementAsset batman_asset{};
    failure_result = E_FAIL;
    const bool batman_succeeded = helen::TextureReplacementAssetLoader::TryLoadDds(
        batman_dds_path,
        batman_asset,
        failure_result);
    Expect(batman_succeeded, "Expected the checked-in Batman A8R8G8B8 DDS replacement asset to load successfully.");
    Expect(failure_result == S_OK, "Expected a successful HRESULT for the checked-in Batman DDS replacement asset.");
    Expect(batman_asset.Width == 1024u, "Checked-in Batman replacement asset width mismatch.");
    Expect(batman_asset.Height == 1024u, "Checked-in Batman replacement asset height mismatch.");
    Expect(batman_asset.Format == D3DFMT_A8R8G8B8, "Checked-in Batman replacement asset format mismatch.");
    Expect(batman_asset.Level0Bytes.size() == 1024u * 1024u * 4u, "Checked-in Batman replacement payload size mismatch.");

    const std::filesystem::path malformed_dds_path = test_directory / "replacement-missing-required-flags.dds";
    WriteDdsWithHeaderFlags(batman_dds_path, malformed_dds_path, 0x00000008u);
    helen::TextureReplacementAsset malformed_asset{};
    failure_result = S_OK;
    const bool malformed_succeeded = helen::TextureReplacementAssetLoader::TryLoadDds(
        malformed_dds_path,
        malformed_asset,
        failure_result);
    Expect(!malformed_succeeded, "Expected an A8R8G8B8 DDS missing required header flags to be rejected.");
    Expect(failure_result == E_FAIL, "Expected a malformed DDS header to report E_FAIL.");

    const std::filesystem::path invalid_pitch_dds_path = test_directory / "replacement-invalid-pitch.dds";
    WriteDdsWithHeaderField(batman_dds_path, invalid_pitch_dds_path, 20, 2048u);
    helen::TextureReplacementAsset invalid_pitch_asset{};
    failure_result = S_OK;
    const bool invalid_pitch_succeeded = helen::TextureReplacementAssetLoader::TryLoadDds(
        invalid_pitch_dds_path,
        invalid_pitch_asset,
        failure_result);
    Expect(!invalid_pitch_succeeded, "Expected an A8R8G8B8 DDS with an invalid pitch to be rejected.");

    const std::filesystem::path invalid_mask_dds_path = test_directory / "replacement-invalid-mask.dds";
    WriteDdsWithHeaderField(batman_dds_path, invalid_mask_dds_path, 92, 0x000000FFu);
    helen::TextureReplacementAsset invalid_mask_asset{};
    failure_result = S_OK;
    const bool invalid_mask_succeeded = helen::TextureReplacementAssetLoader::TryLoadDds(
        invalid_mask_dds_path,
        invalid_mask_asset,
        failure_result);
    Expect(!invalid_mask_succeeded, "Expected an A8R8G8B8 DDS with incompatible channel masks to be rejected.");

    const std::filesystem::path pack_a_root = test_directory / "pack-a";
    const std::filesystem::path pack_a_build_root = pack_a_root / "builds" / "test-build";
    const std::filesystem::path pack_b_root = test_directory / "pack-b";
    const std::filesystem::path pack_b_build_root = pack_b_root / "builds" / "test-build";
    const std::filesystem::path pack_a_dds_path = pack_a_build_root / "assets" / "replacement.dds";
    const std::filesystem::path pack_b_dds_path = pack_b_build_root / "assets" / "replacement.dds";
    WriteTestDds(pack_a_dds_path, 0x35545844u, 16u);
    WriteTestDds(pack_b_dds_path, 0x35545844u, 16u);

    helen::TextureReplacementDefinition replacement_a_definition{};
    replacement_a_definition.Id = "replace-a";
    replacement_a_definition.Api = "d3d9";
    replacement_a_definition.Width = 256u;
    replacement_a_definition.Height = 256u;
    replacement_a_definition.Format = "DXT5";
    replacement_a_definition.Hash = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    replacement_a_definition.ReplacementPath = "assets/replacement.dds";

    helen::TextureReplacementDefinition replacement_b_definition{};
    replacement_b_definition.Id = "replace-b";
    replacement_b_definition.Api = "d3d9";
    replacement_b_definition.Width = 512u;
    replacement_b_definition.Height = 512u;
    replacement_b_definition.Format = "DXT5";
    replacement_b_definition.Hash = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    replacement_b_definition.ReplacementPath = "assets/replacement.dds";

    const helen::PackScopedTextureReplacementDefinition replacement_a(
        "pack-a",
        "test-build",
        helen::PackAssetResolver(pack_a_root, pack_a_build_root),
        replacement_a_definition);
    const helen::PackScopedTextureReplacementDefinition replacement_b(
        "pack-b",
        "test-build",
        helen::PackAssetResolver(pack_b_root, pack_b_build_root),
        replacement_b_definition);

    const std::optional<std::filesystem::path> resolved_a =
        replacement_a.AssetResolver.Resolve(replacement_a.Definition.ReplacementPath);
    const std::optional<std::filesystem::path> resolved_b =
        replacement_b.AssetResolver.Resolve(replacement_b.Definition.ReplacementPath);
    Expect(resolved_a.has_value(), "Expected the first pack-scoped texture replacement asset to resolve.");
    Expect(resolved_b.has_value(), "Expected the second pack-scoped texture replacement asset to resolve.");
    Expect(*resolved_a == pack_a_dds_path, "First pack-scoped texture replacement resolved the wrong asset path.");
    Expect(*resolved_b == pack_b_dds_path, "Second pack-scoped texture replacement resolved the wrong asset path.");
}
