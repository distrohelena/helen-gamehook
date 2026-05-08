#pragma once

#include <vector>

#include <HelenHook/LoadedBuildPack.h>

namespace helen
{
    /**
     * @brief Stores one ordered set of loaded build packs selected for the same executable.
     *
     * The pack order is significant and follows the explicit runtime configuration so later runtime
     * bootstrap stages can preserve deterministic command ordering and conflict reporting.
     */
    class LoadedBuildPackSet
    {
    public:
        /** @brief Ordered loaded packs selected for one executable runtime session. */
        std::vector<LoadedBuildPack> Packs;
    };
}
