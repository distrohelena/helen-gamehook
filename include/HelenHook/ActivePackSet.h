#pragma once

#include <string>
#include <vector>

#include <HelenHook/CommandDefinition.h>
#include <HelenHook/ConfigEntryDefinition.h>
#include <HelenHook/ExternalBindingDefinition.h>
#include <HelenHook/FeatureDefinition.h>
#include <HelenHook/LoadedBuildPack.h>
#include <HelenHook/MemoryStateObserverDefinition.h>
#include <HelenHook/PackScopedHookDefinition.h>
#include <HelenHook/PackScopedTextureReplacementDefinition.h>
#include <HelenHook/PackScopedVirtualFileRegistration.h>
#include <HelenHook/RuntimeSlotDefinition.h>

namespace helen
{
    /**
     * @brief Captures one fully validated multi-pack runtime view for a single executable session.
     *
     * The active pack set is the merged source of truth consumed by runtime services after pack loading.
     * It preserves ordered startup commands, additive hidden paths, shared declaration registries, and
     * pack-scoped asset-backed declarations for systems that must resolve files from different pack roots.
     */
    class ActivePackSet
    {
    public:
        /** @brief Ordered loaded packs selected for the current executable session. */
        std::vector<LoadedBuildPack> LoadedPacks;

        /** @brief Unified config entries registered into the runtime command surface. */
        std::vector<ConfigEntryDefinition> ConfigEntries;

        /** @brief Unified editor-visible feature declarations exposed by the active pack set. */
        std::vector<FeatureDefinition> Features;

        /** @brief Unified command definitions available to runtime callbacks and startup sequencing. */
        std::vector<CommandDefinition> Commands;

        /** @brief Unified runtime slot declarations used by native hook blobs. */
        std::vector<RuntimeSlotDefinition> RuntimeSlots;

        /** @brief Unified external callback bindings used by patched gameplay and frontend assets. */
        std::vector<ExternalBindingDefinition> ExternalBindings;

        /** @brief Unified state observers mirrored into config and command execution. */
        std::vector<MemoryStateObserverDefinition> StateObservers;

        /** @brief Ordered startup command identifiers concatenated in enabled-pack order. */
        std::vector<std::string> StartupCommandIds;

        /** @brief Deduplicated hidden game-relative paths declared across every enabled pack. */
        std::vector<std::string> MissingPaths;

        /** @brief Pack-scoped virtual file registrations keyed later by virtual file service registration. */
        std::vector<PackScopedVirtualFileRegistration> VirtualFiles;

        /** @brief Pack-scoped native hook definitions installed by the shared hook installer. */
        std::vector<PackScopedHookDefinition> Hooks;

        /** @brief Pack-scoped texture replacement definitions consumed by the D3D9 hook set. */
        std::vector<PackScopedTextureReplacementDefinition> TextureReplacements;

        /** @brief True when any enabled pack requires D3D9 texture replacement hook installation. */
        bool EnableD3d9TextureReplacementHooks = false;

        /** @brief True when any enabled pack requires D3D9 texture hash logging. */
        bool EnableD3d9TextureHashLogging = false;

        /** @brief True when any enabled pack requires D3D9 texture dump output. */
        bool EnableD3d9TextureImageDumping = false;
    };
}
