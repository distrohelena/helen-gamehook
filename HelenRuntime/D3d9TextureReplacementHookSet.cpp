#include <HelenHook/D3d9TextureReplacementHookSet.h>

#include <HelenHook/Log.h>
#include <HelenHook/Memory.h>
#include <HelenHook/TextureDumpSerializer.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <string_view>
#include <utility>
#include <vector>
#include <wincrypt.h>

#pragma comment(lib, "advapi32.lib")

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
    std::unordered_map<void**, std::vector<void*>> g_original_vtable_snapshots;

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
        /** @brief Device that created the tracked texture and owns reset invalidation for it. */
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

        const auto snapshot_iterator = g_original_vtable_snapshots.find(real_vtable);
        if (snapshot_iterator == g_original_vtable_snapshots.end())
        {
            g_original_vtable_snapshots.emplace(real_vtable, std::vector<void*>(real_vtable, real_vtable + slot_count));
        }

        const std::vector<void*>& original_vtable_storage = g_original_vtable_snapshots.find(real_vtable)->second;

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
        const PackAssetResolver& asset_resolver,
        bool enable_hooking,
        bool enable_hash_logging,
        bool enable_image_dumping,
        std::filesystem::path texture_dump_directory,
        const std::vector<TextureReplacementDefinition>& replacements)
        : asset_resolver_(asset_resolver)
        , enable_hooking_(enable_hooking)
        , enable_hash_logging_(enable_hash_logging)
        , enable_image_dumping_(enable_image_dumping)
        , texture_dump_directory_(std::move(texture_dump_directory))
        , replacements_(replacements)
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

        active_instance_ = this;
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
        if (active_instance_ == this)
        {
            active_instance_ = nullptr;
        }

        direct3d_create9_hook_.Remove();
    }

    bool D3d9TextureReplacementHookSet::IsInstalled() const noexcept
    {
        return direct3d_create9_hook_.IsInstalled();
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

    ULONG WINAPI D3d9TextureReplacementHookSet::Direct3d9ReleaseDetour(IDirect3D9* self)
    {
        Direct3d9HookContext* const context = FindDirect3d9HookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return 0;
        }

        const auto original = reinterpret_cast<Direct3d9ReleaseFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9ReleaseVtableIndex));
        if (original == nullptr)
        {
            return 0;
        }

        const ULONG reference_count = original(context->Real);
        if (reference_count == 0)
        {
            IDirect3D9* const real = context->Real;
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            RemoveProxyContext(g_direct3d9_hook_contexts, g_direct3d9_hook_by_real, self);
            g_direct3d9_hook_by_real.erase(real);
        }

        return reference_count;
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
        IDirect3DDevice9* self,
        UINT width,
        UINT height,
        UINT levels,
        DWORD usage,
        D3DFORMAT format,
        D3DPOOL pool,
        IDirect3DTexture9** returned_texture,
        HANDLE* shared_handle)
    {
        D3d9TextureReplacementHookSet* const active = Current();
        if (active == nullptr || returned_texture == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        Direct3d9DeviceHookContext* const device_context = FindDeviceHookContext(self);
        if (device_context == nullptr || device_context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            const auto original_vtable = device_context->OriginalVtable;
            if (original_vtable == nullptr)
            {
                return D3DERR_INVALIDCALL;
            }
        }

        const auto original = reinterpret_cast<Direct3d9CreateTextureFunction>(
            GetVtableSlot(device_context->OriginalVtable, Direct3d9CreateTextureVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        IDirect3DTexture9* real_texture = nullptr;
        const HRESULT result = original(
            device_context->Real,
            width,
            height,
            levels,
            usage,
            format,
            pool,
            &real_texture,
            shared_handle);
        if (FAILED(result) || real_texture == nullptr)
        {
            return result;
        }

        Logf(L"[d3d9] CreateTexture detour begin real_texture=0x%p", real_texture);

        if (!active->InstallTextureInstanceHooks(real_texture, self))
        {
            Logf(L"[d3d9] failed to install shadow hooks for created texture 0x%p.", real_texture);
            *returned_texture = real_texture;
            return result;
        }

        std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
        const auto proxy_iterator = g_direct3d9_texture_hook_by_real.find(real_texture);
        if (proxy_iterator == g_direct3d9_texture_hook_by_real.end())
        {
            Logf(L"[d3d9] texture shadow hook succeeded but no live texture was registered for real texture 0x%p.", real_texture);
            *returned_texture = real_texture;
            return result;
        }

        *returned_texture = proxy_iterator->second;

        D3DSURFACE_DESC description{};
        if (!TryGetTextureDescription(*real_texture, description))
        {
            description.Width = width;
            description.Height = height;
            description.Format = format;
            description.Usage = usage;
            description.Pool = pool;
            description.Type = D3DRTYPE_TEXTURE;
        }

        {
            if (device_context != nullptr)
            {
                device_context->TrackedTextures.insert(*returned_texture);
            }

            TrackedTextureRecord& record = g_tracked_texture_records[*returned_texture];
            record.OwningDevice = self;
            record.Description = description;
            record.HasDescription = true;
            record.Dirty = false;
            record.HasFingerprint = false;
            record.FingerprintLogged = false;
            record.ReplacementMatched = false;
            record.Fingerprint = {};
        }

        if (active->enable_hash_logging_ || active->enable_image_dumping_ || !active->replacements_.empty())
        {
            Logf(
                L"[d3d9] tracked texture created ptr=0x%p size=%ux%u format=%hs pool=%hs usage=0x%08lX levels=%u",
                *returned_texture,
                static_cast<unsigned int>(description.Width),
                static_cast<unsigned int>(description.Height),
                GetD3d9FormatToken(description.Format).data(),
                GetD3d9PoolToken(description.Pool).data(),
                static_cast<unsigned long>(description.Usage),
                static_cast<unsigned int>(levels));
        }

        Logf(L"[d3d9] CreateTexture detour installed texture hook ptr=0x%p real=0x%p", *returned_texture, real_texture);

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

    HRESULT WINAPI D3d9TextureReplacementHookSet::ResetDetour(
        IDirect3DDevice9* self,
        D3DPRESENT_PARAMETERS* presentation_parameters)
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

        const auto original = reinterpret_cast<Direct3d9ResetFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9ResetVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const HRESULT result = original(context->Real, presentation_parameters);
        if (FAILED(result))
        {
            return result;
        }

        if (!active->ShouldTrackTextures())
        {
            return result;
        }

        std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
        for (IDirect3DTexture9* const texture : context->TrackedTextures)
        {
            const auto record_iterator = g_tracked_texture_records.find(texture);
            if (record_iterator == g_tracked_texture_records.end())
            {
                continue;
            }

            record_iterator->second.Dirty = false;
            record_iterator->second.HasFingerprint = false;
            record_iterator->second.FingerprintLogged = false;
            record_iterator->second.ReplacementMatched = false;
            record_iterator->second.Fingerprint.Hash.clear();
        }

        Logf(
            L"[d3d9] reset cleared tracked texture fingerprints for device=0x%p trackedTextures=%zu.",
            self,
            context->TrackedTextures.size());

        if (active->enable_hash_logging_)
        {
            Logf(L"[d3d9] reset complete device=0x%p.", self);
        }

        return result;
    }

#if 0
    HRESULT WINAPI D3d9TextureReplacementHookSet::TextureGetSurfaceLevelDetour(
        IDirect3DTexture9* self,
        UINT level,
        IDirect3DSurface9** returned_surface)
    {
        if (returned_surface == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        Direct3d9TextureHookContext* const context = FindTextureHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9TextureGetSurfaceLevelFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9TextureGetSurfaceLevelVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        IDirect3DSurface9* real_surface = nullptr;
        const HRESULT result = original(context->Real, level, &real_surface);
        if (FAILED(result) || real_surface == nullptr)
        {
            return result;
        }

        D3d9TextureReplacementHookSet* const active = Current();
        if (active == nullptr || !active->InstallSurfaceInstanceHooks(real_surface, self, nullptr))
        {
            *returned_surface = real_surface;
            return result;
        }

        *returned_surface = WrapProxyPointer(g_direct3d9_surface_hook_by_real, real_surface);
        return result;
    }

    #endif

    HRESULT WINAPI D3d9TextureReplacementHookSet::TextureLockRectDetour(
        IDirect3DTexture9* self,
        UINT level,
        D3DLOCKED_RECT* locked_rect,
        const RECT* rect,
        DWORD flags)
    {
        D3d9TextureReplacementHookSet* const active = Current();
        if (active == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        Direct3d9TextureHookContext* const context = FindTextureHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9TextureLockRectFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9TextureLockRectVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const HRESULT result = original(context->Real, level, locked_rect, rect, flags);
        if (FAILED(result) || level != 0 || (flags & D3DLOCK_READONLY) != 0)
        {
            return result;
        }

        std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
        TrackedTextureRecord* const record = FindTrackedTextureRecord(self);
        if (record != nullptr)
        {
            record->Dirty = true;
            record->HasWritableLockSnapshot = false;
            record->WritableLockBytes = nullptr;
            record->WritableLockPitch = 0;

            if (locked_rect != nullptr &&
                locked_rect->pBits != nullptr &&
                locked_rect->Pitch > 0 &&
                rect == nullptr &&
                level == 0)
            {
                record->HasWritableLockSnapshot = true;
                record->WritableLockBytes = static_cast<const std::uint8_t*>(locked_rect->pBits);
                record->WritableLockPitch = locked_rect->Pitch;
            }
        }

        return result;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::TextureUnlockRectDetour(IDirect3DTexture9* self, UINT level)
    {
        D3d9TextureReplacementHookSet* const active = Current();
        if (active == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        Direct3d9TextureHookContext* const context = FindTextureHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9TextureUnlockRectFunction>(
            GetVtableSlot(context->OriginalVtable, Direct3d9TextureUnlockRectVtableIndex));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const std::uint8_t* source_bytes = nullptr;
        LONG source_pitch = 0;
        D3DSURFACE_DESC description{};
        bool should_compute_fingerprint = false;
        bool has_snapshot = false;
        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            const TrackedTextureRecord* const record = FindTrackedTextureRecord(self);
            if (record != nullptr && record->Dirty)
            {
                should_compute_fingerprint =
                    active->enable_hash_logging_ ||
                    active->enable_image_dumping_ ||
                    !active->replacements_.empty();

                if (record->HasDescription)
                {
                    description = record->Description;
                }

                if (record->HasWritableLockSnapshot)
                {
                    source_bytes = record->WritableLockBytes;
                    source_pitch = record->WritableLockPitch;
                    has_snapshot = source_bytes != nullptr && source_pitch > 0;
                }

                TrackedTextureRecord* const mutable_record = FindTrackedTextureRecord(self);
                if (mutable_record != nullptr)
                {
                    mutable_record->Dirty = false;
                    mutable_record->HasWritableLockSnapshot = false;
                    mutable_record->WritableLockBytes = nullptr;
                    mutable_record->WritableLockPitch = 0;
                }
            }
        }

        if (level != 0)
        {
            return original(context->Real, level);
        }

        if (!has_snapshot && !TryGetTextureDescription(*context->Real, description))
        {
            return original(context->Real, level);
        }

        if (!has_snapshot)
        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            TrackedTextureRecord* const record = FindTrackedTextureRecord(self);
            if (record != nullptr)
            {
                if (!record->HasDescription)
                {
                    record->Description = description;
                    record->HasDescription = true;
                }
            }

            Logf(
                L"[d3d9] tracked texture upload observed ptr=0x%p size=%ux%u format=%hs",
                self,
                static_cast<unsigned int>(description.Width),
                static_cast<unsigned int>(description.Height),
                GetD3d9FormatToken(description.Format).data());
            return original(context->Real, level);
        }

        if (!should_compute_fingerprint)
        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            TrackedTextureRecord* const record = FindTrackedTextureRecord(self);
            if (record != nullptr)
            {
                record->HasDescription = true;
                record->Description = description;
            }

            Logf(
                L"[d3d9] tracked texture upload observed ptr=0x%p size=%ux%u format=%hs",
                self,
                static_cast<unsigned int>(description.Width),
                static_cast<unsigned int>(description.Height),
                GetD3d9FormatToken(description.Format).data());
            return original(context->Real, level);
        }

        std::string digest;
        HRESULT hash_result = S_OK;
        const bool hash_succeeded = TryComputeTextureSha256FromBytes(description, source_bytes, source_pitch, digest, hash_result);
        if (!hash_succeeded)
        {
            if (active->enable_hash_logging_ || active->enable_image_dumping_)
            {
                Logf(
                    L"[d3d9] texture fingerprint failed ptr=0x%p size=%ux%u format=%hs hr=0x%08lX",
                    self,
                    static_cast<unsigned int>(description.Width),
                    static_cast<unsigned int>(description.Height),
                    GetD3d9FormatToken(description.Format).data(),
                    static_cast<unsigned long>(hash_result));
            }

            return original(self, level);
        }

        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            TrackedTextureRecord* const record = FindTrackedTextureRecord(self);
            if (record == nullptr)
            {
                return original(context->Real, level);
            }

            record->HasDescription = true;
            record->Description = description;
            record->HasFingerprint = true;
            record->Fingerprint.Width = description.Width;
            record->Fingerprint.Height = description.Height;
            record->Fingerprint.Format = description.Format;
            record->Fingerprint.Hash = digest;
        }

        const bool matches_declared_replacement = active->MatchesDeclaredTexture(description, digest);
        if (matches_declared_replacement)
        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            TrackedTextureRecord* const record = FindTrackedTextureRecord(self);
            if (record != nullptr && !record->ReplacementMatched)
            {
                record->ReplacementMatched = true;
                record->FingerprintLogged = true;
                Logf(
                    L"[d3d9] texture matched declared replacement ptr=0x%p size=%ux%u format=%hs hash=%hs",
                    self,
                    static_cast<unsigned int>(description.Width),
                    static_cast<unsigned int>(description.Height),
                    GetD3d9FormatToken(description.Format).data(),
                    digest.c_str());
            }
        }
        else if (active->enable_hash_logging_ || active->enable_image_dumping_ || active->replacements_.empty())
        {
            bool should_log_candidate = false;
            {
                std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
                TrackedTextureRecord* const candidate_record = FindTrackedTextureRecord(self);
                if (candidate_record != nullptr && !candidate_record->FingerprintLogged)
                {
                    candidate_record->FingerprintLogged = true;
                    should_log_candidate = true;
                }
            }

            if (should_log_candidate)
            {
                Logf(
                    L"[d3d9] tracked texture fingerprint ptr=0x%p size=%ux%u format=%hs hash=%hs pool=%hs usage=0x%08lX",
                    self,
                    static_cast<unsigned int>(description.Width),
                    static_cast<unsigned int>(description.Height),
                    GetD3d9FormatToken(description.Format).data(),
                    digest.c_str(),
                    GetD3d9PoolToken(description.Pool).data(),
                    static_cast<unsigned long>(description.Usage));

                if (active->enable_image_dumping_)
                {
                    const std::string sanitized_format = SanitizeFilenameToken(GetD3d9FormatToken(description.Format));
                    const std::wstring hash_text(digest.begin(), digest.end());
                    const bool compressed_dump =
                        description.Format == D3DFMT_DXT1 ||
                        description.Format == D3DFMT_DXT3 ||
                        description.Format == D3DFMT_DXT5;
                    const std::filesystem::path output_path =
                        active->texture_dump_directory_ /
                        (L"tracked_" + std::to_wstring(description.Width) +
                         L"x" + std::to_wstring(description.Height) +
                         L"_" + std::wstring(sanitized_format.begin(), sanitized_format.end()) +
                         L"_" + hash_text +
                         (compressed_dump ? L".dds" : L".bmp"));

                    HRESULT dump_result = S_OK;
                    if (TryDumpTextureImageFromBytes(description, source_bytes, source_pitch, output_path, dump_result))
                    {
                        Logf(L"[d3d9] dumped tracked texture image ptr=0x%p path=%ls", self, output_path.c_str());
                    }
                    else
                    {
                        Logf(
                            L"[d3d9] texture image dump failed ptr=0x%p format=%hs hr=0x%08lX",
                            self,
                            GetD3d9FormatToken(description.Format).data(),
                            static_cast<unsigned long>(dump_result));
                    }
                }
            }
        }

        if (hash_succeeded)
        {
            Logf(
                L"[d3d9] tracked texture unlock complete ptr=0x%p size=%ux%u format=%hs hash=%hs",
                self,
                static_cast<unsigned int>(description.Width),
                static_cast<unsigned int>(description.Height),
                GetD3d9FormatToken(description.Format).data(),
                digest.c_str());
        }

        return original(context->Real, level);
    }

    ULONG WINAPI D3d9TextureReplacementHookSet::DeviceReleaseDetour(IDirect3DDevice9* self)
    {
        Direct3d9DeviceHookContext* const context = FindDeviceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return 0;
        }

        const auto original = reinterpret_cast<Direct3d9DeviceReleaseFunction>(
            GetVtableSlot(context->OriginalVtable, ReleaseVtableIndex));
        if (original == nullptr)
        {
            return 0;
        }

        const ULONG reference_count = original(context->Real);
        if (reference_count == 0)
        {
            Logf(
                L"[d3d9] device release final ref=0 ptr=0x%p trackedTextures=%zu.",
                self,
                context->TrackedTextures.size());

            IDirect3DDevice9* const real = context->Real;
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            g_direct3d9_device_hook_contexts.erase(self);
            g_direct3d9_device_hook_by_real.erase(real);
        }

        return reference_count;
    }

    ULONG WINAPI D3d9TextureReplacementHookSet::TextureReleaseDetour(IDirect3DTexture9* self)
    {
        Direct3d9TextureHookContext* const context = FindTextureHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return 0;
        }

        const auto original = reinterpret_cast<Direct3d9TextureReleaseFunction>(
            GetVtableSlot(context->OriginalVtable, ReleaseVtableIndex));
        if (original == nullptr)
        {
            return 0;
        }

        IDirect3DDevice9* owning_device = nullptr;
        bool had_tracked_record = false;
        bool had_fingerprint = false;
        bool matched_replacement = false;
        {
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            const auto record_iterator = g_tracked_texture_records.find(self);
            if (record_iterator != g_tracked_texture_records.end())
            {
                owning_device = record_iterator->second.OwningDevice;
                had_tracked_record = true;
                had_fingerprint = record_iterator->second.HasFingerprint;
                matched_replacement = record_iterator->second.ReplacementMatched;
            }
        }

        const ULONG reference_count = original(context->Real);
        if (had_tracked_record)
        {
            Logf(
                L"[d3d9] tracked texture release ptr=0x%p refCount=%lu fingerprint=%ls matched=%ls",
                self,
                static_cast<unsigned long>(reference_count),
                had_fingerprint ? L"true" : L"false",
                matched_replacement ? L"true" : L"false");
        }

        if (reference_count == 0)
        {
            IDirect3DTexture9* const real = context->Real;
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            g_direct3d9_texture_hook_contexts.erase(self);
            g_direct3d9_texture_hook_by_real.erase(real);
            g_tracked_texture_records.erase(self);

            if (owning_device != nullptr)
            {
                const auto device_iterator = g_direct3d9_device_hook_contexts.find(owning_device);
                if (device_iterator != g_direct3d9_device_hook_contexts.end())
                {
                    device_iterator->second->TrackedTextures.erase(self);
                }
            }
        }

        return reference_count;
    }

    #if 0
    ULONG WINAPI D3d9TextureReplacementHookSet::SurfaceReleaseDetour(IDirect3DSurface9* self)
    {
        Direct3d9SurfaceHookContext* const context = FindSurfaceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return 0;
        }

        const auto original = reinterpret_cast<Direct3d9SurfaceReleaseFunction>(
            GetVtableSlot(context->OriginalVtable, ReleaseVtableIndex));
        if (original == nullptr)
        {
            return 0;
        }

        const ULONG reference_count = original(context->Real);
        if (reference_count == 0)
        {
            IDirect3DSurface9* const real = context->Real;
            std::lock_guard<std::mutex> lock(g_d3d9_hook_mutex);
            g_direct3d9_surface_hook_contexts.erase(self);
            g_direct3d9_surface_hook_by_real.erase(real);
        }

        return reference_count;
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::SurfaceGetDeviceDetour(
        IDirect3DSurface9* self,
        IDirect3DDevice9** returned_device)
    {
        if (returned_device == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        Direct3d9SurfaceHookContext* const context = FindSurfaceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9SurfaceGetDeviceFunction>(
            GetVtableSlot(context->OriginalVtable, 3));
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

    HRESULT WINAPI D3d9TextureReplacementHookSet::SurfaceLockRectDetour(
        IDirect3DSurface9* self,
        D3DLOCKED_RECT* locked_rect,
        const RECT* rect,
        DWORD flags)
    {
        Direct3d9SurfaceHookContext* const context = FindSurfaceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9SurfaceLockRectFunction>(
            GetVtableSlot(context->OriginalVtable, 13));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        return original(context->Real, locked_rect, rect, flags);
    }

    HRESULT WINAPI D3d9TextureReplacementHookSet::SurfaceUnlockRectDetour(IDirect3DSurface9* self)
    {
        Direct3d9SurfaceHookContext* const context = FindSurfaceHookContext(self);
        if (context == nullptr || context->Real == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        const auto original = reinterpret_cast<Direct3d9SurfaceUnlockRectFunction>(
            GetVtableSlot(context->OriginalVtable, 14));
        if (original == nullptr)
        {
            return D3DERR_INVALIDCALL;
        }

        return original(context->Real);
    }

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

        for (const TextureReplacementDefinition& replacement : replacements_)
        {
            if (replacement.Id.empty() || replacement.Api.empty())
            {
                Log(L"[d3d9] texture replacement validation failed: id/api must be non-empty.");
                return false;
            }

            if (!EqualsAsciiIgnoreCase(replacement.Api, "d3d9"))
            {
                Logf(
                    L"[d3d9] texture replacement validation failed for id=%hs: unsupported api=%hs.",
                    replacement.Id.c_str(),
                    replacement.Api.c_str());
                return false;
            }

            if (replacement.Width == 0 || replacement.Height == 0 || replacement.Format.empty() || replacement.Hash.empty())
            {
                Logf(
                    L"[d3d9] texture replacement validation failed for id=%hs: match fields must be populated.",
                    replacement.Id.c_str());
                return false;
            }

            if (replacement.ReplacementPath.empty())
            {
                Logf(
                    L"[d3d9] texture replacement validation failed for id=%hs: replacement path must be non-empty.",
                    replacement.Id.c_str());
                return false;
            }

            const std::optional<std::filesystem::path> resolved_path = asset_resolver_.Resolve(replacement.ReplacementPath);
            if (!resolved_path.has_value())
            {
                Logf(
                    L"[d3d9] texture replacement validation failed for id=%hs: replacement path escaped pack root.",
                    replacement.Id.c_str());
                return false;
            }

            if (!std::filesystem::exists(*resolved_path) || !std::filesystem::is_regular_file(*resolved_path))
            {
                Logf(
                    L"[d3d9] texture replacement validation failed for id=%hs: replacement asset is missing (%ls).",
                    replacement.Id.c_str(),
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
            });
        if (context == nullptr)
        {
            return false;
        }

        Logf(L"[d3d9] install device proxy complete proxy=0x%p real=0x%p", reinterpret_cast<IDirect3DDevice9*>(context), device);
        return true;
    }

    bool D3d9TextureReplacementHookSet::InstallSurfaceInstanceHooks(
        IDirect3DSurface9* surface,
        IDirect3DTexture9* owner_texture,
        IDirect3DSwapChain9* owner_swap_chain)
    {
        (void)owner_texture;
        (void)owner_swap_chain;
        return surface != nullptr ? false : false;
    }

    bool D3d9TextureReplacementHookSet::InstallSwapChainInstanceHooks(IDirect3DSwapChain9* swap_chain, IDirect3DDevice9* owner_device)
    {
        (void)owner_device;
        return swap_chain != nullptr ? false : false;
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
        if (replacements_.empty())
        {
            return false;
        }

        const std::string_view format_token = GetD3d9FormatToken(description.Format);
        for (const TextureReplacementDefinition& replacement : replacements_)
        {
            if (!EqualsAsciiIgnoreCase(replacement.Api, "d3d9"))
            {
                continue;
            }

            if (replacement.Width != description.Width || replacement.Height != description.Height)
            {
                continue;
            }

            if (!EqualsAsciiIgnoreCase(replacement.Format, format_token))
            {
                continue;
            }

            if (!EqualsAsciiIgnoreCase(replacement.Hash, digest))
            {
                continue;
            }

            return true;
        }

        return false;
    }
}
