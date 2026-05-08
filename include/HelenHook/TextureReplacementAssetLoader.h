#pragma once

#include <cstdint>
#include <d3d9.h>
#include <filesystem>
#include <vector>

namespace helen
{
    /**
     * @brief Describes one replacement DDS asset loaded from disk for texture swap decisions.
     *
     * The loader returns the replacement image dimensions, format, and raw level-0 payload so the
     * runtime can create a higher-resolution texture object instead of rewriting the source texture
     * in place.
     */
    struct TextureReplacementAsset
    {
        /** @brief Replacement texture width in pixels. */
        std::uint32_t Width{};
        /** @brief Replacement texture height in pixels. */
        std::uint32_t Height{};
        /** @brief Replacement texture format. */
        D3DFORMAT Format{};
        /** @brief Raw level-0 bytes copied from the DDS payload. */
        std::vector<std::uint8_t> Level0Bytes;
    };

    /**
     * @brief Loads replacement DDS assets without requiring the source and replacement dimensions to match.
     *
     * The loader validates that the DDS header is well-formed, reads the declared image dimensions
     * and format, and returns the level-0 compressed payload bytes for later upload into a live
     * replacement texture object.
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
