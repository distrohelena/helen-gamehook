#pragma once

#include <string>
#include <utility>

#include <HelenHook/HookDefinition.h>
#include <HelenHook/PackAssetResolver.h>

namespace helen
{
    /**
     * @brief Stores one native hook declaration together with the pack/build context that owns its blob assets.
     *
     * Blob-backed native hooks must keep pack-local resolver context so each hook payload resolves from
     * the pack that declared it instead of whichever pack happened to initialize first.
     */
    class PackScopedHookDefinition
    {
    public:
        /**
         * @brief Creates one pack-scoped native hook definition with explicit pack/build ownership.
         * @param pack_id Stable pack identifier that declared the hook.
         * @param build_id Stable build identifier inside the owning pack.
         * @param asset_resolver Pack-local asset resolver used to resolve the hook blob safely.
         * @param definition Original hook declaration loaded from the owning build.
         */
        PackScopedHookDefinition(
            std::string pack_id,
            std::string build_id,
            PackAssetResolver asset_resolver,
            HookDefinition definition)
            : PackId(std::move(pack_id))
            , BuildId(std::move(build_id))
            , AssetResolver(std::move(asset_resolver))
            , Definition(std::move(definition))
        {
        }

        /** @brief Stable pack identifier that declared the hook. */
        std::string PackId;

        /** @brief Stable build identifier inside the owning pack. */
        std::string BuildId;

        /** @brief Pack-local asset resolver used to resolve the hook blob path safely. */
        PackAssetResolver AssetResolver;

        /** @brief Original hook declaration loaded from the owning build. */
        HookDefinition Definition;
    };
}
