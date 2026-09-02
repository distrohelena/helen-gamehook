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
        Expect(loaded_batman_pack->Build.StateObservers.size() == 2, "Checked-in Batman graphics pack state-observer count mismatch.");
        Expect(loaded_batman_pack->Build.Hooks.empty(), "Checked-in Batman graphics pack unexpectedly declared hooks.");
        Expect(loaded_batman_pack->Build.TextureReplacements.empty(), "Checked-in Batman graphics pack unexpectedly declared texture replacements.");
        Expect(loaded_batman_pack->Build.Commands.size() == 2, "Checked-in Batman graphics pack command count mismatch.");
        Expect(loaded_batman_pack->Build.ExternalBindings.empty(), "Checked-in Batman graphics pack unexpectedly declared external bindings.");
        Expect(loaded_batman_pack->Pack.ConfigEntries.size() == 16, "Checked-in Batman graphics pack config-entry count mismatch.");
        Expect(loaded_batman_pack->Pack.Features.empty(), "Checked-in Batman graphics pack unexpectedly declared features.");
        const helen::MemoryStateObserverDefinition& checked_in_vsync_observer = loaded_batman_pack->Build.StateObservers[0];
        const helen::MemoryStateObserverDefinition& checked_in_apply_observer = loaded_batman_pack->Build.StateObservers[1];
        Expect(checked_in_vsync_observer.Id == "graphicsObserverVsync", "Checked-in Batman VSync observer id mismatch.");
        Expect(checked_in_vsync_observer.TargetConfigKey == "vsync", "Checked-in Batman VSync observer target mismatch.");
        Expect(!checked_in_vsync_observer.CommandId.has_value(), "Checked-in Batman VSync observer unexpectedly declared a command.");
        Expect(checked_in_apply_observer.Id == "graphicsObserverApplySignal", "Checked-in Batman apply observer id mismatch.");
        Expect(checked_in_apply_observer.TargetConfigKey == "applySignal", "Checked-in Batman apply observer target mismatch.");
        Expect(checked_in_apply_observer.CommandId.has_value() && *checked_in_apply_observer.CommandId == "applyBatmanGraphicsDraft", "Checked-in Batman apply observer command mismatch.");
        const int expected_graphics_address_match_values[] = { 4101, 4102, 4103, 4104, 4105, 4106, 4200, 4210, 4211, 4990, 4991 };
        for (const helen::MemoryStateObserverDefinition* observer : { &checked_in_vsync_observer, &checked_in_apply_observer })
        {
            Expect(observer->ScanStartAddress == 0x10000000, "Checked-in Batman graphics scan start mismatch.");
            Expect(observer->ScanEndAddress == 0x30000000, "Checked-in Batman graphics scan end must cover the observed randomized heap range.");
            Expect(observer->AddressMatchValues.size() == std::size(expected_graphics_address_match_values), "Checked-in Batman graphics address-match value count mismatch.");
            for (std::size_t index = 0; index < std::size(expected_graphics_address_match_values); ++index)
            {
                Expect(observer->AddressMatchValues[index] == expected_graphics_address_match_values[index], "Checked-in Batman graphics address-match value mismatch.");
            }
        }
        Expect(checked_in_vsync_observer.Mappings.size() == 2, "Checked-in Batman VSync mapping count mismatch.");
        Expect(checked_in_vsync_observer.Mappings[0].Match == 4210 && checked_in_vsync_observer.Mappings[0].Value == 0, "Checked-in Batman VSync first mapping mismatch.");
        Expect(checked_in_vsync_observer.Mappings[1].Match == 4211 && checked_in_vsync_observer.Mappings[1].Value == 1, "Checked-in Batman VSync second mapping mismatch.");
        Expect(checked_in_vsync_observer.ResponseRequestValue.has_value() && *checked_in_vsync_observer.ResponseRequestValue == 4200, "Checked-in Batman VSync response request mismatch.");
        Expect(checked_in_vsync_observer.ResponseMappings.size() == 2, "Checked-in Batman VSync response mapping count mismatch.");
        Expect(checked_in_vsync_observer.ResponseMappings[0].Match == 0 && checked_in_vsync_observer.ResponseMappings[0].Value == 4210, "Checked-in Batman VSync disabled response mapping mismatch.");
        Expect(checked_in_vsync_observer.ResponseMappings[1].Match == 1 && checked_in_vsync_observer.ResponseMappings[1].Value == 4211, "Checked-in Batman VSync enabled response mapping mismatch.");
        Expect(!checked_in_apply_observer.ResponseRequestValue.has_value() && checked_in_apply_observer.ResponseMappings.empty(), "Checked-in Batman apply observer unexpectedly declared a response mapping.");
        Expect(checked_in_apply_observer.Mappings.size() == 2, "Checked-in Batman apply mapping count mismatch.");
        Expect(checked_in_apply_observer.Mappings[0].Match == 4990 && checked_in_apply_observer.Mappings[0].Value == 0, "Checked-in Batman apply first mapping mismatch.");
        Expect(checked_in_apply_observer.Mappings[1].Match == 4991 && checked_in_apply_observer.Mappings[1].Value == 1, "Checked-in Batman apply second mapping mismatch.");

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
        Expect(checked_in_active_pack_set.Commands.size() == 4, "Checked-in Batman active pack-set command count mismatch.");
        Expect(checked_in_active_pack_set.ExternalBindings.size() == 3, "Checked-in Batman active pack-set external-binding count mismatch.");
        Expect(checked_in_active_pack_set.StateObservers.size() == 3, "Checked-in Batman active pack-set state-observer count mismatch.");
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
