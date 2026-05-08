#pragma once

#include <memory>
#include <utility>

#include <HelenHook/PackScopedVirtualFileRegistration.h>

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
        /**
         * @brief Creates one registered virtual-file record from one pack-scoped registration and optional shared source.
         * @param registration Pack-scoped registration that owns the declaration and asset resolver.
         * @param shared_source Optional reusable shared source for registrations that can be materialized once.
         */
        RegisteredVirtualFile(
            PackScopedVirtualFileRegistration registration,
            std::shared_ptr<VirtualFileSource> shared_source)
            : Registration(std::move(registration))
            , SharedSource(std::move(shared_source))
        {
        }

        /** @brief Pack-scoped registration used to create sources for this registered virtual file. */
        PackScopedVirtualFileRegistration Registration;

        /** @brief Optional reusable shared source for registrations that can be materialized once at registration time. */
        std::shared_ptr<VirtualFileSource> SharedSource;
    };
}
