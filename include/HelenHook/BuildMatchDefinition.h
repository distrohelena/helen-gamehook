#pragma once

#include <cstdint>
#include <string>

namespace helen
{
    /**
     * @brief Captures the executable fingerprint required to activate a build-specific pack variant.
     */
    class BuildMatchDefinition
    {
    public:
        /** @brief Executable file name that this build entry targets. */
        std::string ExecutableName;
        /** @brief Exact executable file size required for the build to match. */
        std::uintmax_t FileSize{};
        /** @brief Lowercase SHA-256 hex digest required for the build to match. */
        std::string Sha256;
    };
}