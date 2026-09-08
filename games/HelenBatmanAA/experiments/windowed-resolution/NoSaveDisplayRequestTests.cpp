#include "NoSaveDisplayRequest.h"
#include "NoSaveFullscreenModes.h"
#include <windows.h>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <memory>

namespace {
    /** Releases a test-owned COM reference without leaving a live device behind. */
    template<typename T> void ReleaseCom(T* value) { value->Release(); }

    /** Destroys only the hidden test window; no Batman windows are touched. */
    void DestroyTestWindow(HWND window) { DestroyWindow(window); }

    /** Reports contract violations without assertion dialogs. */
    void Expect(bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    }

    /** Requires invalid input to be rejected before an engine invocation can be prepared. */
    void ExpectRejected(int width, int height, int fullscreen, unsigned currentWidth,
        unsigned currentHeight, bool currentFullscreen, const std::vector<helen::BatmanDisplayMode>& modes) {
        bool rejected = false;
        try {
            helen::NoSaveDisplayRequest request(width, height, fullscreen, currentWidth, currentHeight, currentFullscreen, modes);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        Expect(rejected, "Invalid or unchanged display request was accepted");
    }
}

/** Tests real request validation without invoking Batman or changing the desktop display mode. */
int wmain() {
    SetErrorMode(0x8003);
    try {
        const std::unique_ptr<HWND__, decltype(&DestroyTestWindow)> window(
            CreateWindowExW(0, L"STATIC", L"Fullscreen catalog test", WS_OVERLAPPEDWINDOW,
                0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr), &DestroyTestWindow);
        Expect(window != nullptr, "Hidden window creation failed");
        const std::unique_ptr<IDirect3D9, decltype(&ReleaseCom<IDirect3D9>)> d3d(
            Direct3DCreate9(D3D_SDK_VERSION), &ReleaseCom<IDirect3D9>);
        Expect(d3d != nullptr, "D3D9 unavailable");
        D3DPRESENT_PARAMETERS parameters{};
        parameters.Windowed = TRUE;
        parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
        parameters.hDeviceWindow = window.get();
        parameters.BackBufferWidth = 64;
        parameters.BackBufferHeight = 64;
        IDirect3DDevice9* rawDevice = nullptr;
        Expect(SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window.get(),
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &parameters, &rawDevice)), "Hidden windowed device creation failed");
        const std::unique_ptr<IDirect3DDevice9, decltype(&ReleaseCom<IDirect3DDevice9>)> device(
            rawDevice, &ReleaseCom<IDirect3DDevice9>);
        const std::vector<helen::BatmanDisplayMode> actualModes = helen::NoSaveFullscreenModes::Enumerate(*device);
        Expect(!actualModes.empty(), "Real active adapter returned no compatible fullscreen modes");
        D3DDISPLAYMODE desktop;
        Expect(SUCCEEDED(d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &desktop)), "Desktop mode unavailable");
        const UINT count = d3d->GetAdapterModeCount(D3DADAPTER_DEFAULT, desktop.Format);
        for (const helen::BatmanDisplayMode& actual : actualModes) {
            bool found = false;
            for (UINT index = 0; index < count; ++index) {
                D3DDISPLAYMODE mode;
                Expect(SUCCEEDED(d3d->EnumAdapterModes(D3DADAPTER_DEFAULT, desktop.Format, index, &mode)), "Driver enumeration failed");
                if (mode.Width == static_cast<UINT>(actual.GetWidth()) && mode.Height == static_cast<UINT>(actual.GetHeight())) {
                    found = true;
                }
            }
            Expect(found, "Fullscreen catalog invented a driver mode");
        }
        Expect(!IsWindowVisible(window.get()), "Catalog test made its window visible");
        IDirect3DSwapChain9* rawSwapChain = nullptr;
        Expect(SUCCEEDED(device->GetSwapChain(0, &rawSwapChain)), "Fixture swap chain unavailable");
        const std::unique_ptr<IDirect3DSwapChain9, decltype(&ReleaseCom<IDirect3DSwapChain9>)> swapChain(
            rawSwapChain, &ReleaseCom<IDirect3DSwapChain9>);
        D3DPRESENT_PARAMETERS after;
        Expect(SUCCEEDED(swapChain->GetPresentParameters(&after)) && after.Windowed &&
            after.BackBufferWidth == 64 && after.BackBufferHeight == 64,
            "Fullscreen capability query changed the windowed device state");
        const std::vector<helen::BatmanDisplayMode> modes{{1920, 1080}, {2560, 1440}};
        const helen::NoSaveDisplayRequest enter(1920, 1080, 1, 1920, 1080, false, modes);
        Expect(enter.GetFullscreen() == 1 && enter.GetWidth() == 1920 && enter.GetHeight() == 1080,
            "Mode-only fullscreen entry lost its engine arguments");
        const helen::NoSaveDisplayRequest leave(1920, 1080, 0, 1920, 1080, true, {});
        Expect(leave.GetFullscreen() == 0, "Mode-only fullscreen exit was not windowed");
        const helen::NoSaveDisplayRequest resize(1600, 900, 0, 1920, 1080, false, {});
        Expect(resize.GetWidth() == 1600 && resize.GetHeight() == 900, "Windowed resize changed");
        const helen::NoSaveDisplayRequest fullscreenResize(2560, 1440, 1, 1920, 1080, true, modes);
        Expect(fullscreenResize.GetFullscreen() == 1, "Fullscreen resize lost mode");
        ExpectRejected(1920, 1080, 0, 1920, 1080, false, modes);
        ExpectRejected(1920, 1080, 1, 1920, 1080, true, modes);
        ExpectRejected(1920, 1440, 1, 1920, 1080, false, modes);
        ExpectRejected(1600, 900, 1, 1920, 1080, false, modes);
        ExpectRejected(1920, 1080, 1, 1920, 1080, false, {});
        ExpectRejected(0, 1080, 0, 1920, 1080, false, modes);
        ExpectRejected(1920, -1, 0, 1920, 1080, false, modes);
        ExpectRejected(1920, 1080, 2, 1920, 1080, false, modes);
        ExpectRejected(1920, 1080, -1, 1920, 1080, false, modes);
        ExpectRejected(1920, 1080, 1, 0, 1080, false, modes);
        std::cout << "NO_SAVE_DISPLAY_REQUEST_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
