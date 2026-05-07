#include <cstdint>
#include <d3d9.h>

#include <array>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace helen
{
    /**
     * @brief Serializes texture dump payloads for compressed and uncompressed diagnostics.
     *
     * The runtime implementation will provide the concrete serializer. The test file keeps a local
     * declaration so the build fails at link time until the serializer exists and is wired up.
     */
    class TextureDumpSerializer
    {
    public:
        /**
         * @brief Builds a DDS payload for a compressed `D3D9` texture image.
         * @param format Compressed texture format.
         * @param width Logical image width in pixels.
         * @param height Logical image height in pixels.
         * @param raw_bytes Raw level-0 texture bytes copied from the locked surface.
         * @param serialized_bytes Receives the complete DDS file bytes on success.
         * @return True when the DDS bytes are produced successfully; otherwise false.
         */
        static bool TryBuildDdsBytes(
            D3DFORMAT format,
            std::uint32_t width,
            std::uint32_t height,
            const std::vector<std::uint8_t>& raw_bytes,
            std::vector<std::uint8_t>& serialized_bytes);
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
     * @brief Reads one little-endian 32-bit integer from a byte span.
     * @param bytes Input buffer whose leading bytes should be interpreted.
     * @param offset Byte offset into `bytes`.
     * @return Little-endian 32-bit value stored at the requested offset.
     */
    std::uint32_t ReadU32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
    {
        return
            static_cast<std::uint32_t>(bytes[offset + 0]) |
            (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
            (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
            (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    }
}

/**
 * @brief Verifies that compressed `D3D9` texture dumps can be serialized as DDS payloads for later inspection.
 */
void RunTextureDumpSerializerTests()
{
    const std::vector<std::uint8_t> raw_bytes{
        0x01, 0x02, 0x03, 0x04,
        0x05, 0x06, 0x07, 0x08};

    std::vector<std::uint8_t> serialized_bytes;
    const bool succeeded = helen::TextureDumpSerializer::TryBuildDdsBytes(
        D3DFMT_DXT1,
        4,
        4,
        raw_bytes,
        serialized_bytes);
    Expect(succeeded, "Expected DDS serialization for DXT1 to succeed.");
    Expect(serialized_bytes.size() == 4u + 124u + raw_bytes.size(), "DDS payload size mismatch.");
    Expect(std::string_view(reinterpret_cast<const char*>(serialized_bytes.data()), 4) == "DDS ", "DDS magic mismatch.");
    Expect(ReadU32(serialized_bytes, 4) == 124u, "DDS header size mismatch.");
    Expect(ReadU32(serialized_bytes, 12) == 4u, "DDS height mismatch.");
    Expect(ReadU32(serialized_bytes, 16) == 4u, "DDS width mismatch.");
    Expect(ReadU32(serialized_bytes, 84) == 0x31545844u, "DDS DXT1 fourCC mismatch.");
}
