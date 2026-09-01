# Batman Skip-Videos File-Path Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the standalone Batman skip-video pack hide its five declared startup movies at the Win32 file-open boundary.

**Architecture:** Normalize incoming paths before matching, and give `HiddenPathMatcher` both the installation root and the Binaries request base so canonical relative, Binaries-relative, and absolute requests resolve consistently. Keep suppression in `FileApiHookSet` and remove the disproven `_BinkOpen@8` filename hook because Batman passes in-memory movie buffers there.

**Tech Stack:** C++20, Win32 `CreateFileA/W` IAT hooks, `std::filesystem`, Visual Studio MSBuild, PowerShell pack-contract tests.

---

## File Map

- `tests/HelenRuntime.Tests/HiddenPathMatcherTests.cpp`: regression coverage for lexical normalization, request-base resolution, and outside-root rejection.
- `include/HelenHook/HiddenPathMatcher.h`: declares the installation root and request-base matching contract.
- `HelenRuntime/HiddenPathMatcher.cpp`: implements direct canonical matching plus request-base resolution.
- `include/HelenHook/FileApiHookSet.h`: exposes explicit installation-root and request-base constructor inputs.
- `HelenRuntime/FileApiHookSet.cpp`: applies hidden matching to file opens and retains concise suppression logs.
- `HelenGameHook/HelenGameHook.cpp`: wires the installation root and Binaries base into the file hook and removes Bink-hook ownership.
- `HelenRuntime/HelenRuntime.vcxproj`: removes the experimental Bink source/header entries.
- `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`: removes the experimental Bink test entry.
- `tests/HelenRuntime.Tests/TestMain.cpp`: removes experimental Bink test registration.
- `include/HelenHook/BinkMovieHookSet.h`, `HelenRuntime/BinkMovieHookSet.cpp`, and `tests/HelenRuntime.Tests/BinkMovieHookSetTests.cpp`: delete the disproven experimental implementation.
- `games/HelenBatmanAA/scripts/Test-BatmanSkipVideosPack.ps1`: retain exact five-path contract verification.

### Task 1: Normalize Observed Absolute Movie Paths

**Files:**

- Modify: `tests/HelenRuntime.Tests/HiddenPathMatcherTests.cpp`
- Modify: `HelenRuntime/HiddenPathMatcher.cpp`

- [ ] **Step 1: Add the exact failing Steam-path regression**

Add these assertions after the existing absolute `Legal.bik` assertion:

```cpp
    Expect(
        matcher.ShouldHidePath(std::filesystem::path(
            L"D:\\steam\\steamapps\\common\\Batman Arkham Asylum GOTY\\Binaries\\..\\BmGame\\Movies\\Legal.bik")),
        "Expected the observed Binaries-relative absolute Legal.bik path to match.");
    Expect(
        !matcher.ShouldHidePath(std::filesystem::path(
            L"C:\\Users\\Helena\\Documents\\Square Enix\\Batman Arkham Asylum GOTY\\Binaries\\..\\BmGame\\Movies\\Legal.bik")),
        "Expected the Documents fallback movie path to remain visible.");
```

- [ ] **Step 2: Run the test executable and verify RED**

Run:

```powershell
[Environment]::SetEnvironmentVariable('PATH',$null,[EnvironmentVariableTarget]::Process)
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /t:Build /p:Configuration=Debug /p:Platform=Win32 /m:1 /v:minimal
& '.\bin\Win32\Debug\tests\HelenRuntimeTests.exe'
```

Expected: the build succeeds and the test executable fails with `Expected the observed Binaries-relative absolute Legal.bik path to match.`

- [ ] **Step 3: Lexically normalize candidates before relativizing**

Change the start of `HiddenPathMatcher::NormalizeToRelativeGamePath` to:

```cpp
    std::wstring HiddenPathMatcher::NormalizeToRelativeGamePath(const std::filesystem::path& candidate_path) const
    {
        std::filesystem::path normalized_candidate = candidate_path.lexically_normal();
        if (normalized_candidate.is_absolute())
        {
            normalized_candidate = normalized_candidate.lexically_relative(game_root_);
        }

        return FoldPath(normalized_candidate.wstring());
    }
```

- [ ] **Step 4: Rebuild and verify GREEN**

Run the commands from Step 2 again.

Expected: `PASS`.

### Task 2: Resolve Relative Requests Against Binaries

**Files:**

- Modify: `tests/HelenRuntime.Tests/HiddenPathMatcherTests.cpp`
- Modify: `tests/HelenRuntime.Tests/FileApiHookSetTests.cpp`
- Modify: `include/HelenHook/HiddenPathMatcher.h`
- Modify: `HelenRuntime/HiddenPathMatcher.cpp`
- Modify: `include/HelenHook/FileApiHookSet.h`
- Modify: `HelenRuntime/FileApiHookSet.cpp`
- Modify: `HelenGameHook/HelenGameHook.cpp`

- [ ] **Step 1: Change the matcher test to require a request base**

Construct the matcher in both matcher test files with an installation root followed by the Binaries request base:

```cpp
    helen::HiddenPathMatcher matcher(
        std::filesystem::path(L"D:/steam/steamapps/common/Batman Arkham Asylum GOTY"),
        std::filesystem::path(L"D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries"),
        {
            "bmgame/movies/legal.bik",
            "bmgame/movies/nvidia.bik"
        });
```

Add this assertion to `HiddenPathMatcherTests.cpp`:

```cpp
    Expect(
        matcher.ShouldHidePath(std::filesystem::path(L"..\\BmGame\\Movies\\Legal.bik")),
        "Expected a Binaries-relative Legal.bik request to match.");
```

Keep the Documents fallback and outside-installation assertions so host paths cannot match by suffix.

- [ ] **Step 2: Build and verify RED**

Run the Task 1 build commands.

Expected: compilation fails because `HiddenPathMatcher` does not yet accept an explicit request base.

- [ ] **Step 3: Add the request-base matching contract**

Change the declaration in `include/HelenHook/HiddenPathMatcher.h` to:

```cpp
        /**
         * @brief Creates one matcher with separate installation and relative-request roots.
         * @param game_root Absolute installation root used to relativize absolute candidates.
         * @param request_base_directory Absolute directory used to resolve candidates containing parent traversal.
         * @param hidden_paths Canonical relative paths that should be treated as hidden.
         */
        HiddenPathMatcher(
            std::filesystem::path game_root,
            std::filesystem::path request_base_directory,
            std::vector<std::string> hidden_paths);
```

Add this field before `hidden_paths_`:

```cpp
        /** @brief Absolute directory used to resolve relative runtime requests after direct matching. */
        std::filesystem::path request_base_directory_;
```

Implement the constructor in `HelenRuntime/HiddenPathMatcher.cpp` as:

```cpp
    HiddenPathMatcher::HiddenPathMatcher(
        std::filesystem::path game_root,
        std::filesystem::path request_base_directory,
        std::vector<std::string> hidden_paths)
        : game_root_(std::filesystem::weakly_canonical(std::move(game_root))),
          request_base_directory_(std::filesystem::weakly_canonical(std::move(request_base_directory)))
    {
        if (game_root_.empty() || request_base_directory_.empty())
        {
            throw std::invalid_argument("HiddenPathMatcher requires non-empty game and request roots.");
        }

        for (const std::string& hidden_path : hidden_paths)
        {
            hidden_paths_.push_back(FoldPath(std::filesystem::path(hidden_path).wstring()));
        }
    }
```

Implement direct matching followed by request-base resolution:

```cpp
    bool HiddenPathMatcher::ShouldHidePath(const std::filesystem::path& candidate_path) const
    {
        const std::wstring normalized = NormalizeToRelativeGamePath(candidate_path);
        if (std::find(hidden_paths_.begin(), hidden_paths_.end(), normalized) != hidden_paths_.end())
        {
            return true;
        }

        if (!candidate_path.is_relative())
        {
            return false;
        }

        const std::filesystem::path resolved_candidate =
            (request_base_directory_ / candidate_path).lexically_normal();
        const std::wstring resolved = NormalizeToRelativeGamePath(resolved_candidate);
        return std::find(hidden_paths_.begin(), hidden_paths_.end(), resolved) != hidden_paths_.end();
    }
```

- [ ] **Step 4: Wire both roots into the file hook**

Change the `FileApiHookSet` constructor to accept these explicit paths:

```cpp
        FileApiHookSet(
            VirtualFileService& virtual_files,
            std::filesystem::path game_installation_root,
            std::filesystem::path request_base_directory,
            std::vector<std::string> hidden_paths);
```

Update its implementation:

```cpp
    FileApiHookSet::FileApiHookSet(
        VirtualFileService& virtual_files,
        std::filesystem::path game_installation_root,
        std::filesystem::path request_base_directory,
        std::vector<std::string> hidden_paths)
        : virtual_files_(virtual_files),
          hidden_path_matcher_(
              std::move(game_installation_root),
              std::move(request_base_directory),
              std::move(hidden_paths))
    {
    }
```

Update `HelenGameHook.cpp` so the runtime call is:

```cpp
        g_file_hooks = std::make_unique<helen::FileApiHookSet>(
            *g_virtual_files,
            layout.GameRoot.parent_path(),
            layout.GameRoot,
            active_pack_set.MissingPaths);
```

Update Doxygen parameter descriptions to name the installation root and Binaries request base precisely.

- [ ] **Step 5: Rebuild and verify GREEN**

Run the Task 1 build and test commands.

Expected: `PASS`, including the exact absolute path, relative `..` path, Documents fallback, and undeclared movie coverage.

### Task 3: Remove the Disproven Bink Hook and Diagnostic Noise

**Files:**

- Delete: `include/HelenHook/BinkMovieHookSet.h`
- Delete: `HelenRuntime/BinkMovieHookSet.cpp`
- Delete: `tests/HelenRuntime.Tests/BinkMovieHookSetTests.cpp`
- Modify: `HelenGameHook/HelenGameHook.cpp`
- Modify: `HelenRuntime/HelenRuntime.vcxproj`
- Modify: `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`
- Modify: `tests/HelenRuntime.Tests/TestMain.cpp`
- Modify: `HelenRuntime/FileApiHookSet.cpp`

- [ ] **Step 1: Remove Bink ownership and project registration**

Remove the Bink include, `g_bink_movie_hooks` field, initialization block, reset/release calls, project source/header entries, test declaration/call, and test project entry. Delete the three untracked Bink files with `apply_patch` so no experimental implementation remains in the package.

- [ ] **Step 2: Replace temporary observation logs with suppression-only logs**

Remove `IsBinkMoviePath`, its `<algorithm>` and `<cwctype>` dependencies when no longer needed, and all `hide=false` observation logging. In each `CreateFile` detour, log only the branch that actually hides a path:

```cpp
            if (should_hide)
            {
                helen::Logf(L"[file] suppressed CreateFileW path=%ls", lpFileName);
                SetLastError(ERROR_FILE_NOT_FOUND);
                return INVALID_HANDLE_VALUE;
            }
```

Use the corresponding converted wide path in `CreateFileADetour`:

```cpp
                if (should_hide)
                {
                    helen::Logf(L"[file] suppressed CreateFileA path=%ls", hidden_path->c_str());
                    SetLastError(ERROR_FILE_NOT_FOUND);
                    return INVALID_HANDLE_VALUE;
                }
```

- [ ] **Step 3: Run automated verification**

Run:

```powershell
[Environment]::SetEnvironmentVariable('PATH',$null,[EnvironmentVariableTarget]::Process)
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /t:Build /p:Configuration=Debug /p:Platform=Win32 /m:1 /v:minimal
& '.\bin\Win32\Debug\tests\HelenRuntimeTests.exe'
& '.\games\HelenBatmanAA\scripts\Test-BatmanSkipVideosPack.ps1'
rtk.exe git diff --check
```

Expected: the build succeeds, both test commands print `PASS`, and `git diff --check` exits zero.

### Task 4: Release Build and Isolated Live Proof

**Files:**

- Build: `bin/Win32/Release/HelenGameHook.dll`
- Install: `C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\HelenGameHook.dll`
- Verify: `C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\helengamehook\logs\HelenGameHook.log`

- [ ] **Step 1: Build Win32 Release**

Run:

```powershell
[Environment]::SetEnvironmentVariable('PATH',$null,[EnvironmentVariableTarget]::Process)
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'HelenGameHook\HelenGameHook.vcxproj' /t:Build /p:Configuration=Release /p:Platform=Win32 /m:1 /v:minimal
```

Expected: `HelenGameHook.vcxproj -> ...\bin\Win32\Release\HelenGameHook.dll` with no errors.

- [ ] **Step 2: Record immutable movie hashes and install the DLL**

Run the following with approval for the writes below `Program Files (x86)`:

```powershell
$binariesRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries'
$moviesRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\BmGame\Movies'
$sourceDll = 'C:\dev\helenhook\bin\Win32\Release\HelenGameHook.dll'
$targetDll = Join-Path $binariesRoot 'HelenGameHook.dll'
$backupDirectory = Join-Path $binariesRoot 'helengamehook\backups\skip-path-fix-before-live-20260901'
$hashEvidenceDirectory = 'C:\dev\helenhook\output\batman-aa-skip-videos-test'
$movieNames = @(
    'baa_logo_run_v5_h264.bik',
    'Legal.bik',
    'Legalus.bik',
    'nvidia.bik',
    'utlogo.bik'
)

if (Get-Process -Name 'ShippingPC-BmGame' -ErrorAction SilentlyContinue)
{
    throw 'Batman is running; refusing to replace the hook DLL.'
}

New-Item -ItemType Directory -Path $backupDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $hashEvidenceDirectory -Force | Out-Null
$movieNames |
    ForEach-Object { Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $moviesRoot $_) } |
    Select-Object Path, Hash |
    Export-Csv -NoTypeInformation -LiteralPath (Join-Path $hashEvidenceDirectory 'movie-hashes-before.csv')
Copy-Item -LiteralPath $targetDll -Destination (Join-Path $backupDirectory 'HelenGameHook.dll') -Force
Copy-Item -LiteralPath $sourceDll -Destination $targetDll -Force

$dllHashes = Get-FileHash -Algorithm SHA256 -LiteralPath $sourceDll, $targetDll
if ($dllHashes[0].Hash -ne $dllHashes[1].Hash)
{
    throw 'Installed DLL hash does not match the Release build.'
}

Get-Content -LiteralPath (Join-Path $binariesRoot 'helengamehook\config\packs.json')
```

Expected: the DLL hashes match and `packs.json` lists only `batman-aa-skip-videos`.

- [ ] **Step 3: Cold-launch Batman and inspect only the appended log segment**

Expected live evidence:

```text
[runtime] loaded explicit pack set executable=ShippingPC-BmGame.exe count=1
[runtime] active pack set ready executable=ShippingPC-BmGame.exe count=1 startup=0 hidden=5
[file] suppressed CreateFileW path=...\BmGame\Movies\baa_logo_run_v5_h264.bik
[file] suppressed CreateFileW path=...\BmGame\Movies\UTlogo.bik
[file] suppressed CreateFileW path=...\BmGame\Movies\Legal.bik
```

There must be no Bink-hook installation or binary-buffer log entry. The two startup videos must not play.

- [ ] **Step 4: Re-hash real movies and commit the verified implementation**

Verify the five movie hashes are unchanged:

```powershell
$moviesRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\BmGame\Movies'
$beforePath = 'C:\dev\helenhook\output\batman-aa-skip-videos-test\movie-hashes-before.csv'
$movieNames = @(
    'baa_logo_run_v5_h264.bik',
    'Legal.bik',
    'Legalus.bik',
    'nvidia.bik',
    'utlogo.bik'
)
$beforeHashes = Import-Csv -LiteralPath $beforePath
$afterHashes = $movieNames |
    ForEach-Object { Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $moviesRoot $_) } |
    Select-Object Path, Hash
$hashDifferences = Compare-Object $beforeHashes $afterHashes -Property Path, Hash
if ($hashDifferences)
{
    $hashDifferences | Format-Table -AutoSize
    throw 'One or more real Batman movie files changed during verification.'
}
```

Stage only the hidden-path matcher, file hook, runtime wiring, exact skip-pack contract, and tests. Do not stage `batma/` or generated output:

```powershell
rtk.exe git add -- `
    'include/HelenHook/HiddenPathMatcher.h' `
    'HelenRuntime/HiddenPathMatcher.cpp' `
    'include/HelenHook/FileApiHookSet.h' `
    'HelenRuntime/FileApiHookSet.cpp' `
    'HelenGameHook/HelenGameHook.cpp' `
    'tests/HelenRuntime.Tests/HiddenPathMatcherTests.cpp' `
    'tests/HelenRuntime.Tests/FileApiHookSetTests.cpp' `
    'games/HelenBatmanAA/scripts/Test-BatmanSkipVideosPack.ps1'
rtk.exe git diff --cached --name-only
rtk.exe git commit -m 'Fix Batman startup video path hiding'
```

Expected: one implementation commit after live proof, with the existing subtitle texture commit and design commit preserved in history.
