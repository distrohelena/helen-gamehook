#pragma once

#include <HelenHook/BatmanGraphicsApplyOutcome.h>
#include <filesystem>
#include <utility>
#include <vector>

namespace helen {
    /** @brief Carries an explicit persistence or session-only outcome and retained transaction evidence for diagnosis. */
    struct BatmanGraphicsApplyResult {
        /** @brief Explicit verified outcome; a false boolean cannot represent these distinctions. */
        BatmanGraphicsApplyOutcome Outcome;
        /** @brief Owned transaction paths that may remain after recovery or cleanup failure. */
        std::vector<std::filesystem::path> RecoveryPaths;
        /** @brief Requires a terminal outcome and explicit evidence paths, never inventing successful rollback. */
        BatmanGraphicsApplyResult(BatmanGraphicsApplyOutcome outcome, std::vector<std::filesystem::path> recovery_paths)
            : Outcome(outcome), RecoveryPaths(std::move(recovery_paths)) {
        }
    };
}
