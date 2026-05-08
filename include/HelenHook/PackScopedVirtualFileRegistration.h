#pragma once

#include <string>
#include <utility>

#include <HelenHook/PackAssetResolver.h>
#include <HelenHook/VirtualFileDefinition.h>

namespace helen
{
    /**
     * @brief Stores one virtual-file declaration together with the pack/build context that owns its assets.
     *
     * Multi-pack runtime loading cannot treat virtual files as raw path declarations only because each
     * backing source asset must resolve relative to the pack and build that declared it.
     */
    class PackScopedVirtualFileRegistration
    {
    public:
        /**
         * @brief Creates one pack-scoped virtual file registration with explicit pack/build ownership.
         * @param pack_id Stable pack identifier that declared the virtual file.
         * @param build_id Stable build identifier inside the owning pack.
         * @param asset_resolver Pack-local asset resolver used to resolve the source path safely.
         * @param definition Original virtual-file declaration loaded from the owning build.
         */
        PackScopedVirtualFileRegistration(
            std::string pack_id,
            std::string build_id,
            PackAssetResolver asset_resolver,
            VirtualFileDefinition definition)
            : PackId(std::move(pack_id))
            , BuildId(std::move(build_id))
            , AssetResolver(std::move(asset_resolver))
            , Definition(std::move(definition))
        {
        }

        /** @brief Stable pack identifier that declared the virtual file. */
        std::string PackId;

        /** @brief Stable build identifier inside the owning pack. */
        std::string BuildId;

        /** @brief Pack-local asset resolver used to resolve the virtual file source path safely. */
        PackAssetResolver AssetResolver;

        /** @brief Original virtual-file declaration loaded from the owning build. */
        VirtualFileDefinition Definition;
    };
}
