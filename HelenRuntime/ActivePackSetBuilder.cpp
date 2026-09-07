#include <HelenHook/ActivePackSetBuilder.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <string_view>

namespace
{
    /**
     * @brief Converts one ASCII string into lowercase.
     * @param value Text that should be folded for case-insensitive conflict checks.
     * @return Lowercase copy of the supplied text.
     */
    std::string ToLowerAscii(std::string value)
    {
        for (char& character : value)
        {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }

        return value;
    }

    /**
     * @brief Normalizes one filesystem-style game path for case-insensitive conflict checks.
     * @param path Path text that should be normalized.
     * @return Lowercase slash-normalized path text without a leading slash.
     */
    std::string NormalizeGamePathText(std::string path)
    {
        std::filesystem::path filesystem_path(path);
        path = filesystem_path.lexically_normal().generic_string();
        path = ToLowerAscii(std::move(path));
        while (!path.empty() && path.front() == '/')
        {
            path.erase(path.begin());
        }

        return path;
    }

    /**
     * @brief Returns whether a route path can select the same effective file as a virtual/missing path.
     * @param route_path Normalized route game-relative path.
     * @param occupied_path Normalized virtual or missing game-relative path.
     * @return True for exact normalized identity or the virtual service's declared-path suffix match.
     */
    bool IsEffectiveGamePathOverlap(const std::string& route_path, const std::string& occupied_path)
    {
        if (route_path == occupied_path)
        {
            return true;
        }

        if (route_path.size() <= occupied_path.size())
        {
            return false;
        }

        const std::size_t suffix_offset = route_path.size() - occupied_path.size();
        return route_path.compare(suffix_offset, occupied_path.size(), occupied_path) == 0 &&
            suffix_offset > 0 && route_path[suffix_offset - 1] == '/';
    }

    /**
     * @brief Builds one readable conflict string for one duplicated declaration key.
     * @param label Human-readable declaration class such as `command id`.
     * @param key Duplicated key text.
     * @param first_pack_id Pack id that first claimed the key.
     * @param second_pack_id Pack id that duplicated the key.
     * @return Readable conflict text used in failure diagnostics.
     */
    std::string BuildConflictReason(
        std::string_view label,
        std::string_view key,
        std::string_view first_pack_id,
        std::string_view second_pack_id)
    {
        std::ostringstream stream;
        stream << "Duplicate " << label << " '" << key << "' declared by packs '"
               << first_pack_id << "' and '" << second_pack_id << "'.";
        return stream.str();
    }

    /**
     * @brief Returns one effective external binding contract key used for fail-fast duplicate checks.
     * @param definition Binding definition that should be normalized into a conflict key.
     * @return Stable conflict key representing the externally visible meaning of the binding.
     */
    std::string BuildBindingContractKey(const helen::ExternalBindingDefinition& definition)
    {
        return ToLowerAscii(definition.ExternalName) + "|" +
            ToLowerAscii(definition.Mode) + "|" +
            ToLowerAscii(definition.ConfigKey) + "|" +
            ToLowerAscii(definition.CommandId);
    }

    /**
     * @brief Returns one effective texture replacement match signature used for duplicate-target checks.
     * @param definition Texture replacement declaration that should be normalized into a conflict key.
     * @return Stable conflict key representing the effective source-texture match signature.
     */
    std::string BuildTextureMatchKey(const helen::TextureReplacementDefinition& definition)
    {
        std::ostringstream stream;
        stream << ToLowerAscii(definition.Api) << "|"
               << definition.Width << "|"
               << definition.Height << "|"
               << ToLowerAscii(definition.Format) << "|"
               << ToLowerAscii(definition.Hash) << "|";
        if (definition.SamplerStage.has_value())
        {
            stream << *definition.SamplerStage;
        }
        else
        {
            stream << "*";
        }

        return stream.str();
    }

    /**
     * @brief Inserts one declaration key into the supplied ownership map or reports the conflict.
     * @param owners Map keyed by normalized declaration key whose value stores the first owning pack id.
     * @param key Effective normalized declaration key being inserted.
     * @param owner_pack_id Pack id currently claiming the key.
     * @param label Human-readable declaration class used in the failure reason.
     * @param failure_reason Receives a readable failure reason when insertion fails.
     * @return True when the key was not seen before; otherwise false.
     */
    bool TryClaimKey(
        std::map<std::string, std::string>& owners,
        const std::string& key,
        const std::string& owner_pack_id,
        std::string_view label,
        std::string& failure_reason)
    {
        const auto found = owners.find(key);
        if (found != owners.end())
        {
            failure_reason = BuildConflictReason(label, key, found->second, owner_pack_id);
            return false;
        }

        owners.emplace(key, owner_pack_id);
        return true;
    }
}

namespace helen
{
    bool ActivePackSetBuilder::TryBuild(
        const LoadedBuildPackSet& loaded_pack_set,
        ActivePackSet& active_pack_set,
        std::string& failure_reason) const
    {
        if (loaded_pack_set.Packs.empty())
        {
            failure_reason = "No loaded packs were supplied.";
            return false;
        }

        ActivePackSet merged_pack_set;
        std::map<std::string, std::string> config_entry_owners;
        std::map<std::string, std::string> feature_owners;
        std::map<std::string, std::string> command_owners;
        std::map<std::string, std::string> runtime_slot_owners;
        std::map<std::string, std::string> binding_owners;
        std::map<std::string, std::string> observer_owners;
        std::map<std::string, std::string> hook_owners;
        std::map<std::string, std::string> virtual_file_owners;
        std::map<std::string, std::string> file_write_route_id_owners;
        std::map<std::string, std::string> file_write_route_target_owners;
        std::map<std::string, std::string> texture_id_owners;
        std::map<std::string, std::string> texture_match_owners;
        std::set<std::string> missing_path_set;
        std::set<std::string> occupied_game_file_targets;

        for (const LoadedBuildPack& pack : loaded_pack_set.Packs)
        {
            for (const VirtualFileDefinition& definition : pack.Build.VirtualFiles)
            {
                occupied_game_file_targets.insert(NormalizeGamePathText(definition.GamePath.generic_string()));
            }

            for (const std::string& missing_path : pack.Build.MissingPaths)
            {
                occupied_game_file_targets.insert(NormalizeGamePathText(missing_path));
            }
        }

        for (const LoadedBuildPack& pack : loaded_pack_set.Packs)
        {
            merged_pack_set.LoadedPacks.push_back(pack);
            merged_pack_set.EnableD3d9TextureReplacementHooks =
                merged_pack_set.EnableD3d9TextureReplacementHooks || pack.Build.EnableD3d9TextureReplacementHooks;
            merged_pack_set.EnableD3d9TextureHashLogging =
                merged_pack_set.EnableD3d9TextureHashLogging || pack.Build.EnableD3d9TextureHashLogging;
            merged_pack_set.EnableD3d9TextureImageDumping =
                merged_pack_set.EnableD3d9TextureImageDumping || pack.Build.EnableD3d9TextureImageDumping;

            for (const ConfigEntryDefinition& entry : pack.Pack.ConfigEntries)
            {
                if (!TryClaimKey(config_entry_owners, entry.Key, pack.Pack.Id, "config key", failure_reason))
                {
                    return false;
                }

                merged_pack_set.ConfigEntries.push_back(entry);
            }

            for (const FeatureDefinition& feature : pack.Pack.Features)
            {
                if (!TryClaimKey(feature_owners, feature.Id, pack.Pack.Id, "feature id", failure_reason))
                {
                    return false;
                }

                merged_pack_set.Features.push_back(feature);
            }

            for (const CommandDefinition& command : pack.Build.Commands)
            {
                if (!TryClaimKey(command_owners, command.Id, pack.Pack.Id, "command id", failure_reason))
                {
                    return false;
                }

                merged_pack_set.Commands.push_back(command);
            }

            for (const RuntimeSlotDefinition& slot : pack.Build.RuntimeSlots)
            {
                if (!TryClaimKey(runtime_slot_owners, slot.Id, pack.Pack.Id, "runtime slot id", failure_reason))
                {
                    return false;
                }

                merged_pack_set.RuntimeSlots.push_back(slot);
            }

            for (const ExternalBindingDefinition& binding : pack.Build.ExternalBindings)
            {
                const std::string contract_key = BuildBindingContractKey(binding);
                if (!TryClaimKey(binding_owners, contract_key, pack.Pack.Id, "binding contract", failure_reason))
                {
                    return false;
                }

                merged_pack_set.ExternalBindings.push_back(binding);
            }

            for (const MemoryStateObserverDefinition& observer : pack.Build.StateObservers)
            {
                if (!TryClaimKey(observer_owners, observer.Id, pack.Pack.Id, "state observer id", failure_reason))
                {
                    return false;
                }

                merged_pack_set.StateObservers.push_back(observer);
            }

            for (const std::string& startup_command_id : pack.Build.StartupCommandIds)
            {
                merged_pack_set.StartupCommandIds.push_back(startup_command_id);
            }

            for (const std::string& missing_path : pack.Build.MissingPaths)
            {
                if (missing_path_set.insert(missing_path).second)
                {
                    merged_pack_set.MissingPaths.push_back(missing_path);
                }
            }

            for (const VirtualFileDefinition& definition : pack.Build.VirtualFiles)
            {
                const std::string game_path_key = NormalizeGamePathText(definition.GamePath.generic_string());
                if (!TryClaimKey(virtual_file_owners, game_path_key, pack.Pack.Id, "virtual file path", failure_reason))
                {
                    return false;
                }

                merged_pack_set.VirtualFiles.emplace_back(
                    pack.Pack.Id,
                    pack.Build.Id,
                    PackAssetResolver(pack.PackDirectory, pack.BuildDirectory),
                    definition);
            }

            for (const HookDefinition& definition : pack.Build.Hooks)
            {
                if (!TryClaimKey(hook_owners, definition.Id, pack.Pack.Id, "hook id", failure_reason))
                {
                    return false;
                }

                merged_pack_set.Hooks.emplace_back(
                    pack.Pack.Id,
                    pack.Build.Id,
                    PackAssetResolver(pack.PackDirectory, pack.BuildDirectory),
                    definition);
            }

            for (const TextureReplacementDefinition& definition : pack.Build.TextureReplacements)
            {
                if (!TryClaimKey(texture_id_owners, definition.Id, pack.Pack.Id, "texture replacement id", failure_reason))
                {
                    return false;
                }

                const std::string texture_match_key = BuildTextureMatchKey(definition);
                if (!TryClaimKey(texture_match_owners, texture_match_key, pack.Pack.Id, "texture replacement match signature", failure_reason))
                {
                    return false;
                }

                merged_pack_set.TextureReplacements.emplace_back(
                    pack.Pack.Id,
                    pack.Build.Id,
                    PackAssetResolver(pack.PackDirectory, pack.BuildDirectory),
                    definition);
            }

            for (const FileWriteRouteDefinition& definition : pack.Build.FileWriteRoutes)
            {
                if (!TryClaimKey(file_write_route_id_owners, NormalizeGamePathText(definition.Id), pack.Pack.Id,
                    "file-write route id", failure_reason))
                {
                    return false;
                }

                const std::string normalized_path = NormalizeGamePathText(definition.Path.generic_string());
                const std::string target_key = NormalizeGamePathText(definition.Root) + "|" + normalized_path;
                if (!TryClaimKey(file_write_route_target_owners, target_key, pack.Pack.Id,
                    "file-write route target", failure_reason))
                {
                    return false;
                }

                const bool overlaps_occupied_game_path = definition.Root == "game" &&
                    std::any_of(
                        occupied_game_file_targets.begin(),
                        occupied_game_file_targets.end(),
                        [&normalized_path](const std::string& occupied_path) {
                            return IsEffectiveGamePathOverlap(normalized_path, occupied_path);
                        });
                if (overlaps_occupied_game_path)
                {
                    failure_reason = BuildConflictReason("file-write route target", normalized_path, pack.Pack.Id, "virtual-or-missing-file");
                    return false;
                }

                merged_pack_set.FileWriteRoutes.push_back(definition);
            }
        }

        active_pack_set = std::move(merged_pack_set);
        failure_reason.clear();
        return true;
    }
}
