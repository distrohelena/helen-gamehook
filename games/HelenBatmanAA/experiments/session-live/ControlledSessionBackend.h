#pragma once
#include <HelenHook/BatmanGraphicsSessionBackend.h>
#include <HelenHook/BatmanGraphicsDraftState.h>
#include <functional>
#include <stdexcept>

/** @brief Controlled native boundary for exercising the real editor protocol, not a claim to simulate Batman rendering. */
class ControlledSessionBackend final : public helen::BatmanGraphicsSessionBackend {
public:
    /** @brief Authoritative fixture state, advanced only when this boundary reports verified session application. */
    mutable helen::BatmanGraphicsDraftState Applied;
    /** @brief Explicit next outcome allows rejection and integrity-lock testing without GPU calls. */
    helen::BatmanGraphicsApplyOutcome Next = helen::BatmanGraphicsApplyOutcome::SessionApplied;
    /** @brief Backend-lifetime uncertainty survives closing or replacing editors. */
    mutable bool Locked = false;
    /** @brief Optional callback exercises reentrant protocol calls while Commit owns its guard. */
    std::function<void()> DuringApply;
    /** @brief Requires a complete valid initial state rather than synthesizing configuration defaults. */
    explicit ControlledSessionBackend(const helen::BatmanGraphicsDraftState& initial) : Applied(initial) {}
    /** @brief Returns all normalized values as captured state for the real session to transfer. */
    helen::BatmanGraphicsSnapshot CaptureReadSnapshot() const override {
        helen::BatmanGraphicsSnapshot::Values values;
        for (std::size_t index = 0; index < values.size(); ++index) {
            values[index] = Applied.Get(static_cast<helen::BatmanGraphicsField>(index));
        }
        return helen::BatmanGraphicsSnapshot(std::move(values));
    }
    /** @brief Reports injected integrity uncertainty independently of editor lifetime. */
    bool IsApplyLocked() const noexcept override { return Locked; }
    /** @brief This protocol fixture supports all fields; native capability filtering has separate tests. */
    int SupportedFields() const noexcept override { return 16383; }
    /** @brief Rejects stale baselines, optionally reenters, and publishes only successful fixture outcomes. */
    helen::BatmanGraphicsApplyResult ApplySessionDraft(const helen::BatmanGraphicsDraftState& baseline,
        const helen::BatmanGraphicsDraftState& draft) const override {
        for (unsigned index = 0; index < 14; ++index) {
            const helen::BatmanGraphicsField field = static_cast<helen::BatmanGraphicsField>(index);
            if (baseline.Get(field) != Applied.Get(field)) { throw std::runtime_error("Session passed stale baseline"); }
        }
        if (DuringApply) { DuringApply(); }
        if (Next == helen::BatmanGraphicsApplyOutcome::SessionApplied) { Applied = draft; }
        if (Next == helen::BatmanGraphicsApplyOutcome::IntegrityUncertain) { Locked = true; }
        return helen::BatmanGraphicsApplyResult(Next, {});
    }
};
