# Batman Bink Startup Video Hiding Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the enabled `batman-aa-skip-videos` pack suppress exactly five Batman startup `.bik` files at the game's direct `binkw32.dll!_BinkOpen@8` import.

**Architecture:** Add a focused `BinkMovieHookSet` that owns the main executable's `_BinkOpen@8` IAT slot. It reuses `HiddenPathMatcher` for absolute/relative, case-insensitive path matching, returns `nullptr` for a configured startup movie, and invokes the captured original Bink function for every other request. `HelenGameHook` constructs this hook only when the merged active pack set has hidden paths, so the existing subtitle-only configuration remains unchanged.

**Tech Stack:** C++20, Win32 IAT hooks, Bink 1 `__stdcall` import contract, existing `HelenRuntime.Tests` console harness, PowerShell deployment scripts.

## Global Constraints

- Hide only the five paths declared by `batman-aa-skip-videos`; never hide gameplay cutscenes.
- Do not delete, rename, replace, or modify real `.bik` files.
- Fail runtime initialization when hidden paths are configured but the expected Bink import cannot be installed.
- Keep the Bink ABI opaque: use `void*` for the `BinkOpen` result and preserve `const char*, unsigned int` arguments.
- Add substantive Doxygen comments to every new class, member, and function.

---

### Task 1: Add Bink request matching coverage

**Files:**

- Create: `include/HelenHook/BinkMovieHookSet.h`
- Create: `tests/HelenRuntime.Tests/BinkMovieHookSetTests.cpp`
- Modify: `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`
- Modify: `tests/HelenRuntime.Tests/TestMain.cpp`

**Interfaces:**

- Produces `helen::BinkMovieHookSet::ShouldSuppressMoviePath(std::string_view) const` for the later Bink detour.
- Consumes `HiddenPathMatcher` through a game root and merged `MissingPaths` constructor argument.

- [ ] **Step 1: Write the failing matcher test**

Create `BinkMovieHookSetTests.cpp` with an isolated test that constructs a hook set with the five pack paths and asserts:

```cpp
helen::BinkMovieHookSet hook_set(
    std::filesystem::path(L"C:/Game"),
    { "bmgame/movies/legal.bik", "bmgame/movies/nvidia.bik" });

Expect(hook_set.ShouldSuppressMoviePath("BmGame\\Movies\\LEGAL.BIK"),
    "Expected a configured Bink movie to be suppressed.");
Expect(hook_set.ShouldSuppressMoviePath("C:/Game/BmGame/Movies/nvidia.bik"),
    "Expected an absolute configured Bink movie to be suppressed.");
Expect(!hook_set.ShouldSuppressMoviePath("BmGame/Movies/101_Batmobile_Intro.bik"),
    "Expected gameplay Bink movies to remain available.");
Expect(!hook_set.ShouldSuppressMoviePath(nullptr),
    "Expected a null Bink movie path to pass through.");
```

Register `RunBinkMovieHookSetTests()` in `TestMain.cpp` and add the test source to the test project.

- [ ] **Step 2: Verify RED**

Run:

```powershell
rtk proxy powershell.exe -NoProfile -Command "msbuild .\tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32 /t:Build"
```

Expected: compilation fails because `BinkMovieHookSet` does not exist.

- [ ] **Step 3: Implement the matcher-facing class declaration**

Define `BinkMovieHookSet` with a constructor accepting `std::filesystem::path game_root` and `std::vector<std::string> hidden_paths`, plus `ShouldSuppressMoviePath(LPCSTR movie_path) const`. Convert the ANSI input with `MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, ...)` and delegate the resulting path to `HiddenPathMatcher::ShouldHidePath`.

- [ ] **Step 4: Verify GREEN**

Build and run:

```powershell
rtk proxy powershell.exe -NoProfile -Command "msbuild .\tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32 /t:Build; & .\bin\Win32\Debug\tests\HelenRuntimeTests.exe"
```

Expected: `PASS` and the three matching cases prove only declared startup paths are suppressible.

### Task 2: Hook the direct Bink import

**Files:**

- Modify: `include/HelenHook/BinkMovieHookSet.h`
- Create: `HelenRuntime/BinkMovieHookSet.cpp`
- Modify: `HelenRuntime/HelenRuntime.vcxproj`
- Modify: `tests/HelenRuntime.Tests/BinkMovieHookSetTests.cpp`

**Interfaces:**

- Produces `bool Install()`, `void Remove()`, and `bool IsInstalled() const noexcept`.
- Consumes `helen::IatHook`, `helen::QueryMainModule`, and `helen::FindImportAddress` from existing runtime infrastructure.

- [ ] **Step 1: Extend the failing test with lifecycle preconditions**

Add assertions that a new hook set reports not installed before `Install()`, and remains not installed after `Remove()` without an installation. Keep matching behavior assertions separate from lifecycle assertions.

- [ ] **Step 2: Verify RED**

Run the Debug test harness. Expected: compile failure because lifecycle methods are absent.

- [ ] **Step 3: Implement the IAT hook lifecycle**

In `BinkMovieHookSet.cpp`:

- declare `using BinkOpenFunction = void* (WINAPI*)(const char*, unsigned int);`
- add one static active-instance pointer and one `IatHook bink_open_hook_` member
- in `Install()`, resolve the main module, install `bink_open_hook_` against `binkw32.dll` / `_BinkOpen@8`, and fail when the import is absent or the IAT write fails
- in `BinkOpenDetour`, return `nullptr` and emit `[bink] suppressed movie path=...` when `ShouldSuppressMoviePath` matches; otherwise invoke `bink_open_hook_.Original<BinkOpenFunction>()` with the original arguments
- make `Remove()` restore the IAT and clear the active pointer
- make `IsInstalled()` require both active ownership and the installed IAT hook

Do not create a synthetic Bink handle or alter Bink flags.

- [ ] **Step 4: Verify GREEN**

Build and run the complete Debug harness. Expected: `PASS`.

- [ ] **Step 5: Verify the real import contract**

Run:

```powershell
rtk proxy 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x86\dumpbin.exe' /imports 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe'
```

Expected: the `binkw32.dll` section includes `_BinkOpen@8`.

### Task 3: Wire Bink suppression into the active pack runtime

**Files:**

- Modify: `HelenGameHook/HelenGameHook.cpp`
- Modify: `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanSkipVideosPack.ps1`

**Interfaces:**

- Consumes `ActivePackSet::MissingPaths` and `RuntimeLayout::GameRoot`.
- Produces one active `g_bink_movie_hooks` runtime owner when hidden paths are configured.

- [ ] **Step 1: Write the failing pack contract assertion**

Extend `Test-BatmanSkipVideosPack.ps1` to compare the `missingPaths` array exactly, in order, to the five allowed startup paths. Add a repository test that the checked-in enabled pack set merges exactly five hidden paths.

- [ ] **Step 2: Verify RED**

Run the PowerShell contract test and Debug harness. Expected: the new exact-list assertion/test registration fails until the implementation wiring is present.

- [ ] **Step 3: Implement startup and teardown ownership**

Add `std::unique_ptr<helen::BinkMovieHookSet> g_bink_movie_hooks`. During initialization, after `g_active_runtime_pack_set` is built and before the game begins loading assets:

```cpp
if (!g_active_runtime_pack_set->MissingPaths.empty())
{
    g_bink_movie_hooks = std::make_unique<helen::BinkMovieHookSet>(
        g_layout->GameRoot,
        g_active_runtime_pack_set->MissingPaths);
    if (!g_bink_movie_hooks->Install())
    {
        helen::Log(L"[runtime] failed to install Bink movie hooks for configured hidden paths.");
        return false;
    }
}
```

Reset `g_bink_movie_hooks` before the runtime layout, active pack set, and log path are released. Add clear install/success log lines with the hidden-path count.

- [ ] **Step 4: Verify GREEN**

Run the contract test and the complete Debug harness. Expected: both print `PASS`.

### Task 4: Build, deploy, and prove live Bink suppression

**Files:**

- Modify only generated deployment outputs under the Steam install.

- [ ] **Step 1: Build Release**

Run:

```powershell
rtk proxy powershell.exe -NoProfile -Command "msbuild .\HelenGameHook.sln /m /p:Configuration=Release /p:Platform=Win32"
```

Expected: successful Win32 Release build with no errors.

- [ ] **Step 2: Deploy through the validated script**

Run `Deploy-Batman.ps1` with:

```powershell
-GameBin 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries' -Configuration Release
```

Expected: `DEPLOYED` after pack and installed-base validation.

- [ ] **Step 3: Live verification**

Launch Batman normally, then inspect `Binaries\helengamehook\logs\HelenGameHook.log`.

Expected:

- startup shows the Bink hook installed for five hidden paths;
- each startup movie request yields `[bink] suppressed movie path=...`;
- no suppression entry names gameplay/cutscene movies;
- the real five `.bik` files still exist in `BmGame\Movies`.

- [ ] **Step 4: Commit**

```powershell
rtk git add -- include/HelenHook/BinkMovieHookSet.h HelenRuntime/BinkMovieHookSet.cpp HelenRuntime/HelenRuntime.vcxproj HelenGameHook/HelenGameHook.cpp tests/HelenRuntime.Tests/BinkMovieHookSetTests.cpp tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj tests/HelenRuntime.Tests/TestMain.cpp tests/HelenRuntime.Tests/PackRepositoryTests.cpp games/HelenBatmanAA/scripts/Test-BatmanSkipVideosPack.ps1 docs/superpowers/plans/2026-07-25-batman-bink-startup-video-hiding.md
rtk git commit -m "Hide Batman startup movies through Bink"
```
