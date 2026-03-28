#pragma once

#include <filesystem>
#include <string>

namespace helen
{
    /**
     * @brief Declares one game-relative file that the runtime may virtualize for a matching build.
     */
    class VirtualFileDefinition
    {
    public:
        /** @brief Stable virtual file identifier used by tooling and diagnostics. */
        std::string Id;
        /** @brief Game-relative file path intercepted by the runtime. */
        std::filesystem::path GamePath;
        /** @brief Virtualization mode such as replace-on-read. */
        std::string Mode;
        /** @brief Pack-relative source asset path that provides the replacement bytes. */
        std::filesystem::path Source;
    };
}