#pragma once

#include <HelenHook/BatmanDisplayMode.h>
#include <d3d9.h>
#include <vector>

namespace helen {
    /** Reads exclusive modes for the live device's adapter without switching display state. */
    class NoSaveFullscreenModes {
    public:
        /** Enumerates exact mode pairs compatible with the device's current backbuffer format.
         * Discovery failures throw; unsupported format combinations return an empty catalog.
         * The caller must keep the device alive throughout this synchronous read.
         */
        static std::vector<BatmanDisplayMode> Enumerate(IDirect3DDevice9& device);
    };
}
