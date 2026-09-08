#include "NoSaveFullscreenModes.h"
#include <stdexcept>
#include <memory>
#include <limits>

namespace {
    /** Releases only the reference acquired for this synchronous display capability query. */
    template<typename T> void ReleaseCom(T* value) { value->Release(); }
}

namespace helen {
    std::vector<BatmanDisplayMode> NoSaveFullscreenModes::Enumerate(IDirect3DDevice9& device) {
        D3DDEVICE_CREATION_PARAMETERS creation;
        if (FAILED(device.GetCreationParameters(&creation))) {
            throw std::runtime_error("Fullscreen adapter identity unavailable");
        }
        IDirect3D9* rawD3d = nullptr;
        if (FAILED(device.GetDirect3D(&rawD3d)) || rawD3d == nullptr) {
            throw std::runtime_error("Fullscreen D3D interface unavailable");
        }
        const std::unique_ptr<IDirect3D9, decltype(&ReleaseCom<IDirect3D9>)> d3d(rawD3d, &ReleaseCom<IDirect3D9>);
        D3DDISPLAYMODE desktop;
        if (FAILED(d3d->GetAdapterDisplayMode(creation.AdapterOrdinal, &desktop))) {
            throw std::runtime_error("Fullscreen adapter display format unavailable");
        }
        IDirect3DSurface9* rawSurface = nullptr;
        if (FAILED(device.GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &rawSurface)) || rawSurface == nullptr) {
            throw std::runtime_error("Fullscreen backbuffer unavailable");
        }
        const std::unique_ptr<IDirect3DSurface9, decltype(&ReleaseCom<IDirect3DSurface9>)> surface(
            rawSurface, &ReleaseCom<IDirect3DSurface9>);
        D3DSURFACE_DESC description;
        if (FAILED(surface->GetDesc(&description))) {
            throw std::runtime_error("Fullscreen backbuffer format unavailable");
        }
        const HRESULT compatibility = d3d->CheckDeviceType(creation.AdapterOrdinal, creation.DeviceType,
            desktop.Format, description.Format, FALSE);
        if (compatibility == D3DERR_NOTAVAILABLE) { return {}; }
        if (FAILED(compatibility)) {
            throw std::runtime_error("Fullscreen format compatibility query failed");
        }
        std::vector<BatmanDisplayMode> modes;
        const UINT count = d3d->GetAdapterModeCount(creation.AdapterOrdinal, desktop.Format);
        for (UINT index = 0; index < count; ++index) {
            D3DDISPLAYMODE mode;
            if (FAILED(d3d->EnumAdapterModes(creation.AdapterOrdinal, desktop.Format, index, &mode))) {
                throw std::runtime_error("Fullscreen adapter enumeration failed");
            }
            if (mode.Width == 0 || mode.Height == 0 || mode.Width > static_cast<UINT>(std::numeric_limits<int>::max()) ||
                mode.Height > static_cast<UINT>(std::numeric_limits<int>::max())) {
                throw std::runtime_error("Fullscreen adapter returned invalid dimensions");
            }
            modes.emplace_back(static_cast<int>(mode.Width), static_cast<int>(mode.Height));
        }
        return modes;
    }
}
