#pragma once

namespace helen {
    /** @brief Session policy for D3D9 presentation; changing it does not initiate a device reset. */
    enum class D3d9VsyncOverride {
        /** @brief Forward the application's interval without alteration. */
        GameControlled,
        /** @brief Request one vertical refresh interval at the next intercepted creation/reset. */
        ForceOn,
        /** @brief Request immediate presentation at the next intercepted creation/reset. */
        ForceOff
    };
}
