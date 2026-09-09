#include "RefreshActivation.h"
#include "ActivationMatcher.h"
#include <stdexcept>
namespace helen {
    thread_local RefreshActivation* RefreshActivation::Active = nullptr;
    RefreshActivation::RefreshActivation(std::uintptr_t renderer, std::uintptr_t device,
        const std::array<std::uintptr_t, 4>& returns)
        : Returns{returns[0], returns[1], returns[2], returns[3], 0}, Renderer(renderer), Device(device) {
        if (Active != nullptr || renderer == 0 || device == 0) {
            throw std::invalid_argument("Refresh requires idle activation and live renderer/device identities");
        }
        for (const std::uintptr_t site : returns) {
            if (site == 0) { throw std::invalid_argument("Refresh return site unavailable"); }
        }
    }
    RefreshActivation::~RefreshActivation() noexcept { Clear(this); }
    bool __cdecl RefreshActivation::Publish(void* context, std::uintptr_t anchor, std::uintptr_t returnSite) noexcept {
        if (context == nullptr || Active != nullptr || returnSite == 0) { return false; }
        RefreshActivation& activation = *static_cast<RefreshActivation*>(context);
        if (!activation.Request.Arm(1, activation.Renderer, activation.Device, GetCurrentThreadId(), anchor)) { return false; }
        activation.Anchor = anchor;
        activation.Returns[4] = returnSite;
        Active = &activation;
        return true;
    }
    void __cdecl RefreshActivation::Clear(void* context) noexcept {
        if (Active == context && Active != nullptr) {
            Active->Request.Disarm(1);
            Active->Anchor = 0;
            Active = nullptr;
        }
    }
    bool RefreshActivation::ExpectsRenderer(std::uintptr_t renderer) noexcept {
        return Active != nullptr && Active->Renderer == renderer;
    }
    bool RefreshActivation::Consume(std::uintptr_t frame, std::uintptr_t renderer, std::uintptr_t device) noexcept {
        if (!ExpectsRenderer(renderer) || Active->Device != device) { return false; }
        const NT_TIB* const tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
        return ActivationMatcher::Matches(frame, Active->Anchor,
            reinterpret_cast<std::uintptr_t>(tib->StackLimit), reinterpret_cast<std::uintptr_t>(tib->StackBase), Active->Returns)
            && Active->Request.Consume(renderer, device, GetCurrentThreadId(), Active->Anchor);
    }
    bool RefreshActivation::WasConsumed() noexcept { return Request.WasConsumed(1); }
}
