#include "SessionGraphicsBackend.h"
#include <stdexcept>
#include <HelenHook/Log.h>
#include <HelenHook/BatmanGraphicsOperationGuard.h>
namespace helen {
    SessionGraphicsBackend::SessionGraphicsBackend(SessionGraphicsEngine& engine) : Engine(engine) {}
    BatmanGraphicsSnapshot SessionGraphicsBackend::Snapshot() const {
        if (!Applied.has_value()) { throw std::logic_error("Session has no complete engine capture"); }
        BatmanGraphicsSnapshot::Values values;
        for (unsigned index=0; index<14; ++index) { values[index] = Applied->Get(static_cast<BatmanGraphicsField>(index)); }
        return BatmanGraphicsSnapshot(values);
    }
    BatmanGraphicsSnapshot SessionGraphicsBackend::CaptureReadSnapshot() const {
        const BatmanGraphicsOperationGuard guard(Busy);
        if (!guard.IsAcquired()) { throw std::runtime_error("Session operation is already active"); }
        if (!Applied.has_value()) {
            const BatmanGraphicsSnapshot captured = Engine.Capture();
            const std::optional<BatmanGraphicsDraftState> draft = captured.TryCreateDraft();
            if (!draft.has_value()) { return captured; }
            Applied = draft;
        }
        return Snapshot();
    }
    bool SessionGraphicsBackend::IsApplyLocked() const noexcept { return Locked.load(); }
    int SessionGraphicsBackend::SupportedFields() const noexcept {
        const auto supported = Engine.Capabilities();
        int mask = 0;
        for (unsigned index=0; index<supported.size(); ++index) {
            if (supported[index]) { mask |= 1 << index; }
        }
        return mask;
    }
    BatmanGraphicsApplyResult SessionGraphicsBackend::ApplySessionDraft(const BatmanGraphicsDraftState& baseline,
        const BatmanGraphicsDraftState& draft) const {
        const BatmanGraphicsOperationGuard guard(Busy);
        if (!guard.IsAcquired()) { return BatmanGraphicsApplyResult(BatmanGraphicsApplyOutcome::NotApplied,{}); }
        if (Locked.load()) { return BatmanGraphicsApplyResult(BatmanGraphicsApplyOutcome::IntegrityUncertain,{}); }
        bool mutationStarted = false;
        const char* stage = "validation";
        try {
            if (!Applied.has_value()) { throw std::runtime_error("Apply requires a complete initial engine capture"); }
            for (unsigned index=0; index<14; ++index) {
                const BatmanGraphicsField field = static_cast<BatmanGraphicsField>(index);
                if (baseline.Get(field) != Applied->Get(field)) { throw std::runtime_error("Stale applied-session baseline"); }
            }
            const SessionGraphicsDelta delta(baseline,draft,Engine.Capabilities());
            if (delta.Fields().empty()) { return BatmanGraphicsApplyResult(BatmanGraphicsApplyOutcome::SessionApplied,{}); }
            Engine.Preflight(baseline,draft,delta);
            stage = "session staging";
            mutationStarted = true;
            Engine.Stage(draft,delta);
            stage = "native apply";
            Engine.Apply(draft,delta);
            stage = "readback";
            Engine.Verify(draft,delta);
            Applied = draft;
            Logf(L"[session-live] PASS: %u changed fields verified; session baseline published; no persistent save.",
                static_cast<unsigned>(delta.Fields().size()));
            return BatmanGraphicsApplyResult(BatmanGraphicsApplyOutcome::SessionApplied,{});
        } catch (const std::exception& error) {
            Locked.store(mutationStarted);
            Logf(L"[session-live] STOP stage=%hs integrity-locked=%d: %hs",stage,mutationStarted,error.what());
            return BatmanGraphicsApplyResult(mutationStarted ? BatmanGraphicsApplyOutcome::IntegrityUncertain : BatmanGraphicsApplyOutcome::NotApplied,{});
        }
    }
}
