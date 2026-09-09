#include "SameSizeRefresh.h"
#include "DecisionBridge.h"
#include "RefreshActivation.h"
#include "../../../../HelenGameHook/StartupHookRuntime.h"
#include <HelenHook/ExecutableFingerprint.h>
#include <HelenHook/Memory.h>
#include <HelenHook/Log.h>
#include <array>
#include <cstring>
#include <stdexcept>

namespace {
    /** @brief CPU-only decision callback; reads the device only after matching the active renderer identity. */
    bool __cdecl ConsumeRefresh(std::uintptr_t frame, std::uintptr_t renderer) noexcept {
        if (!helen::RefreshActivation::ExpectsRenderer(renderer)) { return false; }
        std::uintptr_t device = 0;
        SIZE_T copied = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(renderer + 0x10),
            &device, sizeof(device), &copied) || copied != sizeof(device)) { return false; }
        return helen::RefreshActivation::Consume(frame, renderer, device);
    }
}

namespace helen {
    void SameSizeRefresh::InstallAtStartup() {
        if (!CanInstallStartupHooks() || Installed.load()) {
            throw std::runtime_error("Same-size refresh requires the one-time static startup window");
        }
        const std::optional<ModuleView> module = QueryMainModule();
        if (!module || module->base_address != 0x400000) {
            throw std::runtime_error("Same-size refresh requires preferred-base Batman executable");
        }
        const ExecutableFingerprint fingerprint = ExecutableFingerprint::FromPath(module->path);
        if (fingerprint.FileSize != 38758728 || fingerprint.Sha256 != "4dac1f5e2ac6710b7378fdce74601f616f4753e3756cb5fda63c7519cc2eb028") {
            throw std::runtime_error("Same-size refresh rejects foreign executable");
        }
        constexpr std::uintptr_t site = 0xEA65A6;
        constexpr std::array<unsigned char, 8> expected{0x85,0xC0,0x0F,0x84,0xDA,0x0D,0x00,0x00};
        std::array<unsigned char, 8> actual{};
        SIZE_T copied = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(site), actual.data(),
            actual.size(), &copied) || copied != actual.size() || actual != expected) {
            throw std::runtime_error("Same-size refresh decision instructions differ from pinned build");
        }
        // StartupHookLifetime pins this DLL and excludes game execution during this splice.
        // Commit a permanent jump: never allocate or execute an unrelocated Jcc trampoline,
        // and never restore executable bytes from a destructor during process shutdown.
        DecisionCallback = ConsumeRefresh;
        SkipTarget = reinterpret_cast<void*>(0xEA7388);
        RebuildTarget = reinterpret_cast<void*>(0xEA65AE);
        std::array<unsigned char, 8> jump{0xE9,0,0,0,0,0x90,0x90,0x90};
        const std::uint32_t relative = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&DecisionBridge) - (site + 5));
        std::memcpy(jump.data() + 1, &relative, sizeof(relative));
        if (!WriteMemory(reinterpret_cast<void*>(site), jump.data(), jump.size())) {
            throw std::runtime_error("Same-size refresh splice failed and was rolled back");
        }
        Installed.store(true);
        Log(L"[same-size-refresh] Startup decision hook installed; inactive until an anchored Apply.");
    }
}
