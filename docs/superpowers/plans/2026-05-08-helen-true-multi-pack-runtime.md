# Helen True Multi-Pack Runtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the temporary primary-pack restriction with true multi-pack runtime support so multiple enabled packs can contribute functional behavior together with fail-fast conflict handling.

**Architecture:** Introduce one merged `ActivePackSet` runtime model built from a validated `LoadedBuildPackSet`, then make each asset-backed subsystem pack-aware instead of assuming one global pack root. Shared runtime registries remain unified, but virtual files, hook blobs, and texture replacement assets retain pack-local source context for resolution.

**Tech Stack:** C++20, Win32, Direct3D 9 hook layer, JSON pack manifests, MSBuild, `HelenRuntimeTests.exe`, PowerShell Batman contract tests

---

## File Structure

- Create: `include/HelenHook/ActivePackSet.h`
  - Stores the merged runtime declarations and pack-local asset-backed entries.
- Create: `include/HelenHook/ActivePackSetBuilder.h`
  - Declares the validator/merger that turns a `LoadedBuildPackSet` into one `ActivePackSet`.
- Create: `HelenRuntime/ActivePackSetBuilder.cpp`
  - Implements conflict validation, ordered startup merge, and pack-local declaration capture.
- Create: `include/HelenHook/PackScopedVirtualFileRegistration.h`
  - Carries one `VirtualFileDefinition` plus source pack/build resolver context.
- Create: `include/HelenHook/PackScopedHookDefinition.h`
  - Carries one `HookDefinition` plus source pack/build resolver context.
- Create: `include/HelenHook/PackScopedTextureReplacementDefinition.h`
  - Carries one `TextureReplacementDefinition` plus source pack/build resolver context.
- Modify: `HelenRuntime/HelenRuntime.vcxproj`
  - Builds the new active-pack-set merger.
- Modify: `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`
  - Includes the new test file(s).
- Modify: `tests/HelenRuntime.Tests/TestMain.cpp`
  - Executes the new test suite(s).
- Create: `tests/HelenRuntime.Tests/ActivePackSetBuilderTests.cpp`
  - Covers merge success and fail-fast conflict cases.
- Modify: `include/HelenHook/RegisteredVirtualFile.h`
  - Stores pack-local virtual file registration data instead of only raw `VirtualFileDefinition`.
- Modify: `include/HelenHook/VirtualFileService.h`
  - Registers pack-scoped virtual files instead of relying on one resolver.
- Modify: `HelenRuntime/VirtualFileService.cpp`
  - Uses each virtual file's own resolver context to load its source asset.
- Modify: `tests/HelenRuntime.Tests/VirtualFileServiceTests.cpp`
  - Verifies disjoint virtual files from different pack roots coexist.
- Modify: `include/HelenHook/BuildHookInstaller.h`
  - Installs pack-scoped hook definitions.
- Modify: `HelenRuntime/BuildHookInstaller.cpp`
  - Resolves each hook blob through its source pack context.
- Modify: `tests/HelenRuntime.Tests/BuildHookInstallerTests.cpp`
  - Verifies two disjoint hooks from different pack roots can install together.
- Modify: `include/HelenHook/D3d9TextureReplacementHookSet.h`
  - Accepts pack-scoped texture replacement entries instead of one resolver plus raw definitions.
- Modify: `HelenRuntime/D3d9TextureReplacementHookSet.cpp`
  - Loads replacement assets through each entry's source resolver and validates merged replacement conflicts.
- Modify: `tests/HelenRuntime.Tests/TextureReplacementAssetLoaderTests.cpp`
  - Adds merged replacement asset ownership coverage.
- Modify: `HelenGameHook/HelenGameHook.cpp`
  - Replaces `g_active_pack` bootstrap assumptions with `ActivePackSet` bootstrap.
- Modify: `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`
  - Extends checked-in Batman coverage from pack-set loading to full active-pack-set merge success.
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanSkipVideosPack.ps1`
  - Keeps the explicit pack-set config contract aligned after the runtime model changes.

### Task 1: Add the merged `ActivePackSet` model and fail-fast validator

**Files:**
- Create: `include/HelenHook/ActivePackSet.h`
- Create: `include/HelenHook/ActivePackSetBuilder.h`
- Create: `HelenRuntime/ActivePackSetBuilder.cpp`
- Create: `include/HelenHook/PackScopedVirtualFileRegistration.h`
- Create: `include/HelenHook/PackScopedHookDefinition.h`
- Create: `include/HelenHook/PackScopedTextureReplacementDefinition.h`
- Create: `tests/HelenRuntime.Tests/ActivePackSetBuilderTests.cpp`
- Modify: `HelenRuntime/HelenRuntime.vcxproj`
- Modify: `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`
- Modify: `tests/HelenRuntime.Tests/TestMain.cpp`

- [ ] **Step 1: Write the failing active-pack-set builder tests**

Create `tests/HelenRuntime.Tests/ActivePackSetBuilderTests.cpp`:

```cpp
#include <HelenHook/ActivePackSet.h>
#include <HelenHook/ActivePackSetBuilder.h>
#include <HelenHook/LoadedBuildPackSet.h>
#include <HelenHook/PackAssetResolver.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace
{
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    void WriteAllText(const std::filesystem::path& path, std::string_view text)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::runtime_error("Failed to create an active-pack-set test file.");
        }

        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream)
        {
            throw std::runtime_error("Failed to write an active-pack-set test file.");
        }
    }

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
        pack.Pack.ConfigEntries.push_back({ "shared." + pack_id, "int", 1 });
        pack.Pack.Features.push_back({ "feature." + pack_id, pack_id, "enum", "shared." + pack_id, 1, {} });

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

        helen::ActivePackSet active_pack_set;
        std::string failure_reason;
        const helen::ActivePackSetBuilder builder;
        const bool succeeded = builder.TryBuild(loaded_pack_set, active_pack_set, failure_reason);
        Expect(succeeded, "Expected the active-pack-set builder to merge disjoint packs.");
        Expect(active_pack_set.LoadedPacks.size() == 2, "Merged loaded-pack count mismatch.");
        Expect(active_pack_set.StartupCommandIds.size() == 2, "Merged startup command count mismatch.");
        Expect(active_pack_set.StartupCommandIds[0] == "commandA", "Startup command ordering mismatch.");
        Expect(active_pack_set.StartupCommandIds[1] == "commandB", "Startup command ordering mismatch.");
        Expect(active_pack_set.VirtualFiles.size() == 2, "Merged virtual-file count mismatch.");
        Expect(active_pack_set.MissingPaths.size() == 2, "Merged missing-path count mismatch.");

        loaded_pack_set.Packs[1].Build.Commands[0].Id = "commandA";
        helen::ActivePackSet conflicting_pack_set;
        failure_reason.clear();
        const bool conflict_succeeded = builder.TryBuild(loaded_pack_set, conflicting_pack_set, failure_reason);
        Expect(!conflict_succeeded, "Expected duplicate command ids to reject the whole pack set.");
        Expect(failure_reason.find("commandA") != std::string::npos, "Expected the failure reason to name the duplicate command id.");
    }
    catch (...)
    {
        std::filesystem::remove_all(root);
        throw;
    }

    std::filesystem::remove_all(root);
}
```

Add the runner declaration and call to `tests/HelenRuntime.Tests/TestMain.cpp`:

```cpp
void RunActivePackSetBuilderTests();
```

```cpp
        RunActivePackSetBuilderTests();
```

Add the file to `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`:

```xml
<ClCompile Include="ActivePackSetBuilderTests.cpp" />
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /p:Configuration=Debug /p:Platform=Win32
```

Expected: FAIL because the new active-pack-set types and builder do not exist yet.

- [ ] **Step 3: Add the merged runtime types**

Create `include/HelenHook/PackScopedVirtualFileRegistration.h`:

```cpp
#pragma once

#include <string>

#include <HelenHook/PackAssetResolver.h>
#include <HelenHook/VirtualFileDefinition.h>

namespace helen
{
    class PackScopedVirtualFileRegistration
    {
    public:
        std::string PackId;
        std::string BuildId;
        PackAssetResolver AssetResolver;
        VirtualFileDefinition Definition;
    };
}
```

Create `include/HelenHook/PackScopedHookDefinition.h`:

```cpp
#pragma once

#include <string>

#include <HelenHook/HookDefinition.h>
#include <HelenHook/PackAssetResolver.h>

namespace helen
{
    class PackScopedHookDefinition
    {
    public:
        std::string PackId;
        std::string BuildId;
        PackAssetResolver AssetResolver;
        HookDefinition Definition;
    };
}
```

Create `include/HelenHook/PackScopedTextureReplacementDefinition.h`:

```cpp
#pragma once

#include <string>

#include <HelenHook/PackAssetResolver.h>
#include <HelenHook/TextureReplacementDefinition.h>

namespace helen
{
    class PackScopedTextureReplacementDefinition
    {
    public:
        std::string PackId;
        std::string BuildId;
        PackAssetResolver AssetResolver;
        TextureReplacementDefinition Definition;
    };
}
```

Create `include/HelenHook/ActivePackSet.h`:

```cpp
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
    class ActivePackSet
    {
    public:
        std::vector<LoadedBuildPack> LoadedPacks;
        std::vector<ConfigEntryDefinition> ConfigEntries;
        std::vector<FeatureDefinition> Features;
        std::vector<CommandDefinition> Commands;
        std::vector<RuntimeSlotDefinition> RuntimeSlots;
        std::vector<ExternalBindingDefinition> ExternalBindings;
        std::vector<MemoryStateObserverDefinition> StateObservers;
        std::vector<std::string> StartupCommandIds;
        std::vector<std::string> MissingPaths;
        std::vector<PackScopedVirtualFileRegistration> VirtualFiles;
        std::vector<PackScopedHookDefinition> Hooks;
        std::vector<PackScopedTextureReplacementDefinition> TextureReplacements;
        bool EnableD3d9TextureReplacementHooks = false;
        bool EnableD3d9TextureHashLogging = false;
        bool EnableD3d9TextureImageDumping = false;
    };
}
```

Create `include/HelenHook/ActivePackSetBuilder.h`:

```cpp
#pragma once

#include <string>

#include <HelenHook/ActivePackSet.h>
#include <HelenHook/LoadedBuildPackSet.h>

namespace helen
{
    class ActivePackSetBuilder
    {
    public:
        bool TryBuild(const LoadedBuildPackSet& loaded_pack_set, ActivePackSet& active_pack_set, std::string& failure_reason) const;
    };
}
```

- [ ] **Step 4: Implement fail-fast merge validation**

Create `HelenRuntime/ActivePackSetBuilder.cpp`:

```cpp
#include <HelenHook/ActivePackSetBuilder.h>

#include <set>
#include <sstream>

namespace
{
    template <typename TKey>
    bool TryInsertUnique(std::set<TKey>& seen, const TKey& key, std::string_view label, std::string& failure_reason)
    {
        if (seen.insert(key).second)
        {
            return true;
        }

        std::ostringstream stream;
        stream << "Duplicate " << label << ": " << key;
        failure_reason = stream.str();
        return false;
    }

    std::string BuildTextureMatchKey(const helen::TextureReplacementDefinition& definition)
    {
        std::ostringstream stream;
        stream << definition.Api << "|"
               << definition.Width << "|"
               << definition.Height << "|"
               << definition.Format << "|"
               << definition.Hash << "|";
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

        ActivePackSet merged;
        std::set<std::string> config_keys;
        std::set<std::string> feature_ids;
        std::set<std::string> command_ids;
        std::set<std::string> runtime_slot_ids;
        std::set<std::string> observer_ids;
        std::set<std::string> hook_ids;
        std::set<std::string> virtual_file_paths;
        std::set<std::string> texture_ids;
        std::set<std::string> texture_match_keys;
        std::set<std::string> binding_contract_keys;
        std::set<std::string> missing_paths;

        for (const LoadedBuildPack& pack : loaded_pack_set.Packs)
        {
            merged.LoadedPacks.push_back(pack);
            merged.EnableD3d9TextureReplacementHooks =
                merged.EnableD3d9TextureReplacementHooks || pack.Build.EnableD3d9TextureReplacementHooks;
            merged.EnableD3d9TextureHashLogging =
                merged.EnableD3d9TextureHashLogging || pack.Build.EnableD3d9TextureHashLogging;
            merged.EnableD3d9TextureImageDumping =
                merged.EnableD3d9TextureImageDumping || pack.Build.EnableD3d9TextureImageDumping;

            for (const ConfigEntryDefinition& entry : pack.Pack.ConfigEntries)
            {
                if (!TryInsertUnique(config_keys, entry.Key, "config key", failure_reason))
                {
                    return false;
                }

                merged.ConfigEntries.push_back(entry);
            }

            for (const FeatureDefinition& feature : pack.Pack.Features)
            {
                if (!TryInsertUnique(feature_ids, feature.Id, "feature id", failure_reason))
                {
                    return false;
                }

                merged.Features.push_back(feature);
            }

            for (const CommandDefinition& command : pack.Build.Commands)
            {
                if (!TryInsertUnique(command_ids, command.Id, "command id", failure_reason))
                {
                    return false;
                }

                merged.Commands.push_back(command);
            }

            for (const RuntimeSlotDefinition& slot : pack.Build.RuntimeSlots)
            {
                if (!TryInsertUnique(runtime_slot_ids, slot.Id, "runtime slot id", failure_reason))
                {
                    return false;
                }

                merged.RuntimeSlots.push_back(slot);
            }

            for (const MemoryStateObserverDefinition& observer : pack.Build.StateObservers)
            {
                if (!TryInsertUnique(observer_ids, observer.Id, "state observer id", failure_reason))
                {
                    return false;
                }

                merged.StateObservers.push_back(observer);
            }

            for (const ExternalBindingDefinition& binding : pack.Build.ExternalBindings)
            {
                const std::string binding_key = binding.ExternalName + "|" + binding.Mode + "|" + binding.ConfigKey + "|" + binding.CommandId;
                if (!TryInsertUnique(binding_contract_keys, binding_key, "binding contract", failure_reason))
                {
                    return false;
                }

                merged.ExternalBindings.push_back(binding);
            }

            for (const std::string& startup_command_id : pack.Build.StartupCommandIds)
            {
                merged.StartupCommandIds.push_back(startup_command_id);
            }

            for (const std::string& missing_path : pack.Build.MissingPaths)
            {
                if (missing_paths.insert(missing_path).second)
                {
                    merged.MissingPaths.push_back(missing_path);
                }
            }

            const PackAssetResolver asset_resolver(pack.PackDirectory, pack.BuildDirectory);

            for (const VirtualFileDefinition& definition : pack.Build.VirtualFiles)
            {
                const std::string game_path = definition.GamePath.generic_string();
                if (!TryInsertUnique(virtual_file_paths, game_path, "virtual file path", failure_reason))
                {
                    return false;
                }

                PackScopedVirtualFileRegistration registration;
                registration.PackId = pack.Pack.Id;
                registration.BuildId = pack.Build.Id;
                registration.AssetResolver = asset_resolver;
                registration.Definition = definition;
                merged.VirtualFiles.push_back(std::move(registration));
            }

            for (const HookDefinition& definition : pack.Build.Hooks)
            {
                if (!TryInsertUnique(hook_ids, definition.Id, "hook id", failure_reason))
                {
                    return false;
                }

                PackScopedHookDefinition scoped_hook;
                scoped_hook.PackId = pack.Pack.Id;
                scoped_hook.BuildId = pack.Build.Id;
                scoped_hook.AssetResolver = asset_resolver;
                scoped_hook.Definition = definition;
                merged.Hooks.push_back(std::move(scoped_hook));
            }

            for (const TextureReplacementDefinition& definition : pack.Build.TextureReplacements)
            {
                if (!TryInsertUnique(texture_ids, definition.Id, "texture replacement id", failure_reason))
                {
                    return false;
                }

                const std::string texture_match_key = BuildTextureMatchKey(definition);
                if (!TryInsertUnique(texture_match_keys, texture_match_key, "texture replacement match signature", failure_reason))
                {
                    return false;
                }

                PackScopedTextureReplacementDefinition scoped_replacement;
                scoped_replacement.PackId = pack.Pack.Id;
                scoped_replacement.BuildId = pack.Build.Id;
                scoped_replacement.AssetResolver = asset_resolver;
                scoped_replacement.Definition = definition;
                merged.TextureReplacements.push_back(std::move(scoped_replacement));
            }
        }

        active_pack_set = std::move(merged);
        failure_reason.clear();
        return true;
    }
}
```

Add the new source to `HelenRuntime/HelenRuntime.vcxproj`:

```xml
<ClCompile Include="ActivePackSetBuilder.cpp" />
```

- [ ] **Step 5: Re-run the runtime tests**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /p:Configuration=Debug /p:Platform=Win32
& 'C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe'
```

Expected: PASS for the new builder coverage.

- [ ] **Step 6: Commit**

```bash
git add include/HelenHook/ActivePackSet.h include/HelenHook/ActivePackSetBuilder.h include/HelenHook/PackScopedVirtualFileRegistration.h include/HelenHook/PackScopedHookDefinition.h include/HelenHook/PackScopedTextureReplacementDefinition.h HelenRuntime/ActivePackSetBuilder.cpp HelenRuntime/HelenRuntime.vcxproj tests/HelenRuntime.Tests/ActivePackSetBuilderTests.cpp tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj tests/HelenRuntime.Tests/TestMain.cpp
git commit -m "Add active pack set merge validation"
```

### Task 2: Make `VirtualFileService` pack-aware

**Files:**
- Modify: `include/HelenHook/RegisteredVirtualFile.h`
- Modify: `include/HelenHook/VirtualFileService.h`
- Modify: `HelenRuntime/VirtualFileService.cpp`
- Modify: `tests/HelenRuntime.Tests/VirtualFileServiceTests.cpp`

- [ ] **Step 1: Write the failing virtual file pack-ownership test**

Add this case to `tests/HelenRuntime.Tests/VirtualFileServiceTests.cpp`:

```cpp
    const std::filesystem::path pack_a_root = root / "pack-a";
    const std::filesystem::path pack_a_build_root = pack_a_root / "builds" / "test-build";
    const std::filesystem::path pack_b_root = root / "pack-b";
    const std::filesystem::path pack_b_build_root = pack_b_root / "builds" / "test-build";
    std::filesystem::create_directories(pack_a_build_root / "assets");
    std::filesystem::create_directories(pack_b_build_root / "assets");
    WriteAllBytes(pack_a_build_root / "assets" / "A.bin", { 0x41, 0x42, 0x43 });
    WriteAllBytes(pack_b_build_root / "assets" / "B.bin", { 0x51, 0x52, 0x53 });

    helen::VirtualFileService service(root / "cache");

    helen::PackScopedVirtualFileRegistration registration_a;
    registration_a.PackId = "pack-a";
    registration_a.BuildId = "test-build";
    registration_a.AssetResolver = helen::PackAssetResolver(pack_a_root, pack_a_build_root);
    registration_a.Definition.Id = "fileA";
    registration_a.Definition.GamePath = std::filesystem::path("Game/A.bin");
    registration_a.Definition.Mode = "replace-on-read";
    registration_a.Definition.Source.Kind = helen::VirtualFileSourceKind::FullFile;
    registration_a.Definition.Source.Path = std::filesystem::path("assets/A.bin");
    Expect(service.RegisterVirtualFile(registration_a), "Expected pack A virtual file registration to succeed.");

    helen::PackScopedVirtualFileRegistration registration_b;
    registration_b.PackId = "pack-b";
    registration_b.BuildId = "test-build";
    registration_b.AssetResolver = helen::PackAssetResolver(pack_b_root, pack_b_build_root);
    registration_b.Definition.Id = "fileB";
    registration_b.Definition.GamePath = std::filesystem::path("Game/B.bin");
    registration_b.Definition.Mode = "replace-on-read";
    registration_b.Definition.Source.Kind = helen::VirtualFileSourceKind::FullFile;
    registration_b.Definition.Source.Path = std::filesystem::path("assets/B.bin");
    Expect(service.RegisterVirtualFile(registration_b), "Expected pack B virtual file registration to succeed.");
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /p:Configuration=Debug /p:Platform=Win32
```

Expected: FAIL because `VirtualFileService` still expects one resolver in its constructor and one raw `VirtualFileDefinition` in registration.

- [ ] **Step 3: Move the resolver onto each registered virtual file**

Update `include/HelenHook/RegisteredVirtualFile.h` so it stores one pack-scoped registration:

```cpp
        /** @brief Pack-scoped virtual file registration that owns the source resolver context. */
        PackScopedVirtualFileRegistration Registration;
```

Update `include/HelenHook/VirtualFileService.h`:

```cpp
        explicit VirtualFileService(const std::filesystem::path& cache_directory);
```

```cpp
        bool RegisterVirtualFile(const PackScopedVirtualFileRegistration& registration);
```

Remove the service-wide resolver field:

```cpp
        /** @brief Writable helengamehook cache directory used by source implementations that materialize files. */
        std::filesystem::path cache_directory_;
```

- [ ] **Step 4: Resolve each source asset through its own registration**

Update `HelenRuntime/VirtualFileService.cpp`:

```cpp
    VirtualFileService::VirtualFileService(const std::filesystem::path& cache_directory)
        : cache_directory_(cache_directory)
    {
    }
```

```cpp
    bool VirtualFileService::RegisterVirtualFile(const PackScopedVirtualFileRegistration& registration)
    {
        std::wstring normalized_path;
        if (!NormalizeDeclaredPath(registration.Definition.GamePath, normalized_path))
        {
            return false;
        }

        RegisteredVirtualFile registered_virtual_file;
        registered_virtual_file.NormalizedDeclaredPath = normalized_path;
        registered_virtual_file.Registration = registration;
        virtual_files_[normalized_path] = std::move(registered_virtual_file);
        return true;
    }
```

Update `LoadReplacementBytes(...)` so it accepts the registration resolver:

```cpp
    bool VirtualFileService::LoadReplacementBytes(
        const PackScopedVirtualFileRegistration& registration,
        const std::filesystem::path& asset_path,
        std::vector<std::uint8_t>& bytes) const
    {
        const std::optional<std::filesystem::path> resolved_path = registration.AssetResolver.Resolve(asset_path);
        if (!resolved_path.has_value())
        {
            return false;
        }

        // keep the existing file read logic here
    }
```

Update `CreateSource(...)` and every call site to use `registered_virtual_file.Registration`.

- [ ] **Step 5: Re-run the runtime tests**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /p:Configuration=Debug /p:Platform=Win32
& 'C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe'
```

Expected: PASS with the pack-local virtual file test included.

- [ ] **Step 6: Commit**

```bash
git add include/HelenHook/RegisteredVirtualFile.h include/HelenHook/VirtualFileService.h HelenRuntime/VirtualFileService.cpp tests/HelenRuntime.Tests/VirtualFileServiceTests.cpp
git commit -m "Make virtual files pack-aware"
```

### Task 3: Make native hook installation pack-aware

**Files:**
- Modify: `include/HelenHook/BuildHookInstaller.h`
- Modify: `HelenRuntime/BuildHookInstaller.cpp`
- Modify: `tests/HelenRuntime.Tests/BuildHookInstallerTests.cpp`

- [ ] **Step 1: Write the failing pack-scoped hook installer test**

Add to `tests/HelenRuntime.Tests/BuildHookInstallerTests.cpp`:

```cpp
    std::vector<helen::PackScopedHookDefinition> hooks;

    helen::PackScopedHookDefinition first_hook;
    first_hook.PackId = "pack-a";
    first_hook.BuildId = "test-build";
    first_hook.AssetResolver = helen::PackAssetResolver(pack_a_root, pack_a_build_root);
    first_hook.Definition = build_first_test_hook();
    hooks.push_back(first_hook);

    helen::PackScopedHookDefinition second_hook;
    second_hook.PackId = "pack-b";
    second_hook.BuildId = "test-build";
    second_hook.AssetResolver = helen::PackAssetResolver(pack_b_root, pack_b_build_root);
    second_hook.Definition = build_second_test_hook();
    hooks.push_back(second_hook);

    Expect(installer.Install(hooks, runtime_values), "Expected pack-scoped hook installation to succeed.");
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /p:Configuration=Debug /p:Platform=Win32
```

Expected: FAIL because `BuildHookInstaller::Install` still accepts raw `HookDefinition` values.

- [ ] **Step 3: Change the installer interface to pack-scoped hooks**

Update `include/HelenHook/BuildHookInstaller.h`:

```cpp
        BuildHookInstaller() = default;
```

```cpp
        bool Install(const std::vector<PackScopedHookDefinition>& hooks, const RuntimeValueStore& runtime_values);
```

Remove the resolver-owning constructor and any resolver field so the installer no longer owns one global pack root.

- [ ] **Step 4: Resolve each blob with the hook's own resolver**

Update `HelenRuntime/BuildHookInstaller.cpp`:

```cpp
    bool BuildHookInstaller::Install(const std::vector<PackScopedHookDefinition>& hooks, const RuntimeValueStore& runtime_values)
    {
        Remove();

        for (const PackScopedHookDefinition& scoped_hook : hooks)
        {
            const std::optional<std::filesystem::path> blob_path =
                scoped_hook.AssetResolver.Resolve(scoped_hook.Definition.Blob.AssetPath);
            if (!blob_path.has_value())
            {
                Remove();
                return false;
            }

            if (!InstallOneHook(scoped_hook.Definition, *blob_path, runtime_values))
            {
                Remove();
                return false;
            }
        }

        return true;
    }
```

Refactor any helper that currently reaches for `asset_resolver_` so it receives the already-resolved blob path instead.

- [ ] **Step 5: Re-run the runtime tests**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /p:Configuration=Debug /p:Platform=Win32
& 'C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe'
```

Expected: PASS with pack-local hook installation coverage.

- [ ] **Step 6: Commit**

```bash
git add include/HelenHook/BuildHookInstaller.h HelenRuntime/BuildHookInstaller.cpp tests/HelenRuntime.Tests/BuildHookInstallerTests.cpp
git commit -m "Make native hook blobs pack-aware"
```

### Task 4: Make texture replacements pack-aware

**Files:**
- Modify: `include/HelenHook/D3d9TextureReplacementHookSet.h`
- Modify: `HelenRuntime/D3d9TextureReplacementHookSet.cpp`
- Modify: `tests/HelenRuntime.Tests/TextureReplacementAssetLoaderTests.cpp`

- [ ] **Step 1: Write the failing pack-scoped replacement ownership test**

Add to `tests/HelenRuntime.Tests/TextureReplacementAssetLoaderTests.cpp`:

```cpp
    helen::PackScopedTextureReplacementDefinition replacement_a;
    replacement_a.PackId = "pack-a";
    replacement_a.BuildId = "test-build";
    replacement_a.AssetResolver = helen::PackAssetResolver(pack_a_root, pack_a_build_root);
    replacement_a.Definition.Id = "replace-a";
    replacement_a.Definition.Api = "d3d9";
    replacement_a.Definition.Width = 256;
    replacement_a.Definition.Height = 256;
    replacement_a.Definition.Format = "DXT5";
    replacement_a.Definition.Hash = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    replacement_a.Definition.ReplacementPath = "assets/A.dds";

    helen::PackScopedTextureReplacementDefinition replacement_b;
    replacement_b.PackId = "pack-b";
    replacement_b.BuildId = "test-build";
    replacement_b.AssetResolver = helen::PackAssetResolver(pack_b_root, pack_b_build_root);
    replacement_b.Definition.Id = "replace-b";
    replacement_b.Definition.Api = "d3d9";
    replacement_b.Definition.Width = 512;
    replacement_b.Definition.Height = 512;
    replacement_b.Definition.Format = "DXT5";
    replacement_b.Definition.Hash = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    replacement_b.Definition.ReplacementPath = "assets/B.dds";
```

Then assert each replacement resolves from its own pack root through the hook-set or loader path you expose for testing.

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /p:Configuration=Debug /p:Platform=Win32
```

Expected: FAIL because the texture hook set still accepts one resolver plus raw replacements.

- [ ] **Step 3: Replace the global resolver plus raw list with pack-scoped entries**

Update `include/HelenHook/D3d9TextureReplacementHookSet.h`:

```cpp
        D3d9TextureReplacementHookSet(
            bool enable_hooking,
            bool enable_hash_logging,
            bool enable_image_dumping,
            std::filesystem::path texture_dump_directory,
            const std::vector<PackScopedTextureReplacementDefinition>& replacements);
```

Replace the fields:

```cpp
        /** @brief Build-scoped replacement entries declared through `textures.json`. */
        const std::vector<PackScopedTextureReplacementDefinition>& replacements_;
```

- [ ] **Step 4: Resolve each replacement asset through its own source pack**

Update `HelenRuntime/D3d9TextureReplacementHookSet.cpp`:

```cpp
    const PackScopedTextureReplacementDefinition* FindDeclaredReplacement(
        const D3DSURFACE_DESC& description,
        std::string_view digest) const;
```

```cpp
    const PackScopedTextureReplacementDefinition* D3d9TextureReplacementHookSet::FindDeclaredReplacement(
        const D3DSURFACE_DESC& description,
        std::string_view digest) const
    {
        for (const PackScopedTextureReplacementDefinition& replacement : replacements_)
        {
            const TextureReplacementDefinition& definition = replacement.Definition;
            if (definition.Api == "d3d9" &&
                definition.Width == description.Width &&
                definition.Height == description.Height &&
                definition.Format == ToFormatString(description.Format) &&
                definition.Hash == digest)
            {
                return &replacement;
            }
        }

        return nullptr;
    }
```

And in the cache/load path:

```cpp
        const std::optional<std::filesystem::path> replacement_path =
            replacement_definition.AssetResolver.Resolve(replacement_definition.Definition.ReplacementPath);
```

Keep all matching logic based on `replacement_definition.Definition`.

- [ ] **Step 5: Re-run the runtime tests**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /p:Configuration=Debug /p:Platform=Win32
& 'C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe'
```

Expected: PASS with pack-local texture replacement ownership coverage.

- [ ] **Step 6: Commit**

```bash
git add include/HelenHook/D3d9TextureReplacementHookSet.h HelenRuntime/D3d9TextureReplacementHookSet.cpp tests/HelenRuntime.Tests/TextureReplacementAssetLoaderTests.cpp
git commit -m "Make texture replacements pack-aware"
```

### Task 5: Switch runtime bootstrap from `LoadedBuildPack` to `ActivePackSet`

**Files:**
- Modify: `HelenGameHook/HelenGameHook.cpp`
- Modify: `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanSkipVideosPack.ps1`

- [ ] **Step 1: Write the failing checked-in Batman active-pack-set contract**

Extend `tests/HelenRuntime.Tests/PackRepositoryTests.cpp` after the checked-in Batman pack-set load:

```cpp
        helen::ActivePackSet active_pack_set;
        std::string failure_reason;
        const helen::ActivePackSetBuilder active_pack_set_builder;
        const bool built_checked_in_pack_set = active_pack_set_builder.TryBuild(
            *checked_in_batman_pack_set,
            active_pack_set,
            failure_reason);
        Expect(built_checked_in_pack_set, "Expected the checked-in Batman subtitles plus skip-videos pack set to merge into an active runtime pack set.");
        Expect(active_pack_set.LoadedPacks.size() == 2, "Checked-in Batman active-pack-set loaded-pack count mismatch.");
        Expect(active_pack_set.MissingPaths.size() == 5, "Checked-in Batman active-pack-set hidden-path count mismatch.");
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /p:Configuration=Debug /p:Platform=Win32
& 'C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe'
```

Expected: FAIL because `HelenGameHook.cpp` still uses primary-pack validation and service initialization paths.

- [ ] **Step 3: Replace the primary-pack globals with one active runtime pack set**

Update `HelenGameHook/HelenGameHook.cpp` globals:

```cpp
    /** @brief Active merged runtime pack set chosen for the host executable, when available. */
    std::optional<helen::ActivePackSet> g_active_runtime_pack_set;
```

Remove:

```cpp
    std::optional<helen::LoadedBuildPack> g_active_pack;
```

Update repository initialization so explicit pack selection becomes:

```cpp
                helen::ActivePackSet active_pack_set;
                std::string failure_reason;
                const helen::ActivePackSetBuilder builder;
                if (!g_active_pack_set.has_value() ||
                    !builder.TryBuild(*g_active_pack_set, active_pack_set, failure_reason))
                {
                    g_active_pack_set.reset();
                    g_active_runtime_pack_set.reset();
                    helen::Logf(L"[runtime] active pack-set validation failed: %ls", ToWideString(failure_reason).c_str());
                    return false;
                }

                g_active_runtime_pack_set = std::move(active_pack_set);
```

For single-pack fallback, wrap the loaded pack in a one-item `LoadedBuildPackSet` and build the same `ActivePackSet`.

- [ ] **Step 4: Register merged declarations and initialize services from `ActivePackSet`**

Change the registration helpers in `HelenGameHook/HelenGameHook.cpp` to accept merged vectors instead of one `LoadedBuildPack`, for example:

```cpp
    bool RegisterDeclaredConfigEntries(const std::vector<helen::ConfigEntryDefinition>& entries)
```

```cpp
    bool RegisterDeclaredCommands(const std::vector<helen::CommandDefinition>& commands)
```

```cpp
    bool RegisterDeclaredExternalBindings(const std::vector<helen::ExternalBindingDefinition>& bindings)
```

```cpp
    bool RegisterDeclaredRuntimeSlots(const std::vector<helen::RuntimeSlotDefinition>& slots)
```

Change pack runtime initialization:

```cpp
    bool InitializeActivePackRuntime(const helen::RuntimeLayout& layout, const helen::ActivePackSet& active_pack_set)
```

Use the merged declarations:

```cpp
        if (!RegisterDeclaredConfigEntries(active_pack_set.ConfigEntries))
        {
            return false;
        }
```

```cpp
        if (!RegisterDeclaredCommands(active_pack_set.Commands))
        {
            return false;
        }
```

```cpp
        if (!RegisterDeclaredExternalBindings(active_pack_set.ExternalBindings))
        {
            return false;
        }
```

Construct services from merged asset-backed entries:

```cpp
        g_virtual_files = std::make_unique<helen::VirtualFileService>(layout.CacheDirectory);
        for (const helen::PackScopedVirtualFileRegistration& registration : active_pack_set.VirtualFiles)
        {
            if (!g_virtual_files->RegisterVirtualFile(registration))
            {
                return false;
            }
        }
```

```cpp
        g_build_hooks = std::make_unique<helen::BuildHookInstaller>();
        if (!g_build_hooks->Install(active_pack_set.Hooks, *g_runtime_values))
        {
            return false;
        }
```

```cpp
        g_d3d9_texture_hooks = std::make_unique<helen::D3d9TextureReplacementHookSet>(
            active_pack_set.EnableD3d9TextureReplacementHooks,
            active_pack_set.EnableD3d9TextureHashLogging,
            active_pack_set.EnableD3d9TextureImageDumping,
            layout.LogsDirectory / L"d3d9-textures",
            active_pack_set.TextureReplacements);
```

```cpp
        g_build_runtime_coordinator = std::make_unique<helen::BuildRuntimeCoordinator>(
            active_pack_set.StartupCommandIds,
            active_pack_set.StateObservers,
            *g_command_dispatcher,
            *g_command_executor);
```

Use merged hidden paths:

```cpp
        g_file_hooks = std::make_unique<helen::FileApiHookSet>(
            *g_virtual_files,
            layout.GameRoot,
            active_pack_set.MissingPaths);
```

Remove the hidden-path-only addon validator entirely.

- [ ] **Step 5: Re-run full verification**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File 'games\HelenBatmanAA\scripts\Test-BatmanSkipVideosPack.ps1'
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /p:Configuration=Debug /p:Platform=Win32
& 'C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe'
```

Expected:

- `PASS` from `Test-BatmanSkipVideosPack.ps1`
- build succeeds
- `HelenRuntimeTests.exe` prints `PASS`

- [ ] **Step 6: Commit**

```bash
git add HelenGameHook/HelenGameHook.cpp tests/HelenRuntime.Tests/PackRepositoryTests.cpp games/HelenBatmanAA/scripts/Test-BatmanSkipVideosPack.ps1
git commit -m "Enable true multi-pack runtime bootstrap"
```

## Self-Review

- Spec coverage:
  - merged runtime model: Task 1
  - pack-local virtual files: Task 2
  - pack-local hook blobs: Task 3
  - pack-local texture assets: Task 4
  - bootstrap and shared runtime registries: Task 5
- Placeholder scan:
  - No `TODO` / `TBD` markers remain.
  - Every task includes file paths, code shape, commands, and expected outcomes.
- Type consistency:
  - `ActivePackSet`, `ActivePackSetBuilder`, `PackScopedVirtualFileRegistration`, `PackScopedHookDefinition`, and `PackScopedTextureReplacementDefinition` are used consistently across later tasks.
