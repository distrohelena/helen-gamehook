// Compile the real implementation in this dedicated fixture translation unit so tests
// can replace saved driver entries without adding fault-injection APIs to production.
#pragma warning(push)
#pragma warning(disable: 4505)
#include "../../HelenRuntime/D3d9TextureReplacementHookSet.cpp"
#pragma warning(pop)
#define wmain ResetFixtureMain
#include "D3d9ReplacementResetTests.cpp"
#undef wmain

namespace {
    /** Selected failure boundary; each case runs on real COM resources. */
    enum class Fault { Create, Lock, Unlock, InvalidateCreate, Duplicate, Reuse, Bind, InvalidateBind, NewBinding, InvalidateBindChange, RewriteCreate };
    /** Current case, set before entering any injected callback. */
    Fault g_fault = Fault::Create;
    /** Native driver methods copied before fixture interception. */
    Direct3d9CreateTextureFunction g_create;
    /** Native release used to count actual destruction, not fabricated reference counts. */
    Direct3d9TextureReleaseFunction g_release;
    /** Native lock used for successful real uploads. */
    Direct3d9TextureLockRectFunction g_lock;
    /** Native unlock always closes a successful lock before an injected failure. */
    Direct3d9TextureUnlockRectFunction g_unlock;
    /** Native stage query used outside the injected error boundary. */
    Direct3d9GetTextureFunction g_get;
    /** Native stage bind used to expose partial side effects. */
    Direct3d9SetTextureFunction g_set;
    /** Source identity remains externally owned by the test during callback reentry. */
    IDirect3DTexture9* g_source;
    /** Newly created texture whose eventual destruction must occur exactly once. */
    IDirect3DTexture9* g_created;
    /** Number of final releases observed for the current replacement. */
    unsigned g_destroyed;
    /** Native texture table restored before the case's resources leave scope. */
    void** g_texture_table;
    /** Saved dispatch entries for the table temporarily instrumented by this fixture. */
    std::vector<void*> g_texture_originals;
    /** Case asset declaration used by duplicate-operation reentry. */
    const helen::PackScopedTextureReplacementDefinition* g_definition;
    /** Independently queried description for duplicate-operation reentry. */
    D3DSURFACE_DESC g_description;
    /** Records rejection of a nested attempt while the outer operation owns its ticket. */
    bool g_duplicate_rejected;
    /** Ensures the game-side stage change is injected once, during the first query. */
    bool g_rebound;

    /** Counts actual replacement destruction while forwarding every release. */
    ULONG WINAPI CountRelease(IDirect3DTexture9* self) {
        const ULONG count = g_release(self);
        if (self == g_created && count == 0) { ++g_destroyed; }
        return count;
    }

    /** Injects upload-lock failure without acquiring a real lock in that case. */
    HRESULT WINAPI FaultLock(IDirect3DTexture9* self, UINT level, D3DLOCKED_RECT* rect, const RECT* area, DWORD flags) {
        return g_fault == Fault::Lock && self == g_created ? E_OUTOFMEMORY : g_lock(self, level, rect, area, flags);
    }

    /** Preserves native unlocking while injecting its reported failure. */
    HRESULT WINAPI FaultUnlock(IDirect3DTexture9* self, UINT level) {
        const HRESULT result = g_unlock(self, level);
        return g_fault == Fault::Unlock && self == g_created ? E_FAIL : result;
    }

    /** Reenters the real tracking registry from a driver callback; a held mutex would deadlock. */
    void InvalidateSource(bool reuse) {
        std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
        TrackedTextureRecord& record = g_tracked_texture_records.at(g_source);
        if (reuse) {
            IDirect3DDevice9* const device = record.OwningDevice;
            g_tracked_texture_records.erase(g_source);
            g_tracked_texture_records[g_source].OwningDevice = device;
        } else { record.PendingReplacement.reset(); }
    }

    /** Creates a real texture, then instruments its original upload/release boundary. */
    HRESULT WINAPI FaultCreate(IDirect3DDevice9* self, UINT width, UINT height, UINT levels, DWORD usage,
        D3DFORMAT format, D3DPOOL pool, IDirect3DTexture9** result, HANDLE* shared) {
        if (g_fault == Fault::Create) { *result = nullptr; return E_OUTOFMEMORY; }
        if (g_fault == Fault::InvalidateCreate || g_fault == Fault::Reuse) { InvalidateSource(g_fault == Fault::Reuse); }
        if (g_fault == Fault::RewriteCreate) {
            D3DLOCKED_RECT writable{};
            RequireSuccess(g_source->LockRect(0, &writable, nullptr, 0), "Reentrant source rewrite lock");
            for (int row = 0; row < 4; ++row) { std::memset(static_cast<char*>(writable.pBits) + row * writable.Pitch, 1, 16); }
            RequireSuccess(g_source->UnlockRect(0), "Reentrant source rewrite unlock");
        }
        if (g_fault == Fault::Duplicate) {
            HRESULT nested = S_OK;
            g_duplicate_rejected = !TryCacheReplacementTexture(*self, g_source, g_description, *g_definition, nested) &&
                nested == D3DERR_INVALIDCALL;
        }
        const HRESULT hr = g_create(self, width, height, levels, usage, format, pool, result, shared);
        if (FAILED(hr)) { return hr; }
        g_created = *result;
        g_texture_table = GetComVtable(*result);
        g_texture_originals = g_original_dispatch.Capture(g_texture_table, Direct3d9TextureVtableSlotCount);
        g_release = reinterpret_cast<Direct3d9TextureReleaseFunction>(g_texture_originals[2]);
        g_lock = reinterpret_cast<Direct3d9TextureLockRectFunction>(g_texture_originals[19]);
        g_unlock = reinterpret_cast<Direct3d9TextureUnlockRectFunction>(g_texture_originals[20]);
        std::vector<void*> patched = g_texture_originals;
        patched[2] = reinterpret_cast<void*>(&CountRelease);
        patched[19] = reinterpret_cast<void*>(&FaultLock);
        patched[20] = reinterpret_cast<void*>(&FaultUnlock);
        g_original_dispatch.Replace(g_texture_table, patched);
        return hr;
    }

    /** Fails after stage zero was rebound, exposing cleanup of a partially applied cache operation. */
    HRESULT WINAPI FaultGet(IDirect3DDevice9* self, DWORD stage, IDirect3DBaseTexture9** texture) {
        if (g_fault == Fault::Bind && stage == 1) { *texture = nullptr; return E_FAIL; }
        const HRESULT result = g_get(self, stage, texture);
        if (g_fault == Fault::NewBinding && stage == 0 && !g_rebound && SUCCEEDED(result)) {
            g_rebound = true;
            RequireSuccess(self->SetTexture(stage, nullptr), "Reentrant application stage change");
        }
        return result;
    }

    /** Invalidates publication during a successful external bind. */
    HRESULT WINAPI FaultSet(IDirect3DDevice9* self, DWORD stage, IDirect3DBaseTexture9* texture) {
        if (g_fault == Fault::InvalidateBind && texture == g_created) { InvalidateSource(false); }
        const HRESULT result = g_set(self, stage, texture);
        if (g_fault == Fault::InvalidateBindChange && texture == g_created) {
            RequireSuccess(self->SetTexture(stage, nullptr), "Newer application binding before rollback");
            InvalidateSource(false);
        }
        return result;
    }

    /** Exercises cancellation, cleanup and nested calls against production cache logic and real D3D9. */
    void RunFailures(IDirect3DDevice9& device, const std::filesystem::path& root) {
        {
            Microsoft::WRL::ComPtr<IDirect3DTexture9> texture;
            RequireSuccess(device.CreateTexture(128, 128, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                D3DPOOL_DEFAULT, texture.GetAddressOf(), nullptr), "Create surface-owned lifetime source");
            IDirect3DTexture9* const identity = texture.Get();
            Microsoft::WRL::ComPtr<IDirect3DSurface9> surface;
            RequireSuccess(texture->GetSurfaceLevel(0, surface.GetAddressOf()), "Retain texture through its surface");
            texture.Reset();
            surface.Reset();
            {
                std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
                Expect(FindTrackedTextureRecord(identity) == nullptr,
                    "Last surface release destroyed its texture but left a stale texture registration.");
            }
        }
        helen::TextureReplacementDefinition definition;
        definition.Id = "fault-fixture"; definition.Api = "d3d9";
        definition.Width = 4; definition.Height = 4; definition.Format = "A8R8G8B8";
        definition.ReplacementPath = "replacement.dds";
        const helen::PackScopedTextureReplacementDefinition scoped("fault", "test", helen::PackAssetResolver(root, root), definition);
        g_definition = &scoped;
        for (Fault fault : {Fault::Create, Fault::Lock, Fault::Unlock, Fault::InvalidateCreate,
            Fault::Duplicate, Fault::Reuse, Fault::Bind, Fault::InvalidateBind, Fault::NewBinding, Fault::InvalidateBindChange, Fault::RewriteCreate}) {
            g_fault = fault; g_created = nullptr; g_texture_table = nullptr; g_destroyed = 0; g_duplicate_rejected = false; g_rebound = false;
            Microsoft::WRL::ComPtr<IDirect3DTexture9> source;
            RequireSuccess(device.CreateTexture(4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                source.GetAddressOf(), nullptr), "Create fault source");
            g_source = source.Get();
            RequireSuccess(source->GetLevelDesc(0, &g_description), "Read fault source");
            void** const device_table = GetComVtable(&device);
            const std::vector<void*> originals = g_original_dispatch.Capture(device_table, Direct3d9DeviceVtableSlotCount);
            g_create = reinterpret_cast<Direct3d9CreateTextureFunction>(originals[23]);
            g_get = reinterpret_cast<Direct3d9GetTextureFunction>(originals[64]);
            g_set = reinterpret_cast<Direct3d9SetTextureFunction>(originals[65]);
            RequireSuccess(g_set(&device, 0, g_source), "Bind fault source");
            std::vector<void*> patched = originals;
            patched[23] = reinterpret_cast<void*>(&FaultCreate);
            patched[64] = reinterpret_cast<void*>(&FaultGet);
            patched[65] = reinterpret_cast<void*>(&FaultSet);
            g_original_dispatch.Replace(device_table, patched);
            HRESULT error = S_OK;
            const bool cached = TryCacheReplacementTexture(device, g_source, g_description, scoped, error);
            g_original_dispatch.Replace(device_table, originals);
            Expect(cached == (fault == Fault::Duplicate || fault == Fault::NewBinding), "Fault case returned an incorrect cache result.");
            const HRESULT expected_error = cached ? S_OK :
                (fault == Fault::Create || fault == Fault::Lock) ? E_OUTOFMEMORY :
                (fault == Fault::Unlock || fault == Fault::Bind) ? E_FAIL : D3DERR_INVALIDCALL;
            Expect(error == expected_error, "Cache changed the reported failure HRESULT.");
            if (fault == Fault::Duplicate) { Expect(g_duplicate_rejected, "Nested cache attempt was not rejected."); }
            Microsoft::WRL::ComPtr<IDirect3DBaseTexture9> bound;
            RequireSuccess(g_get(&device, 0, bound.GetAddressOf()), "Read binding after fault");
            IDirect3DBaseTexture9* const expected_binding = fault == Fault::NewBinding || fault == Fault::InvalidateBindChange
                ? nullptr : (cached ? g_created : g_source);
            Expect(bound.Get() == expected_binding, "Cache or rollback overwrote a newer binding, or left an unpublished replacement bound.");
            bound.Reset();
            RequireSuccess(g_set(&device, 0, nullptr), "Unbind fault source");
            {
                std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
                Expect(g_tracked_texture_records.at(g_source).PendingReplacement.expired(), "Failure left an operation permanently pending.");
            }
            source.Reset();
            Expect(g_destroyed == (fault == Fault::Create ? 0u : 1u), "Replacement was leaked or destroyed more than once.");
            if (g_texture_table != nullptr) { g_original_dispatch.Replace(g_texture_table, g_texture_originals); }
        }
        std::cout << "D3D9_REPLACEMENT_FAILURE_PASS\n";
    }
}

/** Runs the white-box driver-callback fixture with no production test hooks or dialogs. */
int wmain(int argc, wchar_t** argv) {
    SetErrorMode(0x8003);
    try {
        Expect(argc == 2, "Expected a fresh fixture directory.");
        Run(argv[1], &RunFailures);
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
