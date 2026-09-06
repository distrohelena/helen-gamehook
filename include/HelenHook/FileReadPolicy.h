#pragma once

namespace helen
{
    /**
     * @brief Selects the source used by later reads of a declared original.
     */
    enum class FileReadPolicy
    {
        /** @brief Reads the current original file on disk. */
        Original,

        /** @brief Reads the session-owned redirected copy. */
        Redirected,
    };
}
