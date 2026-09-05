#pragma once

#include <HelenHook/BatmanGraphicsDraftState.h>
#include <HelenHook/BatmanGraphicsResolutionSelection.h>
#include <cstdint>

namespace helen {
    /** @brief Owns the isolated attempted draft until a synchronous commit consumes or cancellation discards it. */
    struct BatmanGraphicsTransaction {
        /** @brief Non-reused transaction identity checked together with the session identity. */
        std::uint32_t Id;
        /** @brief Validated complete draft; staging never mutates baseline, dispatcher, or files. */
        BatmanGraphicsDraftState Draft;
        /** @brief Explicit display edit, absent for scalar-only transactions that retain configured dimensions. */
        std::optional<BatmanGraphicsResolutionSelection> Resolution;
        /** @brief Starts from a required complete baseline and explicit identity, without persistence. */
        BatmanGraphicsTransaction(std::uint32_t id, BatmanGraphicsDraftState draft) : Id(id), Draft(std::move(draft)) {
        }
    };
}
