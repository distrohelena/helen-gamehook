#pragma once

#include <string>
#include <vector>

#include <HelenHook/ConfigEntryDefinition.h>
#include <HelenHook/FeatureDefinition.h>

namespace helen
{
    /**
     * @brief Stores the shared metadata and top-level declarations for one split pack.
     */
    class PackDefinition
    {
    public:
        /** @brief Stable pack identifier used for discovery, diagnostics, and future persistence. */
        std::string Id;
        /** @brief Human-readable pack name shown in tooling and logs. */
        std::string Name;
        /** @brief Descriptive summary explaining the pack's purpose and scope. */
        std::string Description;
        /** @brief Executable file names supported by at least one declared target in the pack. */
        std::vector<std::string> Executables;
        /** @brief Feature declarations surfaced to tooling and command binding layers. */
        std::vector<FeatureDefinition> Features;
        /** @brief Config keys owned by the pack and persisted by the runtime. */
        std::vector<ConfigEntryDefinition> ConfigEntries;
        /** @brief Build identifiers that resolve to build-specific declaration folders. */
        std::vector<std::string> BuildIds;
    };
}