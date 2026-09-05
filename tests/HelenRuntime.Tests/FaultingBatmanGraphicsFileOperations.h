#pragma once

#include <HelenHook/BatmanGraphicsFileOperations.h>
#include <windows.h>

/** @brief Injects deterministic publication failures while all uninjected operations still modify real temporary files. */
class FaultingBatmanGraphicsFileOperations : public helen::BatmanGraphicsFileOperations {
private:
    /** @brief Whether the launcher publication and subsequent recovery move must fail. */
    bool BreakRecovery;
    /** @brief Whether deletion of owned transaction artifacts must fail after actual publication. */
    bool BreakCleanup;
    /** @brief Counts destructive publication attempts, allowing the first target to be genuinely published. */
    mutable unsigned ReplacementCount = 0;
public:
    /** @brief Selects explicit injected failures; tests control only external filesystem boundaries. */
    FaultingBatmanGraphicsFileOperations(bool break_recovery, bool break_cleanup)
        : BreakRecovery(break_recovery), BreakCleanup(break_cleanup) {
    }
    /** @brief Fails the second replacement when requested; otherwise delegates to actual Win32 replacement. */
    bool Replace(const std::filesystem::path& target, const std::filesystem::path& staged, unsigned long& error) const override {
        ++ReplacementCount;
        if (BreakRecovery && ReplacementCount == 2) {
            error = ERROR_ACCESS_DENIED;
            return false;
        }
        return BatmanGraphicsFileOperations::Replace(target, staged, error);
    }
    /** @brief Prevents recovery movement only in the hard-reconciliation-failure scenario. */
    bool Restore(const std::filesystem::path& target, const std::filesystem::path& staged, unsigned long& error) const override {
        if (BreakRecovery) {
            error = ERROR_ACCESS_DENIED;
            return false;
        }
        return BatmanGraphicsFileOperations::Restore(target, staged, error);
    }
    /** @brief Preserves real owned artifacts when the cleanup-failure scenario is selected. */
    bool Remove(const std::filesystem::path& path) const override {
        return !BreakCleanup && BatmanGraphicsFileOperations::Remove(path);
    }
};
