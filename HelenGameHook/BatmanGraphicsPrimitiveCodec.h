#pragma once

#include "BatmanGraphicsPrimitiveResult.h"

#include <cstdint>
#include <optional>

namespace helen {
    /** @brief Converts only unsigned integer primitives at the verified Batman ExternalInterface boundary. */
    class BatmanGraphicsPrimitiveCodec {
    public:
        /**
         * @brief Reads a finite, integral uint32 from an engine-converted numeric argument without coercion.
         * @param arguments Borrowed buffer containing count readable 16-byte engine argument records, or null.
         * @param count Actual record count supplied by the engine; adapter separately checks operation arity.
         * @param index Zero-based record to decode; out-of-range, null, and invalid primitives return no value.
         * @return Exact unsigned integer, including zero, or no value for malformed input.
         */
        static std::optional<std::uint32_t> ReadUnsigned(const void* arguments, unsigned count, unsigned index) noexcept;

        /**
         * @brief Writes an exactly representable uint32 as a GAS double into pre-cleared borrowed result storage.
         * @param result Engine result already cleared to undefined; never pass a managed or uncleared value.
         * @param value Integer to return; all uint32 values are exactly representable by the engine double.
         */
        static void WriteNumber(BatmanGraphicsPrimitiveResult& result, std::uint32_t value) noexcept;
    };
}
