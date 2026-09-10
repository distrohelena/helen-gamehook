#include "ControlledSessionBackend.h"
#include <HelenHook/BatmanGraphicsSessionService.h>
#include <HelenHook/BatmanDisplayModeService.h>
#include <iostream>

/** @brief Fails through the console, never a graphical assertion dialog. */
void Expect(bool condition, const char* message) { if (!condition) { throw std::runtime_error(message); } }
/** @brief Supplies a stable display environment so protocol tests do not depend on GPU enumeration. */
std::optional<helen::BatmanDisplayEnvironment> Environment() {
    return helen::BatmanDisplayEnvironment(L"SessionProtocol", {{1280,720}}, helen::BatmanDisplayMode(1280,720), helen::BatmanDisplayMode(1280,720));
}
/** @brief Exercises repeat/reverse Apply, reopen, consumed IDs, reentry, rejected drafts and persistent lockout through the real service. */
int main() {
    try {
        const auto initial = helen::BatmanGraphicsDraftState::TryCreate({0,0,0,1,1,0,0,0,1,0,0,0,1280,720});
        Expect(initial.has_value(), "Invalid fixture");
        ControlledSessionBackend backend(*initial);
        helen::BatmanDisplayModeService display(&Environment);
        helen::BatmanGraphicsSessionService sessions(backend, display);
        const auto session = sessions.Open();
        Expect(session.has_value(), "Open failed");
        Expect(sessions.Get(*session,static_cast<helen::BatmanGraphicsField>(17)) == 16383,"Backend field capabilities missing");
        Expect(sessions.EndRead(*session), "EndRead failed");
        backend.DuringApply = [&sessions, &session]() {
            Expect(!sessions.BeginApply(*session).has_value() && !sessions.Open().has_value(), "Reentry accepted");
        };
        const auto first = sessions.BeginApply(*session);
        Expect(first.has_value() && sessions.SetField(*session,*first,helen::BatmanGraphicsField::Bloom,0), "First staging failed");
        const auto firstResult = sessions.Commit(*session,*first);
        Expect(firstResult.has_value() && firstResult->Outcome == helen::BatmanGraphicsApplyOutcome::SessionApplied, "First session success lost");
        Expect(!sessions.Commit(*session,*first).has_value(), "Transaction replay accepted");
        const auto second = sessions.BeginApply(*session);
        Expect(second.has_value() && sessions.SetField(*session,*second,helen::BatmanGraphicsField::Bloom,1), "Reverse staging failed");
        const auto secondResult = sessions.Commit(*session,*second);
        Expect(secondResult.has_value() && secondResult->Outcome == helen::BatmanGraphicsApplyOutcome::SessionApplied,
            "Reverse Apply used a stale baseline");
        backend.Next = helen::BatmanGraphicsApplyOutcome::NotApplied;
        const auto rejected = sessions.BeginApply(*session);
        Expect(rejected.has_value() && sessions.SetField(*session,*rejected,helen::BatmanGraphicsField::Bloom,0), "Rejected staging failed");
        Expect(sessions.Commit(*session,*rejected)->Outcome == helen::BatmanGraphicsApplyOutcome::NotApplied, "Rejection lost");
        backend.Next = helen::BatmanGraphicsApplyOutcome::SessionApplied;
        const auto third = sessions.BeginApply(*session);
        Expect(third.has_value() && sessions.SetField(*session,*third,helen::BatmanGraphicsField::Bloom,0), "Third staging failed");
        Expect(sessions.Commit(*session,*third)->Outcome == helen::BatmanGraphicsApplyOutcome::SessionApplied, "Rejected draft polluted baseline");
        Expect(sessions.Close(*session), "Close failed");
        const auto reopened = sessions.Open();
        Expect(reopened.has_value() && sessions.Get(*reopened,helen::BatmanGraphicsField::Bloom) == 0, "Reopen lost session state");
        Expect(!sessions.BeginApply(*session).has_value(), "Stale editor accepted");
        Expect(sessions.EndRead(*reopened), "Reopen EndRead failed");
        backend.DuringApply = {};
        backend.Next = helen::BatmanGraphicsApplyOutcome::IntegrityUncertain;
        const auto uncertain = sessions.BeginApply(*reopened);
        Expect(uncertain.has_value(), "Uncertain transaction failed to begin");
        Expect(sessions.Commit(*reopened,*uncertain)->Outcome == helen::BatmanGraphicsApplyOutcome::IntegrityUncertain, "Uncertainty lost");
        const auto locked = sessions.Open();
        Expect(locked.has_value() && sessions.Get(*locked,helen::BatmanGraphicsField::CanApply) == 0, "Reopen cleared integrity lock");
        Expect(sessions.EndRead(*locked) && !sessions.BeginApply(*locked).has_value(), "Locked backend accepted Apply");
        std::cout << "SESSION_PROTOCOL_PASS\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
