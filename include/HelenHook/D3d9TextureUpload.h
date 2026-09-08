#pragma once
#include <HelenHook/D3d9ReplacementTicket.h>
#include <d3d9.h>
#include <cstdint>
#include <memory>
#include <vector>

namespace helen {
    /** Owns packed level-zero pixels copied before UnlockRect invalidates the driver buffer. */
    struct D3d9TextureUpload {
        /** Validated source description used to interpret the packed bytes. */
        D3DSURFACE_DESC Description;
        /** Owned rows without driver pitch padding; never points into an unlocked surface. */
        std::vector<std::uint8_t> Bytes;
        /** Positive packed row size, including compressed block-row formats. */
        LONG Pitch;
        /** Record identity prevents an old upload from modifying a reused object address. */
        std::shared_ptr<D3d9ReplacementTicket> Identity;
    };
}
