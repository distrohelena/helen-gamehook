#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace helen
{
    /**
     * @brief Describes one runtime graphics texture replacement rule declared by a build.
     *
     * The first implementation slice targets `D3D9` textures matched by dimensions, format, and
     * a stable content hash while preserving the game's existing text layout behavior.
     */
    class TextureReplacementDefinition
    {
    public:
        /** @brief Stable replacement identifier used for diagnostics and future tooling. */
        std::string Id;
        /** @brief Graphics API identifier such as `d3d9`. */
        std::string Api;
        /** @brief Required source texture width in pixels. */
        std::uint32_t Width{};
        /** @brief Required source texture height in pixels. */
        std::uint32_t Height{};
        /** @brief Required source texture format token such as `A8R8G8B8`. */
        std::string Format;
        /** @brief Required lowercase SHA-256 hex digest of the source texture bytes. */
        std::string Hash;
        /** @brief Build-relative path to the replacement image asset. */
        std::filesystem::path ReplacementPath;
        /** @brief Optional sampler stage used to narrow replacement scope for shared atlases. */
        std::optional<int> SamplerStage;
    };
}
