#include "RefreshRequest.h"
#include "DecisionBridge.h"
using helen::DecisionBridge;
using helen::SkipTarget;
using helen::RebuildTarget;
#include <cstring>
#include <iostream>
#include <stdexcept>
static_assert(sizeof(void*) == 4, "x86 fixture required");
namespace {
    /** @brief CPU request shared with the instruction callback. */
    helen::RefreshRequest Request;
    /** @brief Original renderer decision supplied by each test. */
    unsigned Input = 0;
    /** @brief Observed renderer identity, varied for refusal tests. */
    std::uintptr_t RendererIdentity = 11;
    /** @brief Observed activation, independently derived in the real-stack fixture. */
    std::uintptr_t ActivationIdentity = 33;
    /** @brief Records the actually executed destination. */
    unsigned Branch = 0;
    /** @brief Actual saved engine registers forwarded by the bridge, not fabricated by the callback. */
    std::uintptr_t ObservedFrame = 0;
    /** @brief Actual renderer register forwarded alongside the captured frame. */
    std::uintptr_t ObservedRenderer = 0;
    /** @brief Original GPR, ESP and TEST-produced EFLAGS snapshot. */
    unsigned Before[9]{};
    /** @brief Destination GPR, ESP and EFLAGS snapshot. */
    unsigned After[9]{};
    /** @brief Caller FP state restored before returning to C++. */
    alignas(16) unsigned char OriginalFx[512]{};
    /** @brief Seeded pre-bridge x87 and SIMD context. */
    alignas(16) unsigned char BeforeFx[512]{};
    /** @brief Captured destination x87 and SIMD context. */
    alignas(16) unsigned char AfterFx[512]{};
    /** @brief Nonzero SIMD sentinel. */
    alignas(16) unsigned Pattern[4]{0x12345678, 0x98765432, 0xDEADBEEF, 0x11223344};

    /** @brief Consumes once and intentionally clobbers volatile FP state to challenge the bridge. */
    bool __cdecl Consume(std::uintptr_t frame, std::uintptr_t renderer) noexcept {
        ObservedFrame = frame;
        ObservedRenderer = renderer;
        __asm { pxor xmm0, xmm0 }
        __asm { fninit }
        return Request.Consume(RendererIdentity, 22, GetCurrentThreadId(), ActivationIdentity);
    }
    /** @brief Enters via JMP with literal register sentinels and captures CPU state at each destination. */
    __declspec(naked) void RunBridge() {
        __asm {
            pushfd
            pushad
            fxsave OriginalFx
            mov eax, offset Skipped
            mov SkipTarget, eax
            mov eax, offset Rebuilt
            mov RebuildTarget, eax
            fninit
            fld1
            movdqa xmm0, Pattern
            movdqa xmm1, Pattern
            movdqa xmm2, Pattern
            movdqa xmm3, Pattern
            movdqa xmm4, Pattern
            movdqa xmm5, Pattern
            movdqa xmm6, Pattern
            movdqa xmm7, Pattern
            mov eax, Input
            mov ebx, 11223344h
            mov ecx, 22334455h
            mov edx, 33445566h
            mov esi, 44556677h
            mov edi, 55667788h
            mov ebp, 66778899h
            test eax, eax
            mov Before[0], eax
            mov Before[4], ebx
            mov Before[8], ecx
            mov Before[12], edx
            mov Before[16], esi
            mov Before[20], edi
            mov Before[24], ebp
            mov Before[28], esp
            pushfd
            pop Before[32]
            fxsave BeforeFx
            jmp DecisionBridge
        Skipped:
            mov Branch, 0
            jmp Capture
        Rebuilt:
            mov Branch, 1
        Capture:
            mov After[0], eax
            mov After[4], ebx
            mov After[8], ecx
            mov After[12], edx
            mov After[16], esi
            mov After[20], edi
            mov After[24], ebp
            mov After[28], esp
            pushfd
            pop After[32]
            fxsave AfterFx
            fxrstor OriginalFx
            popad
            popfd
            ret
        }
    }
    /** @brief Reports assertions through exceptions caught by the console entry point. */
    void Expect(bool value, const char* message) {
        if (!value) { throw std::runtime_error(message); }
    }
    /** @brief Checks the actual branch and defined machine state; TEST leaves AF undefined. */
    void Check(unsigned input, unsigned branch) {
        Input = input;
        RunBridge();
        Expect(ObservedFrame == 0x66778899 && ObservedRenderer == 0x11223344, "Bridge forwarded wrong engine identities");
        Expect(Branch == branch, "wrong decision destination");
        for (unsigned index = 0; index < 8; ++index) {
            Expect(Before[index] == After[index], "GPR or ESP changed");
        }
        Expect(((Before[8] ^ After[8]) & ~0x10u) == 0, "EFLAGS changed");
        Expect(std::memcmp(BeforeFx, AfterFx, 28) == 0, "x87 control or MXCSR changed");
        for (unsigned index = 0; index < 8; ++index) {
            Expect(std::memcmp(BeforeFx + 32 + index * 16, AfterFx + 32 + index * 16, 10) == 0, "x87 register changed");
        }
        Expect(std::memcmp(BeforeFx + 160, AfterFx + 160, 128) == 0, "XMM register changed");
    }
}
/** @brief Tests original, refused, forced and consumed decisions without loading Batman. */
int main() {
    SetErrorMode(0x8003);
    helen::DecisionCallback = Consume;
    try {
        Check(0, 0);
        Check(1, 1);
        Check(0x80000000u, 1);
        Expect(Request.Arm(1, 11, 22, GetCurrentThreadId(), 33), "arm failed");
        RendererIdentity = 12;
        Check(0, 0);
        RendererIdentity = 11;
        ActivationIdentity = 34;
        Check(0, 0);
        ActivationIdentity = 33;
        Check(0, 1);
        Expect(Request.WasConsumed(1), "request was not consumed");
        Check(0, 0);
        Request.Disarm(1);
        Check(0, 0);
        Expect(Request.Arm(2, 11, 22, GetCurrentThreadId(), 33), "second arm failed");
        Check(1, 1);
        Expect(Request.WasConsumed(2), "original rebuild did not consume request");
        Check(0, 0);
        Request.Disarm(2);
        std::cout << "VSYNC_DECISION_BRIDGE_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "VSYNC_DECISION_BRIDGE_FAIL: " << error.what() << '\n';
        return 1;
    }
}
