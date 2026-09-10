#pragma once

#include <cstdint>

namespace helen {
    /** @brief Stable normalized scalar identifiers for the direct Batman graphics interface. */
    enum class BatmanGraphicsField : std::uint32_t {
        /** @brief Zero for windowed, one for fullscreen. */
        Fullscreen = 0,
        /** @brief Vertical synchronization toggle. */
        Vsync = 1,
        /** @brief Normalized sample setting; 16x is five, not display position four. */
        Msaa = 2,
        /** @brief Bloom detail toggle. */
        Bloom = 3,
        /** @brief Dynamic shadow detail toggle. */
        DynamicShadows = 4,
        /** @brief Motion blur detail toggle. */
        MotionBlur = 5,
        /** @brief Distortion detail toggle. */
        Distortion = 6,
        /** @brief Fog volume detail toggle. */
        FogVolumes = 7,
        /** @brief Enabled lighting state, inverse of the launcher's disable flag. */
        SphericalHarmonicLighting = 8,
        /** @brief Ambient occlusion detail toggle. */
        AmbientOcclusion = 9,
        /** @brief PhysX quality from zero through two. */
        Physx = 10,
        /** @brief Stereo rendering toggle. */
        Stereo = 11,
        /** @brief Positive launcher-configured width, valid only with its height. */
        PersistedWidth = 12,
        /** @brief Positive launcher-configured height, valid only with its width. */
        PersistedHeight = 13,
        /** @brief Session display enumeration's desktop width; not an INI field. */
        DesktopWidth = 14,
        /** @brief Session display enumeration's desktop height; not an INI field. */
        DesktopHeight = 15,
        /** @brief Session transaction availability; not a persisted setting. */
        CanApply = 16,
        /** @brief Backend-supported editable-field bitmask; an observation, never a draft setting. */
        SupportedFields = 17
    };
}
