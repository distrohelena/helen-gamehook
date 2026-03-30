#include <HelenHook/LoadedBuildPack.h>
#include <HelenHook/PackRepository.h>
#include <HelenHook/VirtualFileSourceKind.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace
{
    /**
     * @brief Throws when one required boolean condition is false so the shared test harness stops at the first failure.
     * @param condition Boolean condition that must evaluate to true.
     * @param message Failure message reported by the shared test runner.
     */
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Writes one exact UTF-8 text payload to disk for temporary pack manifest scenarios.
     * @param path Destination file path that should be created or replaced.
     * @param text Exact text content written into the file.
     */
    void WriteAllText(const std::filesystem::path& path, std::string_view text)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::runtime_error("Failed to create a pack repository test file.");
        }

        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream)
        {
            throw std::runtime_error("Failed to write a pack repository test file.");
        }
    }

    /**
     * @brief Resolves the checked-in Batman pack root inside the current repository.
     * @return Absolute path to the Batman pack root used by the live Helen runtime.
     */
    std::filesystem::path GetBatmanPackRoot()
    {
        const std::filesystem::path source_path(__FILE__);
        return source_path.parent_path().parent_path().parent_path() / "games" / "HelenBatmanAA" / "helengamehook" / "packs";
    }

}

/**
 * @brief Verifies that the pack repository loads a complete synthetic split pack and skips malformed candidate builds.
 */
void RunPackRepositoryTests()
{
    helen::PackRepository repository;

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "HelenRuntimeTests" / "PackRepository";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    try
    {
        const std::filesystem::path packs_root = root / "packs";
        const std::filesystem::path valid_pack_root = packs_root / "batman-aa-subtitles";
        const std::filesystem::path valid_build_root = valid_pack_root / "builds" / "steam-goty-1.0";
        const std::filesystem::path pack_root = packs_root / "broken-pack";
        const std::filesystem::path build_root = pack_root / "builds" / "broken-build";
        const std::filesystem::path mode_mismatch_pack_root = packs_root / "mode-mismatch-pack";
        const std::filesystem::path mode_mismatch_build_root = mode_mismatch_pack_root / "builds" / "mode-mismatch-build";
        const std::filesystem::path malformed_delta_hash_pack_root = packs_root / "malformed-delta-hash-pack";
        const std::filesystem::path malformed_delta_hash_build_root = malformed_delta_hash_pack_root / "builds" / "malformed-delta-hash-build";
        std::filesystem::create_directories(valid_build_root);
        std::filesystem::create_directories(build_root);
        std::filesystem::create_directories(mode_mismatch_build_root);
        std::filesystem::create_directories(malformed_delta_hash_build_root);

        WriteAllText(
            valid_pack_root / "pack.json",
            R"({
  "schemaVersion": 1,
  "id": "batman-aa-subtitles",
  "name": "Batman Arkham Asylum Gameplay Subtitle Slice",
  "targets": [
    {
      "executables": [
        "ShippingPC-BmGame.exe"
      ]
    }
  ],
  "config": [
    {
      "key": "ui.subtitleSize",
      "type": "int",
      "defaultValue": 1
    }
  ],
  "features": [
    {
      "id": "subtitleSize",
      "name": "Subtitle Size",
      "kind": "enum",
      "configKey": "ui.subtitleSize",
      "defaultValue": 1
    }
  ],
  "builds": [
    "steam-goty-1.0"
  ]
})");

        WriteAllText(
            valid_build_root / "build.json",
            R"({
  "id": "steam-goty-1.0",
  "executable": "ShippingPC-BmGame.exe",
  "startupCommands": [
    "applySavedSubtitleSize"
  ],
  "match": {
    "fileSize": 38758728,
    "sha256": "4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028"
  }
})");

        WriteAllText(
            valid_build_root / "files.json",
            R"({
  "virtualFiles": [
    {
      "id": "bmgameGameplayPackage",
      "path": "BmGame/CookedPC/BmGame.u",
      "mode": "delta-on-read",
        "source": {
        "kind": "delta-file",
        "path": "assets/deltas/BmGame-subtitle-signal.hgdelta",
        "base": {
          "size": 101403981,
          "sha256": "AaBbCcDdEeFf00112233445566778899AaBbCcDdEeFf00112233445566778899"
        },
        "target": {
          "size": 101405329,
          "sha256": "FfEeDdCcBbAa99887766554433221100FfEeDdCcBbAa99887766554433221100"
        },
        "chunkSize": 65536
      }
    },
    {
      "id": "frontendMapPackage",
      "path": "BmGame/CookedPC/Maps/Frontend/Frontend.umap",
      "mode": "delta-on-read",
      "source": {
        "kind": "delta-file",
        "path": "assets/deltas/Frontend-main-menu-subtitle-size.hgdelta",
        "base": {
          "size": 2048,
          "sha256": "00112233445566778899AABBCCDDEEFF00112233445566778899AABBCCDDEEFF"
        },
        "target": {
          "size": 2304,
          "sha256": "FFEEDDCCBBAA99887766554433221100FFEEDDCCBBAA99887766554433221100"
        },
        "chunkSize": 65536
      }
    }
  ]
})");

        WriteAllText(valid_build_root / "bindings.json", R"({ "bindings": [] })");

        WriteAllText(
            valid_build_root / "commands.json",
            R"({
  "commands": [
    {
      "id": "applySavedSubtitleSize",
      "name": "Apply Saved Subtitle Size",
      "steps": [
        {
          "kind": "run-command",
          "command": "applySubtitleSize"
        }
      ]
    },
    {
      "id": "applySubtitleSize",
      "name": "Apply Subtitle Size",
      "steps": [
        {
          "kind": "read-config-int",
          "configKey": "ui.subtitleSize",
          "valueName": "subtitleSizeState"
        },
        {
          "kind": "map-int-to-double",
          "inputValueName": "subtitleSizeState",
          "outputValueName": "subtitleScale",
          "mappings": [
            {
              "match": 0,
              "value": 2.0
            },
            {
              "match": 1,
              "value": 4.0
            },
            {
              "match": 2,
              "value": 8.0
            }
          ]
        },
        {
          "kind": "set-live-double",
          "target": "subtitle.scale",
          "valueName": "subtitleScale"
        },
        {
          "kind": "log-message",
          "message": "Applied Batman gameplay subtitle scale."
        }
      ]
    }
  ]
})");

        WriteAllText(
            valid_build_root / "hooks.json",
            R"({
  "runtimeSlots": [
    {
      "id": "subtitle.scale",
      "type": "float32",
      "initialValue": 1.5
    }
  ],
  "stateObservers": [
    {
      "id": "subtitleUiStateObserver",
      "scanStartAddress": "0x2B000000",
      "scanEndAddress": "0x30000000",
      "scanStride": 4,
      "valueOffset": 0,
      "pollIntervalMs": 250,
      "targetConfigKey": "ui.subtitleSize",
      "command": "applySubtitleSize",
      "checks": [
        {
          "comparison": "equals-constant",
          "offset": -16,
          "expectedValue": 50
        },
        {
          "comparison": "equals-value-at-offset",
          "offset": 16,
          "compareOffset": 0
        }
      ],
      "mappings": [
        {
          "match": 4101,
          "value": 0
        },
        {
          "match": 4102,
          "value": 1
        },
        {
          "match": 4103,
          "value": 2
        }
      ]
    }
  ],
  "hooks": [
    {
      "id": "subtitleTextScaleHook",
      "module": "ShippingPC-BmGame.exe",
      "rva": "0x006B00DA",
      "expectedBytes": "D9E8D9542404D91C24",
      "action": "inline-jump-to-pack-blob",
      "overwriteLength": 9,
      "resumeOffsetFromTarget": 45,
      "blob": {
        "assetPath": "assets/native/batman-global-text-scale.bin",
        "entryOffset": 0,
        "relocations": [
          {
            "offset": 2,
            "encoding": "abs32",
            "source": {
              "kind": "runtime-slot",
              "slot": "subtitle.scale"
            }
          },
          {
            "offset": 36,
            "encoding": "abs32",
            "source": {
              "kind": "runtime-slot",
              "slot": "subtitle.scale"
            }
          },
          {
            "offset": 58,
            "encoding": "rel32",
            "source": {
              "kind": "hook-resume"
            }
          }
        ]
      }
    }
  ]
})");

        WriteAllText(
            pack_root / "pack.json",
            R"({
  "schemaVersion": 1,
  "id": "broken-pack",
  "name": "Broken Pack",
  "targets": [
    {
      "executables": [
        "BrokenGame.exe"
      ]
    }
  ],
  "builds": [
    "broken-build"
  ]
})");

        WriteAllText(
            build_root / "build.json",
            R"({
  "id": "broken-build",
  "executable": "BrokenGame.exe",
  "match": {
    "fileSize": 1234,
    "sha256": "ABCDEF"
  }
})");

        WriteAllText(build_root / "hooks.json", "{");

        WriteAllText(
            mode_mismatch_pack_root / "pack.json",
            R"({
  "schemaVersion": 1,
  "id": "mode-mismatch-pack",
  "name": "Mode Mismatch Pack",
  "targets": [
    {
      "executables": [
        "ModeMismatchGame.exe"
      ]
    }
  ],
  "builds": [
    "mode-mismatch-build"
  ]
})");

        WriteAllText(
            mode_mismatch_build_root / "build.json",
            R"({
  "id": "mode-mismatch-build",
  "executable": "ModeMismatchGame.exe",
  "match": {
    "fileSize": 4321,
    "sha256": "1111111111111111111111111111111111111111111111111111111111111111"
  }
})");

        WriteAllText(
            mode_mismatch_build_root / "files.json",
            R"({
  "virtualFiles": [
    {
      "id": "mismatchVirtualFile",
      "path": "Game/Content/Test.bin",
      "mode": "delta-on-read",
      "source": "assets/packages/Test.bin"
    }
  ]
})");

        WriteAllText(mode_mismatch_build_root / "hooks.json", "{}");

        WriteAllText(
            malformed_delta_hash_pack_root / "pack.json",
            R"({
  "schemaVersion": 1,
  "id": "malformed-delta-hash-pack",
  "name": "Malformed Delta Hash Pack",
  "targets": [
    {
      "executables": [
        "MalformedDeltaHashGame.exe"
      ]
    }
  ],
  "builds": [
    "malformed-delta-hash-build"
  ]
})");

        WriteAllText(
            malformed_delta_hash_build_root / "build.json",
            R"({
  "id": "malformed-delta-hash-build",
  "executable": "MalformedDeltaHashGame.exe",
  "match": {
    "fileSize": 8765,
    "sha256": "2222222222222222222222222222222222222222222222222222222222222222"
  }
})");

        WriteAllText(
            malformed_delta_hash_build_root / "files.json",
            R"({
  "virtualFiles": [
    {
      "id": "badHashVirtualFile",
      "path": "Game/Content/Test.bin",
      "mode": "delta-on-read",
      "source": {
        "kind": "delta-file",
        "path": "assets/deltas/Test.hgdelta",
        "base": {
          "size": 16,
          "sha256": "ABCDEF"
        },
        "target": {
          "size": 24,
          "sha256": "12345Z"
        },
        "chunkSize": 4096
      }
    }
  ]
})");

        WriteAllText(malformed_delta_hash_build_root / "hooks.json", "{}");

        const std::optional<helen::LoadedBuildPack> loaded_valid_pack = repository.LoadForExecutable(
            packs_root,
            "ShippingPC-BmGame.exe",
            38758728,
            "4dac1f5e2ac6710b7378fdce74601f616f4753e3756cb5fda63c7519cc2eb028");
        Expect(loaded_valid_pack.has_value(), "Expected the synthetic Batman pack to load for the matching executable fingerprint.");
        Expect(loaded_valid_pack->Pack.Id == "batman-aa-subtitles", "Loaded pack identifier mismatch.");
        Expect(loaded_valid_pack->Build.Id == "steam-goty-1.0", "Loaded build identifier mismatch.");
        Expect(loaded_valid_pack->Build.StartupCommandIds.size() == 1, "Loaded startup command count mismatch.");
        Expect(loaded_valid_pack->Build.StartupCommandIds[0] == "applySavedSubtitleSize", "Loaded startup command identifier mismatch.");
        Expect(loaded_valid_pack->Build.VirtualFiles.size() == 2, "Loaded virtual file count mismatch.");

        const helen::VirtualFileDefinition* valid_gameplay_file = nullptr;
        const helen::VirtualFileDefinition* valid_frontend_file = nullptr;
        for (const helen::VirtualFileDefinition& virtual_file : loaded_valid_pack->Build.VirtualFiles)
        {
            if (virtual_file.Id == "bmgameGameplayPackage")
            {
                valid_gameplay_file = &virtual_file;
            }
            else if (virtual_file.Id == "frontendMapPackage")
            {
                valid_frontend_file = &virtual_file;
            }
        }

        Expect(valid_gameplay_file != nullptr, "Loaded gameplay virtual file was not found.");
        Expect(valid_frontend_file != nullptr, "Loaded frontend virtual file was not found.");
        Expect(valid_gameplay_file->Mode == "delta-on-read", "Gameplay virtual file mode mismatch.");
        Expect(valid_gameplay_file->Source.Kind == helen::VirtualFileSourceKind::DeltaFile, "Gameplay virtual file source kind mismatch.");
        Expect(valid_gameplay_file->Source.Path == std::filesystem::path("assets/deltas/BmGame-subtitle-signal.hgdelta"), "Gameplay virtual file source path mismatch.");
        Expect(valid_gameplay_file->Source.Base.FileSize == 101403981, "Gameplay virtual file base size mismatch.");
        Expect(valid_gameplay_file->Source.Base.Sha256 == "aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899", "Gameplay virtual file base hash normalization mismatch.");
        Expect(valid_gameplay_file->Source.Target.FileSize == 101405329, "Gameplay virtual file target size mismatch.");
        Expect(valid_gameplay_file->Source.Target.Sha256 == "ffeeddccbbaa99887766554433221100ffeeddccbbaa99887766554433221100", "Gameplay virtual file target hash normalization mismatch.");
        Expect(valid_gameplay_file->Source.ChunkSize == 65536, "Gameplay virtual file chunk size mismatch.");
        Expect(valid_frontend_file->Mode == "delta-on-read", "Frontend virtual file mode mismatch.");
        Expect(valid_frontend_file->Source.Kind == helen::VirtualFileSourceKind::DeltaFile, "Frontend virtual file source kind mismatch.");
        Expect(valid_frontend_file->GamePath == std::filesystem::path("BmGame/CookedPC/Maps/Frontend/Frontend.umap"), "Frontend virtual file path mismatch.");
        Expect(valid_frontend_file->Source.Path == std::filesystem::path("assets/deltas/Frontend-main-menu-subtitle-size.hgdelta"), "Frontend virtual file source path mismatch.");
        Expect(valid_frontend_file->Source.Base.FileSize == 2048, "Frontend virtual file base size mismatch.");
        Expect(valid_frontend_file->Source.Base.Sha256 == "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", "Frontend virtual file base hash normalization mismatch.");
        Expect(valid_frontend_file->Source.Target.FileSize == 2304, "Frontend virtual file target size mismatch.");
        Expect(valid_frontend_file->Source.Target.Sha256 == "ffeeddccbbaa99887766554433221100ffeeddccbbaa99887766554433221100", "Frontend virtual file target hash normalization mismatch.");
        Expect(valid_frontend_file->Source.ChunkSize == 65536, "Frontend virtual file chunk size mismatch.");
        Expect(loaded_valid_pack->Build.RuntimeSlots.size() == 1, "Loaded runtime slot count mismatch.");
        Expect(loaded_valid_pack->Build.StateObservers.size() == 1, "Loaded state observer count mismatch.");
        Expect(loaded_valid_pack->Build.Hooks.size() == 1, "Loaded hook count mismatch.");
        Expect(loaded_valid_pack->Build.Commands.size() == 2, "Loaded command count mismatch.");
        Expect(loaded_valid_pack->Build.Hooks[0].RelativeVirtualAddress.has_value(), "Loaded hook did not preserve its exact RVA target.");
        Expect(*loaded_valid_pack->Build.Hooks[0].RelativeVirtualAddress == 0x006B00DA, "Loaded hook RVA mismatch.");

        const std::optional<helen::LoadedBuildPack> mismatched_valid_pack = repository.LoadForExecutable(
            packs_root,
            "ShippingPC-BmGame.exe",
            38758728,
            "0000000000000000000000000000000000000000000000000000000000000000");
        Expect(!mismatched_valid_pack.has_value(), "Pack repository unexpectedly loaded a pack for a mismatched executable hash.");

        const std::optional<helen::LoadedBuildPack> malformed_pack = repository.LoadForExecutable(
            packs_root,
            "BrokenGame.exe",
            1234,
            "abcdef");
        Expect(!malformed_pack.has_value(), "Pack repository unexpectedly loaded a build whose hooks.json was malformed.");

        const std::optional<helen::LoadedBuildPack> mode_mismatch_pack = repository.LoadForExecutable(
            packs_root,
            "ModeMismatchGame.exe",
            4321,
            "1111111111111111111111111111111111111111111111111111111111111111");
        Expect(!mode_mismatch_pack.has_value(), "Pack repository unexpectedly loaded a delta-on-read virtual file without a delta-file source.");

        const std::optional<helen::LoadedBuildPack> malformed_delta_hash_pack = repository.LoadForExecutable(
            packs_root,
            "MalformedDeltaHashGame.exe",
            8765,
            "2222222222222222222222222222222222222222222222222222222222222222");
        Expect(!malformed_delta_hash_pack.has_value(), "Pack repository unexpectedly loaded a delta-backed virtual file with malformed SHA-256 metadata.");

        const std::optional<helen::LoadedBuildPack> loaded_batman_pack = repository.LoadForExecutable(
            GetBatmanPackRoot(),
            "ShippingPC-BmGame.exe",
            38758728,
            "4dac1f5e2ac6710b7378fdce74601f616f4753e3756cb5fda63c7519cc2eb028");
        Expect(loaded_batman_pack.has_value(), "Expected the checked-in Batman pack to load for the matching executable fingerprint.");
        Expect(loaded_batman_pack->Build.VirtualFiles.size() == 2, "Checked-in Batman pack virtual file count mismatch.");

        const helen::VirtualFileDefinition* checked_in_gameplay_file = nullptr;
        const helen::VirtualFileDefinition* checked_in_frontend_file = nullptr;
        for (const helen::VirtualFileDefinition& virtual_file : loaded_batman_pack->Build.VirtualFiles)
        {
            if (virtual_file.Id == "bmgameGameplayPackage")
            {
                checked_in_gameplay_file = &virtual_file;
            }
            else if (virtual_file.Id == "frontendMapPackage")
            {
                checked_in_frontend_file = &virtual_file;
            }
        }

        Expect(checked_in_gameplay_file != nullptr, "Checked-in Batman gameplay virtual file was not found.");
        Expect(checked_in_frontend_file != nullptr, "Checked-in Batman frontend virtual file was not found.");
        Expect(checked_in_gameplay_file->Mode == "delta-on-read", "Checked-in Batman gameplay package is not delta-backed.");
        Expect(checked_in_gameplay_file->Source.Kind == helen::VirtualFileSourceKind::DeltaFile, "Checked-in Batman gameplay package source kind mismatch.");
        Expect(checked_in_gameplay_file->Source.Path == std::filesystem::path("assets/deltas/BmGame-subtitle-signal.hgdelta"), "Checked-in Batman gameplay package delta path mismatch.");
        Expect(checked_in_gameplay_file->Source.Base.FileSize == 100365345, "Checked-in Batman gameplay package base size mismatch.");
        Expect(checked_in_gameplay_file->Source.Base.Sha256 == "621a5c8d99c9f7c7283531d05a4a6d56bdf15ad93ede0d5bf2f5d3e45117ff36", "Checked-in Batman gameplay package base hash mismatch.");
        Expect(checked_in_gameplay_file->Source.Target.FileSize == 101115741, "Checked-in Batman gameplay package target size mismatch.");
        Expect(checked_in_gameplay_file->Source.Target.Sha256 == "d1e97fbd81e5eec121538873b5fc95c4a47dca98dbae51898cf3e80210639d39", "Checked-in Batman gameplay package target hash mismatch.");
        Expect(checked_in_gameplay_file->Source.ChunkSize == 65536, "Checked-in Batman gameplay package chunk size mismatch.");
        Expect(checked_in_frontend_file->Mode == "delta-on-read", "Checked-in Batman frontend package is not delta-backed.");
        Expect(checked_in_frontend_file->Source.Kind == helen::VirtualFileSourceKind::DeltaFile, "Checked-in Batman frontend package source kind mismatch.");
        Expect(checked_in_frontend_file->GamePath == std::filesystem::path("BmGame/CookedPC/Maps/Frontend/Frontend.umap"), "Checked-in Batman frontend package path mismatch.");
        Expect(checked_in_frontend_file->Source.Path == std::filesystem::path("assets/deltas/Frontend-main-menu-subtitle-size.hgdelta"), "Checked-in Batman frontend delta path mismatch.");
        Expect(checked_in_frontend_file->Source.ChunkSize == 65536, "Checked-in Batman frontend chunk size mismatch.");
        Expect(loaded_batman_pack->Build.StateObservers.size() == 1, "Checked-in Batman pack state observer count mismatch.");
        Expect(loaded_batman_pack->Build.StateObservers[0].ScanStartAddress == 0x2B000000, "Checked-in Batman observer scan start drifted from the investigated hot heap window.");
        Expect(loaded_batman_pack->Build.StateObservers[0].ScanEndAddress == 0x30000000, "Checked-in Batman observer scan end drifted from the investigated hot heap window.");
    }
    catch (...)
    {
        std::filesystem::remove_all(root);
        throw;
    }

    std::filesystem::remove_all(root);
}
