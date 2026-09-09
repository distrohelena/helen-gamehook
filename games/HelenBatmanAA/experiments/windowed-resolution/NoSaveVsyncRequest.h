#pragma once

#include <HelenHook/BatmanGraphicsDraftState.h>
#include <HelenHook/D3d9VsyncOverride.h>
#include <optional>

namespace helen {
    /** @brief Selects an explicit VSync edit without touching engine or presentation-policy state. */
    class NoSaveVsyncRequest {
    public:
        /**
         * @brief Returns no policy change when VSync is unedited; otherwise returns the requested on/off override.
         * Rejects unrelated mixed edits. Live display validation and a same-size refresh remain the probe's responsibility.
         */
        static std::optional<D3d9VsyncOverride> Select(const BatmanGraphicsDraftState& baseline,
            const BatmanGraphicsDraftState& draft);
    };
}
