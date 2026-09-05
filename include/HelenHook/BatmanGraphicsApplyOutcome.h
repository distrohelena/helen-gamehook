#pragma once

namespace helen {
    /** @brief Terminal outcomes distinguish verified persistence from rollback and cleanup failures. */
    enum class BatmanGraphicsApplyOutcome {
        /** @brief Both requested target byte sequences verified and transaction artifacts cleaned. */
        Committed = 0,
        /** @brief Targets were never published or both original byte sequences were verified restored. */
        NotApplied = 1,
        /** @brief Target consistency could not be established; further Apply must remain locked. */
        IntegrityUncertain = 2,
        /** @brief Requested targets are verified committed, but owned transaction cleanup failed. */
        CommittedCleanupFailed = 3
    };
}
