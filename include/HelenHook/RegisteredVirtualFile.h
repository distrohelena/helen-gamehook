#pragma once

#include <memory>

#include <HelenHook/VirtualFileDefinition.h>

namespace helen
{
    class VirtualFileSource;

    /**
     * @brief Stores one registered virtual-file declaration and any reusable shared source built from it.
     *
     * Full-file registrations can reuse one shared immutable source across every open. Delta-backed registrations keep
     * only the declaration and create a source per opened base file path.
     */
    class RegisteredVirtualFile
    {
    public:
        /** @brief Original build declaration used to create sources for this registered virtual file. */
        VirtualFileDefinition Definition;

        /** @brief Optional reusable shared source for definitions that can be materialized once at registration time. */
        std::shared_ptr<VirtualFileSource> SharedSource;
    };
}
