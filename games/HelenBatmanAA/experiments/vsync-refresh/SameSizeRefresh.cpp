#include "SameSizeRefresh.h"
#include "AnchoredResize.h"
#include "RefreshActivation.h"
#include <HelenHook/Log.h>
#include <stdexcept>

namespace helen {
    std::atomic<bool> SameSizeRefresh::Installed{false};

    void SameSizeRefresh::RequireInstalled() {
        if (!Installed.load()) { throw std::runtime_error("Same-size refresh startup hook is unavailable"); }
    }

    void SameSizeRefresh::Invoke(std::uintptr_t owner, std::uintptr_t renderer, unsigned width, unsigned height, int fullscreen) {
        RequireInstalled();
        std::uintptr_t device = 0;
        SIZE_T copied = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(renderer + 0x10),
            &device, sizeof(device), &copied) || copied != sizeof(device) || device == 0) {
            throw std::runtime_error("Same-size refresh device unavailable");
        }
        RefreshActivation activation(renderer, device, {0xEAA0EF, 0xCBBA7D, 0xEB7362, 0xEB95FB});
        if (!AnchoredResize(reinterpret_cast<ViewportResize>(0xEB91D0), reinterpret_cast<void*>(owner),
            width, height, fullscreen, &activation, RefreshActivation::Publish, RefreshActivation::Clear)) {
            throw std::runtime_error("Same-size refresh activation refused before engine call");
        }
        if (!activation.WasConsumed()) {
            throw std::runtime_error("Same-size refresh returned without consuming the verified decision activation");
        }
        Log(L"[same-size-refresh] Decision consumed exactly once; stock resize returned.");
    }
}
