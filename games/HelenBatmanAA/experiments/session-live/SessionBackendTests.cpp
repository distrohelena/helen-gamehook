#include "SessionGraphicsBackend.h"
#include "ControlledSessionEngine.h"
#include <iostream>

/** @brief Throws console failures without native assertion dialogs. */
void Expect(bool condition, const char* message) { if (!condition) { throw std::runtime_error(message); } }
/** @brief Checks the production coordinator's repeat, no-op, mixed rejection, stale baseline and partial-failure semantics. */
int main() {
    try {
        using namespace helen;
        const auto initial = BatmanGraphicsDraftState::TryCreate({0,1,0,0,0,0,0,0,1,0,1,0,1920,1080});
        Expect(initial.has_value(),"Invalid fixture");
        BatmanGraphicsDraftState draft = *initial;
        Expect(draft.TrySet(BatmanGraphicsField::Bloom,1) && draft.TrySet(BatmanGraphicsField::DynamicShadows,1),"Draft failed");
        ControlledSessionEngine engine(*initial);
        SessionGraphicsBackend backend(engine);
        Expect(backend.CaptureReadSnapshot().Get(BatmanGraphicsField::Bloom)==0,"Initial capture failed");
        engine.DuringApply = [&]() {
            Expect(backend.ApplySessionDraft(*initial,draft).Outcome==BatmanGraphicsApplyOutcome::NotApplied,"Reentrant Apply accepted");
        };
        Expect(backend.ApplySessionDraft(*initial,draft).Outcome==BatmanGraphicsApplyOutcome::SessionApplied,"Complete Apply not published");
        Expect(engine.Calls=="PSAV","Wrong stage order");
        Expect(backend.CaptureReadSnapshot().Get(BatmanGraphicsField::Bloom)==1,"Session snapshot stale");
        engine.DuringApply = {};
        engine.Calls.clear();
        Expect(backend.ApplySessionDraft(draft,draft).Outcome==BatmanGraphicsApplyOutcome::SessionApplied && engine.Calls.empty(),"No-op invoked engine");
        Expect(backend.ApplySessionDraft(*initial,draft).Outcome==BatmanGraphicsApplyOutcome::NotApplied && engine.Calls.empty(),"Stale baseline accepted");
        Expect(backend.ApplySessionDraft(draft,*initial).Outcome==BatmanGraphicsApplyOutcome::SessionApplied,"Reverse Apply failed");
        Expect(backend.CaptureReadSnapshot().Get(BatmanGraphicsField::Bloom)==0,"Reverse state not published");
        BatmanGraphicsDraftState unsupported = draft;
        Expect(unsupported.TrySet(BatmanGraphicsField::Physx,2),"PhysX draft failed");
        engine.Calls.clear();
        Expect(backend.ApplySessionDraft(*initial,unsupported).Outcome==BatmanGraphicsApplyOutcome::NotApplied && engine.Calls.empty(),"Unsupported mixed edit mutated state");
        for (const std::string failure : {"P","S","A","V"}) {
            ControlledSessionEngine failing(*initial);
            SessionGraphicsBackend coordinator(failing);
            (void)coordinator.CaptureReadSnapshot();
            failing.FailAt = failure;
            const auto result = coordinator.ApplySessionDraft(*initial,draft);
            const bool uncertain = failure != "P";
            Expect(result.Outcome==(uncertain ? BatmanGraphicsApplyOutcome::IntegrityUncertain : BatmanGraphicsApplyOutcome::NotApplied),"Failure outcome concealed possible mutation");
            Expect(coordinator.IsApplyLocked()==uncertain,"Incorrect failure lock");
            Expect(coordinator.CaptureReadSnapshot().Get(BatmanGraphicsField::Bloom)==0,"Failed draft published as applied");
            if (uncertain) {
                const std::string calls = failing.Calls;
                (void)coordinator.ApplySessionDraft(*initial,draft);
                Expect(failing.Calls==calls,"Locked coordinator reentered engine");
            }
        }
        std::cout << "SESSION_BACKEND_PASS\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
