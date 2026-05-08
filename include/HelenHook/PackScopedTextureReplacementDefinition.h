#pragma once

#include <string>
#include <utility>

#include <HelenHook/PackAssetResolver.h>
#include <HelenHook/TextureReplacementDefinition.h>

namespace helen
{
    /**
     * @brief Stores one texture replacement declaration together with the pack/build context that owns its image asset.
     *
     * Texture replacement matching is global at runtime, but the replacement image still belongs to one
     * specific pack/build pair and must resolve through that pack's asset root.
     */
    class PackScopedTextureReplacementDefinition
    {
    public:
        /**
         * @brief Creates one pack-scoped texture replacement definition with explicit pack/build ownership.
         * @param pack_id Stable pack identifier that declared the texture replacement.
         * @param build_id Stable build identifier inside the owning pack.
         * @param asset_resolver Pack-local asset resolver used to resolve the replacement image safely.
         * @param definition Original texture replacement declaration loaded from the owning build.
         */
        PackScopedTextureReplacementDefinition(
            std::string pack_id,
            std::string build_id,
            PackAssetResolver asset_resolver,
            TextureReplacementDefinition definition)
            : PackId(std::move(pack_id))
            , BuildId(std::move(build_id))
            , AssetResolver(std::move(asset_resolver))
            , Definition(std::move(definition))
        {
        }

        /** @brief Stable pack identifier that declared the texture replacement. */
        std::string PackId;

        /** @brief Stable build identifier inside the owning pack. */
        std::string BuildId;

        /** @brief Pack-local asset resolver used to resolve the replacement image path safely. */
        PackAssetResolver AssetResolver;

        /** @brief Original texture replacement declaration loaded from the owning build. */
        TextureReplacementDefinition Definition;
    };
}
