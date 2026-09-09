#pragma once
#include <cstdint>

namespace helen {
    /** @brief Typed access to a verified four-byte Boolean; caller owns storage and engine synchronization. */
    class VsyncScalar {
    private:
        /** @brief Required borrowed field; no adjacent settings are owned or copied. */
        std::uint32_t& Field;
    public:
        /** @brief Binds existing storage without reading, normalizing or modifying it. */
        explicit VsyncScalar(std::uint32_t& field) noexcept;
        /** @brief Reads supported zero/one representations, throwing on any other value. */
        bool Read() const;
        /** @brief Validates existing representation before storing exactly one zero/one word. */
        void Write(bool enabled);
    };
}
