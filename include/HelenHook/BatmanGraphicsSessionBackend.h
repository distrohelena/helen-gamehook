#pragma once
#include <HelenHook/BatmanGraphicsSnapshot.h>
#include <HelenHook/BatmanGraphicsApplyResult.h>

namespace helen {
    class BatmanGraphicsDraftState;

    /** @brief Required state/application boundary separating the menu protocol from persistent or session-only settings ownership. */
    class BatmanGraphicsSessionBackend {
    public:
        /** @brief Allows concrete backend resources to retire through their owned polymorphic lifetime. */
        virtual ~BatmanGraphicsSessionBackend() = default;
        /** @brief Captures authoritative values for a new editor; session-only backends retain verified applied state across editors. */
        virtual BatmanGraphicsSnapshot CaptureReadSnapshot() const = 0;
        /** @brief Reports backend-lifetime integrity lockout; reopening an editor must never clear it. */
        virtual bool IsApplyLocked() const noexcept = 0;
        /** @brief Bitmask of editable fields supported by this backend; Apply independently enforces the same capabilities. */
        virtual int SupportedFields() const noexcept = 0;
        /** @brief Applies a complete draft relative to its captured baseline and reports verified persistence or session-only outcome. */
        virtual BatmanGraphicsApplyResult ApplySessionDraft(const BatmanGraphicsDraftState& baseline,
            const BatmanGraphicsDraftState& draft) const = 0;
    };
}
