#include <HelenHook/TextureDumpSerializer.h>

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace
{
    /**
     * @brief DDS header bit flags used by the serializer.
     */
    constexpr std::uint32_t DdsdCaps = 0x00000001u;
    constexpr std::uint32_t DdsdHeight = 0x00000002u;
    constexpr std::uint32_t DdsdWidth = 0x00000004u;
    constexpr std::uint32_t DdsdPitch = 0x00000008u;
    constexpr std::uint32_t DdsdPixelFormat = 0x00001000u;
    constexpr std::uint32_t DdsdLinearSize = 0x00080000u;

    /**
     * @brief DDS pixel-format flags used for compressed texture payloads.
     */
    constexpr std::uint32_t DdpfFourCc = 0x00000004u;

    /**
     * @brief DDS capability flags required for texture files.
     */
    constexpr std::uint32_t DdscapsTexture = 0x00001000u;

    /**
     * @brief Lowercase hexadecimal digits used only for internal validation helpers.
     */
    constexpr std::uint32_t FourCcDxt1 = 0x31545844u;
    constexpr std::uint32_t FourCcDxt3 = 0x33545844u;
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
     * @brief Returns whether the supplied D3D format is one of the DDS-compressed formats supported by the serializer.
     * @param format Source texture format.
     * @return True when the format is DXT1, DXT3, or DXT5.
     */
    bool IsCompressedTextureFormat(D3DFORMAT format)
    {
        return format == D3DFMT_DXT1 || format == D3DFMT_DXT3 || format == D3DFMT_DXT5;
    }

    /**
     * @brief Returns the expected byte count for one compressed DDS level-0 image.
     * @param format Compressed texture format.
     * @param width Logical image width in pixels.
     * @param height Logical image height in pixels.
     * @return Expected byte count or zero when the format is unsupported.
     */
    std::size_t GetCompressedByteCount(D3DFORMAT format, std::uint32_t width, std::uint32_t height)
    {
        const std::uint32_t blocks_wide = (std::max)(1u, (width + 3u) / 4u);
        const std::uint32_t blocks_high = (std::max)(1u, (height + 3u) / 4u);
        const std::size_t block_count = static_cast<std::size_t>(blocks_wide) * static_cast<std::size_t>(blocks_high);

        switch (format)
        {
        case D3DFMT_DXT1:
            return block_count * 8u;
        case D3DFMT_DXT3:
        case D3DFMT_DXT5:
            return block_count * 16u;
        default:
            return 0;
        }
    }

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
}

namespace helen
{
    bool TextureDumpSerializer::TryBuildBitmapBytes(
        std::uint32_t width,
        std::uint32_t height,
        const std::vector<std::uint8_t>& bgra_pixels,
        std::vector<std::uint8_t>& bitmap_bytes)
    {
        const std::size_t expected_pixel_bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
        if (width == 0 || height == 0 || bgra_pixels.size() != expected_pixel_bytes)
        {
            return false;
        }

        BITMAPFILEHEADER file_header{};
        BITMAPINFOHEADER info_header{};
        file_header.bfType = 0x4D42;
        file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        file_header.bfSize = static_cast<DWORD>(file_header.bfOffBits + bgra_pixels.size());

        info_header.biSize = sizeof(BITMAPINFOHEADER);
        info_header.biWidth = static_cast<LONG>(width);
        info_header.biHeight = -static_cast<LONG>(height);
        info_header.biPlanes = 1;
        info_header.biBitCount = 32;
        info_header.biCompression = BI_RGB;
        info_header.biSizeImage = static_cast<DWORD>(bgra_pixels.size());

        bitmap_bytes.resize(file_header.bfSize, 0);
        std::memcpy(bitmap_bytes.data(), &file_header, sizeof(file_header));
        std::memcpy(bitmap_bytes.data() + sizeof(file_header), &info_header, sizeof(info_header));
        std::memcpy(bitmap_bytes.data() + file_header.bfOffBits, bgra_pixels.data(), bgra_pixels.size());
        return true;
    }

    bool TextureDumpSerializer::TryBuildDdsBytes(
        D3DFORMAT format,
        std::uint32_t width,
        std::uint32_t height,
        const std::vector<std::uint8_t>& raw_bytes,
        std::vector<std::uint8_t>& serialized_bytes)
    {
        const std::size_t expected_byte_count = GetCompressedByteCount(format, width, height);
        if (!IsCompressedTextureFormat(format) || width == 0 || height == 0 || raw_bytes.size() != expected_byte_count)
        {
            return false;
        }

        DdsHeader header{};
        header.Size = 124u;
        header.Flags = DdsdCaps | DdsdHeight | DdsdWidth | DdsdPixelFormat | DdsdLinearSize;
        header.Height = height;
        header.Width = width;
        header.PitchOrLinearSize = static_cast<std::uint32_t>(raw_bytes.size());
        header.Depth = 0;
        header.MipMapCount = 0;
        header.PixelFormat.Size = 32u;
        header.PixelFormat.Flags = DdpfFourCc;
        header.PixelFormat.FourCc = GetFourCc(format);
        header.PixelFormat.RgbBitCount = 0;
        header.PixelFormat.RedMask = 0;
        header.PixelFormat.GreenMask = 0;
        header.PixelFormat.BlueMask = 0;
        header.PixelFormat.AlphaMask = 0;
        header.Caps = DdscapsTexture;
        header.Caps2 = 0;
        header.Caps3 = 0;
        header.Caps4 = 0;
        header.Reserved2 = 0;

        serialized_bytes.resize(4u + sizeof(DdsHeader) + raw_bytes.size(), 0);
        std::memcpy(serialized_bytes.data(), "DDS ", 4);
        std::memcpy(serialized_bytes.data() + 4, &header, sizeof(header));
        std::memcpy(serialized_bytes.data() + 4 + sizeof(header), raw_bytes.data(), raw_bytes.size());
        return true;
    }
}
