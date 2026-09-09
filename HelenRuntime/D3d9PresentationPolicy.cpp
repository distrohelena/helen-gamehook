#include <HelenHook/D3d9PresentationPolicy.h>
#include <stdexcept>

namespace helen {
    void D3d9PresentationPolicy::SetVsyncOverride(D3d9VsyncOverride mode) {
        if (mode != D3d9VsyncOverride::GameControlled && mode != D3d9VsyncOverride::ForceOn &&
            mode != D3d9VsyncOverride::ForceOff) {
            throw std::invalid_argument("Unknown D3D9 VSync override");
        }
        Mode.store(mode, std::memory_order_relaxed);
    }

    D3d9VsyncOverride D3d9PresentationPolicy::GetVsyncOverride() const noexcept {
        return Mode.load(std::memory_order_relaxed);
    }

    void D3d9PresentationPolicy::Apply(std::span<D3DPRESENT_PARAMETERS> parameters) const noexcept {
        const D3d9VsyncOverride mode = GetVsyncOverride();
        if (mode == D3d9VsyncOverride::GameControlled) { return; }
        const UINT interval = mode == D3d9VsyncOverride::ForceOn
            ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
        for (D3DPRESENT_PARAMETERS& entry : parameters) {
            entry.PresentationInterval = interval;
        }
    }
}
