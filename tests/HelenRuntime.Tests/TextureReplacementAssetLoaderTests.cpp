#include <cstdint>
#include <d3d9.h>

#include <filesystem>
#include <fstream>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace helen
{
    /**
     * @brief Describes one replacement DDS asset loaded from disk for texture swap decisions.
     */
    struct TextureReplacementAsset
    {
        /** @brief Replacement texture width in pixels. */
        std::uint32_t Width{};
        /** @brief Replacement texture height in pixels. */
        std::uint32_t Height{};
        /** @brief Replacement texture format. */
        D3DFORMAT Format{};
        /** @brief Raw level-0 bytes that can be copied into a live replacement texture. */
        std::vector<std::uint8_t> Level0Bytes;
    };

    /**
     * @brief Loads replacement DDS assets without requiring the source and replacement dimensions to match.
     */
    class TextureReplacementAssetLoader
    {
    public:
        /**
         * @brief Reads one DDS asset from disk and returns its replacement texture metadata and payload.
         * @param file_path DDS asset path.
         * @param asset Receives the loaded replacement texture metadata.
         * @param failure_result Receives the HRESULT-style failure reason on error.
         * @return True when the DDS asset is valid and loadable.
         */
        static bool TryLoadDds(
            const std::filesystem::path& file_path,
            TextureReplacementAsset& asset,
            HRESULT& failure_result);
    };
}

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
     */
    void WriteTestDds(const std::filesystem::path& path)
    {
        const std::uint32_t width = 1024u;
        const std::uint32_t height = 1024u;
        const std::uint32_t blocks_wide = width / 4u;
        const std::uint32_t blocks_high = height / 4u;
        const std::uint32_t payload_size = blocks_wide * blocks_high * 16u;

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
        write_u32(84, 0x35545844u);
        write_u32(108, 0x00001000u);

        for (std::uint32_t block_index = 0; block_index < blocks_wide * blocks_high; ++block_index)
        {
            const std::size_t offset = 128u + static_cast<std::size_t>(block_index) * 16u;
            bytes[offset + 0] = 0xFF;
            bytes[offset + 1] = 0x00;
            bytes[offset + 8] = 0x00;
            bytes[offset + 9] = 0xF8;
        }

        std::filesystem::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
}

/**
 * @brief Verifies that the replacement asset loader accepts a larger DDS replacement texture.
 */
void RunTextureReplacementAssetLoaderTests()
{
    const std::filesystem::path test_directory = std::filesystem::temp_directory_path() / "helenhook-texture-replacement-loader-tests";
    const std::filesystem::path dds_path = test_directory / "replacement-1024.dds";

    WriteTestDds(dds_path);

    helen::TextureReplacementAsset asset{};
    HRESULT failure_result = E_FAIL;
    const bool succeeded = helen::TextureReplacementAssetLoader::TryLoadDds(dds_path, asset, failure_result);
    Expect(succeeded, "Expected a 1024x1024 DDS replacement asset to load successfully.");
    Expect(failure_result == S_OK, "Expected a successful HRESULT for the loaded replacement asset.");
    Expect(asset.Width == 1024u, "Replacement asset width mismatch.");
    Expect(asset.Height == 1024u, "Replacement asset height mismatch.");
    Expect(asset.Format == D3DFMT_DXT5, "Replacement asset format mismatch.");
    Expect(asset.Level0Bytes.size() == 1024u * 1024u, "Replacement asset payload size mismatch.");
}
