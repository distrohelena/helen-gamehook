#pragma once

namespace helen
{
    /**
     * @brief Selects whether writes to a declared original are denied or session redirected.
     */
    enum class FileWritePolicy
    {
        /** @brief Rejects write-capable opens and destructive creation dispositions. */
        Deny,

        /** @brief Sends write-capable opens to a fresh session-owned file. */
        Redirect,
    };
}
