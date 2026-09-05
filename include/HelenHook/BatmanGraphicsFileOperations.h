#pragma once

#include <filesystem>

namespace helen {
    /** @brief Performs the three destructive filesystem boundaries of graphics publication and recovery. */
    class BatmanGraphicsFileOperations {
    public:
        /** @brief Allows alternate filesystem implementations to be destroyed through the required service reference type. */
        virtual ~BatmanGraphicsFileOperations() = default;
        /** @brief Returns the process-lifetime Win32 implementation used by normal runtime construction. */
        static BatmanGraphicsFileOperations& Native();
        /** @brief Replaces an existing target with its staged sibling; failure requires caller reconciliation. */
        virtual bool Replace(const std::filesystem::path& target, const std::filesystem::path& staged, unsigned long& error) const;
        /** @brief Moves a recovery candidate over an existing or missing target; caller verifies exact bytes afterward. */
        virtual bool Restore(const std::filesystem::path& target, const std::filesystem::path& candidate, unsigned long& error) const;
        /** @brief Removes one explicitly owned transaction artifact; an already absent file counts as removed. */
        virtual bool Remove(const std::filesystem::path& path) const;
    };
}
