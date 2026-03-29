#pragma once

#include <filesystem>
#include <string>

#include <HelenHook/VirtualFileSourceDefinition.h>

namespace helen
{
    /**
     * @brief Declares one game-relative file that the runtime may virtualize for a matching build.
     *
     * The definition keeps the game-facing path and virtualization mode separate from the source metadata so pack
     * parsing can preserve the exact asset model without embedding source-specific parsing details in serving code.
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

        /** @brief Explicit source metadata that describes the declared replacement asset. */
        VirtualFileSourceDefinition Source;
    };
}
