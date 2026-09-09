#include <HelenHook/D3d9TextureReplacementHookSet.h>
#include <HelenHook/Log.h>
#include <wrl/client.h>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <type_traits>

#pragma comment(lib, "d3d9.lib")

namespace {
    /** Independent raw DDS payload used to verify actual GPU upload content after every reset. */
    std::vector<std::uint8_t> ExpectedReplacementBytes;
    /** Fails the console fixture without dialogs or an interactive debugger. */
    void Expect(bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    }

    /** Checks real driver results while preserving the exact HRESULT in the console output. */
    void RequireSuccess(HRESULT result, const char* operation) {
        if (FAILED(result)) {
            std::cerr << operation << " hr=0x" << std::hex << static_cast<unsigned long>(result) << std::dec << '\n';
            throw std::runtime_error(operation);
        }
    }

    /** Registers a game-style default-pool surface, then releases all application ownership. */
    void ExposeRenderTargetSurface(IDirect3DDevice9& device) {
        Microsoft::WRL::ComPtr<IDirect3DTexture9> texture;
        RequireSuccess(device.CreateTexture(16, 16, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
            D3DPOOL_DEFAULT, texture.GetAddressOf(), nullptr), "Create game render-target texture");
        Microsoft::WRL::ComPtr<IDirect3DSurface9> surface;
        RequireSuccess(texture->GetSurfaceLevel(0, surface.GetAddressOf()), "Expose render-target surface");
    }

    /** Reproduces gameplay-style DXT1 allocation after surface-last destruction and allocator address reuse. */
    void ExerciseGameplayTextureLifetimes(IDirect3DDevice9& device) {
        for (UINT iteration = 0; iteration < 256; ++iteration) {
            Microsoft::WRL::ComPtr<IDirect3DTexture9> texture;
            RequireSuccess(device.CreateTexture(128, 128, 8, 0, D3DFMT_DXT1, D3DPOOL_MANAGED,
                texture.GetAddressOf(), nullptr), "Create gameplay DXT1 texture after prior surface destruction");
            Expect(texture->GetLevelCount() == 8, "Gameplay texture lost its mip chain.");
            Microsoft::WRL::ComPtr<IDirect3DSurface9> surface;
            const UINT level = iteration % 2 == 0 ? 0 : 3;
            RequireSuccess(texture->GetSurfaceLevel(level, surface.GetAddressOf()), "Retain gameplay mip surface");
            if (level == 3) {
                D3DLOCKED_RECT locked{};
                RequireSuccess(surface->LockRect(&locked, nullptr, 0), "Lock nonzero gameplay mip");
                for (UINT row = 0; row < 4; ++row) {
                    std::memset(static_cast<char*>(locked.pBits) + row * locked.Pitch, 0, 32);
                }
                RequireSuccess(surface->UnlockRect(), "Nonzero mip must not be hashed as level zero");
            }
            texture.Reset();
            surface.Reset();
        }
        std::cout << "D3D9_GAMEPLAY_TEXTURE_LIFETIME_PASS\n";
    }

    /** Verifies substitution through the real SetTexture/GetTexture boundary without retaining a device resource. */
    void ExpectReplacement(IDirect3DDevice9& device, IDirect3DTexture9& source) {
        RequireSuccess(device.SetTexture(0, &source), "Bind managed source");
        Microsoft::WRL::ComPtr<IDirect3DBaseTexture9> bound;
        RequireSuccess(device.GetTexture(0, bound.GetAddressOf()), "Read substituted texture");
        Expect(bound && bound.Get() != &source, "Replacement disappeared after reset.");
        Microsoft::WRL::ComPtr<IDirect3DTexture9> texture;
        RequireSuccess(bound.As(&texture), "Query substituted texture");
        D3DSURFACE_DESC description{};
        RequireSuccess(texture->GetLevelDesc(0, &description), "Read replacement dimensions");
        Expect(description.Width == 1024 && description.Height == 1024, "Replacement dimensions changed.");
        // The cached texture and its internal surface intentionally have no tracking record.
        // Calls through their shared patched tables must still forward normal COM behavior.
        Microsoft::WRL::ComPtr<IDirect3DSurface9> surface;
        RequireSuccess(texture->GetSurfaceLevel(0, surface.GetAddressOf()), "Get untracked replacement surface");
        Microsoft::WRL::ComPtr<IDirect3DDevice9> owner;
        RequireSuccess(surface->GetDevice(owner.GetAddressOf()), "Get untracked surface device");
        Expect(owner.Get() == &device, "Surface GetDevice changed COM identity.");
        D3DLOCKED_RECT readable{};
        RequireSuccess(surface->LockRect(&readable, nullptr, D3DLOCK_READONLY), "Lock untracked replacement surface");
        Expect(readable.pBits != nullptr && readable.Pitch >= 4096, "Replacement surface has no uploaded storage.");
        bool pixels_match = true;
        for (std::size_t row = 0; row < 1024; ++row) {
            if (std::memcmp(static_cast<const char*>(readable.pBits) + row * readable.Pitch,
                ExpectedReplacementBytes.data() + row * 4096, 4096) != 0) { pixels_match = false; }
        }
        RequireSuccess(surface->UnlockRect(), "Unlock untracked replacement surface");
        Expect(pixels_match, "Replacement pixels differ from the actual Batman DDS payload.");
        RequireSuccess(texture->LockRect(0, &readable, nullptr, D3DLOCK_READONLY), "Lock untracked replacement texture");
        RequireSuccess(texture->UnlockRect(0), "Unlock untracked replacement texture");
        RequireSuccess(device.SetTexture(0, nullptr), "Unbind replacement");
    }

    /** @brief Checks actual device state and releases the observation reference before any later reset. */
    void ExpectPresentation(IDirect3DDevice9& device, UINT interval, UINT width, UINT height) {
        Microsoft::WRL::ComPtr<IDirect3DSwapChain9> chain;
        RequireSuccess(device.GetSwapChain(0, chain.GetAddressOf()), "Observe current swap chain");
        D3DPRESENT_PARAMETERS observed{};
        RequireSuccess(chain->GetPresentParameters(&observed), "Observe actual presentation parameters");
        Expect(observed.PresentationInterval == interval, "Actual swap-chain interval did not honor override");
        Expect(observed.Windowed && observed.BackBufferWidth == width && observed.BackBufferHeight == height,
            "Interval override changed requested dimensions or mode");
        Expect(observed.SwapEffect == D3DSWAPEFFECT_COPY && observed.BackBufferFormat == D3DFMT_A8R8G8B8,
            "Interval override changed swap effect or format");
    }

    /** @brief Proves interval interception needs no texture rules, assets, hashing or dumping. */
    void RunPresentationOnly(const std::filesystem::path& root) {
        std::filesystem::create_directories(root);
        helen::SetLogPath(root / "hook.log");
        helen::D3d9TextureReplacementHookSet hooks(true, false, false, root, {});
        hooks.PresentationPolicy().SetVsyncOverride(helen::D3d9VsyncOverride::ForceOff);
        Expect(hooks.Install(), "Pack-free D3D9 hook installation failed");
        const std::unique_ptr<std::remove_pointer_t<HWND>, decltype(&DestroyWindow)> window(
            CreateWindowExW(0, L"STATIC", L"Helen hidden presentation test", WS_OVERLAPPEDWINDOW,
                0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr), &DestroyWindow);
        Expect(window != nullptr, "Could not create pack-free hidden window");
        Microsoft::WRL::ComPtr<IDirect3D9> direct3d;
        direct3d.Attach(Direct3DCreate9(D3D_SDK_VERSION));
        Expect(direct3d != nullptr, "D3D9 unavailable; pack-free test not run");
        D3DPRESENT_PARAMETERS parameters{};
        parameters.Windowed = TRUE; parameters.hDeviceWindow = window.get();
        parameters.BackBufferWidth = 640; parameters.BackBufferHeight = 480;
        parameters.BackBufferFormat = D3DFMT_A8R8G8B8; parameters.BackBufferCount = 1;
        parameters.SwapEffect = D3DSWAPEFFECT_COPY;
        parameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        Microsoft::WRL::ComPtr<IDirect3DDevice9> device;
        RequireSuccess(direct3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window.get(),
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &parameters, device.GetAddressOf()), "Create pack-free device");
        ExpectPresentation(*device.Get(), D3DPRESENT_INTERVAL_IMMEDIATE, 640, 480);
        Expect(helen::D3d9TextureReplacementHookSet::TrySetVsyncOverride(*device.Get(), helen::D3d9VsyncOverride::ForceOn),
            "Registered device could not publish menu VSync override");
        bool invalid_rejected = false;
        try { (void)helen::D3d9TextureReplacementHookSet::TrySetVsyncOverride(*device.Get(), static_cast<helen::D3d9VsyncOverride>(999)); }
        catch (const std::invalid_argument&) { invalid_rejected = true; }
        Expect(invalid_rejected && hooks.PresentationPolicy().GetVsyncOverride() == helen::D3d9VsyncOverride::ForceOn,
            "Invalid device-bound selection changed policy");
        ExpectPresentation(*device.Get(), D3DPRESENT_INTERVAL_IMMEDIATE, 640, 480);
        parameters.BackBufferWidth = 800; parameters.BackBufferHeight = 600;
        parameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        RequireSuccess(device->Reset(&parameters), "Reset pack-free device");
        ExpectPresentation(*device.Get(), D3DPRESENT_INTERVAL_ONE, 800, 600);
        Expect(helen::D3d9TextureReplacementHookSet::TrySetVsyncOverride(*device.Get(), helen::D3d9VsyncOverride::ForceOff),
            "Could not select VSync off for identical-size reset");
        ExpectPresentation(*device.Get(), D3DPRESENT_INTERVAL_ONE, 800, 600);
        parameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        RequireSuccess(device->Reset(&parameters), "Reset identical dimensions with VSync off");
        ExpectPresentation(*device.Get(), D3DPRESENT_INTERVAL_IMMEDIATE, 800, 600);
        hooks.Remove();
        Expect(!helen::D3d9TextureReplacementHookSet::TrySetVsyncOverride(*device.Get(), helen::D3d9VsyncOverride::ForceOn),
            "Retired hook owner accepted menu selection");
        Expect(hooks.PresentationPolicy().GetVsyncOverride() == helen::D3d9VsyncOverride::ForceOff,
            "Rejected device-bound selection changed policy");
        std::cout << "D3D9_PRESENTATION_WITHOUT_TEXTURES_PASS\n";
    }

    /** Runs real hook/reset integration in a dedicated process, preserving a managed source across resets. */
    void Run(const std::filesystem::path& root,
        void (*additional_checks)(IDirect3DDevice9&, const std::filesystem::path&) = nullptr,
        bool early_surface_hooks = false, bool presentation_override = false) {
        std::filesystem::create_directories(root);
        helen::SetLogPath(root / "hook.log");
        const std::filesystem::path subtitle = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
            "games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/assets/textures/subtitle_scaled.dds";
        std::filesystem::copy_file(subtitle, root / "replacement.dds", std::filesystem::copy_options::overwrite_existing);
        std::ifstream payload(root / "replacement.dds", std::ios::binary);
        payload.seekg(128);
        ExpectedReplacementBytes.resize(1024 * 1024 * 4);
        payload.read(reinterpret_cast<char*>(ExpectedReplacementBytes.data()), ExpectedReplacementBytes.size());
        Expect(payload.good(), "Could not read the independent subtitle DDS payload.");
        helen::TextureReplacementDefinition definition;
        definition.Id = "reset-fixture"; definition.Api = "d3d9";
        definition.Width = 4; definition.Height = 4; definition.Format = "A8R8G8B8";
        definition.Hash = "f5a5fd42d16a20302798ef6ed309979b43003d2320d9f0e8ea9831a92759fb4b";
        definition.ReplacementPath = "replacement.dds";
        std::vector<helen::PackScopedTextureReplacementDefinition> replacements;
        replacements.emplace_back("reset-fixture", "test", helen::PackAssetResolver(root, root), definition);
        helen::D3d9TextureReplacementHookSet hooks(true, false, false, root, std::move(replacements));
        Expect(hooks.Install(), "Could not install real D3D9 import hook.");
        const std::unique_ptr<std::remove_pointer_t<HWND>, decltype(&DestroyWindow)> window(
            CreateWindowExW(0, L"STATIC", L"Helen hidden reset test", WS_OVERLAPPEDWINDOW,
                0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr), &DestroyWindow);
        Expect(window != nullptr, "Could not create hidden test window.");
        Microsoft::WRL::ComPtr<IDirect3D9> direct3d;
        direct3d.Attach(Direct3DCreate9(D3D_SDK_VERSION));
        Expect(direct3d != nullptr, "Direct3D9 unavailable; hardware test cannot run.");
        if (presentation_override) {
            D3DCAPS9 capabilities{};
            RequireSuccess(direct3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &capabilities), "Query interval support");
            Expect((capabilities.PresentationIntervals & D3DPRESENT_INTERVAL_ONE) != 0 &&
                (capabilities.PresentationIntervals & D3DPRESENT_INTERVAL_IMMEDIATE) != 0,
                "UNSUPPORTED: driver lacks required presentation intervals; transition not tested");
            hooks.PresentationPolicy().SetVsyncOverride(helen::D3d9VsyncOverride::ForceOff);
        }
        D3DPRESENT_PARAMETERS parameters{};
        parameters.Windowed = TRUE; parameters.hDeviceWindow = window.get();
        parameters.BackBufferWidth = 640; parameters.BackBufferHeight = 480;
        parameters.BackBufferFormat = D3DFMT_A8R8G8B8; parameters.BackBufferCount = 1;
        parameters.SwapEffect = D3DSWAPEFFECT_COPY;
        parameters.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
        if (presentation_override) { parameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE; }
        Microsoft::WRL::ComPtr<IDirect3DDevice9> device;
        RequireSuccess(direct3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window.get(),
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &parameters, device.GetAddressOf()), "Create hidden device");
        if (presentation_override) {
            ExpectPresentation(*device.Get(), D3DPRESENT_INTERVAL_IMMEDIATE, 640, 480);
            hooks.PresentationPolicy().SetVsyncOverride(helen::D3d9VsyncOverride::ForceOn);
            ExpectPresentation(*device.Get(), D3DPRESENT_INTERVAL_IMMEDIATE, 640, 480);
        }
        if (early_surface_hooks) { ExposeRenderTargetSurface(*device.Get()); }
        Microsoft::WRL::ComPtr<IDirect3DTexture9> source;
        RequireSuccess(device->CreateTexture(4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
            source.GetAddressOf(), nullptr), "Create surviving managed source");
        D3DLOCKED_RECT locked{};
        RequireSuccess(source->LockRect(0, &locked, nullptr, 0), "Lock source");
        for (int row = 0; row < 4; ++row) { std::memset(static_cast<char*>(locked.pBits) + row * locked.Pitch, 0, 16); }
        RequireSuccess(source->UnlockRect(0), "Upload source and create replacement");
        ExpectReplacement(*device.Get(), *source.Get());

        // Upload a second matching source while the first replacement is still bound.
        // This catches leaked GetTexture references in the cache's stage-inspection path.
        RequireSuccess(device->SetTexture(0, source.Get()), "Bind first replacement during second upload");
        Microsoft::WRL::ComPtr<IDirect3DTexture9> secondSource;
        RequireSuccess(device->CreateTexture(4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
            secondSource.GetAddressOf(), nullptr), "Create second managed source");
        RequireSuccess(secondSource->LockRect(0, &locked, nullptr, 0), "Lock second source");
        for (int row = 0; row < 4; ++row) { std::memset(static_cast<char*>(locked.pBits) + row * locked.Pitch, 0, 16); }
        RequireSuccess(secondSource->UnlockRect(0), "Upload second source");
        RequireSuccess(device->SetTexture(0, nullptr), "Unbind before reset");
        {
            Microsoft::WRL::ComPtr<IDirect3DSurface9> sourceSurface;
            RequireSuccess(source->GetSurfaceLevel(0, sourceSurface.GetAddressOf()), "Expose game texture surface before reset");
        }
        ExposeRenderTargetSurface(*device.Get());

        parameters.BackBufferWidth = 800; parameters.BackBufferHeight = 600;
        if (presentation_override) { parameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; }
        RequireSuccess(device->Reset(&parameters), "Reset must release the hook-owned default-pool replacement");
        if (presentation_override) {
            ExpectPresentation(*device.Get(), D3DPRESENT_INTERVAL_ONE, 800, 600);
            hooks.PresentationPolicy().SetVsyncOverride(helen::D3d9VsyncOverride::ForceOff);
        }
        ExpectReplacement(*device.Get(), *source.Get());
        ExpectReplacement(*device.Get(), *secondSource.Get());

        // A retained application backbuffer forces genuine Reset failure independently of our replacement.
        Microsoft::WRL::ComPtr<IDirect3DSurface9> retainedBackbuffer;
        RequireSuccess(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, retainedBackbuffer.GetAddressOf()), "Hold backbuffer");
        parameters.BackBufferWidth = 960; parameters.BackBufferHeight = 540;
        if (presentation_override) { parameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE; }
        Expect(device->Reset(&parameters) == D3DERR_INVALIDCALL, "Application-owned backbuffer must still block reset with the original error.");
        retainedBackbuffer.Reset();
        parameters.BackBufferWidth = 960; parameters.BackBufferHeight = 540;
        RequireSuccess(device->Reset(&parameters), "Retry after releasing application resource");
        if (presentation_override) { ExpectPresentation(*device.Get(), D3DPRESENT_INTERVAL_IMMEDIATE, 960, 540); }
        ExpectReplacement(*device.Get(), *source.Get());
        parameters.BackBufferWidth = 640; parameters.BackBufferHeight = 480;
        if (presentation_override) { parameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE; }
        RequireSuccess(device->Reset(&parameters), "Repeated reset after recovery");
        if (presentation_override) {
            ExpectPresentation(*device.Get(), D3DPRESENT_INTERVAL_IMMEDIATE, 640, 480);
            hooks.PresentationPolicy().SetVsyncOverride(helen::D3d9VsyncOverride::GameControlled);
            parameters.BackBufferWidth = 640; parameters.BackBufferHeight = 480;
            parameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
            RequireSuccess(device->Reset(&parameters), "Reset after returning presentation control to application");
            ExpectPresentation(*device.Get(), D3DPRESENT_INTERVAL_ONE, 640, 480);
            std::cout << "D3D9_PRESENTATION_OVERRIDE_PASS\n";
        }
        ExpectReplacement(*device.Get(), *source.Get());
        ExerciseGameplayTextureLifetimes(*device.Get());
        if (additional_checks != nullptr) { additional_checks(*device.Get(), root); }
    }
}

/** Runs this GPU-dependent fixture separately from the portable native suite; never displays a window or crash dialog. */
int wmain(int argc, wchar_t** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    try {
        Expect(argc == 2 || argc == 3, "Expected a fresh fixture directory and optional initial-surface mode.");
        if (argc == 3 && std::wstring_view(argv[2]) == L"presentation-only") {
            RunPresentationOnly(argv[1]);
            return 0;
        }
        const bool presentation_override = argc == 3 && std::wstring_view(argv[2]) == L"presentation-override";
        Run(argv[1], nullptr, argc == 3 && !presentation_override, presentation_override);
        std::cout << "D3D9_REPLACEMENT_RESET_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
