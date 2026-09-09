#pragma once

#include <cstddef>
#include <d3d9.h>
#include <filesystem>
#include <string_view>

#include <vector>

#include <HelenHook/Hook.h>
#include <HelenHook/D3d9PresentationPolicy.h>
#include <HelenHook/PackScopedTextureReplacementDefinition.h>
#include <HelenHook/TextureReplacementDefinition.h>

namespace helen
{
    /** Owned upload snapshot processed after native UnlockRect. */
    struct D3d9TextureUpload;
    /**
     * @brief Installs the first Direct3D 9 interception layer used by texture replacements.
     *
     * The hook set installs a `Direct3DCreate9` import detour and wraps the live `IDirect3DDevice9`
     * and `IDirect3DTexture9` objects that matter for 2D texture observation. Texture-derived
     * `IDirect3DSurface9` objects are also wrapped so uploads that flow through `GetSurfaceLevel`
     * are visible without touching cube, volume, or swap-chain paths.
     */
    class D3d9TextureReplacementHookSet
    {
    private:
        /** @brief Session policy for intercepted creation/reset calls; default preserves application control. */
        D3d9PresentationPolicy PresentationOverrides;

    public:
        /**
         * @brief Binds the hook set to the declared pack-scoped replacement rules.
         * @param replacements Pack-scoped replacement rules that should activate the subsystem.
         */
        D3d9TextureReplacementHookSet(
            bool enable_hooking,
            bool enable_hash_logging,
            bool enable_image_dumping,
            std::filesystem::path texture_dump_directory,
            std::vector<PackScopedTextureReplacementDefinition> replacements);

        /**
         * @brief Removes any installed `D3D9` import hook.
         */
        ~D3d9TextureReplacementHookSet();

        D3d9TextureReplacementHookSet(const D3d9TextureReplacementHookSet&) = delete;
        D3d9TextureReplacementHookSet& operator=(const D3d9TextureReplacementHookSet&) = delete;
        D3d9TextureReplacementHookSet(D3d9TextureReplacementHookSet&&) = delete;
        D3d9TextureReplacementHookSet& operator=(D3d9TextureReplacementHookSet&&) = delete;

        /**
         * @brief Returns the session policy for all devices intercepted by this hook set.
         * Requires the hook set to outlive callers; changing policy neither installs hooks nor resets devices.
         * Enable hooking explicitly even when no texture replacements are needed.
         */
        D3d9PresentationPolicy& PresentationPolicy() noexcept { return PresentationOverrides; }

        /**
         * @brief Publishes a policy only for a registered, idle device while retaining the active hook owner under its registry lock.
         * Returns false if no active owner/device exists or reset is in progress; makes no COM calls or reset.
         * Unknown modes throw without mutation. The caller must retain the supplied device for this call.
         */
        static bool TrySetVsyncOverride(IDirect3DDevice9& device, D3d9VsyncOverride mode);

        /**
         * @brief Validates declared replacement assets and installs the `Direct3DCreate9` import hook when needed.
         * @return True when no `D3D9` replacements are declared or when validation and hook installation succeed; otherwise false.
         */
        bool Install();

        /**
         * @brief Removes the installed import hook and clears active singleton state.
         */
        void Remove();

        /**
         * @brief Returns whether the `Direct3DCreate9` import hook is currently active.
         * @return True when the hook set owns an active import hook.
         */
        bool IsInstalled() const noexcept;

    private:
        /**
         * @brief Returns the currently active hook set instance used by the static detour.
         * @return Active hook set instance or `nullptr` when the subsystem is not installed.
         */
        static D3d9TextureReplacementHookSet* Current() noexcept;

        /**
         * @brief Import-table detour for `Direct3DCreate9` that logs live `D3D9` activation.
         * @param sdk_version Requested Direct3D SDK version.
         * @return The real `IDirect3D9` interface returned by the original import.
         */
        static IDirect3D9* WINAPI Direct3DCreate9Detour(UINT sdk_version);

        /**
         * @brief Per-instance `IDirect3D9::CreateDevice` detour that wraps the returned device in a proxy object.
         * @return Result of the original device creation call.
         */
        static HRESULT WINAPI CreateDeviceDetour(
            IDirect3D9* self,
            UINT adapter,
            D3DDEVTYPE device_type,
            HWND focus_window,
            DWORD behavior_flags,
            D3DPRESENT_PARAMETERS* presentation_parameters,
            IDirect3DDevice9** returned_device);

        /**
         * @brief Per-instance `IDirect3D9::Release` detour that removes the wrapped proxy on final release.
         * @param self Live `IDirect3D9` instance whose reference count is being decremented.
         * @return Updated COM reference count returned by the original `Release`.
         */
        static ULONG WINAPI Direct3d9ReleaseDetour(IDirect3D9* self);

        /**
         * @brief Per-instance `IDirect3DDevice9::CreateTexture` detour that wraps the returned texture and records lifecycle state.
         * @return Result of the original `CreateTexture` call.
         */
        static HRESULT WINAPI CreateTextureDetour(
            IDirect3DDevice9* self,
            UINT width,
            UINT height,
            UINT levels,
            DWORD usage,
            D3DFORMAT format,
            D3DPOOL pool,
            IDirect3DTexture9** returned_texture,
            HANDLE* shared_handle);

        /**
         * @brief Per-instance `IDirect3DDevice9::UpdateTexture` detour that propagates tracked fingerprints between wrapped textures.
         * @return Result of the original `UpdateTexture` call.
         */
        static HRESULT WINAPI UpdateTextureDetour(
            IDirect3DDevice9* self,
            IDirect3DBaseTexture9* source_texture,
            IDirect3DBaseTexture9* destination_texture);

        /**
         * @brief Per-instance `IDirect3DDevice9::SetTexture` detour that substitutes a higher-resolution replacement texture when one is cached.
         * @return Result of the original `SetTexture` call.
         */
        static HRESULT WINAPI SetTextureDetour(
            IDirect3DDevice9* self,
            DWORD stage,
            IDirect3DBaseTexture9* texture);

        /**
         * @brief Releases owned replacements before reset and restores surviving matches only after success.
         * Failed resets retain match information for the next engine-driven reset attempt.
         * @return Original reset failure, replacement restoration failure, or the successful reset result.
         */
        static HRESULT WINAPI ResetDetour(
            IDirect3DDevice9* self,
            D3DPRESENT_PARAMETERS* presentation_parameters);

        /**
         * @brief Per-instance `IDirect3DTexture9::LockRect` detour that marks a tracked texture as dirty before upload.
         * @return Result of the original `LockRect` call.
         */
        static HRESULT WINAPI TextureLockRectDetour(
            IDirect3DTexture9* self,
            UINT level,
            D3DLOCKED_RECT* locked_rect,
            const RECT* rect,
            DWORD flags);

        /**
         * @brief Per-instance `IDirect3DTexture9::UnlockRect` detour that fingerprints textures after uploads complete.
         * @return Result of the original `UnlockRect` call.
         */
        static HRESULT WINAPI TextureUnlockRectDetour(IDirect3DTexture9* self, UINT level);

        /**
         * @brief Per-instance `IDirect3DTexture9::GetSurfaceLevel` detour that wraps texture-owned level surfaces.
         * @return Result of the original `GetSurfaceLevel` call.
         */
        static HRESULT WINAPI TextureGetSurfaceLevelDetour(
            IDirect3DTexture9* self,
            UINT level,
            IDirect3DSurface9** returned_surface);

        /**
         * @brief Per-instance `IDirect3DDevice9::Release` detour that removes the wrapped proxy on final release.
         * @param self Live `IDirect3DDevice9` instance whose reference count is being decremented.
         * @return Updated COM reference count returned by the original `Release`.
         */
        static ULONG WINAPI DeviceReleaseDetour(IDirect3DDevice9* self);

        /**
         * @brief Per-instance `IDirect3DTexture9::Release` detour that removes tracked texture state on final release.
         * @param self Live `IDirect3DTexture9` instance whose reference count is being decremented.
         * @return Updated COM reference count returned by the original `Release`.
         */
        static ULONG WINAPI TextureReleaseDetour(IDirect3DTexture9* self);

        /**
         * @brief Per-instance `IDirect3DSurface9::Release` detour that removes tracked texture-surface state on final release.
         * @param self Live `IDirect3DSurface9` instance whose reference count is being decremented.
         * @return Updated COM reference count returned by the original `Release`.
         */
        static ULONG WINAPI SurfaceReleaseDetour(IDirect3DSurface9* self);

        /**
         * @brief Per-instance `IDirect3DSurface9::GetDevice` detour that preserves the wrapped device identity.
         * @param self Live `IDirect3DSurface9` instance whose owning device is being queried.
         * @param returned_device Receives the live device pointer returned by the runtime.
         * @return Result of the original `GetDevice` call.
         */
        static HRESULT WINAPI SurfaceGetDeviceDetour(
            IDirect3DSurface9* self,
            IDirect3DDevice9** returned_device);

        /**
         * @brief Per-instance `IDirect3DSurface9::LockRect` detour that marks a texture-owned surface as dirty.
         * @return Result of the original `LockRect` call.
         */
        static HRESULT WINAPI SurfaceLockRectDetour(
            IDirect3DSurface9* self,
            D3DLOCKED_RECT* locked_rect,
            const RECT* rect,
            DWORD flags);

        /**
         * @brief Per-instance `IDirect3DSurface9::UnlockRect` detour that fingerprints texture-owned uploads.
         * @return Result of the original `UnlockRect` call.
         */
        static HRESULT WINAPI SurfaceUnlockRectDetour(IDirect3DSurface9* self);

        /**
         * @brief Validates declared replacement entries and counts the subset that targets `D3D9`.
         * @param d3d9_replacement_count Receives the number of validated `D3D9` replacements.
         * @return True when every declared replacement is valid; otherwise false.
         */
        bool ValidateDeclaredReplacements(std::size_t& d3d9_replacement_count) const;

        /**
         * @brief Installs one wrapped-proxy `CreateDevice` detour on the supplied `IDirect3D9` instance.
         * @param direct3d Live `IDirect3D9` interface returned by the original API.
         * @return True when the instance is already instrumented or when its proxy installs successfully.
         */
        bool InstallDirect3d9InstanceHooks(IDirect3D9* direct3d);

        /**
         * @brief Installs one wrapped-proxy texture lifecycle detour on the supplied `IDirect3DTexture9` instance.
         * @param texture Live `IDirect3DTexture9` interface returned by `CreateTexture`.
         * @param owner_device Live `IDirect3DDevice9` instance that created the texture.
         * @return True when the instance is already instrumented or when its proxy installs successfully.
         */
        bool InstallTextureInstanceHooks(IDirect3DTexture9* texture, IDirect3DDevice9* owner_device);

        /**
         * @brief Installs one wrapped-proxy device lifecycle detour on the supplied `IDirect3DDevice9` instance.
         * @param device Live `IDirect3DDevice9` instance returned by `CreateDevice`.
         * @return True when the instance is already instrumented or when its proxy installs successfully.
         */
        bool InstallDeviceInstanceHooks(IDirect3DDevice9* device);

        /**
         * @brief Tracks texture-owned surfaces at every mip level for correct parent destruction.
         *
         * All texture mip surfaces participate in lifetime tracking; only level zero supplies
         * replacement pixels. Back buffers and standalone device-owned surfaces are not registered.
         *
         * @param surface Live `IDirect3DSurface9` interface returned by the device or swap chain.
         * @param owner_texture Owning texture when the surface came from a texture level.
         * @param owner_swap_chain Owning swap chain when the surface came from a back buffer.
         * @param texture_level Mip level supplied by GetSurfaceLevel; zero for legacy level-zero callers.
         * @return True when a texture-owned surface proxy is installed; otherwise false.
         */
        bool InstallSurfaceInstanceHooks(IDirect3DSurface9* surface, IDirect3DTexture9* owner_texture,
            IDirect3DSwapChain9* owner_swap_chain, UINT texture_level = 0);

        /**
         * @brief Observes a live `IDirect3DSwapChain9` return without installing swap-chain wrapping.
         *
         * Swap-chain wrapping is intentionally disabled in this build so the runtime only tracks
         * device and texture lifecycles. Returning `false` keeps the original swap chain object.
         *
         * @param swap_chain Live `IDirect3DSwapChain9` interface returned by the device.
         * @param owner_device Owning device that created the swap chain.
         * @return False so callers keep the original swap chain without proxy wrapping.
         */
        bool InstallSwapChainInstanceHooks(IDirect3DSwapChain9* swap_chain, IDirect3DDevice9* owner_device);


        /** Processes owned upload bytes after native unlock, preserving source/device lifetimes through the caller. */
        HRESULT CompleteTextureUpload(IDirect3DTexture9& texture, const D3d9TextureUpload& upload) const;

        /**
         * @brief Returns whether one tracked texture fingerprint matches any declared `D3D9` replacement rule.
         * @param description Level-0 texture description for the tracked texture.
         * @param digest Lowercase SHA-256 digest computed from the tracked texture bytes.
         * @return True when at least one declared replacement matches the tracked texture.
         */
        bool MatchesDeclaredTexture(const D3DSURFACE_DESC& description, std::string_view digest) const;

        /**
         * @brief Returns the first declared replacement that matches one tracked texture fingerprint.
         * @param description Level-0 texture description for the tracked texture.
         * @param digest Lowercase SHA-256 digest computed from the tracked texture bytes.
         * @return Pointer to the matching pack-scoped replacement definition, or nullptr when no replacement matches.
         */
        const PackScopedTextureReplacementDefinition* FindDeclaredReplacement(
            const D3DSURFACE_DESC& description,
            std::string_view digest) const;

        /**
         * @brief Returns whether the subsystem should attach lifecycle hooks to live `D3D9` textures.
         *
         * The import hook can still exist without live texture tracking, but per-texture wrapping
         * should only activate when hashing, dumping, or declared replacement rules actually need
         * access to upload lifecycle events.
         *
         * @return True when replacement rules or development diagnostics require live texture tracking.
         */
        bool ShouldTrackTextures() const noexcept;

        /**
         * @brief Returns whether the subsystem should install device-level `D3D9` detours.
         *
         * Device hooks are the minimal import-boundary layer used to observe `CreateDevice`,
         * `CreateTexture`, `UpdateTexture`, and `Reset`. They are safe to install even when
         * texture-object wrapping stays disabled.
         *
         * @return True when the import hook should attach device lifecycle detours.
         */
        bool ShouldInstallDeviceHooks() const noexcept;

        /** @brief Enables the D3D9 hook subsystem even when no replacement entries are declared yet. */
        bool enable_hooking_;
        /** @brief Enables expensive development-time texture hashing/logging for live rule discovery. */
        bool enable_hash_logging_;
        /** @brief Enables development-time dumping of lockable textures to image files. */
        bool enable_image_dumping_;
        /** @brief Output directory used for development-time texture dumps when image dumping is enabled. */
        std::filesystem::path texture_dump_directory_;
        /** @brief Pack-scoped replacement entries declared through merged `textures.json` manifests. */
        std::vector<PackScopedTextureReplacementDefinition> replacements_;
        /** @brief Import-table hook that replaces `d3d9.dll!Direct3DCreate9` in the main executable. */
        IatHook direct3d_create9_hook_;
        /** @brief Singleton-style active hook set used by the static detour function. */
        static D3d9TextureReplacementHookSet* active_instance_;
    };
}
