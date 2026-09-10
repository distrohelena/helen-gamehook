#pragma once
#include <HelenHook/BatmanGraphicsField.h>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace helen {
    /** @brief Explicit binding distinguishes an engine value offset from a genuinely separate renderer mirror. */
    struct BloomEffectBinding {
        /** @brief Byte offset within the stock owner+4 value payload, independently extracted from C20130. */
        std::size_t DataOffset;
        /** @brief Separate renderer mirror when C24B50 transfers this field; absent for directly consumed Bloom. */
        std::optional<std::uintptr_t> RenderMirror;
        /** @brief Requires a concrete payload offset and explicit mirror availability. */
        BloomEffectBinding(std::size_t offset, std::optional<std::uintptr_t> mirror) : DataOffset(offset), RenderMirror(mirror) {}
        /** @brief Selects only the two supported probe fields and refuses unrelated options before mutation. */
        static BloomEffectBinding For(BatmanGraphicsField field);
    };
}
