#include <HelenHook/D3d9TextureReplacementHookSet.h>
#include <HelenHook/D3d9ResetDiagnostics.h>
#include <HelenHook/ComOriginalDispatch.h>
#include <HelenHook/D3d9ReplacementTicket.h>
#include <HelenHook/D3d9TextureUpload.h>
#include <HelenHook/D3d9TextureBindingTransaction.h>
#include <wrl/client.h>

#include <HelenHook/Log.h>
#include <HelenHook/Memory.h>
#include <HelenHook/TextureReplacementAssetLoader.h>
#include <HelenHook/TextureDumpSerializer.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <string_view>
#include <utility>
#include <vector>
#include <wincrypt.h>

#pragma comment(lib, "advapi32.lib")

using helen::Log;
using helen::Logf;

namespace
{
    /**
     * @brief Hexadecimal digits used to materialize lowercase hash text.
     */
    constexpr std::string_view LowerHexDigits = "0123456789abcdef";

    /** @brief Vtable index of `IUnknown::Release` shared by `IDirect3D9` and `IDirect3DDevice9`. */
    constexpr std::size_t ReleaseVtableIndex = 2;

    /** @brief Vtable index of `IDirect3D9::CreateDevice`. */
    constexpr std::size_t Direct3d9CreateDeviceVtableIndex = 16;

    /** @brief Vtable index of `IDirect3D9::Release`. */
    constexpr std::size_t Direct3d9ReleaseVtableIndex = 2;

    /** @brief Vtable index of `IDirect3DDevice9::Reset`. */
    constexpr std::size_t Direct3d9ResetVtableIndex = 16;

    /** @brief Vtable index of `IDirect3DDevice9::GetDirect3D`. */
    constexpr std::size_t Direct3d9GetDirect3DVtableIndex = 6;

    /** @brief Vtable index of `IDirect3DDevice9::SetCursorProperties`. */
    constexpr std::size_t Direct3d9SetCursorPropertiesVtableIndex = 10;

    /** @brief Vtable index of `IDirect3DDevice9::CreateAdditionalSwapChain`. */
    constexpr std::size_t Direct3d9CreateAdditionalSwapChainVtableIndex = 13;

    /** @brief Vtable index of `IDirect3DDevice9::GetSwapChain`. */
    constexpr std::size_t Direct3d9GetSwapChainVtableIndex = 14;

    /** @brief Vtable index of `IDirect3DDevice9::GetBackBuffer`. */
    constexpr std::size_t Direct3d9GetBackBufferVtableIndex = 18;

    /** @brief Vtable index of `IDirect3DDevice9::CreateTexture`. */
    constexpr std::size_t Direct3d9CreateTextureVtableIndex = 23;

    /** @brief Vtable index of `IDirect3DDevice9::CreateRenderTarget`. */
    constexpr std::size_t Direct3d9CreateRenderTargetVtableIndex = 28;

    /** @brief Vtable index of `IDirect3DDevice9::CreateDepthStencilSurface`. */
    constexpr std::size_t Direct3d9CreateDepthStencilSurfaceVtableIndex = 29;

    /** @brief Vtable index of `IDirect3DDevice9::UpdateSurface`. */
    constexpr std::size_t Direct3d9UpdateSurfaceVtableIndex = 30;

    /** @brief Vtable index of `IDirect3DDevice9::UpdateTexture`. */
    constexpr std::size_t Direct3d9UpdateTextureVtableIndex = 31;

    /** @brief Vtable index of `IDirect3DDevice9::GetTexture`. */
    constexpr std::size_t Direct3d9GetTextureVtableIndex = 64;

    /** @brief Vtable index of `IDirect3DDevice9::SetTexture`. */
    constexpr std::size_t Direct3d9SetTextureVtableIndex = 65;

    /** @brief Vtable index of `IDirect3DDevice9::GetRenderTargetData`. */
    constexpr std::size_t Direct3d9GetRenderTargetDataVtableIndex = 32;

    /** @brief Vtable index of `IDirect3DDevice9::GetFrontBufferData`. */
    constexpr std::size_t Direct3d9GetFrontBufferDataVtableIndex = 33;

    /** @brief Vtable index of `IDirect3DDevice9::StretchRect`. */
    constexpr std::size_t Direct3d9StretchRectVtableIndex = 34;

    /** @brief Vtable index of `IDirect3DDevice9::ColorFill`. */
    constexpr std::size_t Direct3d9ColorFillVtableIndex = 35;

    /** @brief Vtable index of `IDirect3DDevice9::CreateOffscreenPlainSurface`. */
    constexpr std::size_t Direct3d9CreateOffscreenPlainSurfaceVtableIndex = 36;

    /** @brief Vtable index of `IDirect3DDevice9::SetRenderTarget`. */
    constexpr std::size_t Direct3d9SetRenderTargetVtableIndex = 37;

    /** @brief Vtable index of `IDirect3DDevice9::GetRenderTarget`. */
    constexpr std::size_t Direct3d9GetRenderTargetVtableIndex = 38;

    /** @brief Vtable index of `IDirect3DDevice9::SetDepthStencilSurface`. */
    constexpr std::size_t Direct3d9SetDepthStencilSurfaceVtableIndex = 39;

    /** @brief Vtable index of `IDirect3DDevice9::GetDepthStencilSurface`. */
    constexpr std::size_t Direct3d9GetDepthStencilSurfaceVtableIndex = 40;

    /** @brief Number of function slots exposed by `IDirect3D9`. */
    constexpr std::size_t Direct3d9VtableSlotCount = 17;

    /** @brief Number of function slots exposed by `IDirect3DDevice9`. */
    constexpr std::size_t Direct3d9DeviceVtableSlotCount = 119;

    /** @brief Vtable index of `IDirect3DTexture9::LockRect`. */
    constexpr std::size_t Direct3d9TextureLockRectVtableIndex = 19;

    /** @brief Vtable index of `IDirect3DTexture9::UnlockRect`. */
    constexpr std::size_t Direct3d9TextureUnlockRectVtableIndex = 20;

    /** @brief Vtable index of `IDirect3DTexture9::GetSurfaceLevel`. */
    constexpr std::size_t Direct3d9TextureGetSurfaceLevelVtableIndex = 18;

    /**
     * @brief Number of function slots exposed by `IDirect3DTexture9`.
     *
     * The texture interface ends at `AddDirtyRect`, so the shadow table must only copy
     * the first twenty-two slots. Copying past the real COM vtable would read unrelated
     * memory and corrupt the replacement table.
     */
    constexpr std::size_t Direct3d9TextureVtableSlotCount = 22;

    /** @brief Number of function slots exposed by `IDirect3DSurface9`. */
    constexpr std::size_t Direct3d9SurfaceVtableSlotCount = 17;

    /** @brief Number of function slots exposed by `IDirect3DSwapChain9`. */
    constexpr std::size_t Direct3d9SwapChainVtableSlotCount = 10;

    /**
     * @brief Preserves the first-seen original vtable image for each patched COM table.
     *
     * The runtime patches shared D3D9 vtables in place. Later objects that point at the
     * same live vtable must reuse the original snapshot captured before the first patch.
     */
    helen::ComOriginalDispatch g_original_dispatch;

    /** @brief Raw function pointer type for one live `IDirect3D9::CreateDevice` implementation. */
    using Direct3d9CreateDeviceFunction = HRESULT(WINAPI*)(
        IDirect3D9*,
        UINT,
        D3DDEVTYPE,
        HWND,
        DWORD,
        D3DPRESENT_PARAMETERS*,
        IDirect3DDevice9**);

    /** @brief Raw function pointer type for one live `IDirect3D9::Release` implementation. */
    using Direct3d9ReleaseFunction = ULONG(WINAPI*)(IDirect3D9*);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::GetDirect3D` implementation. */
    using Direct3d9GetDirect3DFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, IDirect3D9**);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::SetCursorProperties` implementation. */
    using Direct3d9SetCursorPropertiesFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, UINT, UINT, IDirect3DSurface9*);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::CreateAdditionalSwapChain` implementation. */
    using Direct3d9CreateAdditionalSwapChainFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*, IDirect3DSwapChain9**);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::GetSwapChain` implementation. */
    using Direct3d9GetSwapChainFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, UINT, IDirect3DSwapChain9**);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::GetBackBuffer` implementation. */
    using Direct3d9GetBackBufferFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, UINT, UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface9**);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::CreateRenderTarget` implementation. */
    using Direct3d9CreateRenderTargetFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE, DWORD, BOOL, IDirect3DSurface9**, HANDLE*);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::CreateDepthStencilSurface` implementation. */
    using Direct3d9CreateDepthStencilSurfaceFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE, DWORD, BOOL, IDirect3DSurface9**, HANDLE*);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::UpdateSurface` implementation. */
    using Direct3d9UpdateSurfaceFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, IDirect3DSurface9*, const RECT*, IDirect3DSurface9*, const POINT*);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::CreateTexture` implementation. */
    using Direct3d9CreateTextureFunction = HRESULT(WINAPI*)(
        IDirect3DDevice9*,
        UINT,
        UINT,
        UINT,
        DWORD,
        D3DFORMAT,
        D3DPOOL,
        IDirect3DTexture9**,
        HANDLE*);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::UpdateTexture` implementation. */
    using Direct3d9UpdateTextureFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, IDirect3DBaseTexture9*, IDirect3DBaseTexture9*);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::GetTexture` implementation. */
    using Direct3d9GetTextureFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9**);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::SetTexture` implementation. */
    using Direct3d9SetTextureFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::GetRenderTargetData` implementation. */
    using Direct3d9GetRenderTargetDataFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, IDirect3DSurface9*, IDirect3DSurface9*);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::GetFrontBufferData` implementation. */
    using Direct3d9GetFrontBufferDataFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, UINT, IDirect3DSurface9*);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::StretchRect` implementation. */
    using Direct3d9StretchRectFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, IDirect3DSurface9*, const RECT*, IDirect3DSurface9*, const RECT*, D3DTEXTUREFILTERTYPE);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::ColorFill` implementation. */
    using Direct3d9ColorFillFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, IDirect3DSurface9*, const RECT*, D3DCOLOR);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::CreateOffscreenPlainSurface` implementation. */
    using Direct3d9CreateOffscreenPlainSurfaceFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, UINT, UINT, D3DFORMAT, D3DPOOL, IDirect3DSurface9**, HANDLE*);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::SetRenderTarget` implementation. */
    using Direct3d9SetRenderTargetFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, DWORD, IDirect3DSurface9*);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::GetRenderTarget` implementation. */
    using Direct3d9GetRenderTargetFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, DWORD, IDirect3DSurface9**);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::SetDepthStencilSurface` implementation. */
    using Direct3d9SetDepthStencilSurfaceFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, IDirect3DSurface9*);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::GetDepthStencilSurface` implementation. */
    using Direct3d9GetDepthStencilSurfaceFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, IDirect3DSurface9**);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::Reset` implementation. */
    using Direct3d9ResetFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

    /** @brief Raw function pointer type for one live `IDirect3DDevice9::Release` implementation. */
    using Direct3d9DeviceReleaseFunction = ULONG(WINAPI*)(IDirect3DDevice9*);

    /** @brief Raw function pointer type for one live `IDirect3DTexture9::LockRect` implementation. */
    using Direct3d9TextureLockRectFunction = HRESULT(WINAPI*)(
        IDirect3DTexture9*,
        UINT,
        D3DLOCKED_RECT*,
        const RECT*,
        DWORD);

    /** @brief Raw function pointer type for one live `IDirect3DTexture9::UnlockRect` implementation. */
    using Direct3d9TextureUnlockRectFunction = HRESULT(WINAPI*)(IDirect3DTexture9*, UINT);

    /** @brief Raw function pointer type for one live `IDirect3DTexture9::Release` implementation. */
    using Direct3d9TextureReleaseFunction = ULONG(WINAPI*)(IDirect3DTexture9*);

    /** @brief Raw function pointer type for one live `IDirect3DTexture9::GetSurfaceLevel` implementation. */
    using Direct3d9TextureGetSurfaceLevelFunction = HRESULT(WINAPI*)(IDirect3DTexture9*, UINT, IDirect3DSurface9**);

    /** @brief Raw function pointer type for one live `IDirect3DSurface9::Release` implementation. */
    using Direct3d9SurfaceReleaseFunction = ULONG(WINAPI*)(IDirect3DSurface9*);

    /** @brief Raw function pointer type for one live `IDirect3DSurface9::GetDevice` implementation. */
    using Direct3d9SurfaceGetDeviceFunction = HRESULT(WINAPI*)(IDirect3DSurface9*, IDirect3DDevice9**);

    /** @brief Raw function pointer type for one live `IDirect3DSurface9::LockRect` implementation. */
    using Direct3d9SurfaceLockRectFunction = HRESULT(WINAPI*)(IDirect3DSurface9*, D3DLOCKED_RECT*, const RECT*, DWORD);

    /** @brief Raw function pointer type for one live `IDirect3DSurface9::UnlockRect` implementation. */
    using Direct3d9SurfaceUnlockRectFunction = HRESULT(WINAPI*)(IDirect3DSurface9*);

    /** @brief Raw function pointer type for one live `IDirect3DSwapChain9::Release` implementation. */
    using Direct3d9SwapChainReleaseFunction = ULONG(WINAPI*)(IDirect3DSwapChain9*);

    /** @brief Raw function pointer type for one live `IDirect3DSwapChain9::GetDevice` implementation. */
    using Direct3d9SwapChainGetDeviceFunction = HRESULT(WINAPI*)(IDirect3DSwapChain9*, IDirect3DDevice9**);

    /** @brief Raw function pointer type for one live `IDirect3DSwapChain9::GetBackBuffer` implementation. */
    using Direct3d9SwapChainGetBackBufferFunction = HRESULT(WINAPI*)(IDirect3DSwapChain9*, UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface9**);

    /**
     * @brief Stores the stable fingerprint extracted from one tracked texture upload.
     */
    struct TextureFingerprint
    {
        /** @brief Width of the tracked texture in pixels. */
        std::uint32_t Width{};
        /** @brief Height of the tracked texture in pixels. */
        std::uint32_t Height{};
        /** @brief Runtime texture format observed for the tracked image. */
        D3DFORMAT Format{};
        /** @brief Lowercase SHA-256 digest of the level-0 texture bytes. */
        std::string Hash;
    };

    /**
     * @brief Stores the live lifecycle state associated with one tracked `IDirect3DTexture9` instance.
     */
    struct TrackedTextureRecord
    {
        /** @brief Proxy device pointer that created the tracked texture and owns reset invalidation for it. */
        IDirect3DDevice9* OwningDevice{};
        /** @brief Last level-0 description observed for the tracked texture. */
        D3DSURFACE_DESC Description{};
        /** @brief Tracks whether the texture has a description that can be hashed or logged. */
        bool HasDescription{};
        /** @brief Tracks whether a writable lock has occurred since the last fingerprint. */
        bool Dirty{};
        /** @brief Tracks whether the texture has been fingerprinted at least once. */
        bool HasFingerprint{};
        /** @brief Tracks whether the texture fingerprint was already emitted to the log. */
        bool FingerprintLogged{};
        /** @brief Tracks whether a declared replacement rule already matched this fingerprint. */
        bool ReplacementMatched{};
        /** @brief Replacement texture object created from the declared higher-resolution asset. */
        std::shared_ptr<IDirect3DTexture9> ReplacementTexture;
        /** Weak operation identity invalidated by reset/release; it owns no source COM reference. */
        std::weak_ptr<helen::D3d9ReplacementTicket> PendingReplacement;
        /** CPU registration/reset identity; address reuse or reset invalidates previously captured uploads. */
        std::shared_ptr<helen::D3d9ReplacementTicket> Identity = std::make_shared<helen::D3d9ReplacementTicket>();
        /** @brief Retains the matched asset identity across failed resets until its replacement can be recreated. */
        bool ReplacementNeedsRestore{};
        /** @brief Tracks whether the texture has a last-seen `SetTexture` stage that can be rebound after caching. */
        bool HasLastSetTextureStage{};
        /** @brief Last texture stage that observed this texture in `SetTexture`. */
        DWORD LastSetTextureStage{};
        /** @brief Stable fingerprint produced from the latest writable upload. */
        TextureFingerprint Fingerprint;
        /** @brief Tracks whether the current writable lock captured a snapshot that can be fingerprinted before unlock. */
        bool HasWritableLockSnapshot{};
        /** @brief Start address of the most recent writable level-0 lock buffer while it is still valid. */
        const std::uint8_t* WritableLockBytes{};
        /** @brief Pitch in bytes of the most recent writable level-0 lock buffer. */
        LONG WritableLockPitch{};
    };

    /**
     * @brief Common storage for one shadow-vtable COM hook built around a live `D3D9` interface.
     */
    struct D3d9ProxyBase
    {
        /** @brief Preserved original vtable snapshot used for forwarding to the real COM methods. */
        std::vector<void*> OriginalVtableStorage;
        /** @brief Pointer into the preserved original vtable snapshot. */
        void** OriginalVtable{};
        /** @brief Patched vtable image written back into the live COM table. */
        std::vector<void*> ShadowVtable;
    };

    /**
     * @brief Stores one wrapped `IDirect3D9` instance returned from `Direct3DCreate9`.
     */
    struct Direct3d9HookContext : D3d9ProxyBase
    {
        /** @brief Real `IDirect3D9` interface owned by the Direct3D runtime. */
        IDirect3D9* Real{};
    };

    /**
     * @brief Stores one wrapped `IDirect3DDevice9` instance returned from `CreateDevice`.
     */
    struct Direct3d9DeviceHookContext : D3d9ProxyBase
    {
        /** @brief Real `IDirect3DDevice9` interface owned by the Direct3D runtime. */
        IDirect3DDevice9* Real{};
        /** @brief Set of wrapped textures created by this device and tracked through lifecycle hooks. */
        std::unordered_set<IDirect3DTexture9*> TrackedTextures;
        /** @brief Last texture pointer observed for each texture stage on this device. */
        std::unordered_map<DWORD, IDirect3DBaseTexture9*> BoundTextures;
        /** @brief Per-device reset failure suppression, protected by g_d3d9_hook_mutex. */
        helen::D3d9ResetDiagnostics ResetDiagnostics;
        /** @brief Parameter-array length established before publishing the device context; immutable thereafter. */
        UINT PresentationParameterCount{};
        /** Blocks publication during a native reset, including reentrant resource callbacks. */
        bool ResetInProgress{};
        /** Monotonic application binding/reset revision used to reject reentrant stale stage observations. */
        std::uint64_t BindingRevision{};
    };

    /**
     * @brief Stores one wrapped `IDirect3DTexture9` instance returned from `CreateTexture`.
     */
    struct Direct3d9TextureHookContext : D3d9ProxyBase
    {
        /** @brief Real `IDirect3DTexture9` interface owned by the Direct3D runtime. */
        IDirect3DTexture9* Real{};
    };

    /**
     * @brief Stores one wrapped `IDirect3DSurface9` instance returned from a texture, swap chain, or device query.
     */
    struct Direct3d9SurfaceHookContext : D3d9ProxyBase
    {
        /** @brief Real `IDirect3DSurface9` interface owned by the Direct3D runtime. */
        IDirect3DSurface9* Real{};
        /** @brief Owning wrapped texture when the surface came from `GetSurfaceLevel`. */
        IDirect3DTexture9* OwningTexture{};
        /** Texture mip level; every level participates in lifetime tracking, but only zero supplies replacement pixels. */
        UINT TextureLevel{};
        /** @brief Owning wrapped swap chain when the surface came from `GetBackBuffer`. */
        IDirect3DSwapChain9* OwningSwapChain{};
    };

    /**
     * @brief Stores one wrapped `IDirect3DSwapChain9` instance returned from a device query.
     */
    struct Direct3d9SwapChainHookContext : D3d9ProxyBase
    {
        /** @brief Real `IDirect3DSwapChain9` interface owned by the Direct3D runtime. */
        IDirect3DSwapChain9* Real{};
    };

    /**
     * @brief Synchronizes live `IDirect3D9`, `IDirect3DDevice9`, `IDirect3DTexture9`, `IDirect3DSurface9`, and `IDirect3DSwapChain9` proxy registrations.
     */
    std::mutex g_d3d9_hook_mutex;

    /**
     * @brief Live wrapped `IDirect3D9` proxies keyed by proxy pointer.
     */
    std::unordered_map<IDirect3D9*, std::unique_ptr<Direct3d9HookContext>> g_direct3d9_hook_contexts;

    /**
     * @brief Reverse lookup from real `IDirect3D9` pointers to their proxy pointer.
     */
    std::unordered_map<IDirect3D9*, IDirect3D9*> g_direct3d9_hook_by_real;

    /**
     * @brief Live wrapped `IDirect3DDevice9` proxies keyed by proxy pointer.
     */
    std::unordered_map<IDirect3DDevice9*, std::unique_ptr<Direct3d9DeviceHookContext>> g_direct3d9_device_hook_contexts;

    /**
     * @brief Reverse lookup from real `IDirect3DDevice9` pointers to their proxy pointer.
     */
    std::unordered_map<IDirect3DDevice9*, IDirect3DDevice9*> g_direct3d9_device_hook_by_real;

    /**
     * @brief Live wrapped `IDirect3DTexture9` proxies keyed by proxy pointer.
     */
    std::unordered_map<IDirect3DTexture9*, std::unique_ptr<Direct3d9TextureHookContext>> g_direct3d9_texture_hook_contexts;

    /**
     * @brief Reverse lookup from real `IDirect3DTexture9` pointers to their proxy pointer.
     */
    std::unordered_map<IDirect3DTexture9*, IDirect3DTexture9*> g_direct3d9_texture_hook_by_real;

    /**
     * @brief Live wrapped `IDirect3DSurface9` proxies keyed by proxy pointer.
     */
    std::unordered_map<IDirect3DSurface9*, std::unique_ptr<Direct3d9SurfaceHookContext>> g_direct3d9_surface_hook_contexts;

    /**
     * @brief Reverse lookup from real `IDirect3DSurface9` pointers to their proxy pointer.
     */
    std::unordered_map<IDirect3DSurface9*, IDirect3DSurface9*> g_direct3d9_surface_hook_by_real;

    /**
     * @brief Live texture lifecycle records used to track upload, hash, and match state.
     */
    std::unordered_map<IDirect3DTexture9*, TrackedTextureRecord> g_tracked_texture_records;

    /**
     * @brief Returns the unpadded row layout required to fingerprint one level-0 texture image.
     * @param description Level-0 texture description whose data layout should be summarized.
     * @param row_count Receives the number of logical rows or compressed block rows to hash.
     * @param bytes_per_row Receives the unpadded byte count that should be hashed from each row.
     * @return True when the format is recognized and row layout is available; otherwise false.
     */
    bool TryGetTextureRowLayout(
        const D3DSURFACE_DESC& description,
        std::uint32_t& row_count,
        std::uint32_t& bytes_per_row)
    {
        row_count = 0;
        bytes_per_row = 0;

        switch (description.Format)
        {
        case D3DFMT_A8R8G8B8:
            row_count = description.Height;
            bytes_per_row = description.Width * 4;
            return true;
        case D3DFMT_A8:
        case D3DFMT_L8:
            row_count = description.Height;
            bytes_per_row = description.Width;
            return true;
        case D3DFMT_DXT1:
            row_count = (std::max)(1u, (description.Height + 3) / 4);
            bytes_per_row = (std::max)(1u, (description.Width + 3) / 4) * 8;
            return true;
        case D3DFMT_DXT3:
        case D3DFMT_DXT5:
            row_count = (std::max)(1u, (description.Height + 3) / 4);
            bytes_per_row = (std::max)(1u, (description.Width + 3) / 4) * 16;
            return true;
        default:
            return false;
        }
    }

    /**
     * @brief Opens a transient Win32 cryptographic provider suitable for SHA-256 hashing.
     * @param provider Receives the opened provider handle on success.
     * @return True when the provider opens successfully; otherwise false.
     */
    bool TryOpenHashProvider(HCRYPTPROV& provider)
    {
        provider = 0;
        return CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) == TRUE;
    }

    /**
     * @brief Opens a SHA-256 hash object from one transient cryptographic provider.
     * @param provider Open cryptographic provider handle.
     * @param hash Receives the opened hash handle on success.
     * @return True when the hash object opens successfully; otherwise false.
     */
    bool TryOpenSha256Hash(HCRYPTPROV provider, HCRYPTHASH& hash)
    {
        hash = 0;
        return CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash) == TRUE;
    }

    /**
     * @brief Finalizes one SHA-256 hash object into lowercase hexadecimal text.
     * @param hash Open hash handle whose digest should be materialized.
     * @param digest Receives the lowercase hexadecimal digest when hashing succeeds.
     * @return True when the digest is produced successfully; otherwise false.
     */
    bool TryFinalizeHash(HCRYPTHASH hash, std::string& digest)
    {
        DWORD hash_length = 0;
        DWORD hash_length_size = sizeof(hash_length);
        if (CryptGetHashParam(hash, HP_HASHSIZE, reinterpret_cast<BYTE*>(&hash_length), &hash_length_size, 0) != TRUE)
        {
            return false;
        }

        std::vector<std::uint8_t> hash_bytes(hash_length, 0);
        DWORD requested_length = hash_length;
        if (CryptGetHashParam(hash, HP_HASHVAL, reinterpret_cast<BYTE*>(hash_bytes.data()), &requested_length, 0) != TRUE)
        {
            return false;
        }

        if (requested_length != hash_length)
        {
            return false;
        }

        digest.clear();
        digest.reserve(hash_bytes.size() * 2);
        for (const std::uint8_t byte : hash_bytes)
        {
            digest.push_back(LowerHexDigits[(byte >> 4) & 0x0F]);
            digest.push_back(LowerHexDigits[byte & 0x0F]);
        }

        return true;
    }

    /**
     * @brief Writes one full byte buffer to disk, replacing any prior file.
     * @param output_path Absolute or relative destination file path.
     * @param bytes Byte buffer that should be written to disk.
     * @return True when the file is written successfully; otherwise false.
     */
    bool TryWriteAllBytes(const std::filesystem::path& output_path, const std::vector<std::uint8_t>& bytes)
    {
        std::ofstream stream(output_path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            return false;
        }

        stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        return static_cast<bool>(stream);
    }

    /**
     * @brief DDS header bit flags used by the replacement loader.
     */
    constexpr std::uint32_t DdsdCaps = 0x00000001u;
    constexpr std::uint32_t DdsdHeight = 0x00000002u;
    constexpr std::uint32_t DdsdWidth = 0x00000004u;
    constexpr std::uint32_t DdsdPixelFormat = 0x00001000u;
    constexpr std::uint32_t DdsdLinearSize = 0x00080000u;
    constexpr std::uint32_t DdsCapsTexture = 0x00001000u;

    /**
     * @brief DDS pixel-format flags used by the replacement loader.
     */
    constexpr std::uint32_t DdpfFourCc = 0x00000004u;

    /**
     * @brief DDS capability flags required for texture files.
     */
    /**
     * @brief FourCC value for DDS-compressed DXT1 payloads.
     */
    constexpr std::uint32_t FourCcDxt1 = 0x31545844u;

    /**
     * @brief FourCC value for DDS-compressed DXT3 payloads.
     */
    constexpr std::uint32_t FourCcDxt3 = 0x33545844u;

    /**
     * @brief FourCC value for DDS-compressed DXT5 payloads.
     */
    constexpr std::uint32_t FourCcDxt5 = 0x35545844u;

#pragma pack(push, 1)
    /**
     * @brief Raw DDS pixel-format header block used for replacement validation.
     */
    struct DdsPixelFormat
    {
        std::uint32_t Size;
        std::uint32_t Flags;
        std::uint32_t FourCc;
        std::uint32_t RgbBitCount;
        std::uint32_t RedMask;
        std::uint32_t GreenMask;
        std::uint32_t BlueMask;
        std::uint32_t AlphaMask;
    };

    /**
     * @brief Raw DDS header block used for replacement validation.
     */
    struct DdsHeader
    {
        std::uint32_t Size;
        std::uint32_t Flags;
        std::uint32_t Height;
        std::uint32_t Width;
        std::uint32_t PitchOrLinearSize;
        std::uint32_t Depth;
        std::uint32_t MipMapCount;
        std::uint32_t Reserved1[11];
        DdsPixelFormat PixelFormat;
        std::uint32_t Caps;
        std::uint32_t Caps2;
        std::uint32_t Caps3;
        std::uint32_t Caps4;
        std::uint32_t Reserved2;
    };
#pragma pack(pop)

    /**
     * @brief Returns the DDS FourCC used by one supported compressed D3D9 format.
     * @param format Source texture format that should be loaded from a DDS replacement asset.
     * @return Expected FourCC token for the compressed format or zero when unsupported.
     */
    std::uint32_t GetReplacementDdsFourCc(D3DFORMAT format)
    {
        switch (format)
        {
        case D3DFMT_DXT1:
            return FourCcDxt1;
        case D3DFMT_DXT3:
            return FourCcDxt3;
        case D3DFMT_DXT5:
            return FourCcDxt5;
        default:
            return 0;
        }
    }

    /**
     * @brief Reconstructs one DDS file image from a loaded replacement payload.
     * @param asset Replacement asset whose DDS header should be recreated.
     * @param dds_bytes Receives the full DDS file image on success.
     * @param failure_result Receives the HRESULT-style failure reason when reconstruction fails.
     * @return True when the payload can be represented as one valid DDS file.
     */
    bool TryBuildDdsFileBytes(
        const helen::TextureReplacementAsset& asset,
        std::vector<std::uint8_t>& dds_bytes,
        HRESULT& failure_result)
    {
        failure_result = S_OK;
        dds_bytes.clear();

        if (asset.Format != D3DFMT_DXT5 || asset.Width == 0u || asset.Height == 0u || asset.Level0Bytes.empty())
        {
            failure_result = E_FAIL;
            return false;
        }

        const std::uint32_t blocks_wide = (std::max)(1u, (asset.Width + 3u) / 4u);
        const std::uint32_t blocks_high = (std::max)(1u, (asset.Height + 3u) / 4u);
        const std::size_t expected_size = static_cast<std::size_t>(blocks_wide) * static_cast<std::size_t>(blocks_high) * 16u;
        if (asset.Level0Bytes.size() != expected_size)
        {
            failure_result = E_FAIL;
            return false;
        }

        dds_bytes.resize(4u + sizeof(DdsHeader) + asset.Level0Bytes.size(), 0u);
        std::memcpy(dds_bytes.data(), "DDS ", 4u);

        DdsHeader header{};
        header.Size = 124u;
        header.Flags = DdsdCaps | DdsdHeight | DdsdWidth | DdsdPixelFormat | DdsdLinearSize;
        header.Height = asset.Height;
        header.Width = asset.Width;
        header.PitchOrLinearSize = static_cast<std::uint32_t>(asset.Level0Bytes.size());
        header.Depth = 0u;
        header.MipMapCount = 1u;
        header.PixelFormat.Size = 32u;
        header.PixelFormat.Flags = DdpfFourCc;
        header.PixelFormat.FourCc = FourCcDxt5;
        header.Caps = DdsCapsTexture;
        header.Caps2 = 0u;
        header.Caps3 = 0u;
        header.Caps4 = 0u;
        header.Reserved2 = 0u;

        std::memcpy(dds_bytes.data() + 4u, &header, sizeof(header));
        std::memcpy(dds_bytes.data() + 4u + sizeof(header), asset.Level0Bytes.data(), asset.Level0Bytes.size());
        return true;
    }

    /**
     * @brief Raw D3DX texture loader function signature used to avoid a permanent SDK dependency.
     */
    using D3dxCreateTextureFromFileInMemoryExFunction = HRESULT(WINAPI*)(
        LPDIRECT3DDEVICE9,
        LPCVOID,
        UINT,
        UINT,
        UINT,
        UINT,
        DWORD,
        D3DFORMAT,
        D3DPOOL,
        DWORD,
        DWORD,
        D3DCOLOR,
        void*,
        void*,
        LPDIRECT3DTEXTURE9*);

    /**
     * @brief Loads one replacement texture through the local D3DX runtime into a bindable texture object.
     * @param device Real `IDirect3DDevice9` instance that owns the texture replacement.
     * @param asset Replacement DDS payload that should be turned into a live texture object.
     * @param replacement_texture Receives the created replacement texture on success.
     * @param failure_result Receives the HRESULT-style failure reason when loading fails.
     * @return True when D3DX creates the replacement texture successfully.
     */
    bool TryLoadReplacementTextureViaD3dx(
        IDirect3DDevice9* device,
        const helen::TextureReplacementAsset& asset,
        IDirect3DTexture9*& replacement_texture,
        HRESULT& failure_result)
    {
        failure_result = S_OK;
        replacement_texture = nullptr;

        if (device == nullptr)
        {
            failure_result = E_FAIL;
            return false;
        }

        Logf(
            L"[d3d9] d3dx replacement loader begin device=0x%p asset=%ux%u format=0x%08lX bytes=%zu",
            device,
            static_cast<unsigned int>(asset.Width),
            static_cast<unsigned int>(asset.Height),
            static_cast<unsigned long>(asset.Format),
            asset.Level0Bytes.size());

        HMODULE module = LoadLibraryW(L"d3dx9_43.dll");
        if (module == nullptr)
        {
            failure_result = HRESULT_FROM_WIN32(GetLastError());
            Logf(
                L"[d3d9] d3dx replacement loader failed to load d3dx9_43.dll hr=0x%08lX",
                static_cast<unsigned long>(failure_result));
            return false;
        }

        Logf(L"[d3d9] d3dx replacement loader loaded module handle=0x%p", module);

        const auto create_texture = reinterpret_cast<D3dxCreateTextureFromFileInMemoryExFunction>(
            GetProcAddress(module, "D3DXCreateTextureFromFileInMemoryEx"));
        if (create_texture == nullptr)
        {
            failure_result = HRESULT_FROM_WIN32(GetLastError());
            Logf(
                L"[d3d9] d3dx replacement loader failed to resolve D3DXCreateTextureFromFileInMemoryEx hr=0x%08lX",
                static_cast<unsigned long>(failure_result));
            FreeLibrary(module);
            return false;
        }

        Logf(L"[d3d9] d3dx replacement loader resolved D3DXCreateTextureFromFileInMemoryEx");

        std::vector<std::uint8_t> dds_bytes;
        if (!TryBuildDdsFileBytes(asset, dds_bytes, failure_result))
        {
            Logf(
                L"[d3d9] d3dx replacement loader failed to reconstruct DDS file image hr=0x%08lX",
                static_cast<unsigned long>(failure_result));
            FreeLibrary(module);
            return false;
        }

        Logf(
            L"[d3d9] d3dx replacement loader reconstructed DDS file image bytes=%zu",
            dds_bytes.size());

        IDirect3DTexture9* texture = nullptr;
        Logf(L"[d3d9] d3dx replacement loader invoking D3DXCreateTextureFromFileInMemoryEx");
        const HRESULT create_result = create_texture(
            device,
            dds_bytes.data(),
            static_cast<UINT>(dds_bytes.size()),
            asset.Width,
            asset.Height,
            1u,
            0u,
            D3DFMT_A8R8G8B8,
            D3DPOOL_MANAGED,
            0xFFFFFFFFu,
            0xFFFFFFFFu,
            0u,
            nullptr,
            nullptr,
            &texture);
        Logf(
            L"[d3d9] d3dx replacement loader returned hr=0x%08lX texture=0x%p",
            static_cast<unsigned long>(create_result),
            texture);
        FreeLibrary(module);

        if (FAILED(create_result) || texture == nullptr)
        {
            failure_result = create_result;
            return false;
        }

        replacement_texture = texture;
        return true;
    }

    /**
     * @brief Expands one packed `RGB565` color into 8-bit channel values.
     * @param packed_color Packed `RGB565` color value from a DXT color block.
     * @param red Receives the expanded red channel.
     * @param green Receives the expanded green channel.
     * @param blue Receives the expanded blue channel.
     */
    void DecodeRgb565(std::uint16_t packed_color, std::uint8_t& red, std::uint8_t& green, std::uint8_t& blue)
    {
        const std::uint8_t red_5 = static_cast<std::uint8_t>((packed_color >> 11u) & 0x1Fu);
        const std::uint8_t green_6 = static_cast<std::uint8_t>((packed_color >> 5u) & 0x3Fu);
        const std::uint8_t blue_5 = static_cast<std::uint8_t>(packed_color & 0x1Fu);
        red = static_cast<std::uint8_t>((red_5 << 3u) | (red_5 >> 2u));
        green = static_cast<std::uint8_t>((green_6 << 2u) | (green_6 >> 4u));
        blue = static_cast<std::uint8_t>((blue_5 << 3u) | (blue_5 >> 2u));
    }

    /**
     * @brief Decodes one DXT5 replacement payload into a lockable `A8R8G8B8` image buffer.
     * @param asset Replacement asset whose compressed bytes should be expanded.
     * @param decoded_bytes Receives the decoded `A8R8G8B8` pixels on success.
     * @param failure_result Receives the HRESULT-style failure reason when decoding fails.
     * @return True when the payload is a valid DXT5 image and the decoded buffer is produced successfully.
     */
    bool TryDecodeDxt5ReplacementToA8R8G8B8(
        const helen::TextureReplacementAsset& asset,
        std::vector<std::uint8_t>& decoded_bytes,
        HRESULT& failure_result)
    {
        failure_result = S_OK;
        decoded_bytes.clear();

        if (asset.Format != D3DFMT_DXT5 || asset.Width == 0u || asset.Height == 0u)
        {
            failure_result = E_FAIL;
            return false;
        }

        const std::size_t blocks_wide =
            static_cast<std::size_t>(asset.Width / 4u) + static_cast<std::size_t>((asset.Width % 4u) != 0u);
        const std::size_t blocks_high =
            static_cast<std::size_t>(asset.Height / 4u) + static_cast<std::size_t>((asset.Height % 4u) != 0u);
        if (blocks_wide > (std::numeric_limits<std::size_t>::max)() / 16u)
        {
            failure_result = E_FAIL;
            return false;
        }

        const std::size_t compressed_row_bytes = blocks_wide * 16u;
        if (compressed_row_bytes == 0u ||
            blocks_high > (std::numeric_limits<std::size_t>::max)() / compressed_row_bytes)
        {
            failure_result = E_FAIL;
            return false;
        }

        const std::size_t expected_size = blocks_high * compressed_row_bytes;
        if (asset.Level0Bytes.size() != expected_size)
        {
            failure_result = E_FAIL;
            return false;
        }

        if (asset.Width > (std::numeric_limits<std::size_t>::max)() / 4u)
        {
            failure_result = E_FAIL;
            return false;
        }

        const std::size_t decoded_row_bytes = static_cast<std::size_t>(asset.Width) * 4u;
        if (static_cast<std::size_t>(asset.Height) >
            (std::numeric_limits<std::size_t>::max)() / decoded_row_bytes)
        {
            failure_result = E_FAIL;
            return false;
        }

        const std::size_t decoded_size = static_cast<std::size_t>(asset.Height) * decoded_row_bytes;
        decoded_bytes.resize(decoded_size, 0u);

        const std::uint8_t* const source_bytes = asset.Level0Bytes.data();
        for (std::uint32_t block_y = 0; block_y < blocks_high; ++block_y)
        {
            for (std::uint32_t block_x = 0; block_x < blocks_wide; ++block_x)
            {
                const std::size_t block_index = static_cast<std::size_t>(block_y) * static_cast<std::size_t>(blocks_wide) + static_cast<std::size_t>(block_x);
                const std::uint8_t* const block = source_bytes + (block_index * 16u);

                const std::uint8_t alpha_0 = block[0];
                const std::uint8_t alpha_1 = block[1];

                std::uint8_t alpha_palette[8]{};
                alpha_palette[0] = alpha_0;
                alpha_palette[1] = alpha_1;
                if (alpha_0 > alpha_1)
                {
                    alpha_palette[2] = static_cast<std::uint8_t>((6u * alpha_0 + 1u * alpha_1) / 7u);
                    alpha_palette[3] = static_cast<std::uint8_t>((5u * alpha_0 + 2u * alpha_1) / 7u);
                    alpha_palette[4] = static_cast<std::uint8_t>((4u * alpha_0 + 3u * alpha_1) / 7u);
                    alpha_palette[5] = static_cast<std::uint8_t>((3u * alpha_0 + 4u * alpha_1) / 7u);
                    alpha_palette[6] = static_cast<std::uint8_t>((2u * alpha_0 + 5u * alpha_1) / 7u);
                    alpha_palette[7] = static_cast<std::uint8_t>((1u * alpha_0 + 6u * alpha_1) / 7u);
                }
                else
                {
                    alpha_palette[2] = static_cast<std::uint8_t>((4u * alpha_0 + 1u * alpha_1) / 5u);
                    alpha_palette[3] = static_cast<std::uint8_t>((3u * alpha_0 + 2u * alpha_1) / 5u);
                    alpha_palette[4] = static_cast<std::uint8_t>((2u * alpha_0 + 3u * alpha_1) / 5u);
                    alpha_palette[5] = static_cast<std::uint8_t>((1u * alpha_0 + 4u * alpha_1) / 5u);
                    alpha_palette[6] = 0u;
                    alpha_palette[7] = 255u;
                }

                std::uint64_t alpha_indices = 0u;
                for (std::size_t index = 0; index < 6u; ++index)
                {
                    alpha_indices |= static_cast<std::uint64_t>(block[2u + index]) << (8u * index);
                }

                const std::uint16_t color_0 = static_cast<std::uint16_t>(block[8]) | (static_cast<std::uint16_t>(block[9]) << 8u);
                const std::uint16_t color_1 = static_cast<std::uint16_t>(block[10]) | (static_cast<std::uint16_t>(block[11]) << 8u);

                std::uint8_t color_palette_red[4]{};
                std::uint8_t color_palette_green[4]{};
                std::uint8_t color_palette_blue[4]{};
                DecodeRgb565(color_0, color_palette_red[0], color_palette_green[0], color_palette_blue[0]);
                DecodeRgb565(color_1, color_palette_red[1], color_palette_green[1], color_palette_blue[1]);

                if (color_0 > color_1)
                {
                    color_palette_red[2] = static_cast<std::uint8_t>((2u * color_palette_red[0] + 1u * color_palette_red[1]) / 3u);
                    color_palette_green[2] = static_cast<std::uint8_t>((2u * color_palette_green[0] + 1u * color_palette_green[1]) / 3u);
                    color_palette_blue[2] = static_cast<std::uint8_t>((2u * color_palette_blue[0] + 1u * color_palette_blue[1]) / 3u);

                    color_palette_red[3] = static_cast<std::uint8_t>((1u * color_palette_red[0] + 2u * color_palette_red[1]) / 3u);
                    color_palette_green[3] = static_cast<std::uint8_t>((1u * color_palette_green[0] + 2u * color_palette_green[1]) / 3u);
                    color_palette_blue[3] = static_cast<std::uint8_t>((1u * color_palette_blue[0] + 2u * color_palette_blue[1]) / 3u);
                }
                else
                {
                    color_palette_red[2] = static_cast<std::uint8_t>((color_palette_red[0] + color_palette_red[1]) / 2u);
                    color_palette_green[2] = static_cast<std::uint8_t>((color_palette_green[0] + color_palette_green[1]) / 2u);
                    color_palette_blue[2] = static_cast<std::uint8_t>((color_palette_blue[0] + color_palette_blue[1]) / 2u);

                    color_palette_red[3] = 0u;
                    color_palette_green[3] = 0u;
                    color_palette_blue[3] = 0u;
                }

                const std::uint32_t color_indices =
                    static_cast<std::uint32_t>(block[12]) |
                    (static_cast<std::uint32_t>(block[13]) << 8u) |
                    (static_cast<std::uint32_t>(block[14]) << 16u) |
                    (static_cast<std::uint32_t>(block[15]) << 24u);

                for (std::uint32_t pixel_index = 0; pixel_index < 16u; ++pixel_index)
                {
                    const std::uint32_t local_x = pixel_index % 4u;
                    const std::uint32_t local_y = pixel_index / 4u;
                    const std::uint32_t absolute_x = block_x * 4u + local_x;
                    const std::uint32_t absolute_y = block_y * 4u + local_y;
                    if (absolute_x >= asset.Width || absolute_y >= asset.Height)
                    {
                        continue;
                    }

                    const std::uint32_t color_index = (color_indices >> (pixel_index * 2u)) & 0x3u;
                    const std::uint32_t alpha_index = static_cast<std::uint32_t>((alpha_indices >> (pixel_index * 3u)) & 0x7u);
                    const std::uint32_t pixel =
                        (static_cast<std::uint32_t>(alpha_palette[alpha_index]) << 24u) |
                        (static_cast<std::uint32_t>(color_palette_red[color_index]) << 16u) |
                        (static_cast<std::uint32_t>(color_palette_green[color_index]) << 8u) |
                        static_cast<std::uint32_t>(color_palette_blue[color_index]);

                    const std::size_t destination_offset =
                        (static_cast<std::size_t>(absolute_y) * static_cast<std::size_t>(asset.Width) + static_cast<std::size_t>(absolute_x)) * 4u;
                    std::memcpy(decoded_bytes.data() + destination_offset, &pixel, sizeof(pixel));
                }
            }
        }

        return true;
    }

    /**
     * @brief Declares the helper that reads a shadow-patched vtable snapshot for one COM instance.
     * @param instance Live COM object whose current vtable may already be patched in place.
     * @param slot_index Zero-based vtable slot index.
     * @return Original raw function pointer stored in the requested slot or `nullptr` when the snapshot is unavailable.
     */
    void* GetOriginalVtableSlot(void* instance, std::size_t slot_index);

    /**
     * @brief Declares the helper that unwraps one proxy pointer back to its real COM interface.
     * @tparam TInterface COM interface pointer type used by the proxy registry.
     * @tparam TContext Proxy context type stored in the registry.
     * @param contexts_by_proxy Registry keyed by proxy pointer.
     * @param proxy_pointer Proxy pointer that may already refer to the wrapped interface.
     * @return Real interface pointer when the proxy is known; otherwise the original pointer.
     */
    template <typename TInterface, typename TContext>
    TInterface* UnwrapProxyPointer(
        std::unordered_map<TInterface*, std::unique_ptr<TContext>>& contexts_by_proxy,
        TInterface* proxy_pointer);

    /**
     * @brief Creates one lockable `A8R8G8B8` texture from a validated replacement payload.
     * @param device Real `IDirect3DDevice9` instance that owns the texture replacement.
     * @param asset Replacement DDS payload whose A8R8G8B8 pixels are copied directly or decoded from DXT5.
     * @param replacement_texture Receives the created replacement texture on success.
     * @param failure_result Receives the HRESULT-style failure reason when creation or upload fails.
     * @return True when the replacement texture object is created and populated successfully.
     */
    bool TryCreateA8R8G8B8ReplacementTexture(
        IDirect3DDevice9& device,
        const helen::TextureReplacementAsset& asset,
        IDirect3DTexture9*& replacement_texture,
        HRESULT& failure_result)
    {
        failure_result = S_OK;
        replacement_texture = nullptr;

        std::vector<std::uint8_t> decoded_bytes;
        if (asset.Format == D3DFMT_A8R8G8B8)
        {
            if (asset.Width == 0u ||
                asset.Height == 0u ||
                asset.Width > (std::numeric_limits<std::size_t>::max)() / 4u ||
                static_cast<std::size_t>(asset.Height) >
                    (std::numeric_limits<std::size_t>::max)() / (static_cast<std::size_t>(asset.Width) * 4u))
            {
                failure_result = E_FAIL;
                return false;
            }

            const std::size_t expected_size =
                static_cast<std::size_t>(asset.Width) * static_cast<std::size_t>(asset.Height) * 4u;
            if (asset.Level0Bytes.size() != expected_size)
            {
                failure_result = E_FAIL;
                return false;
            }

            decoded_bytes = asset.Level0Bytes;
        }
        else if (!TryDecodeDxt5ReplacementToA8R8G8B8(asset, decoded_bytes, failure_result))
        {
            return false;
        }

        Logf(
            L"[d3d9] decoded replacement texture bytes=%zu size=%ux%u source=0x%08lX",
            decoded_bytes.size(),
            static_cast<unsigned int>(asset.Width),
            static_cast<unsigned int>(asset.Height),
            static_cast<unsigned long>(asset.Format));

        const auto original_create_texture = reinterpret_cast<Direct3d9CreateTextureFunction>(
            GetOriginalVtableSlot(&device, Direct3d9CreateTextureVtableIndex));
        if (original_create_texture == nullptr)
        {
            failure_result = D3DERR_INVALIDCALL;
            return false;
        }

        IDirect3DTexture9* texture = nullptr;
        Logf(L"[d3d9] creating decoded replacement texture object");
        const HRESULT create_result = original_create_texture(
            &device,
            asset.Width,
            asset.Height,
            1u,
            D3DUSAGE_DYNAMIC,
            D3DFMT_A8R8G8B8,
            D3DPOOL_DEFAULT,
            &texture,
            nullptr);
        Logf(
            L"[d3d9] create decoded replacement texture returned hr=0x%08lX texture=0x%p",
            static_cast<unsigned long>(create_result),
            texture);
        if (FAILED(create_result) || texture == nullptr)
        {
            failure_result = create_result;
            return false;
        }

        const auto original_release = reinterpret_cast<Direct3d9TextureReleaseFunction>(
            GetOriginalVtableSlot(texture, ReleaseVtableIndex));
        if (original_release == nullptr)
        {
            texture->Release();
            failure_result = D3DERR_INVALIDCALL;
            return false;
        }

        const auto original_lock_rect = reinterpret_cast<Direct3d9TextureLockRectFunction>(
            GetOriginalVtableSlot(texture, Direct3d9TextureLockRectVtableIndex));
        if (original_lock_rect == nullptr)
        {
            original_release(texture);
            failure_result = D3DERR_INVALIDCALL;
            return false;
        }

        D3DLOCKED_RECT locked_rect{};
        Logf(L"[d3d9] locking decoded replacement texture through original vtable");
        const HRESULT lock_result = original_lock_rect(texture, 0u, &locked_rect, nullptr, D3DLOCK_DISCARD);
        Logf(
            L"[d3d9] lock decoded replacement texture returned hr=0x%08lX pitch=%ld bits=0x%p",
            static_cast<unsigned long>(lock_result),
            static_cast<long>(locked_rect.Pitch),
            locked_rect.pBits);
        if (FAILED(lock_result) || locked_rect.pBits == nullptr || locked_rect.Pitch <= 0)
        {
            original_release(texture);
            failure_result = lock_result;
            return false;
        }

        const std::size_t row_bytes = static_cast<std::size_t>(asset.Width) * 4u;
        const std::uint8_t* const source_bytes = decoded_bytes.data();
        std::uint8_t* const destination_bytes = static_cast<std::uint8_t*>(locked_rect.pBits);
        Logf(L"[d3d9] copying decoded replacement texture rows row_bytes=%zu", row_bytes);
        for (std::uint32_t y = 0; y < asset.Height; ++y)
        {
            std::memcpy(
                destination_bytes + (static_cast<std::size_t>(y) * static_cast<std::size_t>(locked_rect.Pitch)),
                source_bytes + (static_cast<std::size_t>(y) * row_bytes),
                row_bytes);
        }

        const auto original_unlock_rect = reinterpret_cast<Direct3d9TextureUnlockRectFunction>(
            GetOriginalVtableSlot(texture, Direct3d9TextureUnlockRectVtableIndex));
        if (original_unlock_rect == nullptr)
        {
            original_release(texture);
            failure_result = D3DERR_INVALIDCALL;
            return false;
        }

        Logf(L"[d3d9] unlocking decoded replacement texture through original vtable");
        const HRESULT unlock_result = original_unlock_rect(texture, 0u);
        Logf(
            L"[d3d9] unlock decoded replacement texture returned hr=0x%08lX",
            static_cast<unsigned long>(unlock_result));
        if (FAILED(unlock_result))
        {
            original_release(texture);
            failure_result = unlock_result;
            return false;
        }

        replacement_texture = texture;
        return true;
    }

    /**
     * @brief Loads one complete binary file into memory for replacement validation.
     * @param file_path Path of the replacement asset that should be read.
     * @param bytes Receives the complete file contents on success.
     * @param failure_result Receives the HRESULT-style failure reason when the file cannot be loaded.
     * @return True when the file is readable and copied into memory; otherwise false.
     */
    bool TryReadAllBytes(const std::filesystem::path& file_path, std::vector<std::uint8_t>& bytes, HRESULT& failure_result)
    {
        failure_result = S_OK;
        bytes.clear();

        std::error_code error_code;
        const std::uintmax_t file_size = std::filesystem::file_size(file_path, error_code);
        if (error_code)
        {
            failure_result = HRESULT_FROM_WIN32(error_code.value());
            return false;
        }

        if (file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()) ||
            file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
        {
            failure_result = E_FAIL;
            return false;
        }

        std::ifstream stream(file_path, std::ios::binary);
        if (!stream)
        {
            failure_result = HRESULT_FROM_WIN32(GetLastError());
            return false;
        }

        bytes.resize(static_cast<std::size_t>(file_size), 0);
        if (!bytes.empty())
        {
            stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            if (stream.gcount() != static_cast<std::streamsize>(bytes.size()) || stream.bad())
            {
                failure_result = E_FAIL;
                bytes.clear();
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Loads one DDS-compressed replacement payload and validates it against the source texture description.
     * @param file_path Build-relative replacement asset that should be used for the live overwrite.
     * @param description Source texture description that the replacement must match.
     * @param replacement_bytes Receives the raw compressed level-0 bytes on success.
     * @param failure_result Receives the HRESULT-style failure reason when the asset is not usable.
     * @return True when the asset is a valid DDS file for the requested texture; otherwise false.
     */
    bool TryLoadReplacementDdsBytes(
        const std::filesystem::path& file_path,
        const D3DSURFACE_DESC& description,
        std::vector<std::uint8_t>& replacement_bytes,
        HRESULT& failure_result)
    {
        failure_result = S_OK;
        replacement_bytes.clear();

        std::vector<std::uint8_t> file_bytes;
        if (!TryReadAllBytes(file_path, file_bytes, failure_result))
        {
            return false;
        }

        if (file_bytes.size() < 4u + sizeof(DdsHeader) || std::memcmp(file_bytes.data(), "DDS ", 4) != 0)
        {
            failure_result = E_FAIL;
            return false;
        }

        DdsHeader header{};
        std::memcpy(&header, file_bytes.data() + 4u, sizeof(header));
        if (header.Size != 124u ||
            header.PixelFormat.Size != 32u ||
            header.PixelFormat.Flags != DdpfFourCc ||
            header.Width != description.Width ||
            header.Height != description.Height)
        {
            failure_result = E_FAIL;
            return false;
        }

        const std::uint32_t expected_four_cc = GetReplacementDdsFourCc(description.Format);
        if (expected_four_cc == 0 || header.PixelFormat.FourCc != expected_four_cc)
        {
            failure_result = E_FAIL;
            return false;
        }

        std::uint32_t row_count = 0;
        std::uint32_t bytes_per_row = 0;
        if (!TryGetTextureRowLayout(description, row_count, bytes_per_row))
        {
            failure_result = E_NOTIMPL;
            return false;
        }

        const std::size_t expected_payload_size = static_cast<std::size_t>(row_count) * static_cast<std::size_t>(bytes_per_row);
        const std::size_t payload_offset = 4u + sizeof(DdsHeader);
        const std::size_t payload_size = file_bytes.size() - payload_offset;
        if (payload_size != expected_payload_size)
        {
            failure_result = E_FAIL;
            return false;
        }

        replacement_bytes.assign(file_bytes.begin() + static_cast<std::ptrdiff_t>(payload_offset), file_bytes.end());
        return true;
    }

    /**
     * @brief Overwrites one writable level-0 texture buffer with replacement bytes.
     * @param description Source texture description used to validate the replacement payload.
     * @param writable_bytes Start of the live writable buffer.
     * @param writable_pitch Pitch in bytes of the writable buffer.
     * @param replacement_bytes Raw replacement bytes loaded from the DDS asset.
     * @param failure_result Receives the HRESULT-style failure reason when the overwrite cannot be performed.
     * @return True when the writable buffer receives the replacement payload; otherwise false.
     */
    bool TryOverwriteTextureBytes(
        const D3DSURFACE_DESC& description,
        std::uint8_t* writable_bytes,
        LONG writable_pitch,
        const std::vector<std::uint8_t>& replacement_bytes,
        HRESULT& failure_result)
    {
        failure_result = S_OK;
        if (writable_bytes == nullptr || writable_pitch <= 0)
        {
            failure_result = E_FAIL;
            return false;
        }

        std::uint32_t row_count = 0;
        std::uint32_t bytes_per_row = 0;
        if (!TryGetTextureRowLayout(description, row_count, bytes_per_row))
        {
            failure_result = E_NOTIMPL;
            return false;
        }

        const std::size_t expected_payload_size = static_cast<std::size_t>(row_count) * static_cast<std::size_t>(bytes_per_row);
        if (replacement_bytes.size() != expected_payload_size)
        {
            failure_result = E_FAIL;
            return false;
        }

        for (std::uint32_t row_index = 0; row_index < row_count; ++row_index)
        {
            std::uint8_t* const destination_row = writable_bytes + (static_cast<std::size_t>(row_index) * static_cast<std::size_t>(writable_pitch));
            const std::uint8_t* const source_row =
                replacement_bytes.data() + (static_cast<std::size_t>(row_index) * static_cast<std::size_t>(bytes_per_row));
            std::memcpy(destination_row, source_row, bytes_per_row);
        }

        return true;
    }

    /**
     * @brief Converts one lockable `D3D9` texture image into a bitmap or DDS payload.
     * @param texture Texture whose level-0 image should be dumped.
     * @param description Level-0 surface description for `texture`.
     * @param output_path Destination image file path.
     * @param failure_result Receives the reason dumping failed when the operation does not succeed.
     * @return True when the image is written successfully; otherwise false.
     */
    bool TryCopyTextureBytes(
        const D3DSURFACE_DESC& description,
        const std::uint8_t* source_bytes,
        LONG source_pitch,
        std::vector<std::uint8_t>& copied_bytes,
        HRESULT& failure_result)
    {
        failure_result = S_OK;
        copied_bytes.clear();

        if (source_bytes == nullptr || source_pitch <= 0)
        {
            failure_result = E_FAIL;
            return false;
        }

        if (description.Format != D3DFMT_A8R8G8B8 &&
            description.Format != D3DFMT_A8 &&
            description.Format != D3DFMT_L8 &&
            description.Format != D3DFMT_DXT1 &&
            description.Format != D3DFMT_DXT3 &&
            description.Format != D3DFMT_DXT5)
        {
            failure_result = E_NOTIMPL;
            return false;
        }

        if (description.Format == D3DFMT_A8R8G8B8)
        {
            copied_bytes.resize(static_cast<std::size_t>(description.Width) * static_cast<std::size_t>(description.Height) * 4, 0);
            for (std::uint32_t row_index = 0; row_index < description.Height; ++row_index)
            {
                const std::uint8_t* const source_row = source_bytes + (static_cast<std::size_t>(row_index) * static_cast<std::size_t>(source_pitch));
                std::uint8_t* const destination_row =
                    copied_bytes.data() + (static_cast<std::size_t>(row_index) * static_cast<std::size_t>(description.Width) * 4);
                std::memcpy(destination_row, source_row, static_cast<std::size_t>(description.Width) * 4);
            }

            return true;
        }

        std::uint32_t row_count = 0;
        std::uint32_t bytes_per_row = 0;
        if (!TryGetTextureRowLayout(description, row_count, bytes_per_row))
        {
            failure_result = E_NOTIMPL;
            return false;
        }

        copied_bytes.resize(static_cast<std::size_t>(row_count) * static_cast<std::size_t>(bytes_per_row), 0);
        for (std::uint32_t row_index = 0; row_index < row_count; ++row_index)
        {
            const std::uint8_t* const source_row = source_bytes + (static_cast<std::size_t>(row_index) * static_cast<std::size_t>(source_pitch));
            std::uint8_t* const destination_row =
                copied_bytes.data() + (static_cast<std::size_t>(row_index) * static_cast<std::size_t>(bytes_per_row));
            std::memcpy(destination_row, source_row, bytes_per_row);
        }

        return true;
    }

    /**
     * @brief Converts one writable `D3D9` texture upload into a bitmap or DDS payload.
     * @param description Level-0 surface description for the upload.
     * @param source_bytes Start address of the writable buffer while the lock is still valid.
     * @param source_pitch Pitch in bytes of the writable buffer.
     * @param output_path Destination image file path.
     * @param failure_result Receives the reason dumping failed when the operation does not succeed.
     * @return True when the image is written successfully; otherwise false.
     */
    bool TryDumpTextureImageFromBytes(
        const D3DSURFACE_DESC& description,
        const std::uint8_t* source_bytes,
        LONG source_pitch,
        const std::filesystem::path& output_path,
        HRESULT& failure_result)
    {
        failure_result = S_OK;
        if (description.Format != D3DFMT_A8R8G8B8 &&
            description.Format != D3DFMT_A8 &&
            description.Format != D3DFMT_L8 &&
            description.Format != D3DFMT_DXT1 &&
            description.Format != D3DFMT_DXT3 &&
            description.Format != D3DFMT_DXT5)
        {
            failure_result = E_NOTIMPL;
            return false;
        }

        std::vector<std::uint8_t> copied_bytes;
        if (!TryCopyTextureBytes(description, source_bytes, source_pitch, copied_bytes, failure_result))
        {
            return false;
        }

        bool succeeded = true;
        if (description.Format == D3DFMT_A8R8G8B8)
        {
            std::vector<std::uint8_t> bitmap_bytes;
            if (!helen::TextureDumpSerializer::TryBuildBitmapBytes(description.Width, description.Height, copied_bytes, bitmap_bytes))
            {
                succeeded = false;
                failure_result = E_FAIL;
            }
            else if (!TryWriteAllBytes(output_path, bitmap_bytes))
            {
                succeeded = false;
                failure_result = HRESULT_FROM_WIN32(GetLastError());
            }
        }
        else
        {
            std::vector<std::uint8_t> dds_bytes;
            if (!helen::TextureDumpSerializer::TryBuildDdsBytes(
                    description.Format,
                    description.Width,
                    description.Height,
                    copied_bytes,
                    dds_bytes))
            {
                succeeded = false;
                failure_result = E_FAIL;
            }
            else if (!TryWriteAllBytes(output_path, dds_bytes))
            {
                succeeded = false;
                failure_result = HRESULT_FROM_WIN32(GetLastError());
            }
        }

        return succeeded;
    }

    /**
     * @brief Returns one filesystem-safe token derived from diagnostic metadata.
     * @param value Source token that may contain path separators or punctuation.
     * @return ASCII token safe to use in dump file names.
     */
    std::string SanitizeFilenameToken(std::string_view value)
    {
        std::string sanitized;
        sanitized.reserve(value.size());
        for (const unsigned char character : value)
        {
            if (std::isalnum(character) != 0 || character == '-' || character == '_')
            {
                sanitized.push_back(static_cast<char>(character));
            }
            else
            {
                sanitized.push_back('_');
            }
        }

        if (sanitized.empty())
        {
            sanitized = "texture";
        }

        return sanitized;
    }

    /**
     * @brief Computes the SHA-256 digest of one lockable level-0 `D3D9` texture image.
     * @param texture Texture whose level-0 image should be fingerprinted.
     * @param description Level-0 surface description for `texture`.
     * @param digest Receives the lowercase SHA-256 digest when hashing succeeds.
     * @param failure_result Receives the `HRESULT` that prevented hashing when the call fails after a `D3D9` operation.
     * @return True when the texture locks and hashes successfully; otherwise false.
     */
    bool TryComputeTextureSha256FromBytes(
        const D3DSURFACE_DESC& description,
        const std::uint8_t* source_bytes,
        LONG source_pitch,
        std::string& digest,
        HRESULT& failure_result)
    {
        failure_result = S_OK;

        std::uint32_t row_count = 0;
        std::uint32_t bytes_per_row = 0;
        if (!TryGetTextureRowLayout(description, row_count, bytes_per_row))
        {
            failure_result = E_NOTIMPL;
            return false;
        }

        if (source_bytes == nullptr || source_pitch <= 0)
        {
            failure_result = E_FAIL;
            return false;
        }

        HCRYPTPROV provider = 0;
        HCRYPTHASH hash = 0;
        bool succeeded = TryOpenHashProvider(provider) && TryOpenSha256Hash(provider, hash);
        if (!succeeded)
        {
            failure_result = HRESULT_FROM_WIN32(GetLastError());
        }

        if (succeeded)
        {
            if (static_cast<std::uint32_t>(source_pitch) < bytes_per_row)
            {
                succeeded = false;
                failure_result = E_FAIL;
            }
            else
            {
                for (std::uint32_t row_index = 0; row_index < row_count; ++row_index)
                {
                    const BYTE* const current_row = reinterpret_cast<const BYTE*>(source_bytes + (static_cast<std::size_t>(row_index) * static_cast<std::size_t>(source_pitch)));
                    if (CryptHashData(hash, current_row, bytes_per_row, 0) != TRUE)
                    {
                        succeeded = false;
                        failure_result = HRESULT_FROM_WIN32(GetLastError());
                        break;
                    }
                }
            }
        }

        if (succeeded)
        {
            succeeded = TryFinalizeHash(hash, digest);
            if (!succeeded)
            {
                failure_result = HRESULT_FROM_WIN32(GetLastError());
            }
        }

        if (hash != 0)
        {
            CryptDestroyHash(hash);
        }

        if (provider != 0)
        {
            CryptReleaseContext(provider, 0);
        }

        return succeeded;
    }

    /**
     * @brief Returns whether two ASCII strings match case-insensitively.
     * @param left First string to compare.
     * @param right Second string to compare.
     * @return True when both strings match ignoring ASCII case; otherwise false.
     */
    bool EqualsAsciiIgnoreCase(std::string_view left, std::string_view right)
    {
        if (left.size() != right.size())
        {
            return false;
        }

        for (std::size_t index = 0; index < left.size(); ++index)
        {
            const unsigned char left_character = static_cast<unsigned char>(left[index]);
            const unsigned char right_character = static_cast<unsigned char>(right[index]);
            if (std::tolower(left_character) != std::tolower(right_character))
            {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Converts one `D3DFORMAT` value into the token strings used by replacement metadata.
     * @param format Runtime texture format.
     * @return Stable metadata token when the format is recognized; otherwise `UNKNOWN`.
     */
    std::string_view GetD3d9FormatToken(D3DFORMAT format)
    {
        switch (format)
        {
        case D3DFMT_A8R8G8B8:
            return "A8R8G8B8";
        case D3DFMT_A8:
            return "A8";
        case D3DFMT_L8:
            return "L8";
        case D3DFMT_DXT1:
            return "DXT1";
        case D3DFMT_DXT3:
            return "DXT3";
        case D3DFMT_DXT5:
            return "DXT5";
        default:
            return "UNKNOWN";
        }
    }

    /**
     * @brief Converts one `D3DPOOL` value into a stable diagnostic token.
     * @param pool Runtime texture pool.
     * @return Stable pool token when the value is recognized; otherwise `UNKNOWN`.
     */
    std::string_view GetD3d9PoolToken(D3DPOOL pool)
    {
        switch (pool)
        {
        case D3DPOOL_DEFAULT:
            return "DEFAULT";
        case D3DPOOL_MANAGED:
            return "MANAGED";
        case D3DPOOL_SYSTEMMEM:
            return "SYSTEMMEM";
        case D3DPOOL_SCRATCH:
            return "SCRATCH";
        default:
            return "UNKNOWN";
        }
    }

    /**
     * @brief Returns the shared implementation pointer stored at one COM interface vtable slot.
     * @param instance Live COM interface instance whose vtable should be inspected.
     * @param slot_index Zero-based vtable slot index.
     * @return Raw function pointer stored in the requested slot or `nullptr` when the instance is invalid.
     */
    void** GetComVtable(void* instance)
    {
        if (instance == nullptr)
        {
            return nullptr;
        }

        void** const* const vtable = reinterpret_cast<void***>(instance);
        if (vtable == nullptr || *vtable == nullptr)
        {
            return nullptr;
        }

        return *vtable;
    }

    /**
     * @brief Repoints one live COM instance at a replacement vtable.
     * @param instance Live COM interface instance whose vtable pointer should be rewritten.
     * @param vtable Replacement vtable pointer that should serve future virtual dispatch.
     * @return True when the instance is valid and the vtable pointer is rewritten; otherwise false.
     */
    bool TrySetComVtable(void* instance, void** vtable)
    {
        if (instance == nullptr || vtable == nullptr)
        {
            return false;
        }

        auto*** const object_vtable = reinterpret_cast<void***>(instance);
        if (object_vtable == nullptr)
        {
            return false;
        }

        *object_vtable = vtable;
        return true;
    }

    /**
     * @brief Returns the function pointer stored at one COM interface vtable slot.
     * @param instance Live COM interface instance whose vtable should be inspected.
     * @param slot_index Zero-based vtable slot index.
     * @return Raw function pointer stored in the requested slot or `nullptr` when the instance is invalid.
     */
    void* GetComVtableSlot(void* instance, std::size_t slot_index)
    {
        void** const vtable = GetComVtable(instance);
        if (vtable == nullptr)
        {
            return nullptr;
        }

        return vtable[slot_index];
    }

    /**
     * @brief Returns the function pointer stored at one raw vtable slot.
     * @param vtable Raw vtable pointer captured from the live COM object.
     * @param slot_index Zero-based vtable slot index.
     * @return Raw function pointer stored in the requested slot or `nullptr` when the vtable is invalid.
     */
    void* GetVtableSlot(void** vtable, std::size_t slot_index)
    {
        if (vtable == nullptr)
        {
            return nullptr;
        }

        return vtable[slot_index];
    }

    /**
     * @brief Declares the helper that reads a shadow-patched vtable snapshot.
     * @param instance Live COM object whose current vtable may already be patched in place.
     * @param slot_index Zero-based vtable slot index.
     * @return Original raw function pointer stored in the requested slot or `nullptr` when the snapshot is unavailable.
     */
    void* GetOriginalVtableSlot(void* instance, std::size_t slot_index);

    /**
     * @brief Returns the original function pointer stored at one shadow-patched COM vtable slot.
     * @param instance Live COM object whose current vtable may already be patched in place.
     * @param slot_index Zero-based vtable slot index.
     * @return Original raw function pointer stored in the requested slot or `nullptr` when the snapshot is unavailable.
     */
    void* GetOriginalVtableSlot(void* instance, std::size_t slot_index) {
        return g_original_dispatch.Resolve(instance, slot_index);
    }

    /**
     * @brief Reads the level-0 description for one tracked `IDirect3DTexture9` instance.
     * @param texture Texture whose level-0 description should be queried.
     * @param description Receives the queried description on success.
     * @return True when the texture reports a valid level-0 description; otherwise false.
     */
    bool TryGetTextureDescription(IDirect3DTexture9& texture, D3DSURFACE_DESC& description)
    {
        return SUCCEEDED(texture.GetLevelDesc(0, &description));
    }

    /**
     * @brief Returns the live device hook context for one tracked `IDirect3DDevice9` instance.
     * @param device Device whose hook state should be queried.
     * @return Pointer to the tracked context when the device is installed; otherwise nullptr.
     */
    Direct3d9DeviceHookContext* FindDeviceHookContext(IDirect3DDevice9* device)
    {
        if (device == nullptr)
        {
            return nullptr;
        }

        const auto iterator = g_direct3d9_device_hook_contexts.find(device);
        if (iterator == g_direct3d9_device_hook_contexts.end())
        {
            return nullptr;
        }

        return iterator->second.get();
    }

    /**
     * @brief Returns the live texture hook context for one tracked `IDirect3DTexture9` instance.
     * @param texture Texture whose hook state should be queried.
     * @return Pointer to the tracked context when the texture is installed; otherwise nullptr.
     */
    Direct3d9TextureHookContext* FindTextureHookContext(IDirect3DTexture9* texture)
    {
        if (texture == nullptr)
        {
            return nullptr;
        }

        const auto iterator = g_direct3d9_texture_hook_contexts.find(texture);
        if (iterator == g_direct3d9_texture_hook_contexts.end())
        {
            return nullptr;
        }

        return iterator->second.get();
    }

    /**
     * @brief Returns the live surface hook context for one tracked `IDirect3DSurface9` instance.
     * @param surface Surface whose hook state should be queried.
     * @return Pointer to the tracked context when the surface is installed; otherwise nullptr.
     */
    Direct3d9SurfaceHookContext* FindSurfaceHookContext(IDirect3DSurface9* surface)
    {
        if (surface == nullptr)
        {
            return nullptr;
        }

        const auto iterator = g_direct3d9_surface_hook_contexts.find(surface);
        if (iterator == g_direct3d9_surface_hook_contexts.end())
        {
            return nullptr;
        }

        return iterator->second.get();
    }

    /**
     * @brief Returns the live wrapped `IDirect3D9` proxy for one tracked proxy pointer.
     * @param direct3d Proxy pointer whose hook state should be queried.
     * @return Pointer to the wrapped proxy context when the object is known; otherwise nullptr.
     */
    Direct3d9HookContext* FindDirect3d9HookContext(IDirect3D9* direct3d)
    {
        if (direct3d == nullptr)
        {
            return nullptr;
        }

        const auto iterator = g_direct3d9_hook_contexts.find(direct3d);
        if (iterator == g_direct3d9_hook_contexts.end())
        {
            return nullptr;
        }

        return iterator->second.get();
    }

    /**
     * @brief Returns the live tracked-texture record for one tracked `IDirect3DTexture9` instance.
     * @param texture Texture whose lifecycle record should be queried.
     * @return Pointer to the tracked record when the texture is known; otherwise nullptr.
     */
    TrackedTextureRecord* FindTrackedTextureRecord(IDirect3DTexture9* texture)
    {
        if (texture == nullptr)
        {
            return nullptr;
        }

        const auto iterator = g_tracked_texture_records.find(texture);
        if (iterator == g_tracked_texture_records.end())
        {
            return nullptr;
        }

        return &iterator->second;
    }

    /** Copies a writable level-zero snapshot before native UnlockRect invalidates its buffer.
     * Only CPU memory/state is touched under the registry mutex. No snapshot is normal for
     * read-only, partial, unsupported-format or untracked internal surfaces.
     */
    std::optional<helen::D3d9TextureUpload> CaptureWritableUpload(IDirect3DTexture9* texture) {
        std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
        TrackedTextureRecord* const record = FindTrackedTextureRecord(texture);
        if (record == nullptr || !record->Dirty || !record->HasDescription || !record->HasWritableLockSnapshot) {
            return std::nullopt;
        }
        record->Dirty = false;
        record->HasWritableLockSnapshot = false;
        const std::uint8_t* const bytes = record->WritableLockBytes;
        const LONG pitch = record->WritableLockPitch;
        record->WritableLockBytes = nullptr;
        record->WritableLockPitch = 0;
        std::uint32_t rows = 0, row_bytes = 0;
        if (!TryGetTextureRowLayout(record->Description, rows, row_bytes)) { return std::nullopt; }
        if (bytes == nullptr || pitch <= 0 || rows == 0 || row_bytes == 0 ||
            row_bytes > static_cast<std::uint32_t>(pitch) ||
            rows > (std::numeric_limits<std::size_t>::max)() / row_bytes) {
            throw std::logic_error("Invalid writable D3D9 upload layout.");
        }
        helen::D3d9TextureUpload upload{record->Description,
            std::vector<std::uint8_t>(static_cast<std::size_t>(rows) * row_bytes),
            static_cast<LONG>(row_bytes), record->Identity};
        for (std::uint32_t row = 0; row < rows; ++row) {
            std::memcpy(upload.Bytes.data() + static_cast<std::size_t>(row) * row_bytes,
                bytes + static_cast<std::size_t>(row) * pitch, row_bytes);
        }
        return upload;
    }

    /**
     * @brief Creates one higher-resolution replacement texture object from a loaded DDS asset.
     * @param device Real `IDirect3DDevice9` instance that owns the original tracked texture.
     * @param source_description Description of the source texture used to preserve compatibility diagnostics.
     * @param asset Loaded replacement DDS payload that should be uploaded into the new texture object.
     * @param replacement_texture Receives the created replacement texture on success.
     * @param failure_result Receives the HRESULT-style failure reason when creation or upload fails.
     * @return True when the replacement texture object is created and populated successfully.
     */
    bool TryCreateReplacementTexture(
        IDirect3DDevice9* device,
        const D3DSURFACE_DESC& source_description,
        const helen::TextureReplacementAsset& asset,
        const helen::TextureReplacementDefinition& replacement_definition,
        IDirect3DTexture9*& replacement_texture,
        HRESULT& failure_result)
    {
        failure_result = S_OK;
        replacement_texture = nullptr;

        if (device == nullptr || asset.Width == 0u || asset.Height == 0u || asset.Level0Bytes.empty())
        {
            failure_result = E_FAIL;
            return false;
        }

        Logf(
            L"[d3d9] create replacement texture begin device=0x%p source=%ux%u %hs pool=%hs usage=0x%08lX asset=%ux%u %hs path=%ls",
            device,
            static_cast<unsigned int>(source_description.Width),
            static_cast<unsigned int>(source_description.Height),
            GetD3d9FormatToken(source_description.Format).data(),
            GetD3d9PoolToken(source_description.Pool).data(),
            static_cast<unsigned long>(source_description.Usage),
            static_cast<unsigned int>(asset.Width),
            static_cast<unsigned int>(asset.Height),
            GetD3d9FormatToken(asset.Format).data(),
            replacement_definition.ReplacementPath.c_str());

        IDirect3DTexture9* final_texture = nullptr;
        if (!TryCreateA8R8G8B8ReplacementTexture(*device, asset, final_texture, failure_result))
        {
            Logf(
                L"[d3d9] failed to cache declared replacement texture ptr=0x%p path=%ls hr=0x%08lX",
                device,
                replacement_definition.ReplacementPath.c_str(),
                static_cast<unsigned long>(failure_result));
            return false;
        }

        Logf(
            L"[d3d9] create replacement texture complete texture=0x%p size=%ux%u format=%hs source=%hs via=CPU-DECODE",
            final_texture,
            static_cast<unsigned int>(asset.Width),
            static_cast<unsigned int>(asset.Height),
            GetD3d9FormatToken(D3DFMT_A8R8G8B8).data(),
            GetD3d9FormatToken(asset.Format).data());

        replacement_texture = final_texture;
        return true;
    }

    /** Reads a live device's application mutation revision; caller already owns the device lifetime. */
    std::uint64_t ReadBindingRevision(IDirect3DDevice9& device) {
        std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(&device);
        if (context == nullptr) { throw std::logic_error("Binding transaction lost its device registration."); }
        return context->BindingRevision;
    }

    /** Creates a replacement outside tracking locks and publishes only a still-current operation.
     * device is owned by the caller; tracked_texture is an identity key, never dereferenced here.
     * expected_identity rejects work captured before a reset, rewrite or address reuse when supplied.
     * failure_result preserves the failing driver HRESULT; partial bindings roll back before return.
     */
    bool TryCacheReplacementTexture(
        IDirect3DDevice9& device,
        IDirect3DTexture9* tracked_texture,
        const D3DSURFACE_DESC& description,
        const helen::PackScopedTextureReplacementDefinition& replacement_definition,
        HRESULT& failure_result,
        const std::shared_ptr<helen::D3d9ReplacementTicket>& expected_identity = {}) {
        failure_result = S_OK;
        const auto ticket = std::make_shared<helen::D3d9ReplacementTicket>();
        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            TrackedTextureRecord* const record = FindTrackedTextureRecord(tracked_texture);
            Direct3d9DeviceHookContext* const context = FindDeviceHookContext(&device);
            if (record == nullptr || record->OwningDevice != &device || context == nullptr || context->ResetInProgress ||
                (expected_identity && record->Identity != expected_identity)) {
                failure_result = D3DERR_INVALIDCALL;
                return false;
            }
            if (record->ReplacementTexture) { return true; }
            if (!record->PendingReplacement.expired()) {
                failure_result = D3DERR_INVALIDCALL;
                return false;
            }
            record->PendingReplacement = ticket;
        }
        // Caller owns a valid device reference. The source is only an identity key;
        // reset work never dereferences a source potentially released by reentry.
        const std::optional<std::filesystem::path> resolved_path =
            replacement_definition.AssetResolver.Resolve(replacement_definition.Definition.ReplacementPath);
        if (!resolved_path) { failure_result = E_FAIL; return false; }
        helen::TextureReplacementAsset asset{};
        if (!helen::TextureReplacementAssetLoader::TryLoadDds(*resolved_path, asset, failure_result)) { return false; }
        IDirect3DTexture9* created = nullptr;
        if (!TryCreateReplacementTexture(&device, description, asset, replacement_definition.Definition, created, failure_result)) {
            return false;
        }
        const auto release = reinterpret_cast<Direct3d9TextureReleaseFunction>(
            GetOriginalVtableSlot(created, ReleaseVtableIndex));
        const std::shared_ptr<IDirect3DTexture9> replacement(created, release);
        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            TrackedTextureRecord* const record = FindTrackedTextureRecord(tracked_texture);
            Direct3d9DeviceHookContext* const context = FindDeviceHookContext(&device);
            if (record == nullptr || record->PendingReplacement.lock() != ticket ||
                context == nullptr || context->ResetInProgress) {
                failure_result = D3DERR_INVALIDCALL;
                return false;
            }
        }
        // Inspect actual bindings rather than remembered stages. Every returned COM
        // reference is scoped, including stages belonging to another source.
        const auto get_texture = reinterpret_cast<Direct3d9GetTextureFunction>(
            GetOriginalVtableSlot(&device, Direct3d9GetTextureVtableIndex));
        const auto set_texture = reinterpret_cast<Direct3d9SetTextureFunction>(
            GetOriginalVtableSlot(&device, Direct3d9SetTextureVtableIndex));
        helen::D3d9TextureBindingTransaction bindings(device, replacement, get_texture, set_texture, &ReadBindingRevision);
        failure_result = bindings.Apply(tracked_texture);
        if (FAILED(failure_result)) { return false; }
        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            TrackedTextureRecord* const record = FindTrackedTextureRecord(tracked_texture);
            Direct3d9DeviceHookContext* const context = FindDeviceHookContext(&device);
            if (record == nullptr || record->PendingReplacement.lock() != ticket ||
                context == nullptr || context->ResetInProgress) {
                failure_result = D3DERR_INVALIDCALL;
                return false;
            }
            record->ReplacementTexture = replacement;
            record->ReplacementNeedsRestore = false;
            record->PendingReplacement.reset();
        }
        bindings.Commit();
        Logf(L"[d3d9] cached declared replacement texture ptr=0x%p replacement=0x%p path=%ls size=%ux%u",
            tracked_texture, replacement.get(), resolved_path->c_str(), asset.Width, asset.Height);
        return true;
    }

    /**
     * @brief Returns the live wrapped proxy context for one proxy pointer.
     * @tparam TInterface COM interface pointer type used as the lookup key.
     * @tparam TContext Proxy context type stored in the owning registry.
     * @param contexts_by_proxy Registry keyed by proxy pointer.
     * @param proxy_pointer Proxy pointer whose context should be returned.
     * @return Wrapped proxy context when the proxy is known; otherwise nullptr.
     */
    template <typename TInterface, typename TContext>
    TContext* FindProxyContext(
        std::unordered_map<TInterface*, std::unique_ptr<TContext>>& contexts_by_proxy,
        TInterface* proxy_pointer)
    {
        if (proxy_pointer == nullptr)
        {
            return nullptr;
        }

        const auto iterator = contexts_by_proxy.find(proxy_pointer);
        if (iterator == contexts_by_proxy.end())
        {
            return nullptr;
        }

        return iterator->second.get();
    }

    /**
     * @brief Returns the real interface pointer behind one wrapped proxy pointer.
     * @tparam TInterface COM interface pointer type used by the proxy registry.
     * @tparam TContext Proxy context type stored in the registry.
     * @param contexts_by_proxy Registry keyed by proxy pointer.
     * @param proxy_pointer Proxy pointer that may already refer to the wrapped interface.
     * @return Real interface pointer when the proxy is known; otherwise the original pointer.
     */
    template <typename TInterface, typename TContext>
    TInterface* UnwrapProxyPointer(
        std::unordered_map<TInterface*, std::unique_ptr<TContext>>& contexts_by_proxy,
        TInterface* proxy_pointer)
    {
        const TContext* const context = FindProxyContext(contexts_by_proxy, proxy_pointer);
        if (context == nullptr || context->Real == nullptr)
        {
            return proxy_pointer;
        }

        return context->Real;
    }

    /**
     * @brief Returns the wrapped proxy pointer that corresponds to one real interface pointer.
     * @tparam TInterface COM interface pointer type used by the proxy registry.
     * @param contexts_by_real Registry keyed by real interface pointer.
     * @param real_pointer Real interface pointer returned by the runtime.
     * @return Proxy pointer when the real interface is already wrapped; otherwise the original pointer.
     */
    template <typename TInterface>
    TInterface* WrapProxyPointer(
        std::unordered_map<TInterface*, TInterface*>& contexts_by_real,
        TInterface* real_pointer)
    {
        if (real_pointer == nullptr)
        {
            return nullptr;
        }

        const auto iterator = contexts_by_real.find(real_pointer);
        if (iterator == contexts_by_real.end())
        {
            return real_pointer;
        }

        return iterator->second;
    }

    /**
     * @brief Creates or reuses one shared-vtable hook for a live COM interface.
     * @tparam TInterface COM interface pointer type used as the registry key.
     * @tparam TContext Hook context type that stores the real interface pointer and vtable copy.
     * @param contexts_by_proxy Registry keyed by the live object pointer.
     * @param contexts_by_real Registry keyed by the live object pointer.
     * @param real_interface Real COM interface returned by the D3D9 runtime.
     * @param slot_count Number of vtable slots copied into the patched table.
     * @param overrides List of slot overrides written into the copied table.
     * @return Hook context when creation succeeds; otherwise nullptr.
     */
    template <typename TInterface, typename TContext>
    TContext* CreateOrGetProxyContext(
        std::unordered_map<TInterface*, std::unique_ptr<TContext>>& contexts_by_proxy,
        std::unordered_map<TInterface*, TInterface*>& contexts_by_real,
        TInterface* real_interface,
        std::size_t slot_count,
        std::initializer_list<std::pair<std::size_t, void*>> overrides)
    {
        if (real_interface == nullptr)
        {
            return nullptr;
        }

        const auto real_iterator = contexts_by_real.find(real_interface);
        if (real_iterator != contexts_by_real.end())
        {
            return FindProxyContext(contexts_by_proxy, real_iterator->second);
        }

        void** const real_vtable = GetComVtable(real_interface);
        if (real_vtable == nullptr)
        {
            return nullptr;
        }

        const std::vector<void*> original_vtable_storage = g_original_dispatch.Capture(real_vtable, slot_count);

        auto context = std::make_unique<TContext>();
        context->Real = real_interface;
        context->OriginalVtableStorage = original_vtable_storage;
        context->OriginalVtable = context->OriginalVtableStorage.data();
        context->ShadowVtable = context->OriginalVtableStorage;
        for (const auto& [slot_index, replacement] : overrides)
        {
            if (slot_index < context->ShadowVtable.size())
            {
                context->ShadowVtable[slot_index] = replacement;
            }
        }

        if (!helen::WriteMemory(real_vtable, context->ShadowVtable.data(), slot_count * sizeof(void*)))
        {
            return nullptr;
        }

        TContext* const context_ptr = context.get();
        contexts_by_real.emplace(real_interface, real_interface);
        contexts_by_proxy.emplace(real_interface, std::move(context));
        return context_ptr;
    }

    /** Re-establishes device interception after Reset rewrites the driver's dispatch table.
     * Adopts changed driver entries as originals without ever saving our own detours as originals.
     * Caller holds the registry mutex; this performs no Direct3D calls, including on failed Reset.
     */
    bool RefreshDeviceHooksAfterReset(Direct3d9DeviceHookContext& context) {
        void** const live_vtable = GetComVtable(context.Real);
        if (live_vtable == nullptr) { return false; }
        std::vector<void*> originals(live_vtable, live_vtable + context.ShadowVtable.size());
        std::vector<void*> patched = originals;
        for (std::size_t slot = 0; slot < originals.size(); ++slot) {
            if (context.ShadowVtable[slot] != context.OriginalVtableStorage[slot]) {
                if (originals[slot] == context.ShadowVtable[slot]) {
                    originals[slot] = context.OriginalVtableStorage[slot];
                }
                patched[slot] = context.ShadowVtable[slot];
            }
        }
        // Publish original addresses before re-exposing the detours.
        g_original_dispatch.Replace(live_vtable, originals);
        if (!helen::WriteMemory(live_vtable, patched.data(), patched.size() * sizeof(void*))) { return false; }
        // Keep OriginalVtable's storage stable for the existing hook context.
        std::copy(originals.begin(), originals.end(), context.OriginalVtableStorage.begin());
        std::copy(patched.begin(), patched.end(), context.ShadowVtable.begin());

        return true;
    }

    /**
     * @brief Removes one shadow-vtable COM hook from both registries when the real object is no longer referenced.
     * @tparam TInterface COM interface pointer type used as the registry key.
     * @tparam TContext Proxy context type stored in the owning registry.
     * @param contexts_by_proxy Registry keyed by proxy pointer.
     * @param contexts_by_real Registry keyed by real interface pointer.
     * @param proxy_interface Live interface pointer that should be removed.
     */
    template <typename TInterface, typename TContext>
    void RemoveProxyContext(
        std::unordered_map<TInterface*, std::unique_ptr<TContext>>& contexts_by_proxy,
        std::unordered_map<TInterface*, TInterface*>& contexts_by_real,
        TInterface* proxy_interface)
    {
        if (proxy_interface == nullptr)
        {
            return;
        }

        const auto proxy_iterator = contexts_by_proxy.find(proxy_interface);
        if (proxy_iterator == contexts_by_proxy.end())
        {
            return;
        }

        const TInterface* const real_interface = proxy_iterator->second->Real;
        contexts_by_proxy.erase(proxy_iterator);
        if (real_interface != nullptr)
        {
            contexts_by_real.erase(const_cast<TInterface*>(real_interface));
        }
    }
}

namespace helen
{
    D3d9TextureReplacementHookSet* D3d9TextureReplacementHookSet::active_instance_ = nullptr;

    D3d9TextureReplacementHookSet::D3d9TextureReplacementHookSet(
        bool enable_hooking,
        bool enable_hash_logging,
        bool enable_image_dumping,
        std::filesystem::path texture_dump_directory,
        std::vector<PackScopedTextureReplacementDefinition> replacements)
        : enable_hooking_(enable_hooking)
        , enable_hash_logging_(enable_hash_logging)
        , enable_image_dumping_(enable_image_dumping)
        , texture_dump_directory_(std::move(texture_dump_directory))
        , replacements_(std::move(replacements))
    {
    }

    D3d9TextureReplacementHookSet::~D3d9TextureReplacementHookSet()
    {
        Remove();
    }

    bool D3d9TextureReplacementHookSet::Install()
    {
        Logf(
            L"[d3d9] install begin enable=%d hash=%d dump=%d replacements=%zu",
            static_cast<int>(enable_hooking_),
            static_cast<int>(enable_hash_logging_),
            static_cast<int>(enable_image_dumping_),
            replacements_.size());

        if (IsInstalled())
        {
            return true;
        }

        std::size_t d3d9_replacement_count = 0;
        if (!ValidateDeclaredReplacements(d3d9_replacement_count))
        {
            return false;
        }

        Logf(L"[d3d9] install validated %zu d3d9 replacements.", d3d9_replacement_count);

        if (!enable_hooking_ && !enable_hash_logging_ && !enable_image_dumping_ && d3d9_replacement_count == 0)
        {
            Log(L"[d3d9] no D3D9 texture replacements declared.");
            return true;
        }

        if (enable_image_dumping_)
        {
            std::error_code error_code;
            std::filesystem::create_directories(texture_dump_directory_, error_code);
            if (error_code)
            {
                Logf(
                    L"[d3d9] failed to create texture dump directory %ls (error=%d).",
                    texture_dump_directory_.c_str(),
                    error_code.value());
                return false;
            }
        }

        const std::optional<ModuleView> main_module = QueryMainModule();
        if (!main_module.has_value())
        {
            Log(L"[d3d9] failed to query the main module for Direct3DCreate9 hook installation.");
            return false;
        }

        if (!direct3d_create9_hook_.Install(
                *main_module,
                "d3d9.dll",
                "Direct3DCreate9",
                reinterpret_cast<void*>(&Direct3DCreate9Detour)))
        {
            Log(L"[d3d9] failed to install the Direct3DCreate9 import hook.");
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            active_instance_ = this;
        }
        Log(L"[d3d9] install completed Direct3DCreate9 import hook.");
        Logf(
            L"[d3d9] installed Direct3DCreate9 import hook for %zu texture replacements (enabled=%ls hashLogging=%ls imageDumping=%ls).",
            d3d9_replacement_count,
            enable_hooking_ ? L"true" : L"false",
            enable_hash_logging_ ? L"true" : L"false",
            enable_image_dumping_ ? L"true" : L"false");
        return true;
    }

    void D3d9TextureReplacementHookSet::Remove()
    {
        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            if (active_instance_ == this) { active_instance_ = nullptr; }
        }

        direct3d_create9_hook_.Remove();
    }

    bool D3d9TextureReplacementHookSet::IsInstalled() const noexcept
    {
        return direct3d_create9_hook_.IsInstalled();
    }

    bool D3d9TextureReplacementHookSet::TrySetVsyncOverride(IDirect3DDevice9& device, D3d9VsyncOverride mode) {
        std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
        const Direct3d9DeviceHookContext* const context = FindDeviceHookContext(&device);
        if (active_instance_ == nullptr || context == nullptr || context->ResetInProgress) { return false; }
        active_instance_->PresentationOverrides.SetVsyncOverride(mode);
        return true;
    }

    D3d9TextureReplacementHookSet* D3d9TextureReplacementHookSet::Current() noexcept
    {
        return active_instance_;
    }

    IDirect3D9* WINAPI D3d9TextureReplacementHookSet::Direct3DCreate9Detour(UINT sdk_version)
    {
        D3d9TextureReplacementHookSet* const active = Current();
        if (active == nullptr || !active->direct3d_create9_hook_.IsInstalled())
        {
            return nullptr;
        }

        Logf(L"[d3d9] Direct3DCreate9 detour begin sdk=%u", static_cast<unsigned int>(sdk_version));
        const auto original = active->direct3d_create9_hook_.Original<decltype(&Direct3DCreate9)>();
        IDirect3D9* const direct3d = original(sdk_version);
        if (direct3d == nullptr)
        {
            Logf(L"[d3d9] Direct3DCreate9 intercepted (sdk=%u) but returned null.", static_cast<unsigned int>(sdk_version));
            return nullptr;
        }

        if (!active->InstallDirect3d9InstanceHooks(direct3d))
        {
            Log(L"[d3d9] failed to install proxy hooks for the live IDirect3D9 instance.");
            return direct3d;
        }

        std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
        const auto proxy_iterator = g_direct3d9_hook_by_real.find(direct3d);
        if (proxy_iterator == g_direct3d9_hook_by_real.end())
        {
            Log(L"[d3d9] Direct3DCreate9 shadow hook succeeded but no live instance was registered.");
            return direct3d;
        }

        Logf(L"[d3d9] Direct3DCreate9 detour installed IDirect3D9 hook ptr=0x%p real=0x%p", proxy_iterator->second, direct3d);
        Logf(L"[d3d9] Direct3DCreate9 intercepted (sdk=%u).", static_cast<unsigned int>(sdk_version));
        return proxy_iterator->second;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::CreateDeviceDetour(
        IDirect3D9* self,
        UINT adapter,
        D3DDEVTYPE device_type,
        HWND focus_window,
        DWORD behavior_flags,
        D3DPRESENT_PARAMETERS* presentation_parameters,
        IDirect3DDevice9** returned_device)
    {
        D3d9TextureReplacementHookSet* const active = Current();
        if (active == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        Direct3d9HookContext* const direct3d_context = FindDirect3d9HookContext(self);
        if (direct3d_context == nullptr || direct3d_context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9CreateDeviceFunction>(
            GetVtableSlot(direct3d_context->OriginalVtable, Direct3d9CreateDeviceVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        if (presentation_parameters != nullptr &&
            active->PresentationOverrides.GetVsyncOverride() != D3d9VsyncOverride::GameControlled) {
            UINT parameter_count = 1;
            if ((behavior_flags & D3DCREATE_ADAPTERGROUP_DEVICE) != 0) {
                D3DCAPS9 capabilities{};
                const HRESULT queried = direct3d_context->Real->GetDeviceCaps(adapter, device_type, &capabilities);
                if (FAILED(queried)) { return queried; }
                parameter_count = capabilities.NumberOfAdaptersInGroup;
                if (parameter_count == 0) { return D3DERR_INVALIDCALL; }
            }
            const UINT application_interval = presentation_parameters->PresentationInterval;
            active->PresentationOverrides.Apply({presentation_parameters, parameter_count});
            Logf(L"[d3d9] create presentation policy application=%u effective=%u blocks=%u",
                application_interval, presentation_parameters->PresentationInterval, parameter_count);
        }
        const HRESULT result = original(
            direct3d_context->Real,
            adapter,
            device_type,
            focus_window,
            behavior_flags,
            presentation_parameters,
            returned_device);
        if (FAILED(result) || returned_device == nullptr || *returned_device == nullptr)
        {
            return result;
        }

        Logf(L"[d3d9] CreateDevice detour begin real_device=0x%p", *returned_device);
        if (!active->InstallDeviceInstanceHooks(*returned_device))
        {
            Log(L"[d3d9] failed to install proxy hooks for the live IDirect3DDevice9 instance.");
            return result;
        }

        std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
        const auto proxy_iterator = g_direct3d9_device_hook_by_real.find(*returned_device);
        if (proxy_iterator == g_direct3d9_device_hook_by_real.end())
        {
            Log(L"[d3d9] CreateDevice shadow hook succeeded but no live device was registered.");
            return result;
        }

        *returned_device = proxy_iterator->second;
        Logf(L"[d3d9] CreateDevice detour installed IDirect3DDevice9 hook ptr=0x%p real=0x%p", proxy_iterator->second, direct3d_context->Real);
        return result;
    }

    ULONG WINAPI D3d9TextureReplacementHookSet::Direct3d9ReleaseDetour(IDirect3D9* self) {
        const auto original = reinterpret_cast<Direct3d9ReleaseFunction>(
            GetOriginalVtableSlot(self, ReleaseVtableIndex));
        const ULONG count = original(self);
        if (count == 0) {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            g_direct3d9_hook_contexts.erase(self);
            g_direct3d9_hook_by_real.erase(self);
        }
        return count;
    }

#if 0
    HRESULT WINAPI D3d9TextureReplacementHookSet::GetDirect3DDetour(IDirect3DDevice9* self, IDirect3D9** returned_direct3d)
    {
        if (returned_direct3d == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9GetDirect3DFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9GetDirect3DVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        IDirect3D9* real_direct3d = nullptr;
        const HRESULT result = original(context->Real, &real_direct3d);
        if (FAILED(result) || real_direct3d == nullptr)
        {
            return result;
        }

        D3d9TextureReplacementHookSet* const active = Current();
        if (active == nullptr || !active->InstallDirect3d9InstanceHooks(real_direct3d))
        {
            *returned_direct3d = real_direct3d;
            return result;
        }

        *returned_direct3d = WrapProxyPointer(g_direct3d9_hook_by_real, real_direct3d);
        return result;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::SetCursorPropertiesDetour(
        IDirect3DDevice9* self,
        UINT x_hotspot,
        UINT y_hotspot,
        IDirect3DSurface9* cursor_bitmap)
    {
        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9SetCursorPropertiesFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9SetCursorPropertiesVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        IDirect3DSurface9* const real_cursor_bitmap =
            UnwrapProxyPointer(g_direct3d9_surface_hook_contexts, cursor_bitmap);
        return original(context->Real, x_hotspot, y_hotspot, real_cursor_bitmap);
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::CreateAdditionalSwapChainDetour(
        IDirect3DDevice9* self,
        D3DPRESENT_PARAMETERS* presentation_parameters,
        IDirect3DSwapChain9** returned_swap_chain)
    {
        if (returned_swap_chain == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9CreateAdditionalSwapChainFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9CreateAdditionalSwapChainVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        IDirect3DSwapChain9* real_swap_chain = nullptr;
        const HRESULT result = original(context->Real, presentation_parameters, &real_swap_chain);
        if (FAILED(result) || real_swap_chain == nullptr)
        {
            return result;
        }

        D3d9TextureReplacementHookSet* const active = Current();
        if (active == nullptr || !active->InstallSwapChainInstanceHooks(real_swap_chain, self))
        {
            *returned_swap_chain = real_swap_chain;
            return result;
        }

        *returned_swap_chain = WrapProxyPointer(g_direct3d9_swap_chain_hook_by_real, real_swap_chain);
        return result;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::GetSwapChainDetour(
        IDirect3DDevice9* self,
        UINT swap_chain_index,
        IDirect3DSwapChain9** returned_swap_chain)
    {
        if (returned_swap_chain == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9GetSwapChainFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9GetSwapChainVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        IDirect3DSwapChain9* real_swap_chain = nullptr;
        const HRESULT result = original(context->Real, swap_chain_index, &real_swap_chain);
        if (FAILED(result) || real_swap_chain == nullptr)
        {
            return result;
        }

        D3d9TextureReplacementHookSet* const active = Current();
        if (active == nullptr || !active->InstallSwapChainInstanceHooks(real_swap_chain, self))
        {
            *returned_swap_chain = real_swap_chain;
            return result;
        }

        *returned_swap_chain = WrapProxyPointer(g_direct3d9_swap_chain_hook_by_real, real_swap_chain);
        return result;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::GetBackBufferDetour(
        IDirect3DDevice9* self,
        UINT swap_chain_index,
        UINT back_buffer_index,
        D3DBACKBUFFER_TYPE back_buffer_type,
        IDirect3DSurface9** returned_surface)
    {
        if (returned_surface == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9GetBackBufferFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9GetBackBufferVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        IDirect3DSurface9* real_surface = nullptr;
        const HRESULT result = original(context->Real, swap_chain_index, back_buffer_index, back_buffer_type, &real_surface);
        if (FAILED(result) || real_surface == nullptr)
        {
            return result;
        }

        D3d9TextureReplacementHookSet* const active = Current();
        if (active == nullptr || !active->InstallSurfaceInstanceHooks(real_surface, nullptr, nullptr))
        {
            *returned_surface = real_surface;
            return result;
        }

        *returned_surface = WrapProxyPointer(g_direct3d9_surface_hook_by_real, real_surface);
        return result;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::CreateRenderTargetDetour(
        IDirect3DDevice9* self,
        UINT width,
        UINT height,
        D3DFORMAT format,
        D3DMULTISAMPLE_TYPE multi_sample,
        DWORD multi_sample_quality,
        BOOL lockable,
        IDirect3DSurface9** returned_surface,
        HANDLE* shared_handle)
    {
        if (returned_surface == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9CreateRenderTargetFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9CreateRenderTargetVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        IDirect3DSurface9* real_surface = nullptr;
        const HRESULT result = original(
            context->Real,
            width,
            height,
            format,
            multi_sample,
            multi_sample_quality,
            lockable,
            &real_surface,
            shared_handle);
        if (FAILED(result) || real_surface == nullptr)
        {
            return result;
        }

        D3d9TextureReplacementHookSet* const active = Current();
        if (active == nullptr || !active->InstallSurfaceInstanceHooks(real_surface, nullptr, nullptr))
        {
            *returned_surface = real_surface;
            return result;
        }

        *returned_surface = WrapProxyPointer(g_direct3d9_surface_hook_by_real, real_surface);
        return result;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::CreateDepthStencilSurfaceDetour(
        IDirect3DDevice9* self,
        UINT width,
        UINT height,
        D3DFORMAT format,
        D3DMULTISAMPLE_TYPE multi_sample,
        DWORD multi_sample_quality,
        BOOL discard,
        IDirect3DSurface9** returned_surface,
        HANDLE* shared_handle)
    {
        if (returned_surface == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9CreateDepthStencilSurfaceFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9CreateDepthStencilSurfaceVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        IDirect3DSurface9* real_surface = nullptr;
        const HRESULT result = original(
            context->Real,
            width,
            height,
            format,
            multi_sample,
            multi_sample_quality,
            discard,
            &real_surface,
            shared_handle);
        if (FAILED(result) || real_surface == nullptr)
        {
            return result;
        }

        D3d9TextureReplacementHookSet* const active = Current();
        if (active == nullptr || !active->InstallSurfaceInstanceHooks(real_surface, nullptr, nullptr))
        {
            *returned_surface = real_surface;
            return result;
        }

        *returned_surface = WrapProxyPointer(g_direct3d9_surface_hook_by_real, real_surface);
        return result;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::UpdateSurfaceDetour(
        IDirect3DDevice9* self,
        IDirect3DSurface9* source_surface,
        const RECT* source_rect,
        IDirect3DSurface9* destination_surface,
        const POINT* destination_point)
    {
        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9UpdateSurfaceFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9UpdateSurfaceVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        return original(
            context->Real,
            UnwrapProxyPointer(g_direct3d9_surface_hook_contexts, source_surface),
            source_rect,
            UnwrapProxyPointer(g_direct3d9_surface_hook_contexts, destination_surface),
            destination_point);
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::GetRenderTargetDataDetour(
        IDirect3DDevice9* self,
        IDirect3DSurface9* render_target,
        IDirect3DSurface9* destination_surface)
    {
        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9GetRenderTargetDataFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9GetRenderTargetDataVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        return original(
            context->Real,
            UnwrapProxyPointer(g_direct3d9_surface_hook_contexts, render_target),
            UnwrapProxyPointer(g_direct3d9_surface_hook_contexts, destination_surface));
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::GetFrontBufferDataDetour(
        IDirect3DDevice9* self,
        UINT swap_chain_index,
        IDirect3DSurface9* destination_surface)
    {
        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9GetFrontBufferDataFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9GetFrontBufferDataVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        return original(
            context->Real,
            swap_chain_index,
            UnwrapProxyPointer(g_direct3d9_surface_hook_contexts, destination_surface));
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::StretchRectDetour(
        IDirect3DDevice9* self,
        IDirect3DSurface9* source_surface,
        const RECT* source_rect,
        IDirect3DSurface9* destination_surface,
        const RECT* destination_rect,
        D3DTEXTUREFILTERTYPE filter)
    {
        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9StretchRectFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9StretchRectVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        return original(
            context->Real,
            UnwrapProxyPointer(g_direct3d9_surface_hook_contexts, source_surface),
            source_rect,
            UnwrapProxyPointer(g_direct3d9_surface_hook_contexts, destination_surface),
            destination_rect,
            filter);
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::ColorFillDetour(
        IDirect3DDevice9* self,
        IDirect3DSurface9* surface,
        const RECT* rect,
        D3DCOLOR color)
    {
        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9ColorFillFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9ColorFillVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        return original(
            context->Real,
            UnwrapProxyPointer(g_direct3d9_surface_hook_contexts, surface),
            rect,
            color);
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::CreateOffscreenPlainSurfaceDetour(
        IDirect3DDevice9* self,
        UINT width,
        UINT height,
        D3DFORMAT format,
        D3DPOOL pool,
        IDirect3DSurface9** returned_surface,
        HANDLE* shared_handle)
    {
        if (returned_surface == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9CreateOffscreenPlainSurfaceFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9CreateOffscreenPlainSurfaceVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        IDirect3DSurface9* real_surface = nullptr;
        const HRESULT result = original(context->Real, width, height, format, pool, &real_surface, shared_handle);
        if (FAILED(result) || real_surface == nullptr)
        {
            return result;
        }

        D3d9TextureReplacementHookSet* const active = Current();
        if (active == nullptr || !active->InstallSurfaceInstanceHooks(real_surface, nullptr, nullptr))
        {
            *returned_surface = real_surface;
            return result;
        }

        *returned_surface = WrapProxyPointer(g_direct3d9_surface_hook_by_real, real_surface);
        return result;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::SetRenderTargetDetour(
        IDirect3DDevice9* self,
        DWORD render_target_index,
        IDirect3DSurface9* render_target)
    {
        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9SetRenderTargetFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9SetRenderTargetVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        return original(
            context->Real,
            render_target_index,
            UnwrapProxyPointer(g_direct3d9_surface_hook_contexts, render_target));
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::GetRenderTargetDetour(
        IDirect3DDevice9* self,
        DWORD render_target_index,
        IDirect3DSurface9** returned_surface)
    {
        if (returned_surface == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9GetRenderTargetFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9GetRenderTargetVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        IDirect3DSurface9* real_surface = nullptr;
        const HRESULT result = original(context->Real, render_target_index, &real_surface);
        if (FAILED(result) || real_surface == nullptr)
        {
            return result;
        }

        D3d9TextureReplacementHookSet* const active = Current();
        if (active == nullptr || !active->InstallSurfaceInstanceHooks(real_surface, nullptr, nullptr))
        {
            *returned_surface = real_surface;
            return result;
        }

        *returned_surface = WrapProxyPointer(g_direct3d9_surface_hook_by_real, real_surface);
        return result;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::SetDepthStencilSurfaceDetour(
        IDirect3DDevice9* self,
        IDirect3DSurface9* new_depth_surface)
    {
        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9SetDepthStencilSurfaceFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9SetDepthStencilSurfaceVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        return original(
            context->Real,
            UnwrapProxyPointer(g_direct3d9_surface_hook_contexts, new_depth_surface));
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::GetDepthStencilSurfaceDetour(
        IDirect3DDevice9* self,
        IDirect3DSurface9** returned_surface)
    {
        if (returned_surface == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9GetDepthStencilSurfaceFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9GetDepthStencilSurfaceVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        IDirect3DSurface9* real_surface = nullptr;
        const HRESULT result = original(context->Real, &real_surface);
        if (FAILED(result) || real_surface == nullptr)
        {
            return result;
        }

        D3d9TextureReplacementHookSet* const active = Current();
        if (active == nullptr || !active->InstallSurfaceInstanceHooks(real_surface, nullptr, nullptr))
        {
            *returned_surface = real_surface;
            return result;
        }

        *returned_surface = WrapProxyPointer(g_direct3d9_surface_hook_by_real, real_surface);
        return result;
    }

#endif

    HRESULT WINAPI D3d9TextureReplacementHookSet::CreateTextureDetour(
        IDirect3DDevice9* self, UINT width, UINT height, UINT levels, DWORD usage,
        D3DFORMAT format, D3DPOOL pool, IDirect3DTexture9** returned_texture, HANDLE* shared_handle) {
        const auto original = reinterpret_cast<Direct3d9CreateTextureFunction>(
            GetOriginalVtableSlot(self, Direct3d9CreateTextureVtableIndex));
        if (returned_texture == nullptr) { return D3DERR_INVALIDCALL; }
        *returned_texture = nullptr;
        Microsoft::WRL::ComPtr<IDirect3DTexture9> texture;
        const HRESULT result = original(self, width, height, levels, usage, format, pool, texture.GetAddressOf(), shared_handle);
        if (FAILED(result) || !texture) { return FAILED(result) ? result : D3DERR_INVALIDCALL; }
        D3d9TextureReplacementHookSet* const active = Current();
        bool tracked;
        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            tracked = FindDeviceHookContext(self) != nullptr;
        }
        if (!tracked || active == nullptr) {
            *returned_texture = texture.Detach();
            return result;
        }
        D3DSURFACE_DESC description{};
        const HRESULT description_result = texture->GetLevelDesc(0, &description);
        if (FAILED(description_result)) { return description_result; }
        if (!active->InstallTextureInstanceHooks(texture.Get(), self)) {
            Log(L"[d3d9] texture registration failed.");
            return E_FAIL;
        }
        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
            if (context == nullptr || FindTrackedTextureRecord(texture.Get()) != nullptr) { return D3DERR_INVALIDCALL; }
            TrackedTextureRecord& record = g_tracked_texture_records[texture.Get()];
            record.OwningDevice = self;
            record.Description = description;
            record.HasDescription = true;
            context->TrackedTextures.insert(texture.Get());
        }
        Logf(L"[d3d9] tracked texture created ptr=0x%p size=%ux%u format=%hs pool=%hs usage=0x%08lX",
            texture.Get(), description.Width, description.Height, GetD3d9FormatToken(format).data(),
            GetD3d9PoolToken(pool).data(), static_cast<unsigned long>(usage));
        *returned_texture = texture.Detach();
        return result;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::UpdateTextureDetour(
        IDirect3DDevice9* self,
        IDirect3DBaseTexture9* source_texture,
        IDirect3DBaseTexture9* destination_texture)
    {
        D3d9TextureReplacementHookSet* const active = Current();
        if (active == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9UpdateTextureFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9UpdateTextureVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        IDirect3DBaseTexture9* real_source_texture = source_texture;
        IDirect3DBaseTexture9* real_destination_texture = destination_texture;
        if (source_texture != nullptr && source_texture->GetType() == D3DRTYPE_TEXTURE)
        {
            real_source_texture = UnwrapProxyPointer(
                g_direct3d9_texture_hook_contexts,
                reinterpret_cast<IDirect3DTexture9*>(source_texture));
        }

        if (destination_texture != nullptr && destination_texture->GetType() == D3DRTYPE_TEXTURE)
        {
            real_destination_texture = UnwrapProxyPointer(
                g_direct3d9_texture_hook_contexts,
                reinterpret_cast<IDirect3DTexture9*>(destination_texture));
        }

        const HRESULT result = original(context->Real, real_source_texture, real_destination_texture);
        if (FAILED(result) ||
            source_texture == nullptr ||
            destination_texture == nullptr ||
            source_texture->GetType() != D3DRTYPE_TEXTURE ||
            destination_texture->GetType() != D3DRTYPE_TEXTURE)
        {
            return result;
        }

        if (!active->ShouldTrackTextures())
        {
            return result;
        }

        auto* const source = reinterpret_cast<IDirect3DTexture9*>(source_texture);
        auto* const destination = reinterpret_cast<IDirect3DTexture9*>(destination_texture);

        std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
        TrackedTextureRecord* const source_record = FindTrackedTextureRecord(source);
        TrackedTextureRecord* const destination_record = FindTrackedTextureRecord(destination);
        if (source_record == nullptr || destination_record == nullptr || !source_record->HasFingerprint)
        {
            return result;
        }

        destination_record->Fingerprint = source_record->Fingerprint;
        destination_record->HasFingerprint = true;
        destination_record->Dirty = false;
        destination_record->FingerprintLogged = source_record->FingerprintLogged;
        destination_record->ReplacementMatched = source_record->ReplacementMatched;
        destination_record->Description = source_record->Description;
        destination_record->HasDescription = source_record->HasDescription;

        if (active->enable_hash_logging_)
        {
            Logf(
                L"[d3d9] propagated texture fingerprint source=0x%p destination=0x%p hash=%hs",
                source,
                destination,
                source_record->Fingerprint.Hash.c_str());
        }

        Logf(
            L"[d3d9] tracked texture update complete source=0x%p destination=0x%p hash=%hs",
            source,
            destination,
            source_record->Fingerprint.Hash.c_str());

        return result;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::SetTextureDetour(
        IDirect3DDevice9* self, DWORD stage, IDirect3DBaseTexture9* texture) {
        const auto original = reinterpret_cast<Direct3d9SetTextureFunction>(
            GetOriginalVtableSlot(self, Direct3d9SetTextureVtableIndex));
        const bool is_texture = texture != nullptr && texture->GetType() == D3DRTYPE_TEXTURE;
        std::shared_ptr<IDirect3DTexture9> replacement;
        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
            if (context != nullptr) {
                ++context->BindingRevision;
                context->BoundTextures[stage] = texture;
            }
            if (is_texture) {
                TrackedTextureRecord* const record = FindTrackedTextureRecord(static_cast<IDirect3DTexture9*>(texture));
                if (record != nullptr) {
                    replacement = record->ReplacementTexture;
                    record->HasLastSetTextureStage = true;
                    record->LastSetTextureStage = stage;
                }
            }
        }
        return original(self, stage, replacement ? replacement.get() : texture);
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::ResetDetour(
        IDirect3DDevice9* self, D3DPRESENT_PARAMETERS* presentation_parameters) {
        D3d9TextureReplacementHookSet* const active = Current();
        const auto original = reinterpret_cast<Direct3d9ResetFunction>(
            GetOriginalVtableSlot(self, Direct3d9ResetVtableIndex));
        std::vector<std::shared_ptr<IDirect3DTexture9>> released;
        UINT presentation_parameter_count = 0;
        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
            if (context == nullptr || context->ResetInProgress) { return D3DERR_INVALIDCALL; }
            presentation_parameter_count = context->PresentationParameterCount;
            context->ResetInProgress = true;
            ++context->BindingRevision;
            for (IDirect3DTexture9* const key : context->TrackedTextures) {
                TrackedTextureRecord* const record = FindTrackedTextureRecord(key);
                if (record == nullptr) { continue; }
                record->PendingReplacement.reset();
                record->Identity = std::make_shared<helen::D3d9ReplacementTicket>();
                if (record->ReplacementTexture) {
                    released.push_back(std::move(record->ReplacementTexture));
                    record->ReplacementNeedsRestore = true;
                }
            }
        }
        const std::size_t released_count = released.size();
        released.clear(); // Last COM releases run with no registry lock.
        Logf(L"[d3d9] reset released replacements device=0x%p count=%zu", self, released_count);
        if (presentation_parameters != nullptr && active != nullptr) {
            const UINT application_interval = presentation_parameters->PresentationInterval;
            active->PresentationOverrides.Apply({presentation_parameters, presentation_parameter_count});
            Logf(L"[d3d9] reset presentation policy application=%u effective=%u blocks=%u",
                application_interval, presentation_parameters->PresentationInterval, presentation_parameter_count);
        }
        const std::optional<D3DPRESENT_PARAMETERS> requested = presentation_parameters == nullptr
            ? std::nullopt : std::optional<D3DPRESENT_PARAMETERS>(*presentation_parameters);
        const HRESULT result = original(self, presentation_parameters);
        std::optional<std::wstring> diagnostic;
        std::unordered_map<IDirect3DTexture9*, TrackedTextureRecord> restore_requests;
        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
            if (context == nullptr) { return FAILED(result) ? result : D3DERR_INVALIDCALL; }
            context->ResetInProgress = false;
            if (!RefreshDeviceHooksAfterReset(*context)) { return FAILED(result) ? result : E_FAIL; }
            diagnostic = context->ResetDiagnostics.Record(result, requested, GetCurrentThreadId());
            if (SUCCEEDED(result)) {
                context->BoundTextures.clear();
                for (IDirect3DTexture9* const key : context->TrackedTextures) {
                    TrackedTextureRecord* const record = FindTrackedTextureRecord(key);
                    if (record == nullptr) { continue; }
                    record->Dirty = false;
                    record->HasLastSetTextureStage = false;
                    record->HasWritableLockSnapshot = false;
                    record->WritableLockBytes = nullptr;
                    record->WritableLockPitch = 0;
                    if (record->ReplacementNeedsRestore) { restore_requests.emplace(key, *record); }
                    else {
                        record->HasFingerprint = false;
                        record->FingerprintLogged = false;
                        record->ReplacementMatched = false;
                        record->Fingerprint.Hash.clear();
                    }
                }
            }
        }
        if (diagnostic) { Logf(L"[d3d9] reset device=0x%p %ls", self, diagnostic->c_str()); }
        if (FAILED(result)) { return result; }
        for (const auto& request : restore_requests) {
            if (active == nullptr) { return D3DERR_INVALIDCALL; }
            const auto* replacement = active->FindDeclaredReplacement(request.second.Description, request.second.Fingerprint.Hash);
            HRESULT restore_result = E_FAIL;
            if (replacement == nullptr || !::TryCacheReplacementTexture(*self, request.first,
                request.second.Description, *replacement, restore_result, request.second.Identity)) {
                Logf(L"[d3d9] reset nativeSucceeded=true replacement restore failed source=0x%p hr=0x%08lX",
                    request.first, static_cast<unsigned long>(restore_result));
                return FAILED(restore_result) ? restore_result : E_FAIL;
            }
        }
        Logf(L"[d3d9] reset restored replacements device=0x%p count=%zu", self, restore_requests.size());
        return result;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::TextureGetSurfaceLevelDetour(
        IDirect3DTexture9* self, UINT level, IDirect3DSurface9** returned_surface) {
        const auto original = reinterpret_cast<Direct3d9TextureGetSurfaceLevelFunction>(
            GetOriginalVtableSlot(self, Direct3d9TextureGetSurfaceLevelVtableIndex));
        const HRESULT result = original(self, level, returned_surface);
        if (FAILED(result) || returned_surface == nullptr || *returned_surface == nullptr) { return result; }
        bool tracked;
        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            tracked = FindTextureHookContext(self) != nullptr;
        }
        D3d9TextureReplacementHookSet* const active = Current();
        if (tracked && active != nullptr && !active->InstallSurfaceInstanceHooks(*returned_surface, self, nullptr, level)) {
            Log(L"[d3d9] could not install tracked texture surface hooks.");
        }
        return result;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::TextureLockRectDetour(
        IDirect3DTexture9* self, UINT level, D3DLOCKED_RECT* locked_rect, const RECT* rect, DWORD flags) {
        const auto original = reinterpret_cast<Direct3d9TextureLockRectFunction>(
            GetOriginalVtableSlot(self, Direct3d9TextureLockRectVtableIndex));
        const HRESULT result = original(self, level, locked_rect, rect, flags);
        if (FAILED(result) || level != 0 || (flags & D3DLOCK_READONLY) != 0) { return result; }
        std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
        TrackedTextureRecord* const record = FindTrackedTextureRecord(self);
        if (record != nullptr) {
            record->PendingReplacement.reset();
            record->Identity = std::make_shared<helen::D3d9ReplacementTicket>();
            record->Dirty = true;
            record->HasWritableLockSnapshot = locked_rect != nullptr && locked_rect->pBits != nullptr &&
                locked_rect->Pitch > 0 && rect == nullptr;
            record->WritableLockBytes = record->HasWritableLockSnapshot ? static_cast<const std::uint8_t*>(locked_rect->pBits) : nullptr;
            record->WritableLockPitch = record->HasWritableLockSnapshot ? locked_rect->Pitch : 0;
        }
        return result;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::TextureUnlockRectDetour(IDirect3DTexture9* self, UINT level) {
        const auto original = reinterpret_cast<Direct3d9TextureUnlockRectFunction>(
            GetOriginalVtableSlot(self, Direct3d9TextureUnlockRectVtableIndex));
        D3d9TextureReplacementHookSet* const active = Current();
        const std::optional<D3d9TextureUpload> upload = level == 0 && active != nullptr
            ? CaptureWritableUpload(self) : std::nullopt;
        const HRESULT result = original(self, level);
        if (FAILED(result) || !upload) { return result; }
        const HRESULT observed = active->CompleteTextureUpload(*self, *upload);
        return FAILED(observed) ? observed : result;
    }

    ULONG WINAPI D3d9TextureReplacementHookSet::DeviceReleaseDetour(IDirect3DDevice9* self) {
        const auto original = reinterpret_cast<Direct3d9DeviceReleaseFunction>(
            GetOriginalVtableSlot(self, ReleaseVtableIndex));
        const ULONG count = original(self);
        if (count == 0) {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            g_direct3d9_device_hook_contexts.erase(self);
            g_direct3d9_device_hook_by_real.erase(self);
        }
        return count;
    }

    ULONG WINAPI D3d9TextureReplacementHookSet::TextureReleaseDetour(IDirect3DTexture9* self) {
        const auto original = reinterpret_cast<Direct3d9TextureReleaseFunction>(
            GetOriginalVtableSlot(self, ReleaseVtableIndex));
        const ULONG count = original(self);
        std::shared_ptr<IDirect3DTexture9> released;
        if (count == 0) {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            TrackedTextureRecord* const record = FindTrackedTextureRecord(self);
            if (record != nullptr) {
                Direct3d9DeviceHookContext* const device = FindDeviceHookContext(record->OwningDevice);
                if (device != nullptr) { device->TrackedTextures.erase(self); }
                released = std::move(record->ReplacementTexture);
                record->PendingReplacement.reset();
                g_tracked_texture_records.erase(self);
            }
            g_direct3d9_texture_hook_contexts.erase(self);
            g_direct3d9_texture_hook_by_real.erase(self);
            // Texture-owned surfaces can die internally without a final public surface Release.
            for (auto it = g_direct3d9_surface_hook_contexts.begin(); it != g_direct3d9_surface_hook_contexts.end();) {
                if (it->second->OwningTexture == self) {
                    g_direct3d9_surface_hook_by_real.erase(it->first);
                    it = g_direct3d9_surface_hook_contexts.erase(it);
                } else { ++it; }
            }
        }
        return count;
    }

    ULONG WINAPI D3d9TextureReplacementHookSet::SurfaceReleaseDetour(IDirect3DSurface9* self) {
        const auto original = reinterpret_cast<Direct3d9SurfaceReleaseFunction>(
            GetOriginalVtableSlot(self, ReleaseVtableIndex));
        bool has_tracked_parent;
        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            const Direct3d9SurfaceHookContext* const context = FindSurfaceHookContext(self);
            has_tracked_parent = context != nullptr && context->OwningTexture != nullptr;
        }
        IDirect3DTexture9* parent = nullptr;
        if (has_tracked_parent) {
            const HRESULT result = self->GetContainer(__uuidof(IDirect3DTexture9), reinterpret_cast<void**>(&parent));
            if (FAILED(result) || parent == nullptr) {
                Logf(L"[d3d9] tracked surface lost its required parent before Release hr=0x%08lX", static_cast<unsigned long>(result));
                std::terminate();
            }
        }
        // Hand the parent's last reference to our texture release path. Otherwise native
        // surface destruction can destroy its parent internally, bypassing texture tracking.
        // Both acquisition and destruction run without the registry mutex.
        const std::unique_ptr<IDirect3DTexture9, Direct3d9TextureReleaseFunction> parent_reference(parent, &TextureReleaseDetour);
        const ULONG count = original(self);
        if (count == 0) {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            g_direct3d9_surface_hook_contexts.erase(self);
            g_direct3d9_surface_hook_by_real.erase(self);
        }
        return count;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::SurfaceGetDeviceDetour(
        IDirect3DSurface9* self, IDirect3DDevice9** returned_device) {
        const auto original = reinterpret_cast<Direct3d9SurfaceGetDeviceFunction>(GetOriginalVtableSlot(self, 3));
        return original(self, returned_device);
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::SurfaceLockRectDetour(
        IDirect3DSurface9* self, D3DLOCKED_RECT* locked_rect, const RECT* rect, DWORD flags) {
        const auto original = reinterpret_cast<Direct3d9SurfaceLockRectFunction>(GetOriginalVtableSlot(self, 13));
        const HRESULT result = original(self, locked_rect, rect, flags);
        if (FAILED(result) || (flags & D3DLOCK_READONLY) != 0) { return result; }
        std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
        Direct3d9SurfaceHookContext* const context = FindSurfaceHookContext(self);
        TrackedTextureRecord* const record = context == nullptr || context->TextureLevel != 0
            ? nullptr : FindTrackedTextureRecord(context->OwningTexture);
        if (record != nullptr) {
            record->PendingReplacement.reset();
            record->Identity = std::make_shared<helen::D3d9ReplacementTicket>();
            record->Dirty = true;
            record->HasWritableLockSnapshot = locked_rect != nullptr && locked_rect->pBits != nullptr &&
                locked_rect->Pitch > 0 && rect == nullptr;
            record->WritableLockBytes = record->HasWritableLockSnapshot ? static_cast<const std::uint8_t*>(locked_rect->pBits) : nullptr;
            record->WritableLockPitch = record->HasWritableLockSnapshot ? locked_rect->Pitch : 0;
        }
        return result;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::SurfaceUnlockRectDetour(IDirect3DSurface9* self) {
        const auto original = reinterpret_cast<Direct3d9SurfaceUnlockRectFunction>(GetOriginalVtableSlot(self, 14));
        bool tracked;
        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            const Direct3d9SurfaceHookContext* const context = FindSurfaceHookContext(self);
            tracked = context != nullptr && context->OwningTexture != nullptr && context->TextureLevel == 0;
        }
        D3d9TextureReplacementHookSet* const active = Current();
        if (!tracked || active == nullptr) { return original(self); }
        Microsoft::WRL::ComPtr<IDirect3DTexture9> owner;
        const HRESULT owner_result = self->GetContainer(__uuidof(IDirect3DTexture9),
            reinterpret_cast<void**>(owner.GetAddressOf()));
        if (FAILED(owner_result) || !owner) {
            const HRESULT result = original(self);
            return FAILED(result) ? result : (FAILED(owner_result) ? owner_result : D3DERR_INVALIDCALL);
        }
        const std::optional<D3d9TextureUpload> upload = CaptureWritableUpload(owner.Get());
        const HRESULT result = original(self);
        if (FAILED(result) || !upload) { return result; }
        const HRESULT observed = active->CompleteTextureUpload(*owner.Get(), *upload);
        return FAILED(observed) ? observed : result;
    }

    #if 0
    ULONG WINAPI D3d9TextureReplacementHookSet::SwapChainReleaseDetour(IDirect3DSwapChain9* self)
    {
        Direct3d9SwapChainHookContext* const context = FindSwapChainHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return 0;
        }

        const auto original = reinterpret_cast<Direct3d9SwapChainReleaseFunction>(
            GetVtableSlot(context->OriginalVtable, ReleaseVtableIndex));
        if (original == nullptr)
        {
            return 0;
        }

        const ULONG reference_count = original(context->Real);
        if (reference_count == 0)
        {
            IDirect3DSwapChain9* const real = context->Real;
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            g_direct3d9_swap_chain_hook_contexts.erase(self);
            g_direct3d9_swap_chain_hook_by_real.erase(real);
        }

        return reference_count;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::SwapChainGetDeviceDetour(
        IDirect3DSwapChain9* self,
        IDirect3DDevice9** returned_device)
    {
        if (returned_device == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        Direct3d9SwapChainHookContext* const context = FindSwapChainHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9SwapChainGetDeviceFunction>(
            GetVtableSlot(context->OriginalVtable, 8));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        IDirect3DDevice9* real_device = nullptr;
        const HRESULT result = original(context->Real, &real_device);
        if (FAILED(result) || real_device == nullptr)
        {
            return result;
        }

        D3d9TextureReplacementHookSet* const active = Current();
        if (active == nullptr || !active->InstallDeviceInstanceHooks(real_device))
        {
            *returned_device = real_device;
            return result;
        }

        *returned_device = WrapProxyPointer(g_direct3d9_device_hook_by_real, real_device);
        return result;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::SwapChainGetBackBufferDetour(
        IDirect3DSwapChain9* self,
        UINT back_buffer_index,
        D3DBACKBUFFER_TYPE type,
        IDirect3DSurface9** returned_surface)
    {
        if (returned_surface == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        Direct3d9SwapChainHookContext* const context = FindSwapChainHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9SwapChainGetBackBufferFunction>(
            GetVtableSlot(context->OriginalVtable, 5));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        IDirect3DSurface9* real_surface = nullptr;
        const HRESULT result = original(context->Real, back_buffer_index, type, &real_surface);
        if (FAILED(result) || real_surface == nullptr)
        {
            return result;
        }

        D3d9TextureReplacementHookSet* const active = Current();
        if (active == nullptr || !active->InstallSurfaceInstanceHooks(real_surface, nullptr, self))
        {
            *returned_surface = real_surface;
            return result;
        }

        *returned_surface = WrapProxyPointer(g_direct3d9_surface_hook_by_real, real_surface);
        return result;
    }
    #endif

    bool D3d9TextureReplacementHookSet::ValidateDeclaredReplacements(std::size_t& d3d9_replacement_count) const
    {
        d3d9_replacement_count = 0;

        for (const PackScopedTextureReplacementDefinition& replacement : replacements_)
        {
            if (replacement.Definition.Id.empty() || replacement.Definition.Api.empty())
            {
                Log(L"[d3d9] texture replacement validation failed: id/api must be non-empty.");
                return false;
            }

            if (!EqualsAsciiIgnoreCase(replacement.Definition.Api, "d3d9"))
            {
                Logf(
                    L"[d3d9] texture replacement validation failed for id=%hs: unsupported api=%hs.",
                    replacement.Definition.Id.c_str(),
                    replacement.Definition.Api.c_str());
                return false;
            }

            if (replacement.Definition.Width == 0 || replacement.Definition.Height == 0 || replacement.Definition.Format.empty() || replacement.Definition.Hash.empty())
            {
                Logf(
                    L"[d3d9] texture replacement validation failed for id=%hs: match fields must be populated.",
                    replacement.Definition.Id.c_str());
                return false;
            }

            if (replacement.Definition.ReplacementPath.empty())
            {
                Logf(
                    L"[d3d9] texture replacement validation failed for id=%hs: replacement path must be non-empty.",
                    replacement.Definition.Id.c_str());
                return false;
            }

            const std::optional<std::filesystem::path> resolved_path =
                replacement.AssetResolver.Resolve(replacement.Definition.ReplacementPath);
            if (!resolved_path.has_value())
            {
                Logf(
                    L"[d3d9] texture replacement validation failed for id=%hs: replacement path escaped pack root.",
                    replacement.Definition.Id.c_str());
                return false;
            }

            if (!std::filesystem::exists(*resolved_path) || !std::filesystem::is_regular_file(*resolved_path))
            {
                Logf(
                    L"[d3d9] texture replacement validation failed for id=%hs: replacement asset is missing (%ls).",
                    replacement.Definition.Id.c_str(),
                    resolved_path->c_str());
                return false;
            }

            ++d3d9_replacement_count;
        }

        return true;
    }

    bool D3d9TextureReplacementHookSet::InstallDirect3d9InstanceHooks(IDirect3D9* direct3d)
    {
        if (direct3d == nullptr)
        {
            return false;
        }

        Logf(L"[d3d9] install direct3d instance begin ptr=0x%p", direct3d);
        std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
        if (g_direct3d9_hook_by_real.find(direct3d) != g_direct3d9_hook_by_real.end())
        {
            return true;
        }

        Direct3d9HookContext* const context = CreateOrGetProxyContext(
            g_direct3d9_hook_contexts,
            g_direct3d9_hook_by_real,
            direct3d,
            Direct3d9VtableSlotCount,
            {
                {Direct3d9CreateDeviceVtableIndex, reinterpret_cast<void*>(&CreateDeviceDetour)},
                {Direct3d9ReleaseVtableIndex, reinterpret_cast<void*>(&Direct3d9ReleaseDetour)},
            });
        if (context == nullptr)
        {
            return false;
        }

        Logf(L"[d3d9] install direct3d proxy complete proxy=0x%p real=0x%p", reinterpret_cast<IDirect3D9*>(context), direct3d);
        return true;
    }

    bool D3d9TextureReplacementHookSet::InstallTextureInstanceHooks(IDirect3DTexture9* texture, IDirect3DDevice9* owner_device)
    {
        if (texture == nullptr || owner_device == nullptr)
        {
            return false;
        }

        Logf(L"[d3d9] install texture instance begin texture=0x%p device=0x%p", texture, owner_device);
        std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
        if (g_direct3d9_texture_hook_by_real.find(texture) != g_direct3d9_texture_hook_by_real.end())
        {
            return true;
        }

        Direct3d9TextureHookContext* const context = CreateOrGetProxyContext(
            g_direct3d9_texture_hook_contexts,
            g_direct3d9_texture_hook_by_real,
            texture,
            Direct3d9TextureVtableSlotCount,
            {
                {Direct3d9TextureGetSurfaceLevelVtableIndex, reinterpret_cast<void*>(&TextureGetSurfaceLevelDetour)},
                {Direct3d9TextureLockRectVtableIndex, reinterpret_cast<void*>(&TextureLockRectDetour)},
                {Direct3d9TextureUnlockRectVtableIndex, reinterpret_cast<void*>(&TextureUnlockRectDetour)},
                {ReleaseVtableIndex, reinterpret_cast<void*>(&TextureReleaseDetour)},
            });
        if (context == nullptr)
        {
            return false;
        }

        Logf(L"[d3d9] install texture proxy complete proxy=0x%p real=0x%p", reinterpret_cast<IDirect3DTexture9*>(context), texture);
        return true;
    }

    bool D3d9TextureReplacementHookSet::InstallDeviceInstanceHooks(IDirect3DDevice9* device)
    {
        if (device == nullptr)
        {
            return false;
        }

        if (!ShouldInstallDeviceHooks())
        {
            return true;
        }

        Logf(L"[d3d9] install device instance begin ptr=0x%p", device);
        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            if (g_direct3d9_device_hook_by_real.contains(device)) { return true; }
        }
        // Query once while the newly observed device is operational, never during lost-device recovery.
        D3DDEVICE_CREATION_PARAMETERS creation{};
        const HRESULT creation_result = device->GetCreationParameters(&creation);
        if (FAILED(creation_result)) {
            Logf(L"[d3d9] device presentation layout query failed hr=0x%08X", static_cast<unsigned>(creation_result));
            return false;
        }
        UINT presentation_parameter_count = 1;
        if ((creation.BehaviorFlags & D3DCREATE_ADAPTERGROUP_DEVICE) != 0) {
            D3DCAPS9 capabilities{};
            const HRESULT caps_result = device->GetDeviceCaps(&capabilities);
            if (FAILED(caps_result) || capabilities.NumberOfAdaptersInGroup == 0) {
                Logf(L"[d3d9] adapter-group presentation layout unavailable hr=0x%08X", static_cast<unsigned>(caps_result));
                return false;
            }
            presentation_parameter_count = capabilities.NumberOfAdaptersInGroup;
        }
        std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
        if (g_direct3d9_device_hook_by_real.find(device) != g_direct3d9_device_hook_by_real.end())
        {
            return true;
        }

        Direct3d9DeviceHookContext* const context = CreateOrGetProxyContext(
            g_direct3d9_device_hook_contexts,
            g_direct3d9_device_hook_by_real,
            device,
            Direct3d9DeviceVtableSlotCount,
            {
                {ReleaseVtableIndex, reinterpret_cast<void*>(&DeviceReleaseDetour)},
                {Direct3d9ResetVtableIndex, reinterpret_cast<void*>(&ResetDetour)},
                {Direct3d9CreateTextureVtableIndex, reinterpret_cast<void*>(&CreateTextureDetour)},
                {Direct3d9UpdateTextureVtableIndex, reinterpret_cast<void*>(&UpdateTextureDetour)},
                {Direct3d9SetTextureVtableIndex, reinterpret_cast<void*>(&SetTextureDetour)},
            });
        if (context == nullptr)
        {
            return false;
        }

        context->PresentationParameterCount = presentation_parameter_count;
        Logf(L"[d3d9] install device proxy complete proxy=0x%p real=0x%p", reinterpret_cast<IDirect3DDevice9*>(context), device);
        return true;
    }

    bool D3d9TextureReplacementHookSet::InstallSurfaceInstanceHooks(
        IDirect3DSurface9* surface,
        IDirect3DTexture9* owner_texture,
        IDirect3DSwapChain9* owner_swap_chain,
        UINT texture_level)
    {
        if (surface == nullptr || owner_texture == nullptr || owner_swap_chain != nullptr)
        {
            return false;
        }

        Logf(L"[d3d9] install surface instance begin surface=0x%p texture=0x%p", surface, owner_texture);
        std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
        if (g_direct3d9_surface_hook_by_real.find(surface) != g_direct3d9_surface_hook_by_real.end())
        {
            return true;
        }

        Direct3d9SurfaceHookContext* const context = CreateOrGetProxyContext(
            g_direct3d9_surface_hook_contexts,
            g_direct3d9_surface_hook_by_real,
            surface,
            Direct3d9SurfaceVtableSlotCount,
            {
                {ReleaseVtableIndex, reinterpret_cast<void*>(&SurfaceReleaseDetour)},
                {3, reinterpret_cast<void*>(&SurfaceGetDeviceDetour)},
                {13, reinterpret_cast<void*>(&SurfaceLockRectDetour)},
                {14, reinterpret_cast<void*>(&SurfaceUnlockRectDetour)},
            });
        if (context == nullptr)
        {
            return false;
        }

        context->OwningTexture = owner_texture;
        context->TextureLevel = texture_level;
        context->OwningSwapChain = owner_swap_chain;

        Logf(L"[d3d9] install surface proxy complete proxy=0x%p real=0x%p", reinterpret_cast<IDirect3DSurface9*>(context), surface);
        return true;
    }

    bool D3d9TextureReplacementHookSet::InstallSwapChainInstanceHooks(IDirect3DSwapChain9* swap_chain, IDirect3DDevice9* owner_device)
    {
        (void)owner_device;
        return swap_chain != nullptr ? false : false;
    }

    HRESULT D3d9TextureReplacementHookSet::CompleteTextureUpload(
        IDirect3DTexture9& texture, const D3d9TextureUpload& upload) const {
        const D3DSURFACE_DESC& description = upload.Description;
        std::string digest;
        HRESULT failure = S_OK;
        if (!TryComputeTextureSha256FromBytes(description, upload.Bytes.data(), upload.Pitch, digest, failure)) {
            Logf(L"[d3d9] upload fingerprint failed texture=0x%p hr=0x%08lX", &texture, static_cast<unsigned long>(failure));
            return FAILED(failure) ? failure : E_FAIL;
        }
        const PackScopedTextureReplacementDefinition* const replacement = FindDeclaredReplacement(description, digest);
        bool log_candidate = false;
        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            TrackedTextureRecord* const record = FindTrackedTextureRecord(&texture);
            if (record == nullptr || record->Identity != upload.Identity) { return D3DERR_INVALIDCALL; }
            record->HasFingerprint = true;
            record->Fingerprint.Width = description.Width;
            record->Fingerprint.Height = description.Height;
            record->Fingerprint.Format = description.Format;
            record->Fingerprint.Hash = digest;
            record->ReplacementMatched = replacement != nullptr;
            log_candidate = !record->FingerprintLogged;
            record->FingerprintLogged = true;
        }
        if (replacement != nullptr) {
            Microsoft::WRL::ComPtr<IDirect3DDevice9> device;
            const HRESULT device_result = texture.GetDevice(device.GetAddressOf());
            if (FAILED(device_result) || !device) { return FAILED(device_result) ? device_result : D3DERR_INVALIDCALL; }
            if (!::TryCacheReplacementTexture(*device.Get(), &texture, description, *replacement, failure, upload.Identity)) {
                Logf(L"[d3d9] upload replacement failed source=0x%p hr=0x%08lX", &texture, static_cast<unsigned long>(failure));
                return FAILED(failure) ? failure : E_FAIL;
            }
        } else if (log_candidate && (enable_hash_logging_ || enable_image_dumping_ || replacements_.empty())) {
            Logf(L"[d3d9] tracked texture fingerprint ptr=0x%p size=%ux%u format=%hs hash=%hs",
                &texture, description.Width, description.Height, GetD3d9FormatToken(description.Format).data(), digest.c_str());
            if (enable_image_dumping_) {
                const std::wstring hash_text(digest.begin(), digest.end());
                const std::string format = SanitizeFilenameToken(GetD3d9FormatToken(description.Format));
                const bool compressed = description.Format == D3DFMT_DXT1 ||
                    description.Format == D3DFMT_DXT3 || description.Format == D3DFMT_DXT5;
                const std::filesystem::path path = texture_dump_directory_ /
                    (L"tracked_" + std::to_wstring(description.Width) + L"x" + std::to_wstring(description.Height) +
                    L"_" + std::wstring(format.begin(), format.end()) + L"_" + hash_text + (compressed ? L".dds" : L".bmp"));
                if (!TryDumpTextureImageFromBytes(description, upload.Bytes.data(), upload.Pitch, path, failure)) {
                    Logf(L"[d3d9] upload dump failed path=%ls hr=0x%08lX", path.c_str(), static_cast<unsigned long>(failure));
                    return FAILED(failure) ? failure : E_FAIL;
                }
            }
        }
        return S_OK;
    }

    bool D3d9TextureReplacementHookSet::ShouldTrackTextures() const noexcept
    {
        return enable_hash_logging_ || enable_image_dumping_ || !replacements_.empty();
    }

    bool D3d9TextureReplacementHookSet::ShouldInstallDeviceHooks() const noexcept
    {
        return enable_hooking_ || ShouldTrackTextures();
    }

    bool D3d9TextureReplacementHookSet::MatchesDeclaredTexture(const D3DSURFACE_DESC& description, std::string_view digest) const
    {
        return FindDeclaredReplacement(description, digest) != nullptr;
    }

    const PackScopedTextureReplacementDefinition* D3d9TextureReplacementHookSet::FindDeclaredReplacement(
        const D3DSURFACE_DESC& description,
        std::string_view digest) const
    {
        if (replacements_.empty())
        {
            return nullptr;
        }

        const std::string_view format_token = GetD3d9FormatToken(description.Format);
        for (const PackScopedTextureReplacementDefinition& replacement : replacements_)
        {
            const TextureReplacementDefinition& definition = replacement.Definition;
            if (!EqualsAsciiIgnoreCase(definition.Api, "d3d9"))
            {
                continue;
            }

            if (definition.Width != description.Width || definition.Height != description.Height)
            {
                continue;
            }

            if (!EqualsAsciiIgnoreCase(definition.Format, format_token))
            {
                continue;
            }

            if (!EqualsAsciiIgnoreCase(definition.Hash, digest))
            {
                continue;
            }

            return &replacement;
        }

        return nullptr;
    }
}
