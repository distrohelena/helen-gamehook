#pragma once

#include <string>
#include <vector>

#include <HelenHook/BuildMatchDefinition.h>
#include <HelenHook/CommandDefinition.h>
#include <HelenHook/ExternalBindingDefinition.h>
#include <HelenHook/HookDefinition.h>
#include <HelenHook/MemoryStateObserverDefinition.h>
#include <HelenHook/RuntimeSlotDefinition.h>
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
        /** @brief Command identifiers that must run once after the build command surface is registered. */
        std::vector<std::string> StartupCommandIds;
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
        /** @brief Command workflows available to hooks and future runtime entry points. */
        std::vector<CommandDefinition> Commands;
    };
}