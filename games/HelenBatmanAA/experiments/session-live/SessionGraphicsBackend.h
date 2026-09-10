#pragma once
#include "SessionGraphicsEngine.h"
#include <HelenHook/BatmanGraphicsSessionBackend.h>
#include <atomic>

namespace helen {
    /** @brief Owns verified session state across editor lifetimes and locks ambiguous partial application. */
    class SessionGraphicsBackend final : public BatmanGraphicsSessionBackend {
    private:
        /** @brief Required native dependency, initialized before this backend and destroyed after it. */
        SessionGraphicsEngine& Engine;
        /** @brief Serializes captures and Apply; reentry is rejected rather than waited on. */
        mutable std::atomic_flag Busy = ATOMIC_FLAG_INIT;
        /** @brief No snapshot exists until the first successful complete engine capture. */
        mutable std::optional<BatmanGraphicsDraftState> Applied;
        /** @brief Process-session integrity latch; editor reopen never clears it. */
        mutable std::atomic<bool> Locked{false};
        /** @brief Converts the last verified draft to a complete immutable read snapshot. */
        BatmanGraphicsSnapshot Snapshot() const;
    public:
        /** @brief Establishes ownership without touching engine state before startup initialization. */
        explicit SessionGraphicsBackend(SessionGraphicsEngine& engine);
        /** @brief Captures once from native state, then serves only the last verified session state. */
        BatmanGraphicsSnapshot CaptureReadSnapshot() const override;
        /** @brief Reports integrity lockout independently of editor and transaction identifiers. */
        bool IsApplyLocked() const noexcept override;
        /** @brief Projects initialized native capabilities into the primitive menu protocol. */
        int SupportedFields() const noexcept override;
        /** @brief Applies one validated whole delta; publishes SessionApplied only after complete native verification. */
        BatmanGraphicsApplyResult ApplySessionDraft(const BatmanGraphicsDraftState& baseline,
            const BatmanGraphicsDraftState& draft) const override;
    };
}
