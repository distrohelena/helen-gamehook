#include <HelenHook/TextureReplacementAssetLoader.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <limits>

namespace
{
    /** @brief DDS header bit flags used by the replacement asset loader. */
    constexpr std::uint32_t DdsdCaps = 0x00000001u;
    /** @brief DDS header bit flags used by the replacement asset loader. */
    constexpr std::uint32_t DdsdHeight = 0x00000002u;
    /** @brief DDS header bit flags used by the replacement asset loader. */
    constexpr std::uint32_t DdsdWidth = 0x00000004u;
    /** @brief DDS header flag indicating that `PitchOrLinearSize` contains an uncompressed row pitch. */
    constexpr std::uint32_t DdsdPitch = 0x00000008u;
    /** @brief DDS header bit flags used by the replacement asset loader. */
    constexpr std::uint32_t DdsdPixelFormat = 0x00001000u;
    /** @brief DDS header bit flags used by the replacement asset loader. */
    constexpr std::uint32_t DdsdLinearSize = 0x00080000u;

    /** @brief DDS pixel-format flag used by the replacement asset loader. */
    constexpr std::uint32_t DdpfFourCc = 0x00000004u;
    /** @brief DDS pixel-format flag indicating that the payload contains an explicit alpha channel. */
    constexpr std::uint32_t DdpfAlphaPixels = 0x00000001u;
    /** @brief DDS pixel-format flag indicating that the payload contains uncompressed RGB channels. */
    constexpr std::uint32_t DdpfRgb = 0x00000040u;

    /** @brief Red-channel mask required for Direct3D `A8R8G8B8` byte layout. */
    constexpr std::uint32_t A8R8G8B8RedMask = 0x00FF0000u;
    /** @brief Green-channel mask required for Direct3D `A8R8G8B8` byte layout. */
    constexpr std::uint32_t A8R8G8B8GreenMask = 0x0000FF00u;
    /** @brief Blue-channel mask required for Direct3D `A8R8G8B8` byte layout. */
    constexpr std::uint32_t A8R8G8B8BlueMask = 0x000000FFu;
    /** @brief Alpha-channel mask required for Direct3D `A8R8G8B8` byte layout. */
    constexpr std::uint32_t A8R8G8B8AlphaMask = 0xFF000000u;

    /** @brief DDS capability flag required for texture files. */
    constexpr std::uint32_t DdscapsTexture = 0x00001000u;

    /** @brief FourCC value for DDS-compressed DXT1 payloads. */
    constexpr std::uint32_t FourCcDxt1 = 0x31545844u;
    /** @brief FourCC value for DDS-compressed DXT3 payloads. */
    constexpr std::uint32_t FourCcDxt3 = 0x33545844u;
    /** @brief FourCC value for DDS-compressed DXT5 payloads. */
    constexpr std::uint32_t FourCcDxt5 = 0x35545844u;

#pragma pack(push, 1)
    /**
     * @brief Raw DDS pixel-format header block.
     */
    struct DdsPixelFormat
    {
        std::uint32_t Size;
        std::uint32_t Flags;
        std::uint32_t FourCc;
        std::uint32_t RgbBitCount;
        std::uint32_t RedMask;
        std::uint32_t GreenMask;
        std::uint32_t BlueMask;
        std::uint32_t AlphaMask;
    };

    /**
     * @brief Raw DDS header block that follows the `DDS ` magic number.
     */
    struct DdsHeader
    {
        std::uint32_t Size;
        std::uint32_t Flags;
        std::uint32_t Height;
        std::uint32_t Width;
        std::uint32_t PitchOrLinearSize;
        std::uint32_t Depth;
        std::uint32_t MipMapCount;
        std::uint32_t Reserved1[11];
        DdsPixelFormat PixelFormat;
        std::uint32_t Caps;
        std::uint32_t Caps2;
        std::uint32_t Caps3;
        std::uint32_t Caps4;
        std::uint32_t Reserved2;
    };
#pragma pack(pop)

    /**
     * @brief Returns the FourCC value written into the DDS header for one supported compressed format.
     * @param format Source texture format.
     * @return FourCC value for the compressed format or zero when unsupported.
     */
    std::uint32_t GetFourCc(D3DFORMAT format)
    {
        switch (format)
        {
        case D3DFMT_DXT1:
            return FourCcDxt1;
        case D3DFMT_DXT3:
            return FourCcDxt3;
        case D3DFMT_DXT5:
            return FourCcDxt5;
        default:
            return 0;
        }
    }

    /**
     * @brief Returns the level-0 byte count required by one supported DDS-compressed texture.
     * @param format Compressed texture format.
     * @param width Texture width in pixels.
     * @param height Texture height in pixels.
     * @param row_count Receives the number of logical block rows.
     * @param bytes_per_row Receives the byte count for one logical block row.
     * @return True when the format is supported; otherwise false.
     */
    bool TryGetCompressedLayout(
        D3DFORMAT format,
        std::uint32_t width,
        std::uint32_t height,
        std::size_t& row_count,
        std::size_t& bytes_per_row)
    {
        row_count = 0;
        bytes_per_row = 0;

        const std::size_t blocks_wide =
            static_cast<std::size_t>(width / 4u) + static_cast<std::size_t>((width % 4u) != 0u);
        const std::size_t blocks_high =
            static_cast<std::size_t>(height / 4u) + static_cast<std::size_t>((height % 4u) != 0u);

        std::size_t block_size = 0u;

        switch (format)
        {
        case D3DFMT_DXT1:
            block_size = 8u;
            break;
        case D3DFMT_DXT3:
        case D3DFMT_DXT5:
            block_size = 16u;
            break;
        default:
            return false;
        }

        if (blocks_wide > (std::numeric_limits<std::size_t>::max)() / block_size)
        {
            return false;
        }

        row_count = blocks_high;
        bytes_per_row = blocks_wide * block_size;
        return bytes_per_row != 0u &&
            row_count <= (std::numeric_limits<std::size_t>::max)() / bytes_per_row;
    }

    /**
     * @brief Loads one complete binary file into memory for replacement validation.
     * @param file_path Path of the replacement asset that should be read.
     * @param bytes Receives the complete file contents on success.
     * @param failure_result Receives the HRESULT-style failure reason when the file cannot be loaded.
     * @return True when the file is readable and copied into memory; otherwise false.
     */
    bool TryReadAllBytes(const std::filesystem::path& file_path, std::vector<std::uint8_t>& bytes, HRESULT& failure_result)
    {
        failure_result = S_OK;
        bytes.clear();

        std::error_code error_code;
        const std::uintmax_t file_size = std::filesystem::file_size(file_path, error_code);
        if (error_code)
        {
            failure_result = HRESULT_FROM_WIN32(error_code.value());
            return false;
        }

        if (file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()) ||
            file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
        {
            failure_result = E_FAIL;
            return false;
        }

        std::ifstream stream(file_path, std::ios::binary);
        if (!stream)
        {
            failure_result = HRESULT_FROM_WIN32(GetLastError());
            return false;
        }

        bytes.resize(static_cast<std::size_t>(file_size), 0);
        if (!bytes.empty())
        {
            stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            if (stream.gcount() != static_cast<std::streamsize>(bytes.size()) || stream.bad())
            {
                failure_result = E_FAIL;
                bytes.clear();
                return false;
            }
        }

        return true;
    }
}

namespace helen
{
    bool TextureReplacementAssetLoader::TryLoadDds(
        const std::filesystem::path& file_path,
        TextureReplacementAsset& asset,
        HRESULT& failure_result)
    {
        failure_result = S_OK;
        asset = {};

        std::vector<std::uint8_t> file_bytes;
        if (!TryReadAllBytes(file_path, file_bytes, failure_result))
        {
            return false;
        }

        if (file_bytes.size() < 4u + sizeof(DdsHeader) || std::memcmp(file_bytes.data(), "DDS ", 4) != 0)
        {
            failure_result = E_FAIL;
            return false;
        }

        DdsHeader header{};
        std::memcpy(&header, file_bytes.data() + 4u, sizeof(header));
        const std::uint32_t required_header_flags = DdsdCaps | DdsdHeight | DdsdWidth | DdsdPixelFormat;
        if (header.Size != 124u ||
            header.PixelFormat.Size != 32u ||
            header.Width == 0u ||
            header.Height == 0u ||
            (header.Flags & required_header_flags) != required_header_flags ||
            (header.Caps & DdscapsTexture) == 0u)
        {
            failure_result = E_FAIL;
            return false;
        }

        D3DFORMAT format = D3DFMT_UNKNOWN;
        std::size_t row_count = 0u;
        std::size_t bytes_per_row = 0u;
        bool is_compressed = false;
        if (header.PixelFormat.Flags == DdpfFourCc)
        {
            if ((header.Flags & DdsdLinearSize) == 0u)
            {
                failure_result = E_FAIL;
                return false;
            }

            is_compressed = true;
            const std::uint32_t four_cc = header.PixelFormat.FourCc;
            if (four_cc == FourCcDxt1)
            {
                format = D3DFMT_DXT1;
            }
            else if (four_cc == FourCcDxt3)
            {
                format = D3DFMT_DXT3;
            }
            else if (four_cc == FourCcDxt5)
            {
                format = D3DFMT_DXT5;
            }
            else
            {
                failure_result = E_FAIL;
                return false;
            }

            if (!TryGetCompressedLayout(format, header.Width, header.Height, row_count, bytes_per_row))
            {
                failure_result = E_FAIL;
                return false;
            }
        }
        else if (
            header.PixelFormat.Flags == (DdpfRgb | DdpfAlphaPixels) &&
            header.PixelFormat.FourCc == 0u &&
            header.PixelFormat.RgbBitCount == 32u &&
            header.PixelFormat.RedMask == A8R8G8B8RedMask &&
            header.PixelFormat.GreenMask == A8R8G8B8GreenMask &&
            header.PixelFormat.BlueMask == A8R8G8B8BlueMask &&
            header.PixelFormat.AlphaMask == A8R8G8B8AlphaMask)
        {
            if ((header.Flags & DdsdPitch) == 0u ||
                header.Width > (std::numeric_limits<std::uint32_t>::max)() / 4u)
            {
                failure_result = E_FAIL;
                return false;
            }

            format = D3DFMT_A8R8G8B8;
            row_count = static_cast<std::size_t>(header.Height);
            bytes_per_row = static_cast<std::size_t>(header.Width) * 4u;
            if (static_cast<std::size_t>(header.PitchOrLinearSize) != bytes_per_row)
            {
                failure_result = E_FAIL;
                return false;
            }
        }
        else
        {
            failure_result = E_FAIL;
            return false;
        }

        if (bytes_per_row == 0u || row_count > (std::numeric_limits<std::size_t>::max)() / bytes_per_row)
        {
            failure_result = E_FAIL;
            return false;
        }

        const std::size_t expected_payload_size = row_count * bytes_per_row;
        if (is_compressed &&
            (expected_payload_size > (std::numeric_limits<std::uint32_t>::max)() ||
                header.PitchOrLinearSize != static_cast<std::uint32_t>(expected_payload_size)))
        {
            failure_result = E_FAIL;
            return false;
        }

        const std::size_t payload_offset = 4u + sizeof(DdsHeader);
        const std::size_t payload_size = file_bytes.size() - payload_offset;
        if (payload_size != expected_payload_size)
        {
            failure_result = E_FAIL;
            return false;
        }

        asset.Width = header.Width;
        asset.Height = header.Height;
        asset.Format = format;
        asset.Level0Bytes.assign(file_bytes.begin() + static_cast<std::ptrdiff_t>(payload_offset), file_bytes.end());
        return true;
    }
}
