#include <HelenHook/ActivePackSet.h>
#include <HelenHook/ActivePackSetBuilder.h>
#include <HelenHook/LoadedBuildPack.h>
#include <HelenHook/LoadedBuildPackSet.h>
#include <HelenHook/PackRepository.h>
#include <HelenHook/VirtualFileSourceKind.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <stdexcept>
#include <string_view>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

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
     * @brief Reads one complete UTF-8 text manifest from a temporary generator output file.
     * @param path Manifest file path that must be readable.
     * @return Complete manifest text, including every generated protocol member.
     */
    std::string ReadAllText(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            throw std::runtime_error("Failed to open a generated pack repository test file.");
        }

        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
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

    /**
     * @brief Creates a minimal split-pack fixture whose hooks manifest contains one observer scenario.
     * @param packs_root Root directory under which the synthetic pack directory should be created.
     * @param pack_id Stable identifier written to the pack manifest and used for the fixture directory.
     * @param build_id Stable identifier written to the build manifest and used for the fixture directory.
     * @param executable_name Executable name that the synthetic build declares and matches.
     * @param file_size Executable size used as the synthetic build fingerprint.
     * @param sha256 Executable SHA-256 text used as the synthetic build fingerprint.
     * @param hooks_json Complete hooks manifest payload to write for the synthetic build.
     */
    void WriteObserverPackFixture(
        const std::filesystem::path& packs_root,
        std::string_view pack_id,
        std::string_view build_id,
        std::string_view executable_name,
        std::uintmax_t file_size,
        std::string_view sha256,
        std::string_view hooks_json)
    {
        const std::filesystem::path build_root = packs_root / std::string(pack_id) / "builds" / std::string(build_id);
        std::filesystem::create_directories(build_root);

        const std::string pack_json =
            "{\n"
            "  \"schemaVersion\": 1,\n"
            "  \"id\": \"" + std::string(pack_id) + "\",\n"
            "  \"name\": \"Observer Protocol Test Pack\",\n"
            "  \"targets\": [\n"
            "    {\n"
            "      \"executables\": [\n"
            "        \"" + std::string(executable_name) + "\"\n"
            "      ]\n"
            "    }\n"
            "  ],\n"
            "  \"builds\": [\n"
            "    \"" + std::string(build_id) + "\"\n"
            "  ]\n"
            "}";
        WriteAllText(build_root.parent_path().parent_path() / "pack.json", pack_json);

        const std::string build_json =
            "{\n"
            "  \"id\": \"" + std::string(build_id) + "\",\n"
            "  \"executable\": \"" + std::string(executable_name) + "\",\n"
            "  \"match\": {\n"
            "    \"fileSize\": " + std::to_string(file_size) + ",\n"
            "    \"sha256\": \"" + std::string(sha256) + "\"\n"
            "  }\n"
            "}";
        WriteAllText(build_root / "build.json", build_json);
        WriteAllText(build_root / "hooks.json", hooks_json);
    }

    /**
     * @brief Materializes a temporary split pack from the protocol files emitted by the real rebuild script.
     * @param packs_root Root directory under which the generated protocol pack is created.
     * @param protocol_root Directory containing config.json, commands.json, and hooks.json from the script.
     *
     * The pack/build identity is deliberately synthetic; config, commands, and observers are read verbatim
     * from the generator output so this test cannot drift through a hand-maintained protocol fixture.
     */
    void WriteGeneratedProtocolPackFixture(
        const std::filesystem::path& packs_root,
        const std::filesystem::path& protocol_root)
    {
        const std::string config_json = ReadAllText(protocol_root / "config.json");
        const std::string commands_json = ReadAllText(protocol_root / "commands.json");
        const std::string hooks_json = ReadAllText(protocol_root / "hooks.json");
        const std::filesystem::path build_root = packs_root / "batman-aa-generated-protocol" / "builds" / "generated-protocol";
        std::filesystem::create_directories(build_root);

        const std::string pack_json =
            "{\n"
            "  \"schemaVersion\": 1,\n"
            "  \"id\": \"batman-aa-generated-protocol\",\n"
            "  \"name\": \"Generated Batman Graphics Protocol\",\n"
            "  \"targets\": [{\"executables\": [\"GeneratedBatmanProtocol.exe\"]}],\n"
            "  \"config\": " + config_json + ",\n"
            "  \"builds\": [\"generated-protocol\"]\n"
            "}";
        const std::string build_json =
            "{\n"
            "  \"id\": \"generated-protocol\",\n"
            "  \"executable\": \"GeneratedBatmanProtocol.exe\",\n"
            "  \"match\": {\"fileSize\": 123456, \"sha256\": \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}\n"
            "}";
        WriteAllText(build_root.parent_path().parent_path() / "pack.json", pack_json);
        WriteAllText(build_root / "build.json", build_json);
        WriteAllText(build_root / "commands.json", commands_json);
        WriteAllText(build_root / "hooks.json", hooks_json);
    }

    /**
     * @brief Runs the authoritative protocol-only generator and parses its output through PackRepository.
     * @param test_root Disposable root owned by the PackRepository test case.
     * @param repository Repository instance used to parse the generated split pack.
     *
     * This is intentionally an external console-only PowerShell invocation so native tests exercise the
     * exact rebuild construction path without rebuilding or overwriting the checked-in seven-file package.
     */
    void VerifyGeneratedBatmanProtocolPack(
        const std::filesystem::path& test_root,
        helen::PackRepository& repository)
    {
        const std::filesystem::path protocol_root = test_root / "generated-batman-protocol";
        const std::filesystem::path generated_packs_root = test_root / "generated-batman-packs";
        std::filesystem::remove_all(protocol_root);
        std::filesystem::remove_all(generated_packs_root);

        const std::filesystem::path script_path =
            std::filesystem::absolute(std::filesystem::path(__FILE__)).parent_path().parent_path().parent_path() / "games" / "HelenBatmanAA" / "scripts" / "Rebuild-BatmanGraphicsOptionsExperiment.ps1";
        const std::string command =
            "powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" + script_path.string() +
            "\" -ProtocolManifestPath \"" + protocol_root.string() + "\"";
        const int process_result = std::system(command.c_str());
        Expect(process_result == 0, "Protocol-only Batman graphics generator failed.");
        Expect(std::filesystem::is_regular_file(protocol_root / "config.json"), "Protocol-only generator did not write config.json.");
        Expect(std::filesystem::is_regular_file(protocol_root / "commands.json"), "Protocol-only generator did not write commands.json.");
        Expect(std::filesystem::is_regular_file(protocol_root / "hooks.json"), "Protocol-only generator did not write hooks.json.");

        const std::string hooks_json = ReadAllText(protocol_root / "hooks.json");
        const std::string dynamic_marker = "graphicsObserverDisplayModeCatalog";
        const std::size_t dynamic_start = hooks_json.find(dynamic_marker);
        Expect(dynamic_start != std::string::npos, "Generated protocol hooks omitted the dynamic display catalog observer.");
        const std::size_t next_observer = hooks_json.find("\"id\"", dynamic_start + dynamic_marker.size());
        const std::string dynamic_json = hooks_json.substr(dynamic_start, next_observer - dynamic_start);
        for (const std::string_view forbidden_member : {
                 "\"targetConfigKey\"",
                 "\"mappings\"",
                 "\"responseRequestValue\"",
                 "\"responseMappings\"",
                 "\"acknowledgementMappings\"",
                 "\"command\""})
        {
            Expect(dynamic_json.find(forbidden_member) == std::string::npos, "Generated dynamic observer contains a forbidden static-only member.");
        }

        WriteGeneratedProtocolPackFixture(generated_packs_root, protocol_root);
        const std::optional<helen::LoadedBuildPack> generated_pack = repository.LoadForExecutable(
            generated_packs_root,
            "GeneratedBatmanProtocol.exe",
            123456,
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        Expect(generated_pack.has_value(), "PackRepository rejected the generated Batman graphics protocol pack.");
        Expect(generated_pack->Pack.ConfigEntries.size() == 18, "Generated Batman protocol config count mismatch.");
        Expect(generated_pack->Build.Commands.size() == 4, "Generated Batman protocol command count mismatch.");
        Expect(generated_pack->Build.StateObservers.size() == 16, "Generated Batman protocol observer count mismatch.");
        Expect(generated_pack->Build.StateObservers[0].Mappings.size() == 2, "Generated fullscreen observer mapping count mismatch.");
        Expect(generated_pack->Build.StateObservers[2].Mappings.size() == 98, "Generated resolution observer mapping count mismatch.");
        Expect(generated_pack->Build.StateObservers[2].AcknowledgementMappings.size() == 98, "Generated resolution acknowledgement mapping count mismatch.");
        for (std::size_t observer_index = 3; observer_index < 14; ++observer_index)
        {
            Expect(!generated_pack->Build.StateObservers[observer_index].Mappings.empty(), "Generated finite graphics observer omitted its mappings.");
        }
        const helen::MemoryStateObserverDefinition& dynamic_observer = generated_pack->Build.StateObservers[1];
        Expect(dynamic_observer.TargetConfigKey.empty(), "Generated dynamic observer unexpectedly has a target config key.");
        Expect(dynamic_observer.Mappings.empty(), "Generated dynamic observer unexpectedly has static mappings.");
        Expect(dynamic_observer.ResponseMappings.empty(), "Generated dynamic observer unexpectedly has response mappings.");
        Expect(dynamic_observer.AcknowledgementMappings.empty(), "Generated dynamic observer unexpectedly has acknowledgement mappings.");
        Expect(dynamic_observer.DynamicResponseProviderId.has_value() && *dynamic_observer.DynamicResponseProviderId == "batmanDisplayModes", "Generated dynamic observer provider mismatch.");
        Expect(dynamic_observer.DynamicResponseRequestValues.size() == 599, "Generated dynamic observer request count mismatch.");
        for (std::size_t request_index = 0; request_index < 199; ++request_index)
        {
            Expect(dynamic_observer.DynamicResponseRequestValues[request_index] == 4700 + static_cast<int>(request_index), "Generated legacy catalog request order mismatch.");
            Expect(dynamic_observer.DynamicResponseRequestValues[199 + request_index] == 5200 + static_cast<int>(request_index), "Generated windowed catalog request order mismatch.");
            Expect(dynamic_observer.DynamicResponseRequestValues[398 + request_index] == 5400 + static_cast<int>(request_index), "Generated fullscreen catalog request order mismatch.");
        }
        Expect(dynamic_observer.DynamicResponseRequestValues[597] == 5600 && dynamic_observer.DynamicResponseRequestValues[598] == 5601, "Generated desktop catalog request order mismatch.");
        std::cout << "GENERATED_BATMAN_PROTOCOL_PASS\n";
    }

    /**
     * @brief Replaces one required text fragment in a synthetic observer manifest.
     * @param text Manifest text that should be copied and modified.
     * @param source Exact fragment that must occur once in the manifest text.
     * @param replacement Text that should replace the source fragment.
     * @return Modified manifest text.
     */
    std::string ReplaceObserverManifestText(std::string text, std::string_view source, std::string_view replacement)
    {
        if (source.empty())
        {
            throw std::runtime_error("Observer manifest test fragment must occur exactly once; the fragment is empty.");
        }

        const std::size_t first_position = text.find(source);
        if (first_position == std::string::npos)
        {
            throw std::runtime_error("Observer manifest test fragment must occur exactly once; it was not found.");
        }

        if (text.find(source, first_position + 1) != std::string::npos)
        {
            throw std::runtime_error("Observer manifest test fragment must occur exactly once; it occurred more than once.");
        }

        text.replace(first_position, source.size(), replacement);
        return text;
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
        const std::filesystem::path packset_subtitles_pack_root = packs_root / "packset-subtitles";
        const std::filesystem::path packset_subtitles_build_root = packset_subtitles_pack_root / "builds" / "packset-build";
        const std::filesystem::path skip_videos_pack_root = packs_root / "packset-skip-videos";
        const std::filesystem::path skip_videos_build_root = skip_videos_pack_root / "builds" / "steam-goty-1.0";
        const std::filesystem::path pack_root = packs_root / "broken-pack";
        const std::filesystem::path build_root = pack_root / "builds" / "broken-build";
        const std::filesystem::path mode_mismatch_pack_root = packs_root / "mode-mismatch-pack";
        const std::filesystem::path mode_mismatch_build_root = mode_mismatch_pack_root / "builds" / "mode-mismatch-build";
        const std::filesystem::path malformed_delta_hash_pack_root = packs_root / "malformed-delta-hash-pack";
        const std::filesystem::path malformed_delta_hash_build_root = malformed_delta_hash_pack_root / "builds" / "malformed-delta-hash-build";
        const std::filesystem::path duplicate_missing_path_pack_root = packs_root / "duplicate-missing-path-pack";
        const std::filesystem::path duplicate_missing_path_build_root = duplicate_missing_path_pack_root / "builds" / "duplicate-missing-path-build";
        const std::filesystem::path empty_address_match_pack_root = packs_root / "empty-address-match-pack";
        const std::filesystem::path empty_address_match_build_root = empty_address_match_pack_root / "builds" / "empty-address-match-build";
        std::filesystem::create_directories(valid_build_root);
        std::filesystem::create_directories(packset_subtitles_build_root);
        std::filesystem::create_directories(skip_videos_build_root);
        std::filesystem::create_directories(build_root);
        std::filesystem::create_directories(mode_mismatch_build_root);
        std::filesystem::create_directories(malformed_delta_hash_build_root);
        std::filesystem::create_directories(duplicate_missing_path_build_root);
        std::filesystem::create_directories(empty_address_match_build_root);

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
  "enableD3d9TextureReplacementHooks": true,
  "enableD3d9TextureHashLogging": true,
  "enableD3d9TextureImageDumping": true,
  "startupCommands": [
    "applySavedSubtitleSize"
  ],
  "missingPaths": [
    "BmGame/Movies/Legal.bik",
    "bmgame\\movies\\nvidia.bik"
  ],
  "match": {
    "fileSize": 38758728,
    "sha256": "4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028"
  }
})");

        WriteAllText(
            packset_subtitles_pack_root / "pack.json",
            R"({
  "schemaVersion": 1,
  "id": "packset-subtitles",
  "name": "Pack-Set Subtitles",
  "targets": [
    {
      "executables": [
        "PackSetGame.exe"
      ]
    }
  ],
  "builds": [
    "packset-build"
  ]
})");

        WriteAllText(
            packset_subtitles_build_root / "build.json",
            R"({
  "id": "packset-build",
  "executable": "PackSetGame.exe",
  "match": {
    "fileSize": 1111,
    "sha256": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
  }
})");

        WriteAllText(packset_subtitles_build_root / "files.json", R"({ "virtualFiles": [] })");
        WriteAllText(packset_subtitles_build_root / "bindings.json", R"({ "bindings": [] })");
        WriteAllText(packset_subtitles_build_root / "hooks.json", "{}");
        WriteAllText(packset_subtitles_build_root / "textures.json", R"({ "replacements": [] })");
        WriteAllText(packset_subtitles_build_root / "commands.json", R"({ "commands": [] })");

        WriteAllText(
            skip_videos_pack_root / "pack.json",
            R"({
  "schemaVersion": 1,
  "id": "packset-skip-videos",
  "name": "Pack-Set Skip Videos",
  "targets": [
    {
      "executables": [
        "PackSetGame.exe"
      ]
    }
  ],
  "builds": [
    "steam-goty-1.0"
  ]
})");

        WriteAllText(
            skip_videos_build_root / "build.json",
            R"({
  "id": "steam-goty-1.0",
  "executable": "PackSetGame.exe",
  "match": {
    "fileSize": 1111,
    "sha256": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
  }
})");

        WriteAllText(skip_videos_build_root / "files.json", R"({ "virtualFiles": [] })");
        WriteAllText(skip_videos_build_root / "bindings.json", R"({ "bindings": [] })");
        WriteAllText(skip_videos_build_root / "hooks.json", "{}");
        WriteAllText(skip_videos_build_root / "textures.json", R"({ "replacements": [] })");
        WriteAllText(skip_videos_build_root / "commands.json", R"({ "commands": [] })");

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
            valid_build_root / "textures.json",
            R"({
  "replacements": [
    {
      "id": "batman-subtitle-font-atlas",
      "api": "d3d9",
      "match": {
        "width": 1024,
        "height": 512,
        "format": "A8R8G8B8",
        "hash": "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
      },
      "replacement": {
        "path": "assets/textures/batman-subtitle-font-atlas.png"
      },
      "scope": {
        "samplerStage": 0
      }
    }
  ]
})");

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

        WriteAllText(
            duplicate_missing_path_pack_root / "pack.json",
            R"({
  "schemaVersion": 1,
  "id": "duplicate-missing-path-pack",
  "name": "Duplicate Missing Path Pack",
  "targets": [
    {
      "executables": [
        "DuplicateMissingPathGame.exe"
      ]
    }
  ],
  "builds": [
    "duplicate-missing-path-build"
  ]
})");

        WriteAllText(
            duplicate_missing_path_build_root / "build.json",
            R"({
  "id": "duplicate-missing-path-build",
  "executable": "DuplicateMissingPathGame.exe",
  "missingPaths": [
    "BmGame/Movies/Legal.bik",
    "bmgame\\movies\\legal.bik"
  ],
  "match": {
    "fileSize": 9999,
    "sha256": "3333333333333333333333333333333333333333333333333333333333333333"
  }
})");

        WriteAllText(duplicate_missing_path_build_root / "files.json", R"({ "virtualFiles": [] })");
        WriteAllText(duplicate_missing_path_build_root / "bindings.json", R"({ "bindings": [] })");
        WriteAllText(duplicate_missing_path_build_root / "hooks.json", "{}");
        WriteAllText(duplicate_missing_path_build_root / "textures.json", R"({ "replacements": [] })");
        WriteAllText(duplicate_missing_path_build_root / "commands.json", R"({ "commands": [] })");

        WriteAllText(
            empty_address_match_pack_root / "pack.json",
            R"({
  "schemaVersion": 1,
  "id": "empty-address-match-pack",
  "name": "Empty Address Match Pack",
  "targets": [
    {
      "executables": [
        "EmptyAddressMatchGame.exe"
      ]
    }
  ],
  "builds": [
    "empty-address-match-build"
  ]
})");
        WriteAllText(
            empty_address_match_build_root / "build.json",
            R"({
  "id": "empty-address-match-build",
  "executable": "EmptyAddressMatchGame.exe",
  "match": {
    "fileSize": 2222,
    "sha256": "2222222222222222222222222222222222222222222222222222222222222222"
  }
})");
        WriteAllText(
            empty_address_match_build_root / "hooks.json",
            R"({
  "stateObservers": [
    {
      "id": "emptyAddressMatchObserver",
      "scanStartAddress": "0x2B000000",
      "scanEndAddress": "0x30000000",
      "scanStride": 4,
      "valueOffset": 0,
      "pollIntervalMs": 50,
      "targetConfigKey": "empty.addressMatch",
      "checks": [
        {
          "comparison": "equals-constant",
          "offset": -16,
          "expectedValue": 50
        }
      ],
      "mappings": [
        {
          "match": 4101,
          "value": 0
        }
      ],
      "addressMatchValues": []
    }
  ]
})");

        const std::string transactional_observer_hooks = R"({
  "stateObservers": [
    {
      "id": "graphicsObserverMsaa",
      "addressGroup": "batmanFrontendControlType",
      "scanStartAddress": "0x10000000",
      "scanEndAddress": "0x30000000",
      "scanStride": 4,
      "valueOffset": 12,
      "pollIntervalMs": 50,
      "targetConfigKey": "msaa",
      "checks": [
        {
          "comparison": "equals-constant",
          "offset": 0,
          "expectedValue": 4102
        }
      ],
      "addressMatchValues": [
        4300,
        4310,
        4311,
        4312,
        4313,
        4314,
        4320,
        4321,
        4322,
        4323,
        4324,
        4330,
        4331,
        4332,
        4333,
        4334,
        4399
      ],
      "mappings": [
        {
          "match": 4320,
          "value": 0
        },
        {
          "match": 4321,
          "value": 1
        },
        {
          "match": 4322,
          "value": 2
        },
        {
          "match": 4323,
          "value": 3
        },
        {
          "match": 4324,
          "value": 5
        }
      ],
      "responseRequestValue": 4300,
      "responseMappings": [
        {
          "match": 0,
          "value": 4310
        },
        {
          "match": 1,
          "value": 4311
        },
        {
          "match": 2,
          "value": 4312
        },
        {
          "match": 3,
          "value": 4313
        },
        {
          "match": 5,
          "value": 4314
        }
      ],
      "acknowledgementMappings": [
        {
          "match": 4320,
          "value": 4330
        },
        {
          "match": 4321,
          "value": 4331
        },
        {
          "match": 4322,
          "value": 4332
        },
        {
          "match": 4323,
          "value": 4333
        },
        {
          "match": 4324,
          "value": 4334
        }
      ],
      "failureResponseValue": 4399
    }
  ]
})";
        const std::string observer_hash(64, 'a');

        const std::string dynamic_response_observer_hooks = R"({
  "stateObservers": [
    {
      "id": "batmanDisplayModeObserver",
      "scanStartAddress": "0x10000000",
      "scanEndAddress": "0x30000000",
      "scanStride": 4,
      "valueOffset": 12,
      "pollIntervalMs": 50,
      "checks": [
        {
          "comparison": "equals-constant",
          "offset": 0,
          "expectedValue": 4102
        }
      ],
      "addressMatchValues": [
        4700,
        4701,
        4702,
        4799
      ],
      "dynamicResponse": {
        "provider": "batmanDisplayModes",
        "requests": [4700, 4701, 4702],
        "minimumValue": 1,
        "maximumValue": 32767
      },
      "failureResponseValue": 4799
    }
  ]
})";
        WriteObserverPackFixture(
            packs_root,
            "dynamic-response-observer-pack",
            "dynamic-response-observer-build",
            "DynamicResponseObserverGame.exe",
            5015,
            observer_hash,
            dynamic_response_observer_hooks);

        const std::optional<helen::LoadedBuildPack> loaded_dynamic_response_pack = repository.LoadForExecutable(
            packs_root,
            "DynamicResponseObserverGame.exe",
            5015,
            observer_hash);
        Expect(loaded_dynamic_response_pack.has_value(), "Pack repository rejected a valid dynamic response-only observer.");
        Expect(loaded_dynamic_response_pack->Build.StateObservers.size() == 1, "Dynamic response observer count mismatch.");
        const helen::MemoryStateObserverDefinition& dynamic_response_observer = loaded_dynamic_response_pack->Build.StateObservers[0];
        Expect(dynamic_response_observer.Id == "batmanDisplayModeObserver", "Dynamic response observer identifier mismatch.");
        Expect(dynamic_response_observer.TargetConfigKey.empty(), "Dynamic response-only observer unexpectedly declared a target config key.");
        Expect(
            dynamic_response_observer.DynamicResponseProviderId.has_value() &&
                *dynamic_response_observer.DynamicResponseProviderId == "batmanDisplayModes",
            "Dynamic response provider mismatch.");
        const int expected_dynamic_requests[] = { 4700, 4701, 4702 };
        Expect(
            dynamic_response_observer.DynamicResponseRequestValues.size() == std::size(expected_dynamic_requests),
            "Dynamic response request count mismatch.");
        for (std::size_t index = 0; index < std::size(expected_dynamic_requests); ++index)
        {
            Expect(
                dynamic_response_observer.DynamicResponseRequestValues[index] == expected_dynamic_requests[index],
                "Dynamic response request value mismatch.");
        }
        Expect(dynamic_response_observer.DynamicResponseMinimumValue == 1, "Dynamic response minimum bound mismatch.");
        Expect(dynamic_response_observer.DynamicResponseMaximumValue == 32767, "Dynamic response maximum bound mismatch.");
        Expect(dynamic_response_observer.Mappings.empty(), "Dynamic response-only observer unexpectedly declared update mappings.");
        Expect(!dynamic_response_observer.ResponseRequestValue.has_value(), "Dynamic response-only observer unexpectedly declared a static request value.");
        Expect(dynamic_response_observer.ResponseMappings.empty(), "Dynamic response-only observer unexpectedly declared static response mappings.");
        Expect(dynamic_response_observer.AcknowledgementMappings.empty(), "Dynamic response-only observer unexpectedly declared acknowledgement mappings.");
        Expect(
            dynamic_response_observer.FailureResponseValue.has_value() && *dynamic_response_observer.FailureResponseValue == 4799,
            "Dynamic response failure response mismatch.");

        const std::string dynamic_empty_provider_hooks = ReplaceObserverManifestText(
            dynamic_response_observer_hooks,
            "\"provider\": \"batmanDisplayModes\"",
            "\"provider\": \"\"");
        WriteObserverPackFixture(
            packs_root,
            "dynamic-empty-provider-pack",
            "dynamic-empty-provider-build",
            "DynamicEmptyProviderGame.exe",
            5016,
            observer_hash,
            dynamic_empty_provider_hooks);

        const std::string dynamic_empty_requests_hooks = ReplaceObserverManifestText(
            dynamic_response_observer_hooks,
            "\"requests\": [4700, 4701, 4702]",
            "\"requests\": []");
        WriteObserverPackFixture(
            packs_root,
            "dynamic-empty-requests-pack",
            "dynamic-empty-requests-build",
            "DynamicEmptyRequestsGame.exe",
            5017,
            observer_hash,
            dynamic_empty_requests_hooks);

        const std::string dynamic_duplicate_requests_hooks = ReplaceObserverManifestText(
            dynamic_response_observer_hooks,
            "\"requests\": [4700, 4701, 4702]",
            "\"requests\": [4700, 4700, 4702]");
        WriteObserverPackFixture(
            packs_root,
            "dynamic-duplicate-requests-pack",
            "dynamic-duplicate-requests-build",
            "DynamicDuplicateRequestsGame.exe",
            5018,
            observer_hash,
            dynamic_duplicate_requests_hooks);

        const std::string dynamic_nonpositive_bounds_hooks = ReplaceObserverManifestText(
            dynamic_response_observer_hooks,
            "\"minimumValue\": 1",
            "\"minimumValue\": 0");
        WriteObserverPackFixture(
            packs_root,
            "dynamic-nonpositive-bounds-pack",
            "dynamic-nonpositive-bounds-build",
            "DynamicNonpositiveBoundsGame.exe",
            5019,
            observer_hash,
            dynamic_nonpositive_bounds_hooks);

        const std::string dynamic_reversed_bounds_hooks = ReplaceObserverManifestText(
            dynamic_response_observer_hooks,
            "\"minimumValue\": 1",
            "\"minimumValue\": 32768");
        WriteObserverPackFixture(
            packs_root,
            "dynamic-reversed-bounds-pack",
            "dynamic-reversed-bounds-build",
            "DynamicReversedBoundsGame.exe",
            5020,
            observer_hash,
            dynamic_reversed_bounds_hooks);

        const std::string dynamic_request_absent_hooks = ReplaceObserverManifestText(
            dynamic_response_observer_hooks,
            "\"requests\": [4700, 4701, 4702]",
            "\"requests\": [4700, 4701, 4800]");
        WriteObserverPackFixture(
            packs_root,
            "dynamic-request-absent-pack",
            "dynamic-request-absent-build",
            "DynamicRequestAbsentGame.exe",
            5021,
            observer_hash,
            dynamic_request_absent_hooks);

        const std::string dynamic_static_response_hooks = ReplaceObserverManifestText(
            dynamic_response_observer_hooks,
            "      \"failureResponseValue\": 4799",
            "      \"responseRequestValue\": 4700,\n"
            "      \"responseMappings\": [\n"
            "        {\n"
            "          \"match\": 1,\n"
            "          \"value\": 4701\n"
            "        }\n"
            "      ],\n"
            "      \"failureResponseValue\": 4799");
        WriteObserverPackFixture(
            packs_root,
            "dynamic-static-response-pack",
            "dynamic-static-response-build",
            "DynamicStaticResponseGame.exe",
            5022,
            observer_hash,
            dynamic_static_response_hooks);

        const std::string dynamic_update_mappings_hooks = ReplaceObserverManifestText(
            dynamic_response_observer_hooks,
            "      \"failureResponseValue\": 4799",
            "      \"mappings\": [\n"
            "        {\n"
            "          \"match\": 4700,\n"
            "          \"value\": 1\n"
            "        }\n"
            "      ],\n"
            "      \"failureResponseValue\": 4799");
        WriteObserverPackFixture(
            packs_root,
            "dynamic-update-mappings-pack",
            "dynamic-update-mappings-build",
            "DynamicUpdateMappingsGame.exe",
            5023,
            observer_hash,
            dynamic_update_mappings_hooks);

        const std::string dynamic_without_failure_hooks = ReplaceObserverManifestText(
            dynamic_response_observer_hooks,
            ",\n      \"failureResponseValue\": 4799",
            "");
        WriteObserverPackFixture(
            packs_root,
            "dynamic-without-failure-pack",
            "dynamic-without-failure-build",
            "DynamicWithoutFailureGame.exe",
            5024,
            observer_hash,
            dynamic_without_failure_hooks);

        const std::string dynamic_unknown_member_hooks = ReplaceObserverManifestText(
            dynamic_response_observer_hooks,
            "\"maximumValue\": 32767",
            "\"maximumValue\": 32767,\n"
            "        \"unexpected\": 1");
        WriteObserverPackFixture(
            packs_root,
            "dynamic-unknown-member-pack",
            "dynamic-unknown-member-build",
            "DynamicUnknownMemberGame.exe",
            5025,
            observer_hash,
            dynamic_unknown_member_hooks);

        const std::string dynamic_command_hooks = ReplaceObserverManifestText(
            dynamic_response_observer_hooks,
            "      \"pollIntervalMs\": 50,",
            "      \"pollIntervalMs\": 50,\n"
            "      \"command\": \"unused\",");
        WriteObserverPackFixture(
            packs_root,
            "dynamic-command-pack",
            "dynamic-command-build",
            "DynamicCommandGame.exe",
            5026,
            observer_hash,
            dynamic_command_hooks);

        const std::string dynamic_string_target_hooks = ReplaceObserverManifestText(
            dynamic_response_observer_hooks,
            "      \"pollIntervalMs\": 50,",
            "      \"pollIntervalMs\": 50,\n"
            "      \"targetConfigKey\": \"unused\",");
        WriteObserverPackFixture(
            packs_root,
            "dynamic-string-target-pack",
            "dynamic-string-target-build",
            "DynamicStringTargetGame.exe",
            5027,
            observer_hash,
            dynamic_string_target_hooks);

        const std::string dynamic_integer_target_hooks = ReplaceObserverManifestText(
            dynamic_response_observer_hooks,
            "      \"pollIntervalMs\": 50,",
            "      \"pollIntervalMs\": 50,\n"
            "      \"targetConfigKey\": 17,");
        WriteObserverPackFixture(
            packs_root,
            "dynamic-integer-target-pack",
            "dynamic-integer-target-build",
            "DynamicIntegerTargetGame.exe",
            5028,
            observer_hash,
            dynamic_integer_target_hooks);

        const std::string dynamic_null_target_hooks = ReplaceObserverManifestText(
            dynamic_response_observer_hooks,
            "      \"pollIntervalMs\": 50,",
            "      \"pollIntervalMs\": 50,\n"
            "      \"targetConfigKey\": null,");
        WriteObserverPackFixture(
            packs_root,
            "dynamic-null-target-pack",
            "dynamic-null-target-build",
            "DynamicNullTargetGame.exe",
            5029,
            observer_hash,
            dynamic_null_target_hooks);

        const std::string dynamic_array_target_hooks = ReplaceObserverManifestText(
            dynamic_response_observer_hooks,
            "      \"pollIntervalMs\": 50,",
            "      \"pollIntervalMs\": 50,\n"
            "      \"targetConfigKey\": [],");
        WriteObserverPackFixture(
            packs_root,
            "dynamic-array-target-pack",
            "dynamic-array-target-build",
            "DynamicArrayTargetGame.exe",
            5030,
            observer_hash,
            dynamic_array_target_hooks);

        const std::string_view rejected_dynamic_executable_names[] = {
            "DynamicEmptyProviderGame.exe",
            "DynamicEmptyRequestsGame.exe",
            "DynamicDuplicateRequestsGame.exe",
            "DynamicNonpositiveBoundsGame.exe",
            "DynamicReversedBoundsGame.exe",
            "DynamicRequestAbsentGame.exe",
            "DynamicStaticResponseGame.exe",
            "DynamicUpdateMappingsGame.exe",
            "DynamicWithoutFailureGame.exe",
            "DynamicUnknownMemberGame.exe"
        };
        const std::uintmax_t rejected_dynamic_file_sizes[] = {
            5016,
            5017,
            5018,
            5019,
            5020,
            5021,
            5022,
            5023,
            5024,
            5025
        };
        const char* rejected_dynamic_messages[] = {
            "Pack repository accepted a dynamic observer with an empty provider.",
            "Pack repository accepted a dynamic observer with empty requests.",
            "Pack repository accepted duplicate dynamic observer requests.",
            "Pack repository accepted nonpositive dynamic observer bounds.",
            "Pack repository accepted dynamic observer bounds with minimum greater than maximum.",
            "Pack repository accepted a dynamic request absent from address matches.",
            "Pack repository accepted static response mappings on a dynamic observer.",
            "Pack repository accepted update mappings on a dynamic observer.",
            "Pack repository accepted a dynamic observer without a failure response.",
            "Pack repository accepted an unknown dynamic response member."
        };
        for (std::size_t index = 0; index < std::size(rejected_dynamic_executable_names); ++index)
        {
            const std::optional<helen::LoadedBuildPack> rejected_dynamic_pack = repository.LoadForExecutable(
                packs_root,
                std::string(rejected_dynamic_executable_names[index]),
                rejected_dynamic_file_sizes[index],
                observer_hash);
            Expect(!rejected_dynamic_pack.has_value(), rejected_dynamic_messages[index]);
        }

        const std::string_view rejected_dynamic_shape_executable_names[] = {
            "DynamicCommandGame.exe",
            "DynamicStringTargetGame.exe",
            "DynamicIntegerTargetGame.exe",
            "DynamicNullTargetGame.exe",
            "DynamicArrayTargetGame.exe"
        };
        const std::uintmax_t rejected_dynamic_shape_file_sizes[] = {
            5026,
            5027,
            5028,
            5029,
            5030
        };
        const char* rejected_dynamic_shape_messages[] = {
            "Pack repository accepted a dynamic observer with a command member.",
            "Pack repository accepted a dynamic observer with a target config string.",
            "Pack repository accepted a dynamic observer with a target config integer.",
            "Pack repository accepted a dynamic observer with a null target config.",
            "Pack repository accepted a dynamic observer with an array target config."
        };
        for (std::size_t index = 0; index < std::size(rejected_dynamic_shape_executable_names); ++index)
        {
            const std::optional<helen::LoadedBuildPack> rejected_dynamic_shape_pack = repository.LoadForExecutable(
                packs_root,
                std::string(rejected_dynamic_shape_executable_names[index]),
                rejected_dynamic_shape_file_sizes[index],
                observer_hash);
            Expect(!rejected_dynamic_shape_pack.has_value(), rejected_dynamic_shape_messages[index]);
        }

        WriteObserverPackFixture(
            packs_root,
            "transactional-observer-pack",
            "transactional-observer-build",
            "TransactionalObserverGame.exe",
            5001,
            observer_hash,
            transactional_observer_hooks);

        const std::optional<helen::LoadedBuildPack> loaded_transactional_observer_pack = repository.LoadForExecutable(
            packs_root,
            "TransactionalObserverGame.exe",
            5001,
            observer_hash);
        Expect(loaded_transactional_observer_pack.has_value(), "Pack repository rejected a valid grouped transactional observer.");
        Expect(loaded_transactional_observer_pack->Build.StateObservers.size() == 1, "Transactional observer count mismatch.");
        const helen::MemoryStateObserverDefinition& transactional_observer = loaded_transactional_observer_pack->Build.StateObservers[0];
        Expect(
            transactional_observer.AddressGroup.has_value() && *transactional_observer.AddressGroup == "batmanFrontendControlType",
            "Transactional observer address group mismatch.");
        const int expected_acknowledgement_matches[] = { 4320, 4321, 4322, 4323, 4324 };
        const int expected_acknowledgement_values[] = { 4330, 4331, 4332, 4333, 4334 };
        Expect(transactional_observer.AcknowledgementMappings.size() == std::size(expected_acknowledgement_matches), "Transactional acknowledgement mapping count mismatch.");
        for (std::size_t index = 0; index < std::size(expected_acknowledgement_matches); ++index)
        {
            Expect(
                transactional_observer.AcknowledgementMappings[index].Match == expected_acknowledgement_matches[index] &&
                    transactional_observer.AcknowledgementMappings[index].Value == expected_acknowledgement_values[index],
                "Transactional acknowledgement mapping mismatch.");
        }
        Expect(
            transactional_observer.FailureResponseValue.has_value() && *transactional_observer.FailureResponseValue == 4399,
            "Transactional observer failure response mismatch.");

        const std::string empty_address_group_hooks = ReplaceObserverManifestText(
            transactional_observer_hooks,
            "\"addressGroup\": \"batmanFrontendControlType\"",
            "\"addressGroup\": \"\"");
        WriteObserverPackFixture(
            packs_root,
            "observer-empty-address-group-pack",
            "observer-empty-address-group-build",
            "ObserverEmptyAddressGroupGame.exe",
            5002,
            observer_hash,
            empty_address_group_hooks);

        const std::string acknowledgement_without_failure_hooks = ReplaceObserverManifestText(
            transactional_observer_hooks,
            ",\n      \"failureResponseValue\": 4399",
            "");
        WriteObserverPackFixture(
            packs_root,
            "observer-ack-without-failure-pack",
            "observer-ack-without-failure-build",
            "ObserverAckWithoutFailureGame.exe",
            5003,
            observer_hash,
            acknowledgement_without_failure_hooks);

        const std::string failure_without_acknowledgement_hooks = ReplaceObserverManifestText(
            transactional_observer_hooks,
            R"(,
      "acknowledgementMappings": [
        {
          "match": 4320,
          "value": 4330
        },
        {
          "match": 4321,
          "value": 4331
        },
        {
          "match": 4322,
          "value": 4332
        },
        {
          "match": 4323,
          "value": 4333
        },
        {
          "match": 4324,
          "value": 4334
        }
      ])",
            "");
        WriteObserverPackFixture(
            packs_root,
            "observer-failure-without-ack-pack",
            "observer-failure-without-ack-build",
            "ObserverFailureWithoutAckGame.exe",
            5004,
            observer_hash,
            failure_without_acknowledgement_hooks);

        const std::string acknowledgement_input_absent_hooks = ReplaceObserverManifestText(
            transactional_observer_hooks,
            "\"match\": 4320,\n          \"value\": 4330",
            "\"match\": 4340,\n          \"value\": 4330");
        WriteObserverPackFixture(
            packs_root,
            "observer-ack-input-absent-pack",
            "observer-ack-input-absent-build",
            "ObserverAckInputAbsentGame.exe",
            5005,
            observer_hash,
            acknowledgement_input_absent_hooks);

        const std::string acknowledgement_output_absent_hooks = ReplaceObserverManifestText(
            transactional_observer_hooks,
            "\"match\": 4320,\n          \"value\": 4330",
            "\"match\": 4320,\n          \"value\": 4340");
        WriteObserverPackFixture(
            packs_root,
            "observer-ack-output-absent-pack",
            "observer-ack-output-absent-build",
            "ObserverAckOutputAbsentGame.exe",
            5006,
            observer_hash,
            acknowledgement_output_absent_hooks);

        const std::string failure_response_absent_hooks = ReplaceObserverManifestText(
            transactional_observer_hooks,
            "\"failureResponseValue\": 4399",
            "\"failureResponseValue\": 4400");
        WriteObserverPackFixture(
            packs_root,
            "observer-failure-response-absent-pack",
            "observer-failure-response-absent-build",
            "ObserverFailureResponseAbsentGame.exe",
            5007,
            observer_hash,
            failure_response_absent_hooks);

        const std::string duplicate_acknowledgement_input_hooks = ReplaceObserverManifestText(
            transactional_observer_hooks,
            "\"match\": 4321,\n          \"value\": 4331",
            "\"match\": 4320,\n          \"value\": 4331");
        WriteObserverPackFixture(
            packs_root,
            "observer-duplicate-ack-input-pack",
            "observer-duplicate-ack-input-build",
            "ObserverDuplicateAckInputGame.exe",
            5008,
            observer_hash,
            duplicate_acknowledgement_input_hooks);

        const std::string acknowledgement_input_without_mapping_hooks = ReplaceObserverManifestText(
            transactional_observer_hooks,
            R"(        {
          "match": 4320,
          "value": 0
        },
)",
            "");
        WriteObserverPackFixture(
            packs_root,
            "observer-ack-input-without-mapping-pack",
            "observer-ack-input-without-mapping-build",
            "ObserverAckInputWithoutMappingGame.exe",
            5009,
            observer_hash,
            acknowledgement_input_without_mapping_hooks);

        const std::string response_mappings_without_request_hooks = ReplaceObserverManifestText(
            transactional_observer_hooks,
            "      \"responseRequestValue\": 4300,\n",
            "");
        WriteObserverPackFixture(
            packs_root,
            "observer-response-without-request-pack",
            "observer-response-without-request-build",
            "ObserverResponseWithoutRequestGame.exe",
            5010,
            observer_hash,
            response_mappings_without_request_hooks);

        const std::string wrong_type_command_hooks = ReplaceObserverManifestText(
            transactional_observer_hooks,
            "\"id\": \"graphicsObserverMsaa\",\n",
            "\"id\": \"graphicsObserverMsaa\",\n      \"command\": 17,\n");
        WriteObserverPackFixture(
            packs_root,
            "observer-wrong-command-type-pack",
            "observer-wrong-command-type-build",
            "ObserverWrongCommandTypeGame.exe",
            5011,
            observer_hash,
            wrong_type_command_hooks);

        const std::string wrong_type_address_group_hooks = ReplaceObserverManifestText(
            transactional_observer_hooks,
            "\"addressGroup\": \"batmanFrontendControlType\"",
            "\"addressGroup\": 17");
        WriteObserverPackFixture(
            packs_root,
            "observer-wrong-address-group-type-pack",
            "observer-wrong-address-group-type-build",
            "ObserverWrongAddressGroupTypeGame.exe",
            5012,
            observer_hash,
            wrong_type_address_group_hooks);

        const std::string wrong_type_response_request_hooks = ReplaceObserverManifestText(
            transactional_observer_hooks,
            "\"responseRequestValue\": 4300",
            "\"responseRequestValue\": \"4300\"");
        WriteObserverPackFixture(
            packs_root,
            "observer-wrong-response-request-type-pack",
            "observer-wrong-response-request-type-build",
            "ObserverWrongResponseRequestTypeGame.exe",
            5013,
            observer_hash,
            wrong_type_response_request_hooks);

        const std::string wrong_type_failure_response_hooks = ReplaceObserverManifestText(
            transactional_observer_hooks,
            "\"failureResponseValue\": 4399",
            "\"failureResponseValue\": \"4399\"");
        WriteObserverPackFixture(
            packs_root,
            "observer-wrong-failure-response-type-pack",
            "observer-wrong-failure-response-type-build",
            "ObserverWrongFailureResponseTypeGame.exe",
            5014,
            observer_hash,
            wrong_type_failure_response_hooks);

        const std::string_view rejected_observer_executable_names[] = {
            "ObserverEmptyAddressGroupGame.exe",
            "ObserverAckWithoutFailureGame.exe",
            "ObserverFailureWithoutAckGame.exe",
            "ObserverAckInputAbsentGame.exe",
            "ObserverAckOutputAbsentGame.exe",
            "ObserverFailureResponseAbsentGame.exe",
            "ObserverDuplicateAckInputGame.exe",
            "ObserverAckInputWithoutMappingGame.exe",
            "ObserverResponseWithoutRequestGame.exe",
            "ObserverWrongCommandTypeGame.exe",
            "ObserverWrongAddressGroupTypeGame.exe",
            "ObserverWrongResponseRequestTypeGame.exe",
            "ObserverWrongFailureResponseTypeGame.exe"
        };
        const std::uintmax_t rejected_observer_file_sizes[] = {
            5002,
            5003,
            5004,
            5005,
            5006,
            5007,
            5008,
            5009,
            5010,
            5011,
            5012,
            5013,
            5014
        };
        const char* rejected_observer_messages[] = {
            "Pack repository accepted an observer with an empty address group.",
            "Pack repository accepted acknowledgement mappings without a failure response.",
            "Pack repository accepted a failure response without acknowledgement mappings.",
            "Pack repository accepted an acknowledgement input absent from mappings.",
            "Pack repository accepted an acknowledgement output absent from address matches.",
            "Pack repository accepted a failure response absent from address matches.",
            "Pack repository accepted duplicate acknowledgement inputs.",
            "Pack repository accepted an acknowledgement input with no config mapping.",
            "Pack repository accepted response mappings without a response request value.",
            "Pack repository accepted a command with the wrong JSON type.",
            "Pack repository accepted an address group with the wrong JSON type.",
            "Pack repository accepted a response request with the wrong JSON type.",
            "Pack repository accepted a failure response with the wrong JSON type."
        };
        for (std::size_t index = 0; index < std::size(rejected_observer_executable_names); ++index)
        {
            const std::optional<helen::LoadedBuildPack> rejected_pack = repository.LoadForExecutable(
                packs_root,
                std::string(rejected_observer_executable_names[index]),
                rejected_observer_file_sizes[index],
                observer_hash);
            Expect(!rejected_pack.has_value(), rejected_observer_messages[index]);
        }

        const std::optional<helen::LoadedBuildPack> loaded_valid_pack = repository.LoadForExecutable(
            packs_root,
            "ShippingPC-BmGame.exe",
            38758728,
            "4dac1f5e2ac6710b7378fdce74601f616f4753e3756cb5fda63c7519cc2eb028");
        Expect(loaded_valid_pack.has_value(), "Expected the synthetic Batman pack to load for the matching executable fingerprint.");
        Expect(loaded_valid_pack->Pack.Id == "batman-aa-subtitles", "Loaded pack identifier mismatch.");
        Expect(loaded_valid_pack->Build.Id == "steam-goty-1.0", "Loaded build identifier mismatch.");
        Expect(loaded_valid_pack->Build.EnableD3d9TextureReplacementHooks, "Loaded build should enable D3D9 texture replacement hooks.");
        Expect(loaded_valid_pack->Build.EnableD3d9TextureHashLogging, "Loaded build should enable D3D9 texture hash logging.");
        Expect(loaded_valid_pack->Build.EnableD3d9TextureImageDumping, "Loaded build should enable D3D9 texture image dumping.");
        Expect(loaded_valid_pack->Build.StartupCommandIds.size() == 1, "Loaded startup command count mismatch.");
        Expect(loaded_valid_pack->Build.StartupCommandIds[0] == "applySavedSubtitleSize", "Loaded startup command identifier mismatch.");
        Expect(loaded_valid_pack->Build.MissingPaths.size() == 2, "Loaded missing-path count mismatch.");
        Expect(loaded_valid_pack->Build.MissingPaths[0] == "bmgame/movies/legal.bik", "Loaded missing path normalization mismatch for Legal.bik.");
        Expect(loaded_valid_pack->Build.MissingPaths[1] == "bmgame/movies/nvidia.bik", "Loaded missing path normalization mismatch for nvidia.bik.");
        Expect(loaded_valid_pack->Build.VirtualFiles.size() == 2, "Loaded virtual file count mismatch.");

        const std::optional<helen::LoadedBuildPackSet> loaded_pack_set = repository.LoadPackSetForExecutable(
            packs_root,
            "PackSetGame.exe",
            1111,
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            { "packset-subtitles", "packset-skip-videos" });
        Expect(loaded_pack_set.has_value(), "Expected an ordered pack set to load for the matching executable fingerprint.");
        Expect(loaded_pack_set->Packs.size() == 2, "Loaded pack-set size mismatch.");
        Expect(loaded_pack_set->Packs[0].Pack.Id == "packset-subtitles", "Loaded pack-set order mismatch for subtitles.");
        Expect(loaded_pack_set->Packs[1].Pack.Id == "packset-skip-videos", "Loaded pack-set order mismatch for skip videos.");

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
        Expect(loaded_valid_pack->Build.StateObservers[0].AddressMatchValues.size() == 3, "Legacy observer address-match value count mismatch.");
        Expect(loaded_valid_pack->Build.StateObservers[0].AddressMatchValues[0] == 4101, "Legacy observer first address-match value mismatch.");
        Expect(loaded_valid_pack->Build.StateObservers[0].AddressMatchValues[1] == 4102, "Legacy observer second address-match value mismatch.");
        Expect(loaded_valid_pack->Build.StateObservers[0].AddressMatchValues[2] == 4103, "Legacy observer third address-match value mismatch.");
        Expect(loaded_valid_pack->Build.Hooks.size() == 1, "Loaded hook count mismatch.");
        Expect(loaded_valid_pack->Build.TextureReplacements.size() == 1, "Loaded texture replacement count mismatch.");
        Expect(loaded_valid_pack->Build.Commands.size() == 2, "Loaded command count mismatch.");
        Expect(loaded_valid_pack->Build.Hooks[0].RelativeVirtualAddress.has_value(), "Loaded hook did not preserve its exact RVA target.");
        Expect(*loaded_valid_pack->Build.Hooks[0].RelativeVirtualAddress == 0x006B00DA, "Loaded hook RVA mismatch.");
        Expect(loaded_valid_pack->Build.TextureReplacements[0].Id == "batman-subtitle-font-atlas", "Loaded texture replacement id mismatch.");
        Expect(loaded_valid_pack->Build.TextureReplacements[0].Api == "d3d9", "Loaded texture replacement api mismatch.");
        Expect(loaded_valid_pack->Build.TextureReplacements[0].Width == 1024, "Loaded texture replacement width mismatch.");
        Expect(loaded_valid_pack->Build.TextureReplacements[0].Height == 512, "Loaded texture replacement height mismatch.");
        Expect(loaded_valid_pack->Build.TextureReplacements[0].Format == "A8R8G8B8", "Loaded texture replacement format mismatch.");
        Expect(
            loaded_valid_pack->Build.TextureReplacements[0].Hash == "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
            "Loaded texture replacement hash normalization mismatch.");
        Expect(
            loaded_valid_pack->Build.TextureReplacements[0].ReplacementPath == std::filesystem::path("assets/textures/batman-subtitle-font-atlas.png"),
            "Loaded texture replacement path mismatch.");
        Expect(
            loaded_valid_pack->Build.TextureReplacements[0].SamplerStage.has_value() &&
                *loaded_valid_pack->Build.TextureReplacements[0].SamplerStage == 0,
            "Loaded texture replacement sampler stage mismatch.");

        const std::optional<helen::LoadedBuildPack> empty_address_match_pack = repository.LoadForExecutable(
            packs_root,
            "EmptyAddressMatchGame.exe",
            2222,
            "2222222222222222222222222222222222222222222222222222222222222222");
        Expect(!empty_address_match_pack.has_value(), "Pack repository accepted an explicitly empty observer address-match list.");

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

        VerifyGeneratedBatmanProtocolPack(root, repository);

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

        const std::optional<helen::LoadedBuildPack> duplicate_missing_path_pack = repository.LoadForExecutable(
            packs_root,
            "DuplicateMissingPathGame.exe",
            9999,
            "3333333333333333333333333333333333333333333333333333333333333333");
        Expect(!duplicate_missing_path_pack.has_value(), "Pack repository unexpectedly loaded a build with duplicate normalized missing paths.");

        const std::optional<helen::LoadedBuildPack> loaded_batman_pack = repository.LoadForExecutable(
            GetBatmanPackRoot(),
            "ShippingPC-BmGame.exe",
            38758728,
            "4dac1f5e2ac6710b7378fdce74601f616f4753e3756cb5fda63c7519cc2eb028");
        Expect(loaded_batman_pack.has_value(), "Expected the checked-in Batman pack to load for the matching executable fingerprint.");
        Expect(loaded_batman_pack->Pack.Id == "batman-aa-graphics-options", "Checked-in Batman graphics pack identifier mismatch.");
        Expect(loaded_batman_pack->Build.VirtualFiles.size() == 1, "Checked-in Batman graphics pack virtual file count mismatch.");
        Expect(loaded_batman_pack->Build.StartupCommandIds.size() == 1, "Checked-in Batman graphics pack startup command count mismatch.");
        Expect(loaded_batman_pack->Build.StartupCommandIds[0] == "loadBatmanGraphicsDraftIntoConfig", "Checked-in Batman graphics startup command mismatch.");
        Expect(loaded_batman_pack->Build.MissingPaths.empty(), "Checked-in Batman graphics pack should not hide game paths.");
        Expect(!loaded_batman_pack->Build.EnableD3d9TextureReplacementHooks, "Checked-in Batman graphics pack should disable D3D9 texture replacement hooks.");
        Expect(!loaded_batman_pack->Build.EnableD3d9TextureHashLogging, "Checked-in Batman graphics pack should leave D3D9 texture hash logging disabled.");
        Expect(!loaded_batman_pack->Build.EnableD3d9TextureImageDumping, "Checked-in Batman graphics pack should leave D3D9 texture image dumping disabled.");
        Expect(loaded_batman_pack->Build.RuntimeSlots.empty(), "Checked-in Batman graphics pack unexpectedly declared runtime slots.");
        Expect(loaded_batman_pack->Build.StateObservers.size() == 16, "Checked-in Batman graphics pack state-observer count mismatch: checked-in package is stale for Task 8.");
        Expect(loaded_batman_pack->Build.Hooks.empty(), "Checked-in Batman graphics pack unexpectedly declared hooks.");
        Expect(loaded_batman_pack->Build.TextureReplacements.empty(), "Checked-in Batman graphics pack unexpectedly declared texture replacements.");
        Expect(loaded_batman_pack->Build.Commands.size() == 4, "Checked-in Batman graphics pack command count mismatch: checked-in package is stale for Task 8.");
        Expect(loaded_batman_pack->Build.Commands[0].Id == "loadBatmanGraphicsDraftIntoConfig", "Checked-in Batman load command id mismatch.");
        Expect(loaded_batman_pack->Build.Commands[0].Name == "Load Batman Graphics Draft Into Config", "Checked-in Batman load command name mismatch.");
        Expect(loaded_batman_pack->Build.Commands[0].Steps.size() == 1, "Checked-in Batman load command step count mismatch.");
        Expect(loaded_batman_pack->Build.Commands[0].Steps[0].Kind == "load-batman-graphics-draft-into-config", "Checked-in Batman load command step mismatch.");
        Expect(loaded_batman_pack->Build.Commands[1].Id == "syncBatmanGraphicsDetailLevel", "Checked-in Batman sync command id mismatch.");
        Expect(loaded_batman_pack->Build.Commands[1].Name == "Sync Batman Graphics Detail Level", "Checked-in Batman sync command name mismatch.");
        Expect(loaded_batman_pack->Build.Commands[1].Steps.size() == 1, "Checked-in Batman sync command step count mismatch.");
        Expect(loaded_batman_pack->Build.Commands[1].Steps[0].Kind == "sync-batman-graphics-detail-level", "Checked-in Batman sync command step mismatch.");
        Expect(loaded_batman_pack->Build.Commands[2].Id == "setBatmanGraphicsResolutionMode", "Checked-in Batman resolution command id mismatch.");
        Expect(loaded_batman_pack->Build.Commands[2].Name == "Set Batman Graphics Resolution Mode", "Checked-in Batman resolution command name mismatch.");
        Expect(loaded_batman_pack->Build.Commands[2].Steps.size() == 1, "Checked-in Batman resolution command step count mismatch.");
        Expect(loaded_batman_pack->Build.Commands[2].Steps[0].Kind == "set-batman-graphics-resolution-mode", "Checked-in Batman resolution command step mismatch.");
        Expect(loaded_batman_pack->Build.Commands[3].Id == "applyBatmanGraphicsDraft", "Checked-in Batman apply command id mismatch.");
        Expect(loaded_batman_pack->Build.Commands[3].Name == "Apply Batman Graphics Draft", "Checked-in Batman apply command name mismatch.");
        Expect(loaded_batman_pack->Build.Commands[3].Steps.size() == 2, "Checked-in Batman apply command step count mismatch.");
        Expect(loaded_batman_pack->Build.Commands[3].Steps[0].Kind == "apply-batman-graphics-config", "Checked-in Batman apply command first step mismatch.");
        Expect(loaded_batman_pack->Build.Commands[3].Steps[1].Kind == "load-batman-graphics-draft-into-config", "Checked-in Batman apply command second step mismatch.");
        Expect(loaded_batman_pack->Build.ExternalBindings.empty(), "Checked-in Batman graphics pack unexpectedly declared external bindings.");
        Expect(loaded_batman_pack->Pack.Name == "Batman Graphics Options", "Checked-in Batman graphics pack name mismatch.");
        Expect(loaded_batman_pack->Pack.ConfigEntries.size() == 18, "Checked-in Batman graphics pack config-entry count mismatch: checked-in package is stale for Task 8.");
        const char* expected_graphics_config_keys[] = {
            "fullscreen",
            "resolutionWidth",
            "resolutionHeight",
            "resolutionModeIndex",
            "vsync",
            "msaa",
            "detailLevel",
            "bloom",
            "dynamicShadows",
            "motionBlur",
            "distortion",
            "fogVolumes",
            "sphericalHarmonicLighting",
            "ambientOcclusion",
            "physx",
            "stereo",
            "applySignal",
            "rollbackSignal"
        };
        for (std::size_t index = 0; index < std::size(expected_graphics_config_keys); ++index)
        {
            const helen::ConfigEntryDefinition& config_entry = loaded_batman_pack->Pack.ConfigEntries[index];
            Expect(config_entry.Key == expected_graphics_config_keys[index], "Checked-in Batman graphics config-key order mismatch.");
            Expect(config_entry.Type == "int", "Checked-in Batman graphics config-entry type mismatch.");
            const int expected_default = config_entry.Key == "resolutionModeIndex" ? -1 : 0;
            Expect(config_entry.DefaultValue == expected_default, "Checked-in Batman graphics config-entry default mismatch.");
        }
        Expect(loaded_batman_pack->Pack.Features.empty(), "Checked-in Batman graphics pack unexpectedly declared features.");
        const char* expected_graphics_observer_ids[] = {
            "graphicsObserverFullscreen",
            "graphicsObserverDisplayModeCatalog",
            "graphicsObserverResolutionModeIndex",
            "graphicsObserverVsync",
            "graphicsObserverMsaa",
            "graphicsObserverPhysx",
            "graphicsObserverStereo",
            "graphicsObserverBloom",
            "graphicsObserverDynamicShadows",
            "graphicsObserverMotionBlur",
            "graphicsObserverDistortion",
            "graphicsObserverFogVolumes",
            "graphicsObserverSphericalHarmonicLighting",
            "graphicsObserverAmbientOcclusion",
            "graphicsObserverApplySignal",
            "graphicsObserverRollbackSignal"
        };
        const char* expected_graphics_observer_targets[] = {
            "fullscreen", "", "resolutionModeIndex", "vsync", "msaa", "physx", "stereo", "bloom", "dynamicShadows", "motionBlur", "distortion",
            "fogVolumes", "sphericalHarmonicLighting", "ambientOcclusion", "applySignal", "rollbackSignal"
        };
        const int expected_legacy_graphics_address_match_values[] = {
            4200, 4210, 4211, 4220, 4221, 4230, 4231, 4299,
            4300, 4310, 4311, 4312, 4313, 4314, 4320, 4321, 4322, 4323, 4324, 4330, 4331, 4332, 4333, 4334, 4399,
            4400, 4410, 4411, 4412, 4420, 4421, 4422, 4430, 4431, 4432, 4499,
            4500, 4510, 4511, 4520, 4521, 4530, 4531, 4599,
            4600, 4601, 4602, 4603, 4604, 4605, 4606, 4609,
            4610, 4611, 4612, 4613, 4614, 4615, 4616, 4619,
            4620, 4621, 4622, 4623, 4624, 4625, 4626, 4629,
            4630, 4631, 4632, 4633, 4634, 4635, 4636, 4639,
            4640, 4641, 4642, 4643, 4644, 4645, 4646, 4649,
            4650, 4651, 4652, 4653, 4654, 4655, 4656, 4659,
            4660, 4661, 4662, 4663, 4664, 4665, 4666, 4669,
            4960, 4961, 4969, 4970, 4971, 4980, 4981, 4989, 4990, 4991
        };
        std::vector<int> expected_graphics_address_match_values(
            std::begin(expected_legacy_graphics_address_match_values),
            std::end(expected_legacy_graphics_address_match_values));
        expected_graphics_address_match_values.insert(expected_graphics_address_match_values.end(), { 4670, 4671, 4672, 4673, 4674, 4675, 4676, 4679 });
        for (int value = 4700; value <= 4899; ++value) { expected_graphics_address_match_values.push_back(value); }
        for (int value = 5200; value <= 5398; ++value) { expected_graphics_address_match_values.push_back(value); }
        for (int value = 5400; value <= 5598; ++value) { expected_graphics_address_match_values.push_back(value); }
        expected_graphics_address_match_values.push_back(5600);
        expected_graphics_address_match_values.push_back(5601);
        for (int value = 5000; value <= 5097; ++value) { expected_graphics_address_match_values.push_back(value); }
        for (int value = 5100; value <= 5197; ++value) { expected_graphics_address_match_values.push_back(value); }
        expected_graphics_address_match_values.push_back(5199);
        std::sort(expected_graphics_address_match_values.begin(), expected_graphics_address_match_values.end());
        for (std::size_t index = 1; index < expected_graphics_address_match_values.size(); ++index)
        {
            Expect(expected_graphics_address_match_values[index] > expected_graphics_address_match_values[index - 1], "Expected Batman graphics positive carrier union contains a duplicate or ordering collision.");
        }
        for (std::size_t observer_index = 0; observer_index < std::size(expected_graphics_observer_ids); ++observer_index)
        {
            const helen::MemoryStateObserverDefinition& observer = loaded_batman_pack->Build.StateObservers[observer_index];
            Expect(observer.Id == expected_graphics_observer_ids[observer_index], "Checked-in Batman graphics observer order mismatch.");
            Expect(observer.TargetConfigKey == expected_graphics_observer_targets[observer_index], "Checked-in Batman graphics observer target mismatch.");
            Expect(observer.AddressGroup.has_value() && *observer.AddressGroup == "batmanFrontendControlType", "Checked-in Batman graphics observer address group mismatch.");
            Expect(observer.ScanStartAddress == 0x10000000, "Checked-in Batman graphics scan start mismatch.");
            Expect(observer.ScanEndAddress == 0x30000000, "Checked-in Batman graphics scan end must cover the observed randomized heap range.");
            Expect(observer.ScanStride == 4, "Checked-in Batman graphics scan stride mismatch.");
            Expect(observer.ValueOffset == 12, "Checked-in Batman graphics value offset mismatch.");
            const int expected_poll_interval = observer.Id == "graphicsObserverDisplayModeCatalog" ? 10 : 50;
            Expect(observer.PollIntervalMs == expected_poll_interval, "Checked-in Batman graphics poll interval mismatch.");
            Expect(observer.Checks.size() == 11, "Checked-in Batman graphics structural-check count mismatch.");
            Expect(observer.AddressMatchValues.size() == expected_graphics_address_match_values.size(), "Checked-in Batman graphics address-match value count mismatch.");
            for (std::size_t index = 0; index < expected_graphics_address_match_values.size(); ++index)
            {
                Expect(observer.AddressMatchValues[index] == expected_graphics_address_match_values[index], "Checked-in Batman graphics address-match value mismatch.");
            }
        }
        const helen::MemoryStateObserverDefinition& checked_in_fullscreen_observer = loaded_batman_pack->Build.StateObservers[0];
        Expect(checked_in_fullscreen_observer.ResponseRequestValue.has_value() && *checked_in_fullscreen_observer.ResponseRequestValue == 4670, "Checked-in Batman fullscreen response request mismatch.");
        Expect(checked_in_fullscreen_observer.Mappings.size() == 2 && checked_in_fullscreen_observer.Mappings[0].Match == 4673 && checked_in_fullscreen_observer.Mappings[0].Value == 0 && checked_in_fullscreen_observer.Mappings[1].Match == 4674 && checked_in_fullscreen_observer.Mappings[1].Value == 1, "Checked-in Batman fullscreen write mapping mismatch.");
        Expect(checked_in_fullscreen_observer.ResponseMappings.size() == 2 && checked_in_fullscreen_observer.ResponseMappings[0].Match == 0 && checked_in_fullscreen_observer.ResponseMappings[0].Value == 4671 && checked_in_fullscreen_observer.ResponseMappings[1].Match == 1 && checked_in_fullscreen_observer.ResponseMappings[1].Value == 4672, "Checked-in Batman fullscreen response mapping mismatch.");
        Expect(checked_in_fullscreen_observer.AcknowledgementMappings.size() == 2 && checked_in_fullscreen_observer.AcknowledgementMappings[0].Match == 4673 && checked_in_fullscreen_observer.AcknowledgementMappings[0].Value == 4675 && checked_in_fullscreen_observer.AcknowledgementMappings[1].Match == 4674 && checked_in_fullscreen_observer.AcknowledgementMappings[1].Value == 4676, "Checked-in Batman fullscreen acknowledgement mapping mismatch.");
        Expect(checked_in_fullscreen_observer.FailureResponseValue.has_value() && *checked_in_fullscreen_observer.FailureResponseValue == 4679, "Checked-in Batman fullscreen failure response mismatch.");
        const helen::MemoryStateObserverDefinition& checked_in_catalog_observer = loaded_batman_pack->Build.StateObservers[1];
        Expect(checked_in_catalog_observer.TargetConfigKey.empty(), "Checked-in Batman catalog observer unexpectedly targets config.");
        Expect(checked_in_catalog_observer.DynamicResponseProviderId.has_value() && *checked_in_catalog_observer.DynamicResponseProviderId == "batmanDisplayModes", "Checked-in Batman catalog provider mismatch.");
        Expect(checked_in_catalog_observer.DynamicResponseRequestValues.size() == 599, "Checked-in Batman catalog request count mismatch.");
        for (std::size_t request_index = 0; request_index < 199; ++request_index)
        {
            Expect(checked_in_catalog_observer.DynamicResponseRequestValues[request_index] == 4700 + static_cast<int>(request_index), "Checked-in Batman legacy catalog request order mismatch.");
        }
        for (std::size_t request_index = 0; request_index < 199; ++request_index)
        {
            Expect(checked_in_catalog_observer.DynamicResponseRequestValues[199 + request_index] == 5200 + static_cast<int>(request_index), "Checked-in Batman windowed catalog request order mismatch.");
            Expect(checked_in_catalog_observer.DynamicResponseRequestValues[398 + request_index] == 5400 + static_cast<int>(request_index), "Checked-in Batman fullscreen catalog request order mismatch.");
        }
        Expect(checked_in_catalog_observer.DynamicResponseRequestValues[597] == 5600 && checked_in_catalog_observer.DynamicResponseRequestValues[598] == 5601, "Checked-in Batman desktop catalog request order mismatch.");
        Expect(checked_in_catalog_observer.DynamicResponseMinimumValue == 1 && checked_in_catalog_observer.DynamicResponseMaximumValue == 32767, "Checked-in Batman catalog scalar bounds mismatch.");
        Expect(checked_in_catalog_observer.FailureResponseValue.has_value() && *checked_in_catalog_observer.FailureResponseValue == 4899, "Checked-in Batman catalog failure response mismatch.");
        Expect(checked_in_catalog_observer.Mappings.empty() && checked_in_catalog_observer.ResponseMappings.empty() && checked_in_catalog_observer.AcknowledgementMappings.empty(), "Checked-in Batman catalog observer unexpectedly declared static mappings.");
        const helen::MemoryStateObserverDefinition& checked_in_resolution_observer = loaded_batman_pack->Build.StateObservers[2];
        Expect(checked_in_resolution_observer.TargetConfigKey == "resolutionModeIndex", "Checked-in Batman resolution target mismatch.");
        Expect(checked_in_resolution_observer.Mappings.size() == 98 && checked_in_resolution_observer.AcknowledgementMappings.size() == 98, "Checked-in Batman resolution mapping count mismatch.");
        for (std::size_t resolution_index = 0; resolution_index < 98; ++resolution_index)
        {
            Expect(checked_in_resolution_observer.Mappings[resolution_index].Match == 5000 + static_cast<int>(resolution_index) && checked_in_resolution_observer.Mappings[resolution_index].Value == static_cast<int>(resolution_index), "Checked-in Batman resolution mapping mismatch.");
            Expect(checked_in_resolution_observer.AcknowledgementMappings[resolution_index].Match == 5000 + static_cast<int>(resolution_index) && checked_in_resolution_observer.AcknowledgementMappings[resolution_index].Value == 5100 + static_cast<int>(resolution_index), "Checked-in Batman resolution acknowledgement mismatch.");
        }
        Expect(checked_in_resolution_observer.FailureResponseValue.has_value() && *checked_in_resolution_observer.FailureResponseValue == 5199, "Checked-in Batman resolution failure response mismatch.");
        Expect(checked_in_resolution_observer.CommandId.has_value() && *checked_in_resolution_observer.CommandId == "setBatmanGraphicsResolutionMode", "Checked-in Batman resolution command mismatch.");
        const helen::MemoryStateObserverDefinition& checked_in_vsync_observer = loaded_batman_pack->Build.StateObservers[3];
        const helen::MemoryStateObserverDefinition& checked_in_msaa_observer = loaded_batman_pack->Build.StateObservers[4];
        const helen::MemoryStateObserverDefinition& checked_in_physx_observer = loaded_batman_pack->Build.StateObservers[5];
        const helen::MemoryStateObserverDefinition& checked_in_stereo_observer = loaded_batman_pack->Build.StateObservers[6];
        const helen::MemoryStateObserverDefinition& checked_in_apply_observer = loaded_batman_pack->Build.StateObservers[14];
        const helen::MemoryStateObserverDefinition& checked_in_rollback_observer = loaded_batman_pack->Build.StateObservers[15];
        Expect(checked_in_vsync_observer.Mappings.size() == 2, "Checked-in Batman VSync mapping count mismatch.");
        Expect(checked_in_vsync_observer.Mappings[0].Match == 4220 && checked_in_vsync_observer.Mappings[0].Value == 0, "Checked-in Batman VSync first mapping mismatch.");
        Expect(checked_in_vsync_observer.Mappings[1].Match == 4221 && checked_in_vsync_observer.Mappings[1].Value == 1, "Checked-in Batman VSync second mapping mismatch.");
        Expect(checked_in_vsync_observer.ResponseRequestValue.has_value() && *checked_in_vsync_observer.ResponseRequestValue == 4200, "Checked-in Batman VSync response request mismatch.");
        Expect(checked_in_vsync_observer.ResponseMappings.size() == 2, "Checked-in Batman VSync response mapping count mismatch.");
        Expect(checked_in_vsync_observer.ResponseMappings[0].Match == 0 && checked_in_vsync_observer.ResponseMappings[0].Value == 4210, "Checked-in Batman VSync disabled response mapping mismatch.");
        Expect(checked_in_vsync_observer.ResponseMappings[1].Match == 1 && checked_in_vsync_observer.ResponseMappings[1].Value == 4211, "Checked-in Batman VSync enabled response mapping mismatch.");
        Expect(checked_in_vsync_observer.AcknowledgementMappings.size() == 2, "Checked-in Batman VSync acknowledgement mapping count mismatch.");
        Expect(checked_in_vsync_observer.AcknowledgementMappings[0].Match == 4220 && checked_in_vsync_observer.AcknowledgementMappings[0].Value == 4230, "Checked-in Batman VSync disabled acknowledgement mismatch.");
        Expect(checked_in_vsync_observer.AcknowledgementMappings[1].Match == 4221 && checked_in_vsync_observer.AcknowledgementMappings[1].Value == 4231, "Checked-in Batman VSync enabled acknowledgement mismatch.");
        Expect(checked_in_vsync_observer.FailureResponseValue.has_value() && *checked_in_vsync_observer.FailureResponseValue == 4299, "Checked-in Batman VSync failure response mismatch.");

        Expect(checked_in_msaa_observer.Mappings.size() == 5, "Checked-in Batman MSAA mapping count mismatch.");
        Expect(checked_in_msaa_observer.ResponseMappings.size() == 5, "Checked-in Batman MSAA response mapping count mismatch.");
        Expect(checked_in_msaa_observer.AcknowledgementMappings.size() == 5, "Checked-in Batman MSAA acknowledgement mapping count mismatch.");
        const int expected_msaa_mapping_matches[] = { 4320, 4321, 4322, 4323, 4324 };
        const int expected_msaa_mapping_values[] = { 0, 1, 2, 3, 5 };
        const int expected_msaa_response_values[] = { 4310, 4311, 4312, 4313, 4314 };
        const int expected_msaa_acknowledgement_values[] = { 4330, 4331, 4332, 4333, 4334 };
        for (std::size_t index = 0; index < std::size(expected_msaa_mapping_matches); ++index)
        {
            Expect(checked_in_msaa_observer.Mappings[index].Match == expected_msaa_mapping_matches[index] && checked_in_msaa_observer.Mappings[index].Value == expected_msaa_mapping_values[index], "Checked-in Batman MSAA mapping mismatch.");
            Expect(checked_in_msaa_observer.ResponseMappings[index].Match == expected_msaa_mapping_values[index] && checked_in_msaa_observer.ResponseMappings[index].Value == expected_msaa_response_values[index], "Checked-in Batman MSAA response mapping mismatch.");
            Expect(checked_in_msaa_observer.AcknowledgementMappings[index].Match == expected_msaa_mapping_matches[index] && checked_in_msaa_observer.AcknowledgementMappings[index].Value == expected_msaa_acknowledgement_values[index], "Checked-in Batman MSAA acknowledgement mapping mismatch.");
        }
        Expect(checked_in_msaa_observer.ResponseRequestValue.has_value() && *checked_in_msaa_observer.ResponseRequestValue == 4300, "Checked-in Batman MSAA response request mismatch.");
        Expect(checked_in_msaa_observer.FailureResponseValue.has_value() && *checked_in_msaa_observer.FailureResponseValue == 4399, "Checked-in Batman MSAA failure response mismatch.");

        Expect(checked_in_physx_observer.Mappings.size() == 3, "Checked-in Batman PhysX mapping count mismatch.");
        Expect(checked_in_physx_observer.ResponseMappings.size() == 3, "Checked-in Batman PhysX response mapping count mismatch.");
        Expect(checked_in_physx_observer.AcknowledgementMappings.size() == 3, "Checked-in Batman PhysX acknowledgement mapping count mismatch.");
        const int expected_physx_mapping_matches[] = { 4420, 4421, 4422 };
        const int expected_physx_response_values[] = { 4410, 4411, 4412 };
        const int expected_physx_acknowledgement_values[] = { 4430, 4431, 4432 };
        for (std::size_t index = 0; index < std::size(expected_physx_mapping_matches); ++index)
        {
            Expect(checked_in_physx_observer.Mappings[index].Match == expected_physx_mapping_matches[index] && checked_in_physx_observer.Mappings[index].Value == static_cast<int>(index), "Checked-in Batman PhysX mapping mismatch.");
            Expect(checked_in_physx_observer.ResponseMappings[index].Match == static_cast<int>(index) && checked_in_physx_observer.ResponseMappings[index].Value == expected_physx_response_values[index], "Checked-in Batman PhysX response mapping mismatch.");
            Expect(checked_in_physx_observer.AcknowledgementMappings[index].Match == expected_physx_mapping_matches[index] && checked_in_physx_observer.AcknowledgementMappings[index].Value == expected_physx_acknowledgement_values[index], "Checked-in Batman PhysX acknowledgement mapping mismatch.");
        }
        Expect(checked_in_physx_observer.ResponseRequestValue.has_value() && *checked_in_physx_observer.ResponseRequestValue == 4400, "Checked-in Batman PhysX response request mismatch.");
        Expect(checked_in_physx_observer.FailureResponseValue.has_value() && *checked_in_physx_observer.FailureResponseValue == 4499, "Checked-in Batman PhysX failure response mismatch.");

        Expect(checked_in_stereo_observer.Mappings.size() == 2, "Checked-in Batman Stereo mapping count mismatch.");
        Expect(checked_in_stereo_observer.ResponseMappings.size() == 2, "Checked-in Batman Stereo response mapping count mismatch.");
        Expect(checked_in_stereo_observer.AcknowledgementMappings.size() == 2, "Checked-in Batman Stereo acknowledgement mapping count mismatch.");
        const int expected_stereo_mapping_matches[] = { 4520, 4521 };
        const int expected_stereo_response_values[] = { 4510, 4511 };
        const int expected_stereo_acknowledgement_values[] = { 4530, 4531 };
        for (std::size_t index = 0; index < std::size(expected_stereo_mapping_matches); ++index)
        {
            Expect(checked_in_stereo_observer.Mappings[index].Match == expected_stereo_mapping_matches[index] && checked_in_stereo_observer.Mappings[index].Value == static_cast<int>(index), "Checked-in Batman Stereo mapping mismatch.");
            Expect(checked_in_stereo_observer.ResponseMappings[index].Match == static_cast<int>(index) && checked_in_stereo_observer.ResponseMappings[index].Value == expected_stereo_response_values[index], "Checked-in Batman Stereo response mapping mismatch.");
            Expect(checked_in_stereo_observer.AcknowledgementMappings[index].Match == expected_stereo_mapping_matches[index] && checked_in_stereo_observer.AcknowledgementMappings[index].Value == expected_stereo_acknowledgement_values[index], "Checked-in Batman Stereo acknowledgement mapping mismatch.");
        }
        Expect(checked_in_stereo_observer.ResponseRequestValue.has_value() && *checked_in_stereo_observer.ResponseRequestValue == 4500, "Checked-in Batman Stereo response request mismatch.");
        Expect(checked_in_stereo_observer.FailureResponseValue.has_value() && *checked_in_stereo_observer.FailureResponseValue == 4599, "Checked-in Batman Stereo failure response mismatch.");

        const char* expected_quality_targets[] = {
            "bloom", "dynamicShadows", "motionBlur", "distortion", "fogVolumes", "sphericalHarmonicLighting", "ambientOcclusion"
        };
        const int expected_quality_reads[] = { 4600, 4610, 4620, 4630, 4640, 4650, 4660 };
        const int expected_quality_responses[][2] = {
            { 4601, 4602 }, { 4611, 4612 }, { 4621, 4622 }, { 4631, 4632 },
            { 4641, 4642 }, { 4651, 4652 }, { 4661, 4662 }
        };
        const int expected_quality_writes[][2] = {
            { 4603, 4604 }, { 4613, 4614 }, { 4623, 4624 }, { 4633, 4634 },
            { 4643, 4644 }, { 4653, 4654 }, { 4663, 4664 }
        };
        const int expected_quality_acknowledgements[][2] = {
            { 4605, 4606 }, { 4615, 4616 }, { 4625, 4626 }, { 4635, 4636 },
            { 4645, 4646 }, { 4655, 4656 }, { 4665, 4666 }
        };
        const int expected_quality_failures[] = { 4609, 4619, 4629, 4639, 4649, 4659, 4669 };
        for (std::size_t quality_index = 0; quality_index < std::size(expected_quality_targets); ++quality_index)
        {
            const helen::MemoryStateObserverDefinition& quality_observer = loaded_batman_pack->Build.StateObservers[7 + quality_index];
            Expect(quality_observer.TargetConfigKey == expected_quality_targets[quality_index], "Checked-in Batman quality observer target mismatch.");
            Expect(quality_observer.CommandId.has_value() && *quality_observer.CommandId == "syncBatmanGraphicsDetailLevel", "Checked-in Batman quality observer command mismatch.");
            Expect(quality_observer.Mappings.size() == 2, "Checked-in Batman quality mapping count mismatch.");
            Expect(quality_observer.ResponseMappings.size() == 2, "Checked-in Batman quality response mapping count mismatch.");
            Expect(quality_observer.AcknowledgementMappings.size() == 2, "Checked-in Batman quality acknowledgement mapping count mismatch.");
            Expect(quality_observer.ResponseRequestValue.has_value() && *quality_observer.ResponseRequestValue == expected_quality_reads[quality_index], "Checked-in Batman quality response request mismatch.");
            Expect(quality_observer.FailureResponseValue.has_value() && *quality_observer.FailureResponseValue == expected_quality_failures[quality_index], "Checked-in Batman quality failure response mismatch.");
            for (std::size_t value_index = 0; value_index < 2; ++value_index)
            {
                Expect(quality_observer.Mappings[value_index].Match == expected_quality_writes[quality_index][value_index] && quality_observer.Mappings[value_index].Value == static_cast<int>(value_index), "Checked-in Batman quality write mapping mismatch.");
                Expect(quality_observer.ResponseMappings[value_index].Match == static_cast<int>(value_index) && quality_observer.ResponseMappings[value_index].Value == expected_quality_responses[quality_index][value_index], "Checked-in Batman quality response mapping mismatch.");
                Expect(quality_observer.AcknowledgementMappings[value_index].Match == expected_quality_writes[quality_index][value_index] && quality_observer.AcknowledgementMappings[value_index].Value == expected_quality_acknowledgements[quality_index][value_index], "Checked-in Batman quality acknowledgement mapping mismatch.");
            }
        }

        Expect(checked_in_apply_observer.Mappings.size() == 2, "Checked-in Batman apply mapping count mismatch.");
        Expect(checked_in_apply_observer.Mappings[0].Match == 4990 && checked_in_apply_observer.Mappings[0].Value == 0, "Checked-in Batman apply first mapping mismatch.");
        Expect(checked_in_apply_observer.Mappings[1].Match == 4991 && checked_in_apply_observer.Mappings[1].Value == 1, "Checked-in Batman apply second mapping mismatch.");
        Expect(!checked_in_apply_observer.ResponseRequestValue.has_value() && checked_in_apply_observer.ResponseMappings.empty(), "Checked-in Batman apply observer unexpectedly declared a response mapping.");
        Expect(checked_in_apply_observer.AcknowledgementMappings.size() == 2, "Checked-in Batman apply acknowledgement mapping count mismatch.");
        Expect(checked_in_apply_observer.AcknowledgementMappings[0].Match == 4990 && checked_in_apply_observer.AcknowledgementMappings[0].Value == 4980, "Checked-in Batman apply first acknowledgement mismatch.");
        Expect(checked_in_apply_observer.AcknowledgementMappings[1].Match == 4991 && checked_in_apply_observer.AcknowledgementMappings[1].Value == 4981, "Checked-in Batman apply second acknowledgement mismatch.");
        Expect(checked_in_apply_observer.FailureResponseValue.has_value() && *checked_in_apply_observer.FailureResponseValue == 4989, "Checked-in Batman apply failure response mismatch.");
        Expect(checked_in_apply_observer.CommandId.has_value() && *checked_in_apply_observer.CommandId == "applyBatmanGraphicsDraft", "Checked-in Batman apply observer command mismatch.");

        Expect(checked_in_rollback_observer.Mappings.size() == 2, "Checked-in Batman rollback mapping count mismatch.");
        Expect(checked_in_rollback_observer.Mappings[0].Match == 4970 && checked_in_rollback_observer.Mappings[0].Value == 0, "Checked-in Batman rollback first mapping mismatch.");
        Expect(checked_in_rollback_observer.Mappings[1].Match == 4971 && checked_in_rollback_observer.Mappings[1].Value == 1, "Checked-in Batman rollback second mapping mismatch.");
        Expect(!checked_in_rollback_observer.ResponseRequestValue.has_value() && checked_in_rollback_observer.ResponseMappings.empty(), "Checked-in Batman rollback observer unexpectedly declared a response mapping.");
        Expect(checked_in_rollback_observer.AcknowledgementMappings.size() == 2, "Checked-in Batman rollback acknowledgement mapping count mismatch.");
        Expect(checked_in_rollback_observer.AcknowledgementMappings[0].Match == 4970 && checked_in_rollback_observer.AcknowledgementMappings[0].Value == 4960, "Checked-in Batman rollback first acknowledgement mismatch.");
        Expect(checked_in_rollback_observer.AcknowledgementMappings[1].Match == 4971 && checked_in_rollback_observer.AcknowledgementMappings[1].Value == 4961, "Checked-in Batman rollback second acknowledgement mismatch.");
        Expect(checked_in_rollback_observer.FailureResponseValue.has_value() && *checked_in_rollback_observer.FailureResponseValue == 4969, "Checked-in Batman rollback failure response mismatch.");
        Expect(checked_in_rollback_observer.CommandId.has_value() && *checked_in_rollback_observer.CommandId == "loadBatmanGraphicsDraftIntoConfig", "Checked-in Batman rollback observer command mismatch.");

        const helen::VirtualFileDefinition* checked_in_graphics_frontend_file = nullptr;
        for (const helen::VirtualFileDefinition& virtual_file : loaded_batman_pack->Build.VirtualFiles)
        {
            if (virtual_file.Id == "frontendGraphicsOptionsPackage")
            {
                checked_in_graphics_frontend_file = &virtual_file;
            }
        }

        Expect(checked_in_graphics_frontend_file != nullptr, "Checked-in Batman graphics frontend virtual file was not found.");
        Expect(checked_in_graphics_frontend_file->Mode == "delta-on-read", "Checked-in Batman graphics frontend package is not delta-backed.");
        Expect(checked_in_graphics_frontend_file->Source.Kind == helen::VirtualFileSourceKind::DeltaFile, "Checked-in Batman graphics frontend package source kind mismatch.");
        Expect(checked_in_graphics_frontend_file->GamePath == std::filesystem::path("BmGame/CookedPC/Maps/Frontend/Frontend.umap"), "Checked-in Batman graphics frontend package path mismatch.");
        Expect(checked_in_graphics_frontend_file->Source.Path == std::filesystem::path("assets/deltas/Frontend-graphics-options.hgdelta"), "Checked-in Batman graphics frontend delta path mismatch.");
        Expect(checked_in_graphics_frontend_file->Source.Base.FileSize == 2988548, "Checked-in Batman graphics frontend package base size mismatch.");
        Expect(checked_in_graphics_frontend_file->Source.Base.Sha256 == "271916b888f83374122af0fccc5c685804f4c8286a92a772cd71e4f48a00f2cc", "Checked-in Batman graphics frontend package base hash mismatch.");
        Expect(checked_in_graphics_frontend_file->Source.Target.FileSize > 0, "Checked-in Batman graphics frontend package target size must be positive.");
        Expect(checked_in_graphics_frontend_file->Source.Target.Sha256.size() == 64, "Checked-in Batman graphics frontend package target hash must be a SHA-256 value.");
        Expect(checked_in_graphics_frontend_file->Source.ChunkSize == 65536, "Checked-in Batman graphics frontend chunk size mismatch.");

        const std::optional<helen::LoadedBuildPackSet> checked_in_batman_pack_set = repository.LoadPackSetForExecutable(
            GetBatmanPackRoot(),
            "ShippingPC-BmGame.exe",
            38758728,
            "4dac1f5e2ac6710b7378fdce74601f616f4753e3756cb5fda63c7519cc2eb028",
            { "batman-aa-subtitles", "batman-aa-graphics-options" });
        Expect(checked_in_batman_pack_set.has_value(), "Expected the checked-in Batman subtitles plus graphics pack set to load.");
        Expect(checked_in_batman_pack_set->Packs.size() == 2, "Checked-in Batman pack-set size mismatch.");
        Expect(checked_in_batman_pack_set->Packs[0].Pack.Id == "batman-aa-subtitles", "Checked-in Batman pack-set order mismatch for subtitles.");
        Expect(checked_in_batman_pack_set->Packs[1].Pack.Id == "batman-aa-graphics-options", "Checked-in Batman pack-set order mismatch for graphics.");

        helen::ActivePackSet checked_in_active_pack_set;
        std::string checked_in_failure_reason;
        const helen::ActivePackSetBuilder active_pack_set_builder;
        Expect(
            active_pack_set_builder.TryBuild(*checked_in_batman_pack_set, checked_in_active_pack_set, checked_in_failure_reason),
            "Expected the checked-in Batman subtitles plus graphics pack set to merge into an active pack set.");
        Expect(checked_in_active_pack_set.LoadedPacks.size() == 2, "Checked-in Batman active pack-set loaded-pack count mismatch.");
        Expect(checked_in_active_pack_set.StartupCommandIds.size() == 2, "Checked-in Batman active pack-set startup-command count mismatch.");
        Expect(checked_in_active_pack_set.StartupCommandIds[0] == "applySavedSubtitleSize", "Checked-in Batman active pack-set startup command mismatch.");
        Expect(checked_in_active_pack_set.StartupCommandIds[1] == "loadBatmanGraphicsDraftIntoConfig", "Checked-in Batman active graphics startup command mismatch.");
        Expect(checked_in_active_pack_set.MissingPaths.empty(), "Checked-in Batman active pack-set should not hide game paths.");
        Expect(checked_in_active_pack_set.VirtualFiles.size() == 2, "Checked-in Batman active pack-set virtual-file count mismatch.");
        Expect(checked_in_active_pack_set.Hooks.size() == 1, "Checked-in Batman active pack-set hook count mismatch.");
        Expect(checked_in_active_pack_set.TextureReplacements.size() == 1, "Checked-in Batman active pack-set texture replacement count mismatch.");
        // The merged set contains two subtitle commands and four graphics commands, including resolution-mode Apply.
        Expect(checked_in_active_pack_set.Commands.size() == 6, "Checked-in Batman active pack-set command count mismatch.");
        Expect(checked_in_active_pack_set.ExternalBindings.size() == 3, "Checked-in Batman active pack-set external-binding count mismatch.");
        // The merged set contains sixteen graphics observers plus the subtitle observer.
        Expect(checked_in_active_pack_set.StateObservers.size() == 17, "Checked-in Batman active pack-set state-observer count mismatch.");
        Expect(checked_in_active_pack_set.RuntimeSlots.size() == 1, "Checked-in Batman active pack-set runtime-slot count mismatch.");
        Expect(checked_in_active_pack_set.EnableD3d9TextureReplacementHooks, "Checked-in Batman active pack-set should enable D3D9 texture replacement hooks.");
        Expect(!checked_in_active_pack_set.EnableD3d9TextureHashLogging, "Checked-in Batman active pack-set should leave D3D9 texture hash logging disabled.");
        Expect(!checked_in_active_pack_set.EnableD3d9TextureImageDumping, "Checked-in Batman active pack-set should leave D3D9 texture image dumping disabled.");
    }
    catch (...)
    {
        std::filesystem::remove_all(root);
        throw;
    }

    std::filesystem::remove_all(root);
}
