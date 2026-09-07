#include <HelenHook/ActivePackSet.h>
#include <HelenHook/ActivePackSetBuilder.h>
#include <HelenHook/LoadedBuildPackSet.h>
#include <HelenHook/VirtualFileSourceKind.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>
#include <string_view>

namespace
{
    /**
     * @brief Throws when one test expectation is false so the shared runtime test harness stops at the first failure.
     * @param condition Boolean condition that must evaluate to true.
     * @param message Failure text surfaced by the shared test runner.
     */
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Writes one exact text payload to disk for temporary pack-root scaffolding.
     * @param path Destination file path that should be created or replaced.
     * @param text Exact text content written into the file.
     */
    void WriteAllText(const std::filesystem::path& path, std::string_view text)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::runtime_error("Failed to create an active-pack-set builder test file.");
        }

        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream)
        {
            throw std::runtime_error("Failed to write an active-pack-set builder test file.");
        }
    }

    /**
     * @brief Creates one synthetic loaded pack rooted in a temporary filesystem tree.
     * @param root Root directory that should contain the synthetic pack subtree.
     * @param pack_id Stable synthetic pack identifier.
     * @param command_id Stable command identifier unique to this synthetic pack.
     * @param virtual_file_id Stable virtual file identifier unique to this synthetic pack.
     * @param virtual_file_path Game-relative virtual file path unique to this synthetic pack.
     * @return Fully populated synthetic loaded pack used by the active-pack-set builder tests.
     */
    helen::LoadedBuildPack CreatePack(
        const std::filesystem::path& root,
        const std::string& pack_id,
        const std::string& command_id,
        const std::string& virtual_file_id,
        const std::string& virtual_file_path)
    {
        const std::filesystem::path pack_root = root / pack_id;
        const std::filesystem::path build_root = pack_root / "builds" / "test-build";
        std::filesystem::create_directories(build_root / "assets");
        WriteAllText(pack_root / "pack.json", "{}");
        WriteAllText(build_root / "build.json", "{}");

        helen::LoadedBuildPack pack;
        pack.PackDirectory = pack_root;
        pack.BuildDirectory = build_root;
        pack.Pack.Id = pack_id;
        pack.Build.Id = "test-build";

        helen::ConfigEntryDefinition config_entry;
        config_entry.Key = "shared." + pack_id;
        config_entry.Type = "int";
        config_entry.DefaultValue = 1;
        pack.Pack.ConfigEntries.push_back(config_entry);

        helen::FeatureDefinition feature;
        feature.Id = "feature." + pack_id;
        feature.Name = "Feature " + pack_id;
        feature.Kind = "enum";
        feature.ConfigKey = config_entry.Key;
        feature.DefaultValue = 1;
        pack.Pack.Features.push_back(feature);

        helen::CommandDefinition command;
        command.Id = command_id;
        command.Name = command_id;
        pack.Build.Commands.push_back(command);
        pack.Build.StartupCommandIds.push_back(command_id);

        helen::VirtualFileDefinition virtual_file;
        virtual_file.Id = virtual_file_id;
        virtual_file.GamePath = std::filesystem::path(virtual_file_path);
        virtual_file.Mode = "replace-on-read";
        virtual_file.Source.Kind = helen::VirtualFileSourceKind::FullFile;
        virtual_file.Source.Path = std::filesystem::path("assets") / (virtual_file_id + ".bin");
        pack.Build.VirtualFiles.push_back(virtual_file);

        pack.Build.MissingPaths.push_back("bmgame/movies/" + pack_id + ".bik");
        pack.Build.EnableD3d9TextureReplacementHooks = true;
        return pack;
    }
}

/**
 * @brief Runs merge-validation coverage for the active multi-pack runtime builder.
 */
void RunActivePackSetBuilderTests()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "HelenRuntimeTests" / "ActivePackSetBuilder";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    try
    {
        helen::LoadedBuildPackSet loaded_pack_set;
        loaded_pack_set.Packs.push_back(CreatePack(root, "pack-a", "commandA", "vfA", "Game/A.bin"));
        loaded_pack_set.Packs.push_back(CreatePack(root, "pack-b", "commandB", "vfB", "Game/B.bin"));
        loaded_pack_set.Packs[0].Build.FileWriteRoutes.push_back({
            "routeA",
            "game",
            std::filesystem::path("Config") / "A.ini",
            helen::FileWritePolicy::Redirect,
            helen::FileReadPolicy::Redirected});
        loaded_pack_set.Packs[1].Build.FileWriteRoutes.push_back({
            "routeB",
            "documents",
            std::filesystem::path("Config") / "B.ini",
            helen::FileWritePolicy::Deny,
            helen::FileReadPolicy::Original});

        helen::ActivePackSet active_pack_set;
        std::string failure_reason;
        const helen::ActivePackSetBuilder builder;
        const bool succeeded = builder.TryBuild(loaded_pack_set, active_pack_set, failure_reason);
        Expect(succeeded, "Expected the active-pack-set builder to merge disjoint packs.");
        Expect(active_pack_set.LoadedPacks.size() == 2, "Merged loaded-pack count mismatch.");
        Expect(active_pack_set.StartupCommandIds.size() == 2, "Merged startup command count mismatch.");
        Expect(active_pack_set.StartupCommandIds[0] == "commandA", "Startup command ordering mismatch for the first pack.");
        Expect(active_pack_set.StartupCommandIds[1] == "commandB", "Startup command ordering mismatch for the second pack.");
        Expect(active_pack_set.VirtualFiles.size() == 2, "Merged virtual-file count mismatch.");
        Expect(active_pack_set.MissingPaths.size() == 2, "Merged missing-path count mismatch.");
        Expect(active_pack_set.FileWriteRoutes.size() == 2, "Merged file-write route count mismatch.");
        Expect(active_pack_set.EnableD3d9TextureReplacementHooks, "Merged D3D9 texture hook enable flag mismatch.");

        loaded_pack_set.Packs[1].Build.Commands[0].Id = "commandA";
        helen::ActivePackSet conflicting_pack_set;
        failure_reason.clear();
        const bool conflict_succeeded = builder.TryBuild(loaded_pack_set, conflicting_pack_set, failure_reason);
        Expect(!conflict_succeeded, "Expected duplicate command ids to reject the whole active pack set.");
        Expect(failure_reason.find("commandA") != std::string::npos, "Expected the failure reason to name the duplicate command id.");

        loaded_pack_set.Packs[1].Build.Commands[0].Id = "commandB";
        loaded_pack_set.Packs[1].Build.FileWriteRoutes[0].Id = "routeA";
        helen::ActivePackSet duplicate_route_id_set;
        failure_reason.clear();
        Expect(!builder.TryBuild(loaded_pack_set, duplicate_route_id_set, failure_reason),
            "Expected duplicate file-write route ids to reject the active pack set.");

        loaded_pack_set.Packs[1].Build.FileWriteRoutes[0].Id = "routeB";
        loaded_pack_set.Packs[1].Build.FileWriteRoutes[0].Root = "game";
        loaded_pack_set.Packs[1].Build.VirtualFiles[0].GamePath = std::filesystem::path("Config") / "." / "A.bin";
        loaded_pack_set.Packs[1].Build.FileWriteRoutes[0].Path = std::filesystem::path("BmGame") / "Config" / "A.bin";
        helen::ActivePackSet virtual_conflict_set;
        failure_reason.clear();
        Expect(!builder.TryBuild(loaded_pack_set, virtual_conflict_set, failure_reason),
            "Expected dot-alias file-write route overlap with a virtual file to reject the active pack set.");

        loaded_pack_set.Packs[1].Build.VirtualFiles[0].GamePath = std::filesystem::path("Game") / "B.bin";
        loaded_pack_set.Packs[1].Build.FileWriteRoutes[0].Path = std::filesystem::path("BmGame") / "Config" / "." / "B.ini";
        loaded_pack_set.Packs[1].Build.MissingPaths.push_back("BmGame/Config/B.ini");
        helen::ActivePackSet missing_alias_conflict_set;
        failure_reason.clear();
        Expect(!builder.TryBuild(loaded_pack_set, missing_alias_conflict_set, failure_reason),
            "Expected dot-alias file-write route overlap with a missing path to reject the active pack set.");
    }
    catch (...)
    {
        std::filesystem::remove_all(root);
        throw;
    }

    std::filesystem::remove_all(root);
}
