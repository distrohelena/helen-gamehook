# Batman Multi-Pack Skip Videos Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add explicit multi-pack support plus a `batman-aa-skip-videos` pack so Batman can enable subtitles and skip-videos together while declared game paths appear missing without touching real files on disk.

**Architecture:** Extend build metadata with a generic `missingPaths` list, parse it into `BuildDefinition`, and add one focused hidden-path matcher used by `FileApiHookSet`. The runtime will return synthetic “file not found” results for opens, attribute probes, and directory enumeration, while the Batman pack contributes only metadata declaring the five startup `.bik` files.

**Tech Stack:** C++, Win32 file APIs, JSON manifest/config parsing, existing Helen runtime pack manifests, PowerShell pack scripts, `HelenRuntimeTests.exe`

---

## Scope Update

This plan now includes explicit ordered pack selection and pack-set runtime loading before the hidden-path work. The hidden-path and pack-manifest tasks below still apply, but they now depend on multi-pack runtime support.

## File Structure

- Create: `include/HelenHook/HiddenPathMatcher.h`
  - Encapsulates canonical path normalization and “should hide path” matching.
- Create: `HelenRuntime/HiddenPathMatcher.cpp`
  - Implements absolute-to-relative normalization and case-insensitive matching.
- Modify: `include/HelenHook/BuildDefinition.h`
  - Adds build-scoped `MissingPaths`.
- Modify: `HelenRuntime/PackRepository.cpp`
  - Parses `missingPaths` from `build.json` and validates duplicates/empties.
- Modify: `include/HelenHook/FileApiHookSet.h`
  - Declares the new file API detours and enumeration bookkeeping.
- Modify: `HelenRuntime/FileApiHookSet.cpp`
  - Adds hidden-path interception for open, attributes, and enumeration APIs.
- Modify: `HelenGameHook/HelenGameHook.cpp`
  - Passes the resolved runtime game root and active build `missingPaths` into the file hook set.
- Modify: `HelenRuntime/HelenRuntime.vcxproj`
  - Builds the new matcher source file.
- Create: `tests/HelenRuntime.Tests/HiddenPathMatcherTests.cpp`
  - Covers normalization and matching behavior.
- Create: `tests/HelenRuntime.Tests/FileApiHookSetTests.cpp`
  - Covers hidden-path semantics for attributes and enumeration bookkeeping helpers.
- Modify: `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`
  - Includes the new tests.
- Modify: `tests/HelenRuntime.Tests/TestMain.cpp`
  - Executes the new test suites.
- Modify: `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`
  - Adds `missingPaths` parsing coverage and checked-in Batman pack coverage.
- Create: `games/HelenBatmanAA/helengamehook/packs/batman-aa-skip-videos/pack.json`
  - Standalone pack manifest.
- Create: `games/HelenBatmanAA/helengamehook/packs/batman-aa-skip-videos/builds/steam-goty-1.0/build.json`
  - Declares executable match plus `missingPaths`.
- Create: `games/HelenBatmanAA/helengamehook/packs/batman-aa-skip-videos/builds/steam-goty-1.0/files.json`
  - Empty virtual-file manifest.
- Create: `games/HelenBatmanAA/helengamehook/packs/batman-aa-skip-videos/builds/steam-goty-1.0/bindings.json`
  - Empty bindings manifest.
- Create: `games/HelenBatmanAA/helengamehook/packs/batman-aa-skip-videos/builds/steam-goty-1.0/hooks.json`
  - Empty hooks manifest.
- Create: `games/HelenBatmanAA/helengamehook/packs/batman-aa-skip-videos/builds/steam-goty-1.0/textures.json`
  - Empty textures manifest.
- Create: `games/HelenBatmanAA/helengamehook/packs/batman-aa-skip-videos/builds/steam-goty-1.0/commands.json`
  - Empty commands manifest.
- Create: `games/HelenBatmanAA/scripts/Test-BatmanSkipVideosPack.ps1`
  - Verifies the checked-in pack metadata.

### Task 1: Add `missingPaths` Build Metadata Parsing

**Files:**
- Modify: `include/HelenHook/BuildDefinition.h`
- Modify: `HelenRuntime/PackRepository.cpp`
- Modify: `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`

- [ ] **Step 1: Write the failing repository test for valid and invalid `missingPaths`**

Add assertions to `tests/HelenRuntime.Tests/PackRepositoryTests.cpp` in the synthetic build setup:

```cpp
        WriteAllText(
            valid_build_root / "build.json",
            R"({
  "id": "steam-goty-1.0",
  "executable": "ShippingPC-BmGame.exe",
  "missingPaths": [
    "BmGame/Movies/Legal.bik",
    "bmgame\\movies\\nvidia.bik"
  ],
  "match": {
    "fileSize": 38758728,
    "sha256": "4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028"
  }
})");
```

Then add expectations after the valid pack loads:

```cpp
        Expect(loaded_valid_pack->Build.MissingPaths.size() == 2, "Loaded missing-path count mismatch.");
        Expect(
            loaded_valid_pack->Build.MissingPaths[0] == "bmgame/movies/legal.bik",
            "Loaded missing path normalization mismatch for Legal.bik.");
        Expect(
            loaded_valid_pack->Build.MissingPaths[1] == "bmgame/movies/nvidia.bik",
            "Loaded missing path normalization mismatch for nvidia.bik.");
```

Add one malformed build case:

```cpp
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
```

And assert it does not load:

```cpp
        const std::optional<helen::LoadedBuildPack> duplicate_missing_path_pack = repository.LoadForExecutable(
            packs_root,
            "DuplicateMissingPathGame.exe",
            9999,
            "3333333333333333333333333333333333333333333333333333333333333333");
        Expect(!duplicate_missing_path_pack.has_value(), "Pack repository unexpectedly loaded a build with duplicate normalized missing paths.");
```

- [ ] **Step 2: Run the runtime tests to verify the new assertions fail**

Run:

```powershell
msbuild tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
```

Expected: FAIL because `BuildDefinition` does not yet expose `MissingPaths` and `PackRepository` does not parse `missingPaths`.

- [ ] **Step 3: Add `MissingPaths` to the build definition**

Update `include/HelenHook/BuildDefinition.h`:

```cpp
        /** @brief Canonical relative game paths that the runtime should report as missing when this build is active. */
        std::vector<std::string> MissingPaths;
```

- [ ] **Step 4: Implement `missingPaths` parsing and validation in `PackRepository.cpp`**

Add one helper near the other manifest parsers:

```cpp
    bool ParseMissingPaths(const helen::JsonValue& root, helen::BuildDefinition& definition)
    {
        const helen::JsonValue* missing_paths_value = FindObjectMember(root, "missingPaths");
        if (missing_paths_value == nullptr)
        {
            definition.MissingPaths.clear();
            return true;
        }

        if (!missing_paths_value->IsArray())
        {
            return false;
        }

        std::set<std::string> normalized_paths;
        definition.MissingPaths.clear();
        for (const helen::JsonValue& entry : missing_paths_value->AsArray())
        {
            if (!entry.IsString())
            {
                return false;
            }

            const std::string normalized = NormalizeManifestGamePath(entry.AsString());
            if (normalized.empty())
            {
                return false;
            }

            if (!normalized_paths.insert(normalized).second)
            {
                return false;
            }

            definition.MissingPaths.push_back(normalized);
        }

        return true;
    }
```

Call it from `ParseBuildManifest(...)` after the startup-command fields are parsed:

```cpp
        if (!ParseMissingPaths(root, definition))
        {
            return false;
        }
```

Use one shared helper to normalize separators and lowercase manifest paths:

```cpp
    std::string NormalizeManifestGamePath(std::string_view path)
    {
        std::string normalized;
        normalized.reserve(path.size());
        for (char character : path)
        {
            const char folded = character == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            normalized.push_back(folded);
        }

        while (!normalized.empty() && normalized.front() == '/')
        {
            normalized.erase(normalized.begin());
        }

        return normalized;
    }
```

- [ ] **Step 5: Re-run the runtime tests**

Run:

```powershell
msbuild tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
```

Expected: PASS for the new repository coverage.

- [ ] **Step 6: Commit**

```bash
git add include/HelenHook/BuildDefinition.h HelenRuntime/PackRepository.cpp tests/HelenRuntime.Tests/PackRepositoryTests.cpp
git commit -m "Add hidden path metadata parsing"
```

### Task 2: Add a Reusable Hidden Path Matcher

**Files:**
- Create: `include/HelenHook/HiddenPathMatcher.h`
- Create: `HelenRuntime/HiddenPathMatcher.cpp`
- Create: `tests/HelenRuntime.Tests/HiddenPathMatcherTests.cpp`
- Modify: `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`
- Modify: `tests/HelenRuntime.Tests/TestMain.cpp`
- Modify: `HelenRuntime/HelenRuntime.vcxproj`

- [ ] **Step 1: Write the failing matcher test**

Create `tests/HelenRuntime.Tests/HiddenPathMatcherTests.cpp`:

```cpp
#include <HelenHook/HiddenPathMatcher.h>

#include <filesystem>
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
}

void RunHiddenPathMatcherTests()
{
    helen::HiddenPathMatcher matcher(
        std::filesystem::path(L"D:/steam/steamapps/common/Batman Arkham Asylum GOTY"),
        {
            "bmgame/movies/legal.bik",
            "bmgame/movies/nvidia.bik"
        });

    Expect(
        matcher.ShouldHidePath(std::filesystem::path(L"D:\\steam\\steamapps\\common\\Batman Arkham Asylum GOTY\\BmGame\\Movies\\Legal.bik")),
        "Expected absolute Legal.bik path to match.");
    Expect(
        matcher.ShouldHidePath(std::filesystem::path(L"BmGame/Movies/nvidia.bik")),
        "Expected relative nvidia.bik path to match.");
    Expect(
        !matcher.ShouldHidePath(std::filesystem::path(L"BmGame/Movies/Intro.bik")),
        "Unexpected non-hidden movie match.");
}
```

Add the runner declaration and call in `tests/HelenRuntime.Tests/TestMain.cpp`:

```cpp
void RunHiddenPathMatcherTests();
```

```cpp
        RunHiddenPathMatcherTests();
```

Add the new source to `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`:

```xml
<ClCompile Include="HiddenPathMatcherTests.cpp" />
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
msbuild tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32
```

Expected: FAIL because `HiddenPathMatcher` does not exist yet.

- [ ] **Step 3: Add the matcher interface**

Create `include/HelenHook/HiddenPathMatcher.h`:

```cpp
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace helen
{
    class HiddenPathMatcher
    {
    public:
        HiddenPathMatcher(std::filesystem::path game_root, std::vector<std::string> hidden_paths);

        bool ShouldHidePath(const std::filesystem::path& candidate_path) const;
        std::wstring NormalizeToRelativeGamePath(const std::filesystem::path& candidate_path) const;

    private:
        std::filesystem::path game_root_;
        std::vector<std::wstring> hidden_paths_;
    };
}
```

- [ ] **Step 4: Implement normalization and matching**

Create `HelenRuntime/HiddenPathMatcher.cpp`:

```cpp
#include <HelenHook/HiddenPathMatcher.h>

#include <algorithm>
#include <cwctype>
#include <stdexcept>

namespace
{
    std::wstring FoldPath(std::wstring value)
    {
        std::replace(value.begin(), value.end(), L'\\', L'/');
        std::transform(value.begin(), value.end(), value.begin(), [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
        while (!value.empty() && value.front() == L'/')
        {
            value.erase(value.begin());
        }
        return value;
    }
}

namespace helen
{
    HiddenPathMatcher::HiddenPathMatcher(std::filesystem::path game_root, std::vector<std::string> hidden_paths)
        : game_root_(std::filesystem::weakly_canonical(std::move(game_root)))
    {
        if (game_root_.empty())
        {
            throw std::invalid_argument("HiddenPathMatcher requires a non-empty game root.");
        }

        for (const std::string& hidden_path : hidden_paths)
        {
            hidden_paths_.push_back(FoldPath(std::filesystem::path(hidden_path).wstring()));
        }
    }

    std::wstring HiddenPathMatcher::NormalizeToRelativeGamePath(const std::filesystem::path& candidate_path) const
    {
        std::filesystem::path normalized_candidate = candidate_path;
        if (normalized_candidate.is_absolute())
        {
            normalized_candidate = std::filesystem::relative(normalized_candidate, game_root_);
        }

        return FoldPath(normalized_candidate.wstring());
    }

    bool HiddenPathMatcher::ShouldHidePath(const std::filesystem::path& candidate_path) const
    {
        const std::wstring normalized = NormalizeToRelativeGamePath(candidate_path);
        return std::find(hidden_paths_.begin(), hidden_paths_.end(), normalized) != hidden_paths_.end();
    }
}
```

Add the source to `HelenRuntime/HelenRuntime.vcxproj`:

```xml
<ClCompile Include="HiddenPathMatcher.cpp" />
```

- [ ] **Step 5: Re-run the tests**

Run:

```powershell
msbuild tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
```

Expected: PASS with the matcher test now included.

- [ ] **Step 6: Commit**

```bash
git add include/HelenHook/HiddenPathMatcher.h HelenRuntime/HiddenPathMatcher.cpp HelenRuntime/HelenRuntime.vcxproj tests/HelenRuntime.Tests/HiddenPathMatcherTests.cpp tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj tests/HelenRuntime.Tests/TestMain.cpp
git commit -m "Add hidden path matcher"
```

### Task 3: Extend `FileApiHookSet` to Return “Not Found”

**Files:**
- Modify: `include/HelenHook/FileApiHookSet.h`
- Modify: `HelenRuntime/FileApiHookSet.cpp`
- Modify: `HelenGameHook/HelenGameHook.cpp`
- Create: `tests/HelenRuntime.Tests/FileApiHookSetTests.cpp`
- Modify: `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`
- Modify: `tests/HelenRuntime.Tests/TestMain.cpp`

- [ ] **Step 1: Write failing tests for hidden-path helper behavior**

Create `tests/HelenRuntime.Tests/FileApiHookSetTests.cpp` using helper-focused tests so the logic is unit-testable:

```cpp
#include <HelenHook/HiddenPathMatcher.h>

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
}

void RunFileApiHookSetTests()
{
    helen::HiddenPathMatcher matcher(
        std::filesystem::path(L"D:/steam/steamapps/common/Batman Arkham Asylum GOTY"),
        { "bmgame/movies/legal.bik" });

    Expect(
        matcher.ShouldHidePath(std::filesystem::path(L"BmGame/Movies/Legal.bik")),
        "Expected CreateFile-style hidden path match.");
    Expect(
        matcher.ShouldHidePath(std::filesystem::path(L"D:/steam/steamapps/common/Batman Arkham Asylum GOTY/BmGame/Movies/Legal.bik")),
        "Expected GetFileAttributes-style hidden path match.");
}
```

Add the runner declaration and call in `TestMain.cpp`, and add the file to the test project:

```cpp
void RunFileApiHookSetTests();
```

```cpp
        RunFileApiHookSetTests();
```

```xml
<ClCompile Include="FileApiHookSetTests.cpp" />
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
msbuild tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32
```

Expected: FAIL because the new test file has not been added and the runtime has no hook coverage for hidden paths yet.

- [ ] **Step 3: Extend the hook header with the new detours and matcher state**

Update `include/HelenHook/FileApiHookSet.h`:

```cpp
#include <HelenHook/HiddenPathMatcher.h>
```

Add detour declarations:

```cpp
        static DWORD WINAPI GetFileAttributesWDetour(LPCWSTR lpFileName);
        static DWORD WINAPI GetFileAttributesADetour(LPCSTR lpFileName);
        static HANDLE WINAPI FindFirstFileWDetour(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData);
        static HANDLE WINAPI FindFirstFileADetour(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData);
        static BOOL WINAPI FindNextFileWDetour(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData);
        static BOOL WINAPI FindNextFileADetour(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData);
```

Add state:

```cpp
        HiddenPathMatcher hidden_path_matcher_;
        IatHook get_file_attributes_w_hook_;
        IatHook get_file_attributes_a_hook_;
        IatHook find_first_file_w_hook_;
        IatHook find_first_file_a_hook_;
        IatHook find_next_file_w_hook_;
        IatHook find_next_file_a_hook_;
```

- [ ] **Step 4: Implement hidden-path responses in `FileApiHookSet.cpp`**

Change the constructor signature in `include/HelenHook/FileApiHookSet.h` so the hook set receives the real runtime game root and parsed hidden paths directly:

```cpp
        FileApiHookSet(
            VirtualFileService& virtual_files,
            std::filesystem::path game_root,
            std::vector<std::string> hidden_paths);
```

Then initialize the matcher in `HelenRuntime/FileApiHookSet.cpp`:

```cpp
    FileApiHookSet::FileApiHookSet(
        VirtualFileService& virtual_files,
        std::filesystem::path game_root,
        std::vector<std::string> hidden_paths)
        : virtual_files_(virtual_files)
        , hidden_path_matcher_(std::move(game_root), std::move(hidden_paths))
    {
    }
```

Add one shared helper near the top of the file:

```cpp
    bool ShouldHidePath(FileApiHookSet& active, const std::filesystem::path& path)
    {
        return active.hidden_path_matcher_.ShouldHidePath(path);
    }
```

Short-circuit `CreateFileWDetour` and `CreateFileADetour` before virtual-file handling:

```cpp
        if (lpFileName != nullptr && ShouldHidePath(*active, std::filesystem::path(lpFileName)))
        {
            SetLastError(ERROR_FILE_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }
```

Add `GetFileAttributesW/A` detours:

```cpp
    DWORD WINAPI FileApiHookSet::GetFileAttributesWDetour(LPCWSTR lpFileName)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return INVALID_FILE_ATTRIBUTES;
        }

        if (lpFileName != nullptr && ShouldHidePath(*active, std::filesystem::path(lpFileName)))
        {
            SetLastError(ERROR_FILE_NOT_FOUND);
            return INVALID_FILE_ATTRIBUTES;
        }

        const auto get_file_attributes_w = ResolveKernel32Export<decltype(&GetFileAttributesW)>("GetFileAttributesW");
        return get_file_attributes_w == nullptr ? INVALID_FILE_ATTRIBUTES : get_file_attributes_w(lpFileName);
    }
```

Filter enumeration in `FindFirstFileW/A` and `FindNextFileW/A` by skipping any entry whose resolved candidate path matches the matcher. Use a small loop that repeatedly calls the real API until a non-hidden entry is found or enumeration ends:

```cpp
        while (find_next_file_w(hFindFile, lpFindFileData))
        {
            const std::filesystem::path candidate = std::filesystem::path(search_root) / lpFindFileData->cFileName;
            if (!ShouldHidePath(*active, candidate))
            {
                return TRUE;
            }
        }
```

Install and remove the new hooks in `Install()` and `Remove()` the same way the existing optional hooks are installed.

Update `HelenGameHook/HelenGameHook.cpp` so the real runtime bootstrap passes the layout-derived game root and active build metadata:

```cpp
        g_file_hooks = std::make_unique<helen::FileApiHookSet>(
            *g_virtual_files,
            layout.GameRoot,
            active_pack.Build.MissingPaths);
```

- [ ] **Step 5: Re-run the tests**

Run:

```powershell
msbuild tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
```

Expected: PASS with the new `FileApiHookSet` coverage included.

- [ ] **Step 6: Commit**

```bash
git add include/HelenHook/FileApiHookSet.h HelenRuntime/FileApiHookSet.cpp HelenGameHook/HelenGameHook.cpp tests/HelenRuntime.Tests/FileApiHookSetTests.cpp tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj tests/HelenRuntime.Tests/TestMain.cpp
git commit -m "Hide declared missing paths in file hooks"
```

### Task 4: Add the Standalone Batman Skip Videos Pack

**Files:**
- Create: `games/HelenBatmanAA/helengamehook/packs/batman-aa-skip-videos/pack.json`
- Create: `games/HelenBatmanAA/helengamehook/packs/batman-aa-skip-videos/builds/steam-goty-1.0/build.json`
- Create: `games/HelenBatmanAA/helengamehook/packs/batman-aa-skip-videos/builds/steam-goty-1.0/files.json`
- Create: `games/HelenBatmanAA/helengamehook/packs/batman-aa-skip-videos/builds/steam-goty-1.0/bindings.json`
- Create: `games/HelenBatmanAA/helengamehook/packs/batman-aa-skip-videos/builds/steam-goty-1.0/hooks.json`
- Create: `games/HelenBatmanAA/helengamehook/packs/batman-aa-skip-videos/builds/steam-goty-1.0/textures.json`
- Create: `games/HelenBatmanAA/helengamehook/packs/batman-aa-skip-videos/builds/steam-goty-1.0/commands.json`
- Create: `games/HelenBatmanAA/scripts/Test-BatmanSkipVideosPack.ps1`
- Modify: `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`

- [ ] **Step 1: Write the failing Batman pack contract test**

Create `games/HelenBatmanAA/scripts/Test-BatmanSkipVideosPack.ps1`:

```powershell
param([string]$BatmanRoot)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}

$PackRoot = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-skip-videos'
$PackJsonPath = Join-Path $PackRoot 'pack.json'
$BuildJsonPath = Join-Path $PackRoot 'builds\steam-goty-1.0\build.json'

foreach ($RequiredPath in @($PackJsonPath, $BuildJsonPath)) {
    if (-not (Test-Path -LiteralPath $RequiredPath)) {
        throw "Required Batman skip-videos pack file not found: $RequiredPath"
    }
}

$PackJson = Get-Content -LiteralPath $PackJsonPath -Raw | ConvertFrom-Json
$BuildJson = Get-Content -LiteralPath $BuildJsonPath -Raw | ConvertFrom-Json

if ($PackJson.id -ne 'batman-aa-skip-videos') { throw 'Pack id mismatch.' }
if ($BuildJson.id -ne 'steam-goty-1.0') { throw 'Build id mismatch.' }
if (@($BuildJson.missingPaths).Count -ne 5) { throw 'Expected 5 hidden startup videos.' }

Write-Output 'PASS'
```

Add one more synthetic pack case to the existing temporary `packs_root` setup in `PackRepositoryTests.cpp`:

```cpp
        const std::filesystem::path skip_videos_pack_root = packs_root / "batman-aa-skip-videos";
        const std::filesystem::path skip_videos_build_root = skip_videos_pack_root / "builds" / "steam-goty-1.0";
        std::filesystem::create_directories(skip_videos_build_root);
```

Write the synthetic pack files:

```cpp
        WriteAllText(
            skip_videos_pack_root / "pack.json",
            R"({
  "schemaVersion": 1,
  "id": "batman-aa-skip-videos",
  "name": "Batman Arkham Asylum Skip Videos",
  "targets": [
    {
      "gameId": "batman-arkham-asylum",
      "executables": [ "ShippingPC-BmGame.exe" ]
    }
  ],
  "builds": [ "steam-goty-1.0" ]
})");

        WriteAllText(
            skip_videos_build_root / "build.json",
            R"({
  "id": "steam-goty-1.0",
  "executable": "ShippingPC-BmGame.exe",
  "missingPaths": [
    "BmGame/Movies/baa_logo_run_v5_h264.bik",
    "BmGame/Movies/Legal.bik",
    "BmGame/Movies/Legalus.bik",
    "BmGame/Movies/nvidia.bik",
    "BmGame/Movies/utlogo.bik"
  ],
  "match": {
    "fileSize": 38758728,
    "sha256": "9E23F9D4E0E5D81A6B8DFA7937A6E6E7FB6953EFFA607105C8B0E5DED4C72C19"
  }
})");
```

Then load it from the temporary root:

```cpp
        const std::optional<helen::LoadedBuildPack> loaded_skip_videos_pack = repository.LoadForExecutable(
            packs_root,
            "ShippingPC-BmGame.exe",
            38758728,
            "9e23f9d4e0e5d81a6b8dfa7937a6e6e7fb6953effa607105c8b0e5ded4c72c19");
        Expect(loaded_skip_videos_pack->Build.MissingPaths.size() == 5, "Checked-in Batman skip-videos hidden path count mismatch.");
```

- [ ] **Step 2: Run the pack contract test to verify it fails**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File games\HelenBatmanAA\scripts\Test-BatmanSkipVideosPack.ps1
```

Expected: FAIL because the standalone pack files do not exist yet.

- [ ] **Step 3: Add the standalone pack manifests**

Create `games/HelenBatmanAA/helengamehook/packs/batman-aa-skip-videos/pack.json`:

```json
{
  "schemaVersion": 1,
  "id": "batman-aa-skip-videos",
  "name": "Batman Arkham Asylum Skip Videos",
  "description": "Standalone Batman pack that hides startup movie files so the game skips intro videos.",
  "targets": [
    {
      "gameId": "batman-arkham-asylum",
      "executables": [
        "ShippingPC-BmGame.exe"
      ]
    }
  ],
  "builds": [
    "steam-goty-1.0"
  ]
}
```

Create `build.json`:

```json
{
  "id": "steam-goty-1.0",
  "executable": "ShippingPC-BmGame.exe",
  "missingPaths": [
    "BmGame/Movies/baa_logo_run_v5_h264.bik",
    "BmGame/Movies/Legal.bik",
    "BmGame/Movies/Legalus.bik",
    "BmGame/Movies/nvidia.bik",
    "BmGame/Movies/utlogo.bik"
  ],
  "match": {
    "fileSize": 38758728,
    "sha256": "9E23F9D4E0E5D81A6B8DFA7937A6E6E7FB6953EFFA607105C8B0E5DED4C72C19"
  }
}
```

Create empty manifests:

```json
{ "virtualFiles": [] }
```

```json
{ "bindings": [] }
```

```json
{}
```

```json
{ "replacements": [] }
```

```json
{ "commands": [] }
```

- [ ] **Step 4: Run the focused pack test and the runtime tests**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File games\HelenBatmanAA\scripts\Test-BatmanSkipVideosPack.ps1
msbuild tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
```

Expected:

- `Test-BatmanSkipVideosPack.ps1` prints `PASS`
- `HelenRuntimeTests.exe` prints `PASS`

- [ ] **Step 5: Commit**

```bash
git add games/HelenBatmanAA/helengamehook/packs/batman-aa-skip-videos games/HelenBatmanAA/scripts/Test-BatmanSkipVideosPack.ps1 tests/HelenRuntime.Tests/PackRepositoryTests.cpp
git commit -m "Add Batman skip-videos pack"
```

## Self-Review

- Spec coverage:
  - generic `missingPaths` metadata: Task 1
  - reusable path matcher: Task 2
  - CreateFile/GetFileAttributes/FindFirst/FindNext behavior: Task 3
  - standalone Batman pack: Task 4
- Placeholder scan:
  - No `TODO` / `TBD` markers remain.
  - Each task includes code or manifest content plus concrete commands.
- Type consistency:
  - `BuildDefinition::MissingPaths`, `HiddenPathMatcher`, and `RunHiddenPathMatcherTests` are used consistently across tasks.
