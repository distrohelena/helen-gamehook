#pragma once

namespace helen {
    /** @brief Terminal outcomes distinguish verified persistence, session-only application and integrity failures. */
    enum class BatmanGraphicsApplyOutcome {
        /** @brief Both requested target byte sequences verified and transaction artifacts cleaned. */
        Committed = 0,
        /** @brief Targets were never published or both original byte sequences were verified restored. */
        NotApplied = 1,
        /** @brief Target consistency could not be established; further Apply must remain locked. */
        IntegrityUncertain = 2,
        /** @brief Requested targets are verified committed, but owned transaction cleanup failed. */
        CommittedCleanupFailed = 3,
        /** @brief Original targets committed, but the active session overlays could not be synchronized. */
        CommittedSessionSyncFailed = 4,
        /** @brief Requested live state and session copy verified; original configuration was not saved. */
        SessionApplied = 5
    };
}
