#pragma once

#include <cstdint>
#include <d3d9.h>
#include <vector>

namespace helen
{
    /**
     * @brief Serializes texture dump payloads for runtime diagnostics.
     *
     * The serializer keeps binary file formatting separate from the D3D9 hook layer so the hook
     * can focus on lifecycle capture while tests validate the output encoding independently.
     */
    class TextureDumpSerializer
    {
    public:
        /**
         * @brief Builds a BMP payload from raw uncompressed level-0 texture bytes.
         * @param width Logical image width in pixels.
         * @param height Logical image height in pixels.
         * @param bgra_pixels Packed top-down BGRA pixels sized to `width * height * 4`.
         * @param bitmap_bytes Receives the complete BMP file bytes on success.
         * @return True when the payload is well-formed and the BMP bytes were produced; otherwise false.
         */
        static bool TryBuildBitmapBytes(
            std::uint32_t width,
            std::uint32_t height,
            const std::vector<std::uint8_t>& bgra_pixels,
            std::vector<std::uint8_t>& bitmap_bytes);

        /**
         * @brief Builds a DDS payload from raw compressed level-0 texture bytes.
         * @param format Compressed texture format to encode in the DDS header.
         * @param width Logical image width in pixels.
         * @param height Logical image height in pixels.
         * @param raw_bytes Packed level-0 compressed bytes sized for the supplied format and dimensions.
         * @param serialized_bytes Receives the complete DDS file bytes on success.
         * @return True when the payload is well-formed and the DDS bytes were produced; otherwise false.
         */
        static bool TryBuildDdsBytes(
            D3DFORMAT format,
            std::uint32_t width,
            std::uint32_t height,
            const std::vector<std::uint8_t>& raw_bytes,
            std::vector<std::uint8_t>& serialized_bytes);
    };
}
