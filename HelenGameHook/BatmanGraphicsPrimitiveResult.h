#pragma once

#include <cstddef>
#include <cstdint>

namespace helen {
    /** @brief Borrowed GAS result storage for the pinned Batman x86 callback boundary; never owns managed values. */
    struct BatmanGraphicsPrimitiveResult {
        /** @brief Engine GAS tag; the caller must supply pre-cleared undefined storage before a primitive write. */
        std::uint8_t Type;
        /** @brief Engine-owned padding that primitive writes must preserve. */
        std::uint8_t Reserved[3];
        /** @brief Primitive payload begins at offset four; numeric values occupy its first eight bytes. */
        std::uint8_t Payload[12];
    };

    static_assert(sizeof(BatmanGraphicsPrimitiveResult) == 16);
    static_assert(offsetof(BatmanGraphicsPrimitiveResult, Payload) == 4);
}
