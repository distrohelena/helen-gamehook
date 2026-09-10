#pragma once

namespace helen {
    /** @brief Performs checked experimental updates of the engine-owned PhysX scalar, without SDK reinitialization. */
    class SessionPhysxLevel {
    public:
        /** @brief Rejects invalid/stale levels before writing the required live field; callers must ensure engine lifetime and game-thread ownership. */
        static void Apply(int& current, int expected, int selected);
    };
}
