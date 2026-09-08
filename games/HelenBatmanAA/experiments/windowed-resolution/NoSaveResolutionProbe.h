#pragma once

#include <HelenHook/BatmanGraphicsApplyResult.h>
#include <HelenHook/BatmanGraphicsDraftState.h>

namespace helen {
    /** @brief Throwaway experiment replacing only direct Commit's writer, never part of normal builds. */
    class NoSaveResolutionProbe {
    public:
        /** @brief Attempts one guarded engine size/mode transition per process without calling Helen's INI writer.
         * Returns NotApplied deliberately: the ordinary frontend must not claim a saved baseline.
         * Engine-owned persistence remains enabled so external INI snapshots can measure it.
         */
        static BatmanGraphicsApplyResult Apply(const BatmanGraphicsDraftState& draft);
    };
}
