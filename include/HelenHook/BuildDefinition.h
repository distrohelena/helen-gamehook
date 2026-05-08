#pragma once

#include <string>
#include <vector>

#include <HelenHook/BuildMatchDefinition.h>
#include <HelenHook/CommandDefinition.h>
#include <HelenHook/ExternalBindingDefinition.h>
#include <HelenHook/HookDefinition.h>
#include <HelenHook/MemoryStateObserverDefinition.h>
#include <HelenHook/RuntimeSlotDefinition.h>
#include <HelenHook/TextureReplacementDefinition.h>
#include <HelenHook/VirtualFileDefinition.h>

namespace helen
{
    /**
     * @brief Stores the build-scoped declarations loaded from one split-pack build folder.
     */
    class BuildDefinition
    {
    public:
        /** @brief Stable build identifier that matches the owning build folder name. */
        std::string Id;
        /** @brief Executable fingerprint required for this build to activate. */
        BuildMatchDefinition Match;
        /** @brief Enables the Direct3D 9 texture replacement hook subsystem even when no replacements are declared yet. */
        bool EnableD3d9TextureReplacementHooks{};
        /** @brief Enables expensive live D3D9 texture hashing/logging used only for development-time discovery. */
        bool EnableD3d9TextureHashLogging{};
        /** @brief Enables development-time dumping of lockable live D3D9 textures to image files. */
        bool EnableD3d9TextureImageDumping{};
        /** @brief Command identifiers that must run once after the build command surface is registered. */
        std::vector<std::string> StartupCommandIds;
        /** @brief Canonical relative game paths that the runtime should report as missing when this build is active. */
        std::vector<std::string> MissingPaths;
        /** @brief Virtual file declarations served when this build is active. */
        std::vector<VirtualFileDefinition> VirtualFiles;
        /** @brief External callback bindings available to patched gameplay assets. */
        std::vector<ExternalBindingDefinition> ExternalBindings;
        /** @brief Bounded memory observers that mirror live game state into Helen-owned config and commands. */
        std::vector<MemoryStateObserverDefinition> StateObservers;
        /** @brief Live runtime slots declared by hooks.json for blob relocation and command writes. */
        std::vector<RuntimeSlotDefinition> RuntimeSlots;
        /** @brief Hook and patch declarations resolved against the active executable. */
        std::vector<HookDefinition> Hooks;
        /** @brief Runtime graphics texture replacement declarations loaded from `textures.json`. */
        std::vector<TextureReplacementDefinition> TextureReplacements;
        /** @brief Command workflows available to hooks and future runtime entry points. */
        std::vector<CommandDefinition> Commands;
    };
}
