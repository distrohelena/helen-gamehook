#include "SessionGraphicsNative.h"
#include "SessionNativeMemory.h"
#include "BloomIniBinding.h"
#include "SessionPhysxLevel.h"
#include <HelenHook/Log.h>

namespace helen {
    FileWriteRoutingService& SessionGraphicsNative::RequireRouting(const std::shared_ptr<FileWriteRoutingService>& routing) {
        if (!routing) { throw std::invalid_argument("Session-only graphics requires initialized file routing"); }
        return *routing;
    }
    SessionGraphicsNative::SessionGraphicsNative(BatmanGraphicsConfigService& config, std::shared_ptr<FileWriteRoutingService> routing,
        std::filesystem::path ini) : Config(config), Routing(std::move(routing)), Ini(std::move(ini)), Overlay(RequireRouting(Routing),Ini) {}
    SessionGraphicsDelta::Capabilities SessionGraphicsNative::Capabilities() const noexcept {
        SessionGraphicsDelta::Capabilities supported;
        supported.fill(true);
        // Explicit experimental opt-in: engine scalar plus cache; physics reinitialization is not claimed.
        supported[11] = false;
        return supported;
    }
    BatmanGraphicsSnapshot SessionGraphicsNative::Capture() const {
        const SessionNativeObservation observation = SessionNativeObservation::Capture();
        BatmanGraphicsSnapshot::Values values;
        values[10] = observation.Physx;
        for (unsigned index=0; index<14; ++index) {
            const BatmanGraphicsField field = static_cast<BatmanGraphicsField>(index);
            if (field != BatmanGraphicsField::Physx) {
                values[index] = SessionSettingsPayload::Decode(field,observation.Payload.at(SessionSettingsPayload::Offset(field)/4));
            }
        }
        values[0] = observation.Fullscreen;
        const UINT interval = observation.Presentation.PresentationInterval;
        if (interval != D3DPRESENT_INTERVAL_IMMEDIATE && interval != D3DPRESENT_INTERVAL_ONE && interval != D3DPRESENT_INTERVAL_DEFAULT) {
            throw std::runtime_error("Unsupported actual VSync presentation interval");
        }
        values[1] = interval == D3DPRESENT_INTERVAL_IMMEDIATE ? 0 : 1;
        values[12] = static_cast<int>(observation.Width);
        values[13] = static_cast<int>(observation.Height);
        return BatmanGraphicsSnapshot(values);
    }
    void SessionGraphicsNative::Preflight(const BatmanGraphicsDraftState& baseline, const BatmanGraphicsDraftState& draft,
        const SessionGraphicsDelta& delta) {
        // Capture validates the game thread before calling the engine's synchronous rendering flush.
        (void)SessionNativeObservation::Capture();
        SessionNativeMemory::Flush();
        Before = SessionNativeObservation::Capture();
        Filename = SessionNativeMemory::EngineIniPath();
        BloomIniBinding::Require(Filename,SessionNativeMemory::EngineUserRoot(),Ini);
        (void)Overlay.RequirePath();
        OriginalBytes = SessionGraphicsOverlay::ReadBytes(Ini);
        const auto current = Capture().TryCreateDraft();
        if (!current.has_value()) { throw std::runtime_error("Required native snapshot is incomplete"); }
        for (unsigned index=0; index<14; ++index) {
            const BatmanGraphicsField field = static_cast<BatmanGraphicsField>(index);
            if (current->Get(field) != baseline.Get(field)) {
                throw std::runtime_error(std::string("Live state changed outside this editor: ") + SessionGraphicsDelta::Key(field));
            }
        }
        if (SessionNativeMemory::CachedDirectionalLightmaps(Filename) != static_cast<int>(Before->Payload[0x24/4])) {
            throw std::runtime_error("Directional lightmap cache/live mismatch; broad stock Apply refused");
        }
        Before->RequireTarget(draft,delta);
        if (draft.Get(BatmanGraphicsField::Physx) != Before->Physx) {
            MEMORY_BASIC_INFORMATION region;
            const std::uintptr_t field = Before->Engine+0x3C8;
            if (VirtualQuery(reinterpret_cast<const void*>(field),&region,sizeof(region)) != sizeof(region) ||
                region.State != MEM_COMMIT || (region.Protect != PAGE_READWRITE && region.Protect != PAGE_EXECUTE_READWRITE) ||
                field+sizeof(int) > reinterpret_cast<std::uintptr_t>(region.BaseAddress)+region.RegionSize) {
                throw std::runtime_error("Live PhysX field is not writable engine storage");
            }
        }
        Logf(L"[session-live] PREFLIGHT fields=%u viewport=%p renderer=%p",static_cast<unsigned>(delta.Fields().size()),
            reinterpret_cast<void*>(Before->Owner),reinterpret_cast<void*>(Before->Renderer));
    }
    void SessionGraphicsNative::Stage(const BatmanGraphicsDraftState& draft, const SessionGraphicsDelta& delta) {
        if (!Before.has_value()) { throw std::logic_error("Session staging requires preflight"); }
        Overlay.Stage(draft,delta);
    }
    void SessionGraphicsNative::Apply(const BatmanGraphicsDraftState& draft, const SessionGraphicsDelta& delta) {
        if (!Before.has_value()) { throw std::logic_error("Native application requires preflight"); }
        const auto incoming = SessionSettingsPayload::Build(Before->Payload,draft,delta);
        SessionNativeMemory::Reload(Filename);
        for (const BatmanGraphicsField field : delta.Fields()) {
            if (SessionNativeMemory::Cached(field,Filename) != SessionGraphicsDelta::Encode(field,draft.Get(field))) {
                throw std::runtime_error(std::string("Reload did not observe selected ") + SessionGraphicsDelta::Key(field));
            }
        }
        if (SessionNativeMemory::CachedDirectionalLightmaps(Filename) != static_cast<int>(incoming[0x24/4])) {
            throw std::runtime_error("Reloaded directional lightmaps would be toggled by stock Apply");
        }
        // C3FDE0 reloads from cache before comparing its incoming display tuple. Equal values suppress its
        // nested viewport resize, leaving exactly one explicit validated display operation below.
        for (const BatmanGraphicsField field : {BatmanGraphicsField::Fullscreen,BatmanGraphicsField::PersistedWidth,BatmanGraphicsField::PersistedHeight}) {
            if (SessionNativeMemory::Cached(field,Filename) != static_cast<int>(incoming[SessionSettingsPayload::Offset(field)/4])) {
                throw std::runtime_error("Cached display tuple would cause a second stock resize; request locked before native Apply");
            }
        }
        if (draft.Get(BatmanGraphicsField::Physx) != Before->Physx) {
            if (SessionNativeMemory::Read<std::uintptr_t>(0x26C3CDC) != Before->Engine) {
                throw std::runtime_error("Engine lifetime changed before PhysX update");
            }
            SessionPhysxLevel::Apply(*reinterpret_cast<int*>(Before->Engine+0x3C8),Before->Physx,draft.Get(BatmanGraphicsField::Physx));
            Logf(L"[session-live] EXPERIMENTAL PhysX engine-field old=%d selected=%d engine=%p reinitialized=0",
                Before->Physx,draft.Get(BatmanGraphicsField::Physx),reinterpret_cast<void*>(Before->Engine));
        }
        /** @brief Complete pinned FSystemSettings input, with the save flag explicitly false. */
        using ApplySettings = void(__thiscall*)(void*,const void*,int);
        reinterpret_cast<ApplySettings>(0xC40090)(reinterpret_cast<void*>(0x26C0B38),incoming.data(),0);
        SessionNativeMemory::Flush();
        const auto afterEffects = SessionNativeObservation::Capture();
        if (afterEffects.Owner != Before->Owner || afterEffects.Renderer != Before->Renderer || afterEffects.Window != Before->Window ||
            afterEffects.Width != Before->Width || afterEffects.Height != Before->Height || afterEffects.Fullscreen != Before->Fullscreen) {
            throw std::runtime_error("Stock effect application unexpectedly changed viewport ownership or display");
        }
        Before->ApplyDisplay(draft,delta);
        SessionNativeMemory::Flush();
    }
    void SessionGraphicsNative::Verify(const BatmanGraphicsDraftState& draft, const SessionGraphicsDelta& delta) {
        if (!Before.has_value()) { throw std::logic_error("Verification requires preflight"); }
        const auto expected = SessionSettingsPayload::Build(Before->Payload,draft,delta);
        const SessionNativeObservation after = SessionNativeObservation::Capture();
        if (after.Owner != Before->Owner || after.Window != Before->Window || after.Renderer != Before->Renderer || after.Payload != expected ||
            after.Engine != Before->Engine || after.Physx != draft.Get(BatmanGraphicsField::Physx)) {
            throw std::runtime_error("Native payload or unrelated settings/lifetime verification failed");
        }
        if (SessionGraphicsOverlay::ReadBytes(Ini) != OriginalBytes) { throw std::runtime_error("Original BmEngine.ini changed during session Apply"); }
        for (const BatmanGraphicsField field : delta.Fields()) {
            const int selected = SessionGraphicsDelta::Encode(field,draft.Get(field));
            const int cached = SessionNativeMemory::Cached(field,Filename);
            if (field == BatmanGraphicsField::Physx) {
                Logf(L"[session-live] EXPERIMENTAL PhysX selected=%d cache=%d live=%d effects-unverified=1 reinitialized=0",
                    selected,cached,after.Physx);
                if (cached != selected || after.Physx != selected) { throw std::runtime_error("Experimental PhysX cache/live readback mismatch"); }
                continue;
            }
            const int live = static_cast<int>(after.Payload[SessionSettingsPayload::Offset(field)/4]);
            const auto mirror = SessionSettingsPayload::Mirror(field);
            const int rendered = mirror.has_value() ? SessionNativeMemory::Read<int>(*mirror) : -1;
            Logf(L"[session-live] VERIFY key=%hs selected=%d cache=%d live=%d render-mirror=%d",SessionGraphicsDelta::Key(field),selected,cached,live,rendered);
            if (cached != selected || live != selected || (mirror.has_value() && rendered != selected)) {
                throw std::runtime_error(std::string("Setting readback mismatch: ")+SessionGraphicsDelta::Key(field));
            }
        }
        const bool actualVsync = after.Presentation.PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE;
        if (after.Width != static_cast<unsigned>(draft.Get(BatmanGraphicsField::PersistedWidth)) ||
            after.Height != static_cast<unsigned>(draft.Get(BatmanGraphicsField::PersistedHeight)) ||
            after.Fullscreen != draft.Get(BatmanGraphicsField::Fullscreen) || actualVsync != (draft.Get(BatmanGraphicsField::Vsync) == 1)) {
            throw std::runtime_error("Actual display/VSync does not match the requested session state");
        }
        Logf(L"[session-live] DEVICE verified size=%ux%u fullscreen=%d interval=%u original-unchanged=1",after.Width,after.Height,after.Fullscreen,after.Presentation.PresentationInterval);
        Before.reset();
    }
}
