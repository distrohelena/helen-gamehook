#include "BatmanGraphicsPrimitiveCodec.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace helen {
    std::optional<std::uint32_t> BatmanGraphicsPrimitiveCodec::ReadUnsigned(
        const void* arguments, unsigned count, unsigned index) noexcept {
        if (arguments == nullptr || index >= count ||
            index > (std::numeric_limits<std::size_t>::max() - 15) / 16) {
            return std::nullopt;
        }

        const auto* record = static_cast<const unsigned char*>(arguments) + static_cast<std::size_t>(index) * 16;
        std::uint32_t type;
        std::memcpy(&type, record, sizeof(type));
        if (type != 3) {
            return std::nullopt;
        }

        double value;
        std::memcpy(&value, record + 8, sizeof(value));
        if (!std::isfinite(value) || value < 0.0 ||
            value > static_cast<double>(std::numeric_limits<std::uint32_t>::max()) || std::trunc(value) != value) {
            return std::nullopt;
        }

        return static_cast<std::uint32_t>(value);
    }

    void BatmanGraphicsPrimitiveCodec::WriteNumber(BatmanGraphicsPrimitiveResult& result, std::uint32_t value) noexcept {
        const double encoded = static_cast<double>(value);
        std::memcpy(result.Payload, &encoded, sizeof(encoded));
        result.Type = 3;
    }
}
