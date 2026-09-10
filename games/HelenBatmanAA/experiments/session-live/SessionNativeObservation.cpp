#include "SessionNativeObservation.h"
#include "SessionNativeMemory.h"
#include "NoSaveDisplayRequest.h"
#include "NoSaveFullscreenModes.h"
#include "SameSizeRefresh.h"
#include <HelenHook/D3d9TextureReplacementHookSet.h>
#include <HelenHook/Log.h>
#include <wrl/client.h>
#include <algorithm>

namespace {
    /** @brief Tests membership of a field in the already validated complete request. */
    bool Changed(const helen::SessionGraphicsDelta& delta, helen::BatmanGraphicsField field) {
        return std::find(delta.Fields().begin(),delta.Fields().end(),field) != delta.Fields().end();
    }
}
namespace helen {
    SessionNativeObservation SessionNativeObservation::Capture() {
        SessionNativeMemory::RequireExecutable();
        if (SessionNativeMemory::Read<unsigned>(0x26760E8) == 0 || SessionNativeMemory::Read<DWORD>(0x26760E4) != GetCurrentThreadId() ||
            SessionNativeMemory::Read<int>(0x26CCAB4) != 1) { throw std::runtime_error("Session requires initialized game thread and one viewport"); }
        const std::uintptr_t owner = SessionNativeMemory::Read<std::uintptr_t>(SessionNativeMemory::Read<std::uintptr_t>(0x26CCAB0));
        if (SessionNativeMemory::Read<std::uintptr_t>(owner) != 0x212C408 || SessionNativeMemory::Read<std::uintptr_t>(owner+4) != 0x212C380 ||
            SessionNativeMemory::Read<std::uintptr_t>(0x212C40C) != 0xEB91D0) { throw std::runtime_error("Viewport type or resize entry mismatch"); }
        const HWND window = SessionNativeMemory::Read<HWND>(owner+0x60);
        DWORD process = 0;
        const DWORD thread = GetWindowThreadProcessId(window,&process);
        if (!IsWindow(window) || process != GetCurrentProcessId() || thread != GetCurrentThreadId() ||
            SessionNativeMemory::Read<std::uintptr_t>(owner+0x64) != 0 || SessionNativeMemory::Read<unsigned>(owner+0x80) != 0) {
            throw std::runtime_error("Session requires idle top-level viewport on owning thread");
        }
        const std::uintptr_t renderer = SessionNativeMemory::Read<std::uintptr_t>(0x26B0D94);
        if (SessionNativeMemory::Read<std::uintptr_t>(renderer) != 0x2128210 || SessionNativeMemory::Read<std::uintptr_t>(renderer+0x24) != 0 ||
            SessionNativeMemory::Read<unsigned>(0x26C0B38+0x2D4) != 0) { throw std::runtime_error("Renderer busy or settings owner is not the game"); }
        IDirect3DDevice9* const device = SessionNativeMemory::Read<IDirect3DDevice9*>(renderer+0x10);
        if (device == nullptr || device->TestCooperativeLevel() != D3D_OK) { throw std::runtime_error("D3D9 device unavailable or lost"); }
        Microsoft::WRL::ComPtr<IDirect3DSwapChain9> chain;
        D3DPRESENT_PARAMETERS presentation{};
        if (FAILED(device->GetSwapChain(0,chain.GetAddressOf())) || !chain || FAILED(chain->GetPresentParameters(&presentation))) {
            throw std::runtime_error("Actual presentation readback unavailable");
        }
        const unsigned width = SessionNativeMemory::Read<unsigned>(owner+0x4C);
        const unsigned height = SessionNativeMemory::Read<unsigned>(owner+0x50);
        const int fullscreen = static_cast<int>(SessionNativeMemory::Read<unsigned>(owner+0x58)&1);
        if (width == 0 || height == 0 || presentation.BackBufferWidth != width || presentation.BackBufferHeight != height ||
            (presentation.Windowed != FALSE) != (fullscreen == 0)) { throw std::runtime_error("Viewport and actual backbuffer disagree"); }
        const unsigned char physicsGetter[] = {0x8B,0x81,0xC8,0x03,0x00,0x00,0xC3};
        SessionNativeMemory::RequireBytes(0xCB0420,physicsGetter);
        const std::uintptr_t engine = SessionNativeMemory::Read<std::uintptr_t>(0x26C3CDC);
        if (engine == 0) { throw std::runtime_error("Live PhysX requires initialized GEngine"); }
        const int physics = SessionNativeMemory::Read<int>(engine+0x3C8);
        if (physics < 0 || physics > 2) { throw std::runtime_error("Invalid live engine PhysX level"); }
        return {owner,renderer,window,presentation,SessionNativeMemory::Read<SessionSettingsPayload::Values>(0x26C0B3C),width,height,fullscreen,engine,physics};
    }
    void SessionNativeObservation::RequireTarget(const BatmanGraphicsDraftState& draft, const SessionGraphicsDelta& delta) const {
        const int width = draft.Get(BatmanGraphicsField::PersistedWidth);
        const int height = draft.Get(BatmanGraphicsField::PersistedHeight);
        const int fullscreen = draft.Get(BatmanGraphicsField::Fullscreen);
        IDirect3DDevice9* const device = SessionNativeMemory::Read<IDirect3DDevice9*>(Renderer+0x10);
        if (device == nullptr) { throw std::runtime_error("Target validation device unavailable"); }
        const bool resize = width != static_cast<int>(Width) || height != static_cast<int>(Height) || fullscreen != Fullscreen;
        if (resize) {
            const std::vector<BatmanDisplayMode> modes = fullscreen == 1 ? NoSaveFullscreenModes::Enumerate(*device) : std::vector<BatmanDisplayMode>();
            const NoSaveDisplayRequest validated(width,height,fullscreen,Width,Height,Fullscreen != 0,modes);
        }
        if (!resize && (Changed(delta,BatmanGraphicsField::Vsync) || Changed(delta,BatmanGraphicsField::Msaa))) { SameSizeRefresh::RequireInstalled(); }
        if (Changed(delta,BatmanGraphicsField::Msaa)) {
            Microsoft::WRL::ComPtr<IDirect3D9> direct;
            D3DDEVICE_CREATION_PARAMETERS creation{};
            if (FAILED(device->GetDirect3D(direct.GetAddressOf())) || !direct || FAILED(device->GetCreationParameters(&creation))) {
                throw std::runtime_error("MSAA capability query unavailable");
            }
            const int samples = SessionGraphicsDelta::Encode(BatmanGraphicsField::Msaa,draft.Get(BatmanGraphicsField::Msaa));
            const D3DMULTISAMPLE_TYPE type = samples == 1 ? D3DMULTISAMPLE_NONE : static_cast<D3DMULTISAMPLE_TYPE>(samples);
            // Same three engine pixel formats checked during stock renderer initialization at EA718F..EA71E1.
            for (const std::uintptr_t address : {0x25DA7C0u,0x25DA8E0u,0x25DA994u}) {
                const D3DFORMAT format = SessionNativeMemory::Read<D3DFORMAT>(address);
                DWORD quality = 0;
                if (FAILED(direct->CheckDeviceMultiSampleType(creation.AdapterOrdinal,creation.DeviceType,format,
                    fullscreen == 0,type,&quality)) || quality == 0) { throw std::runtime_error("MSAA target unsupported by an engine render/depth format"); }
            }
        }
    }
    void SessionNativeObservation::ApplyDisplay(const BatmanGraphicsDraftState& draft, const SessionGraphicsDelta& delta) const {
        const unsigned width = static_cast<unsigned>(draft.Get(BatmanGraphicsField::PersistedWidth));
        const unsigned height = static_cast<unsigned>(draft.Get(BatmanGraphicsField::PersistedHeight));
        const int fullscreen = draft.Get(BatmanGraphicsField::Fullscreen);
        const bool resize = width != Width || height != Height || fullscreen != Fullscreen;
        const bool vsync = Changed(delta,BatmanGraphicsField::Vsync);
        const bool msaa = Changed(delta,BatmanGraphicsField::Msaa);
        if (vsync) {
            IDirect3DDevice9* const device = SessionNativeMemory::Read<IDirect3DDevice9*>(Renderer+0x10);
            const D3d9VsyncOverride policy = draft.Get(BatmanGraphicsField::Vsync) == 1 ? D3d9VsyncOverride::ForceOn : D3d9VsyncOverride::ForceOff;
            if (device == nullptr || !D3d9TextureReplacementHookSet::TrySetVsyncOverride(*device,policy)) {
                throw std::runtime_error("VSync override requires a registered idle device");
            }
        }
        if (resize) {
            /** @brief Verified existing-window viewport resize ABI; negative positions preserve window placement. */
            using Resize = void(__thiscall*)(void*,unsigned,unsigned,int,int,int);
            reinterpret_cast<Resize>(0xEB91D0)(reinterpret_cast<void*>(Owner),width,height,fullscreen,-1,-1);
        } else if (vsync || msaa) {
            SameSizeRefresh::Invoke(Owner,Renderer,width,height,fullscreen);
        }
        Logf(L"[session-live] DISPLAY resize=%d same-size-refresh=%d requested=%ux%u fullscreen=%d",resize,!resize && (vsync || msaa),width,height,fullscreen);
    }
}
