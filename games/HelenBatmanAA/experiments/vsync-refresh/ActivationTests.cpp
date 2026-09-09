#include "ActivationMatcher.h"
#include "RefreshRequest.h"
#include "RefreshRequestScope.h"
#include "DecisionBridge.h"
#include "AnchoredResize.h"
#include "RefreshActivation.h"
using helen::DecisionBridge;
using helen::SkipTarget;
using helen::RebuildTarget;
#include <windows.h>
#include <iostream>
#include <stdexcept>

static_assert(sizeof(void*) == 4, "This fixture requires x86");

namespace {
    /** @brief The live outer viewport return slot, published only by the top-level invocation. */
    std::uintptr_t Anchor = 0;
    /** @brief Actual code return sites shared by both recursive executions. */
    std::array<std::uintptr_t, 5> Returns{};
    /** @brief Recursion depth distinguishes observations, not matcher decisions. */
    unsigned Depth = 0;
    /** @brief Number of accepted outer observations; exactly one is expected. */
    unsigned OuterAccepted = 0;
    /** @brief Number of accepted nested observations; must stay zero. */
    unsigned NestedAccepted = 0;
    /** @brief Counts actual callback visits so missing nested execution cannot pass. */
    unsigned Visits = 0;
    /** @brief Records stack imbalance after returning from the entire synthetic chain. */
    unsigned StackErrors = 0;
    /** @brief Counts wrong thiscall arguments received by the engine surrogate. */
    unsigned ArgumentErrors = 0;
    /** @brief Shared request exercised by nested and outer activations on the real test stack. */
    helen::RefreshRequest Request;
    /** @brief Reports failure to publish the request from the assembly call wrapper. */
    bool Armed = false;
    /** @brief Counts forbidden duplicate consumption on the same live frame. */
    unsigned DuplicateConsumption = 0;
    /** @brief Counts rebuild destinations reached by the actual outer assembly activation. */
    unsigned OuterRebuilds = 0;
    /** @brief Counts forbidden rebuild destinations in the nested zero-input activation. */
    unsigned NestedRebuilds = 0;
    /** @brief Counts skip destinations so bypassing the bridge cannot satisfy branch assertions. */
    unsigned Skips = 0;
    /** @brief Monotonic identity selected by each root invocation, never a persistent stack address. */
    std::uint64_t CurrentOperation = 1;
    /** @brief Injects a C++ failure from inside the real naked call chain before the root renderer call. */
    bool InjectFailure = false;
    /** @brief Runs the same recursive engine surrogate through the connected activation controller. */
    bool UseController = false;
    /** @brief Counts forbidden cleanup after a refused publication that never acquired ownership. */
    unsigned RefusedCleanup = 0;

    /** @brief Rejects publication to prove the native wrapper never enters the engine on refusal. */
    bool __cdecl RefusePublication(void*, std::uintptr_t, std::uintptr_t) noexcept { return false; }
    /** @brief Detects cleanup of a request that was never armed. */
    void __cdecl CountRefusedCleanup(void*) noexcept { ++RefusedCleanup; }

    /** @brief Arms only the outer invocation; nested calls must not replace its identity. */
    bool __cdecl ArmOuter(void* context, std::uintptr_t anchor, std::uintptr_t returnSite) noexcept {
        if (Depth == 0) {
            Anchor = anchor;
            Returns[4] = returnSite;
            Armed = static_cast<helen::RefreshRequestScope*>(context)->Arm(11, 22, GetCurrentThreadId(), Anchor);
            return Armed;
        }
        return true;
    }

    /** @brief Invalidates the outer anchor before its assembly wrapper returns to its caller. */
    void __cdecl DisarmOuter(void*) noexcept {
        if (Depth == 0) { Request.Disarm(CurrentOperation); }
    }

    /** @brief Throws outside the no-throw decision callback to test unwinding across native fixture frames. */
    void __cdecl ThrowIfRequested() {
        if (InjectFailure && Depth == 0) { throw 42; }
    }

    /** @brief Throws console-reported failures without invoking CRT assertion dialogs. */
    void Expect(bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    }

    /** @brief Detects leaked requests on exception exit and accidental cleanup by rejected nested guards. */
    void CheckScopeCleanup() {
        helen::RefreshRequest request;
        bool caught = false;
        try {
            helen::RefreshRequestScope scope(request, 1);
            Expect(scope.Arm(11, 22, GetCurrentThreadId(), 33), "scope arm failed");
            throw 42;
        } catch (int value) { caught = value == 42; }
        Expect(caught, "exception did not propagate");
        Expect(!request.Consume(11, 22, GetCurrentThreadId(), 33), "exception leaked armed request");
        {
            helen::RefreshRequestScope scope(request, 2);
            Expect(scope.Arm(11, 22, GetCurrentThreadId(), 33), "scope could not rearm");
            {
                helen::RefreshRequestScope nested(request, 2);
                Expect(!nested.Arm(11, 22, GetCurrentThreadId(), 33), "nested scope acquired request");
            }
            Expect(request.Consume(11, 22, GetCurrentThreadId(), 33), "rejected guard disarmed owner");
            Expect(!scope.Arm(11, 22, GetCurrentThreadId(), 33), "scope armed twice");
        }
        Expect(request.WasConsumed(2), "scope lost consumption evidence");
        {
            helen::RefreshRequestScope scope(request, 3);
            Expect(scope.Arm(11, 22, GetCurrentThreadId(), 33), "normal scope arm failed");
        }
        Expect(!request.Consume(11, 22, GetCurrentThreadId(), 33), "normal exit leaked request");
    }

    /** @brief Runs the real matcher on the frame captured by the assembly renderer surrogate. */
    bool __cdecl Observe(std::uintptr_t frame, std::uintptr_t) noexcept {
        const NT_TIB* tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
        const bool origin = helen::ActivationMatcher::Matches(frame, Anchor,
            reinterpret_cast<std::uintptr_t>(tib->StackLimit),
            reinterpret_cast<std::uintptr_t>(tib->StackBase), Returns);
        const bool matched = UseController ? helen::RefreshActivation::Consume(frame, 11, 22) :
            origin && Request.Consume(11, 22, GetCurrentThreadId(), Anchor);
        if (origin && Request.Consume(11, 22, GetCurrentThreadId(), Anchor)) { ++DuplicateConsumption; }
        ++Visits;
        if (Depth == 0) { OuterAccepted += matched ? 1u : 0u; }
        else { NestedAccepted += matched ? 1u : 0u; }
        return matched;
    }

    /** @brief Uses the renderer's frame setup, including alignment, and preserves registers around observation. */
    __declspec(naked) void Renderer() {
        __asm {
            push ebp
            mov ebp, esp
            and esp, -8
            sub esp, 40h
            mov eax, offset Skipped
            mov SkipTarget, eax
            mov eax, offset Rebuilt
            mov RebuildTarget, eax
            xor eax, eax
            jmp DecisionBridge
        Rebuilt:
            cmp Depth, 0
            jne Nested
            inc OuterRebuilds
            jmp Finished
        Nested:
            inc NestedRebuilds
            jmp Finished
        Skipped:
            inc Skips
        Finished:
            mov esp, ebp
            pop ebp
            ret
        }
    }

    /** @brief Mirrors EAA0A0's saved ESI and argument cleanup without executing any game code. */
    __declspec(naked) void RhiViewport() {
        __asm {
            push esi
            call ThrowIfRequested
            mov eax, offset Returned
            mov dword ptr [Returns], eax
            call Renderer
        Returned:
            pop esi
            ret 10h
        }
    }

    /** @brief Models CBB980's 44-byte frame and four-argument RHI call. */
    __declspec(naked) void GenericViewport() {
        __asm {
            sub esp, 2Ch
            mov eax, offset Returned
            mov dword ptr [Returns+4], eax
            push 0
            push 720
            push 1280
            push 1
            call RhiViewport
        Returned:
            add esp, 2Ch
            ret 10h
        }
    }

    /** @brief Models EB7250's 36-byte frame and positive-size generic viewport call. */
    __declspec(naked) void ViewportHelper() {
        __asm {
            sub esp, 24h
            mov eax, offset Returned
            mov dword ptr [Returns+8], eax
            push 0
            push 720
            push 1280
            push 0
            call GenericViewport
        Returned:
            add esp, 24h
            ret 0Ch
        }
    }

    /** @brief Forward declaration for the recursive call through the identical outer call site. */
    void Invoke();

    /** @brief Native anchor publisher borrows the enclosing C++ scope, which owns exceptional cleanup. */
    void InvokeNative(helen::RefreshRequestScope& scope);

    /** @brief Models EB91D0's 56-byte frame, with one nested activation before its normal helper call. */
    __declspec(naked) void WindowsViewport() {
        __asm {
            cmp ecx, 11
            jne BadArguments
            cmp dword ptr [esp+4], 1280
            jne BadArguments
            cmp dword ptr [esp+8], 720
            jne BadArguments
            cmp dword ptr [esp+12], 0
            jne BadArguments
            cmp dword ptr [esp+16], -1
            jne BadArguments
            cmp dword ptr [esp+20], -1
            je ArgumentsChecked
        BadArguments:
            inc ArgumentErrors
        ArgumentsChecked:
            sub esp, 38h
            cmp Depth, 0
            jne SkipNested
            inc Depth
            call Invoke
            dec Depth
        SkipNested:
            mov eax, offset Returned
            mov dword ptr [Returns+12], eax
            push 0
            push 720
            push 1280
            call ViewportHelper
        Returned:
            add esp, 38h
            ret 14h
        }
    }

    /** @brief Publishes the outer call slot before CALL; nested calls retain that same expected anchor. */
    void InvokeNative(helen::RefreshRequestScope& scope) {
        std::uintptr_t before = 0;
        std::uintptr_t after = 0;
        __asm { mov before, esp }
        Expect(helen::AnchoredResize(reinterpret_cast<helen::ViewportResize>(WindowsViewport),
            reinterpret_cast<void*>(11), 1280, 720, 0, &scope, ArmOuter, DisarmOuter),
            "real anchored wrapper did not invoke resize");
        __asm { mov after, esp }
        if (before != after) { ++StackErrors; }
    }

    /** @brief Owns cleanup across native frames; normal cleanup occurs before the anchor publisher returns. */
    void Invoke() {
        if (UseController && Depth == 0) {
            helen::RefreshActivation activation(11, 22, {Returns[0], Returns[1], Returns[2], Returns[3]});
            Expect(helen::AnchoredResize(reinterpret_cast<helen::ViewportResize>(WindowsViewport),
                reinterpret_cast<void*>(11), 1280, 720, 0, &activation,
                helen::RefreshActivation::Publish, helen::RefreshActivation::Clear), "Connected invocation refused");
            Expect(activation.WasConsumed(), "Connected controller missed real native activation");
            return;
        }
        helen::RefreshRequestScope scope(Request, CurrentOperation);
        InvokeNative(scope);
    }
}

/** @brief Runs real recursive x86 calls; no game module is loaded or patched. */
int main() {
    SetErrorMode(0x8003);
    helen::DecisionCallback = Observe;
    try {
        CheckScopeCleanup();
        Expect(!helen::AnchoredResize(reinterpret_cast<helen::ViewportResize>(WindowsViewport),
            reinterpret_cast<void*>(11), 1280, 720, 0, nullptr, RefusePublication, CountRefusedCleanup),
            "Refused publication entered resize");
        Expect(Visits == 0 && RefusedCleanup == 0, "Refused wrapper entered engine or cleared unowned request");
        Invoke();
        Expect(Armed, "outer request not armed");
        Expect(Visits == 2, "both outer and nested calls must execute");
        Expect(StackErrors == 0, "stack imbalance");
        Expect(ArgumentErrors == 0, "anchored wrapper changed engine arguments");
        Expect(OuterAccepted == 1, "outer activation rejected: verify derived offsets");
        Expect(OuterRebuilds == 1, "outer activation did not reach rebuild destination");
        Expect(NestedRebuilds == 0, "nested activation reached rebuild destination");
        Expect(Skips == 1, "nested activation did not reach skip destination");
        Expect(NestedAccepted == 0, "nested activation accepted with identical return sites");
        Expect(DuplicateConsumption == 0, "same activation consumed twice");
        Expect(Request.WasConsumed(1), "consumption evidence missing");
        Expect(!Request.Consume(11, 22, GetCurrentThreadId(), Anchor), "disarmed request consumed");
        Expect(!Request.Arm(1, 11, 22, GetCurrentThreadId(), Anchor), "stale operation rearmed");
        Expect(Request.Arm(2, 11, 22, GetCurrentThreadId(), Anchor), "new operation refused");
        Expect(!Request.Arm(3, 11, 22, GetCurrentThreadId(), Anchor), "nested arm accepted");
        Request.Disarm(3);
        Expect(!Request.Consume(12, 22, GetCurrentThreadId(), Anchor), "wrong renderer consumed");
        Expect(!Request.Consume(11, 23, GetCurrentThreadId(), Anchor), "wrong device consumed");
        Expect(!Request.Consume(11, 22, 0, Anchor), "wrong thread consumed");
        Expect(!Request.Consume(11, 22, GetCurrentThreadId(), Anchor + 4), "wrong activation consumed");
        Expect(Request.Consume(11, 22, GetCurrentThreadId(), Anchor), "refusals damaged request");
        Request.Disarm(2);
        std::array<std::uintptr_t, 52> frameWords{};
        const std::uintptr_t frame = reinterpret_cast<std::uintptr_t>(frameWords.data());
        const std::uintptr_t end = frame + sizeof(frameWords);
        const std::array<std::size_t, 5> slots{1, 3, 19, 33, 51};
        for (std::size_t index = 0; index < slots.size(); ++index) {
            frameWords[slots[index]] = Returns[index];
        }
        Expect(helen::ActivationMatcher::Matches(frame, frame + 204, frame, end, Returns), "valid bounded frame rejected");
        Expect(!helen::ActivationMatcher::Matches(frame, 0, frame, end, Returns), "missing activation accepted");
        Expect(!helen::ActivationMatcher::Matches(frame, frame + 204, frame, end - 1, Returns), "short stack accepted");
        Expect(!helen::ActivationMatcher::Matches(frame, frame + 204, frame + 4, end, Returns), "frame below stack accepted");
        Expect(!helen::ActivationMatcher::Matches(frame + 1, frame + 205, frame, end, Returns), "unaligned frame accepted");
        Expect(!helen::ActivationMatcher::Matches(UINTPTR_MAX - 3, 200, 0, UINTPTR_MAX, Returns), "overflow frame accepted");
        for (std::size_t index = 0; index < slots.size(); ++index) {
            frameWords[slots[index]] ^= 4;
            Expect(!helen::ActivationMatcher::Matches(frame, frame + 204, frame, end, Returns), "wrong return chain accepted");
            frameWords[slots[index]] ^= 4;
        }
        CurrentOperation = 3;
        InjectFailure = true;
        bool caughtNativeFailure = false;
        try { Invoke(); }
        catch (int value) { caughtNativeFailure = value == 42; }
        Expect(caughtNativeFailure, "native chain did not propagate C++ exception");
        Expect(Armed, "exception test never armed request");
        Expect(!Request.Consume(11, 22, GetCurrentThreadId(), Anchor), "native unwind leaked armed request");
        InjectFailure = false;
        UseController = true;
        OuterAccepted = 0;
        NestedAccepted = 0;
        OuterRebuilds = 0;
        NestedRebuilds = 0;
        Skips = 0;
        Visits = 0;
        Invoke();
        Expect(Visits == 2 && OuterAccepted == 1 && NestedAccepted == 0 && OuterRebuilds == 1 &&
            NestedRebuilds == 0 && Skips == 1, "Connected controller did not isolate the real outer invocation");
        Expect(!helen::RefreshActivation::ExpectsRenderer(11), "Connected normal call leaked activation");
        InjectFailure = true;
        caughtNativeFailure = false;
        try { Invoke(); }
        catch (int value) { caughtNativeFailure = value == 42; }
        Expect(caughtNativeFailure, "Connected exception did not propagate");
        Expect(!helen::RefreshActivation::ExpectsRenderer(11), "Connected native unwind leaked activation");
        std::cout << "VSYNC_ACTIVATION_PASS: outer accepted, nested rejected, stack balanced, native unwind disarmed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "VSYNC_ACTIVATION_FAIL: " << error.what() << '\n';
        return 1;
    }
}
