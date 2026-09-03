# Batman Graphics Options Startup Reliability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Batman's four active graphics rows load the launcher-owned persisted values reliably on every menu entry and relaunch.

**Architecture:** `BatmanGraphicsConfigService` will treat sibling `UserEngine.ini` as the sole startup-read authority while retaining two-file Apply persistence. `MemoryStateObserverService` will carry a pass-local set of address groups that already performed a broad scan, allowing cached polling for every member but only one unresolved or replacement scan per group per pass.

**Tech Stack:** C++20, Win32 process-memory APIs, the existing native test harness, MSBuild, PowerShell package validators.

---

### Task 1: Load Startup Graphics State from UserEngine.ini

**Files:**
- Modify: `tests/HelenRuntime.Tests/CommandExecutorTests.cpp`
- Modify: `HelenRuntime/BatmanGraphicsConfigService.cpp`

- [ ] **Step 1: Make the existing graphics-load fixture disagree across the two INIs**

In the Batman graphics command block, keep `BmEngine.ini` at its current generated values and write a launcher fixture whose Group 1 values are different:

```cpp
const std::string launcher_graphics_text =
    "[Engine.Engine]\r\n"
    "PhysXLevel=1\r\n"
    "\r\n"
    "[SystemSettings]\r\n"
    "Fullscreen=False\r\n"
    "UseVsync=True\r\n"
    "ResX=2560\r\n"
    "ResY=1440\r\n"
    "MaxMultisamples=8\r\n"
    "DetailMode=2\r\n"
    "Bloom=True\r\n"
    "DynamicShadows=True\r\n"
    "MotionBlur=True\r\n"
    "Distortion=True\r\n"
    "FogVolumes=True\r\n"
    "DisableSphericalHarmonicLights=False\r\n"
    "AmbientOcclusion=True\r\n"
    "Stereo=True\r\n"
    "LauncherOwnedSentinel=PreserveMe\r\n";
WriteAsciiAsUtf16LittleEndianText(batman_user_ini_path, launcher_graphics_text);
```

Change only the startup expectations to require `vsync == 1`, `msaa == 3`, `physx == 1`, and `stereo == 1`. Keep the existing unrelated-setting expectations.

- [ ] **Step 2: Add fail-fast startup cases**

Add two scoped cases after the main Batman graphics block. Each case must create a valid generated `BmEngine.ini`, register every graphics config key, construct `BatmanGraphicsConfigService`, and invoke the real `loadBatmanGraphicsDraftIntoConfig` command.

The missing-launcher case removes the sibling `UserEngine.ini` and asserts the command returns false:

```cpp
Expect(!missing_user_executor.RunCommand("loadBatmanGraphicsDraftIntoConfig"),
    "Batman graphics startup unexpectedly fell back to generated BmEngine.ini when UserEngine.ini was missing.");
```

The invalid-launcher case writes a UTF-16LE `UserEngine.ini` with `Stereo` omitted and asserts the command returns false:

```cpp
Expect(!invalid_user_executor.RunCommand("loadBatmanGraphicsDraftIntoConfig"),
    "Batman graphics startup unexpectedly accepted an incomplete launcher-owned UserEngine.ini.");
```

Use distinct scenario subdirectories below `CreateTemporaryBatmanGraphicsIniPath().parent_path()` so repeat runs cannot share fixture residue. Every added helper function must be namespace-scoped and carry substantive Doxygen comments.

- [ ] **Step 3: Build and run the native suite to verify RED**

Run:

```powershell
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' '.\tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /t:Build /p:Configuration=Debug /p:Platform=Win32 /m:1 /v:minimal; if (`$LASTEXITCODE -eq 0) { & '.\bin\Win32\Debug\tests\HelenRuntimeTests.exe'; exit `$LASTEXITCODE } else { exit `$LASTEXITCODE }"
```

Expected: FAIL because startup still reads `BmEngine.ini`; the conflicting-values or missing-launcher assertion must be the reported failure.

- [ ] **Step 4: Switch only the startup-read authority**

Update `BatmanGraphicsConfigService::LoadIntoDispatcher`:

```cpp
const std::filesystem::path user_ini_path = ini_path_.parent_path() / "UserEngine.ini";
const std::optional<std::vector<std::string>> lines = TryReadAllLines(user_ini_path);
if (!lines.has_value())
{
    Logf(L"[graphics] Load failed: unable to read launcher INI path=%ls.", user_ini_path.wstring().c_str());
    return false;
}
```

Parse these lines with the existing complete-draft validator and write the result into the dispatcher. Update the method's Doxygen comment to state that startup reads launcher-owned `UserEngine.ini`; do not add a `BmEngine.ini` or default fallback. Leave `ApplyFromDispatcher`'s two-file write order unchanged.

- [ ] **Step 5: Rebuild and verify GREEN**

Run the Step 3 command again.

Expected: `PASS`.

- [ ] **Step 6: Commit Task 1**

```powershell
rtk git add -- HelenRuntime/BatmanGraphicsConfigService.cpp tests/HelenRuntime.Tests/CommandExecutorTests.cpp
rtk git commit -m "Load Batman graphics state from launcher config"
```

### Task 2: Deduplicate Unresolved Address-Group Scans

**Files:**
- Modify: `include/HelenHook/MemoryStateObserverService.h`
- Modify: `HelenRuntime/MemoryStateObserverService.cpp`
- Modify: `tests/HelenRuntime.Tests/MemoryStateObserverServiceTests.cpp`

- [ ] **Step 1: Add a grouped late-carrier regression test**

Add `RunGroupedGraphicsCarrierSingleScanPerPassTest`. Allocate one writable page, build three graphics observers in `batmanFrontendControlType`, and leave the carrier absent for the first manual pass:

```cpp
Expect(service.PollOnce(), "Grouped unresolved observer pass unexpectedly failed.");
std::vector<helen::MemoryStateObserverDebugView> debug_views = service.GetDebugViews();
Expect(debug_views[0].RescanCount == 1, "The first grouped observer did not lead the unresolved scan.");
Expect(debug_views[1].RescanCount == 0, "The second grouped observer duplicated the unresolved scan.");
Expect(debug_views[2].RescanCount == 0, "The third grouped observer duplicated the unresolved scan.");
```

Then call `ConfigureGraphicsCarrierStateBlock(candidate_address, 4200)`, poll again with a config callback returning VSync `1`, and require:

```cpp
Expect(ReadInt32(candidate_address + 12) == 4211, "The late grouped carrier did not receive the VSync response.");
Expect(debug_views[0].RescanCount == 2, "The next pass did not retry grouped discovery.");
Expect(debug_views[1].RescanCount == 0 && debug_views[2].RescanCount == 0,
    "Grouped observers performed duplicate scans after late carrier creation.");
Expect(debug_views[0].CachedAddress == candidate_address &&
       debug_views[1].CachedAddress == candidate_address &&
       debug_views[2].CachedAddress == candidate_address,
    "The resolved late carrier was not shared across the complete group.");
```

Register the test in `RunMemoryStateObserverServiceTests`. Preserve the existing allocation cleanup pattern and substantive Doxygen comments.

- [ ] **Step 2: Run the native suite to verify RED**

Run the Task 1 Step 3 command.

Expected: FAIL because all three unresolved observers currently increment `RescanCount` during one pass.

- [ ] **Step 3: Pass scan ownership through each observer pass**

Add `<unordered_set>` to the service header and change the private method to:

```cpp
bool PollObserver(
    std::size_t observer_index,
    std::unordered_set<std::string>& scanned_address_groups);
```

Update its Doxygen to define the pass-local set and the one-scan invariant. In both `PollOnce` and `PollDueObservers`, create one empty set before iterating and pass it to every actual observer poll:

```cpp
std::unordered_set<std::string> scanned_address_groups;
```

Inside `PollObserver`, after cached validation fails and after clearing a stale cache, permit the broad scan only when the observer is ungrouped or successfully inserts its group into the pass-local set:

```cpp
bool may_rescan = true;
if (definition.AddressGroup.has_value())
{
    may_rescan = scanned_address_groups.insert(*definition.AddressGroup).second;
}

if (may_rescan)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ++debug_views_[observer_index].RescanCount;
}
```

Wrap the existing `VirtualQuery` scan loop in the same `may_rescan` condition. A skipped duplicate scan is a successful no-match poll; it must not stop the worker, increment `RescanCount`, modify pending transactions, or create a default address. Do not change grouped cached validation, signature checks, raw-value mappings, or polling intervals.

- [ ] **Step 4: Rebuild and verify GREEN twice**

Run the Task 1 Step 3 build-and-test command, then run the produced test executable again:

```powershell
rtk proxy powershell.exe -NoProfile -Command "& '.\bin\Win32\Debug\tests\HelenRuntimeTests.exe'; exit `$LASTEXITCODE"
```

Expected: `PASS` from both runs.

- [ ] **Step 5: Commit Task 2**

```powershell
rtk git add -- include/HelenHook/MemoryStateObserverService.h HelenRuntime/MemoryStateObserverService.cpp tests/HelenRuntime.Tests/MemoryStateObserverServiceTests.cpp
rtk git commit -m "Deduplicate grouped observer discovery scans"
```

### Task 3: Review, Verify, Build, and Install

**Files:**
- Verify: all Task 1-2 files
- Install target: `C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries`

- [ ] **Step 1: Run spec and code-quality review**

Review the cumulative Task 1-2 diff against `docs/superpowers/specs/2026-09-03-batman-graphics-options-startup-reliability-design.md`. Blocking findings must be fixed with a failing test first and reviewed again.

- [ ] **Step 2: Run focused and package verification**

Run the native suite twice using Task 2 Step 4, then run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellContract.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsPackage.ps1 -Configuration Release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellDeployment.ps1
rtk git diff --check
```

Expected: every validator reports `PASS`; the shell contract also reports `STATE_MACHINE_PASS`; `git diff --check` is empty. The retail GFX/delta artifacts are unchanged by this runtime-only fix, so the already-passing retail round-trip is not regenerated.

- [ ] **Step 3: Build the Release hook without reusable MSBuild nodes**

```powershell
rtk proxy powershell.exe -NoProfile -Command "`$env:MSBUILDDISABLENODEREUSE='1'; & 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' '.\HelenGameHook.sln' /t:HelenGameHook /p:Configuration=Release /p:Platform=Win32 /m:1 /nodeReuse:false /v:minimal; exit `$LASTEXITCODE"
```

Expected: exit `0`, no MessageBoxes, and a fresh `bin/Win32/Release/HelenGameHook.dll`.

- [ ] **Step 4: Install only the current graphics package/runtime publication**

After obtaining the normal Program Files approval, run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Deploy-BatmanGraphicsOptionsExperiment.ps1 -GameBin 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries' -Configuration Release
```

Expected: `DEPLOYED`. The existing subtitle pack is preserved byte-for-byte; no historical package, `batma/`, or `F:\helenhook.7z` is an input.

- [ ] **Step 5: Hash-verify installed bytes**

Compare SHA-256 for the installed Release hook, proxy, `packs.json`, and all seven graphics-pack files against their repository counterparts. Require an exact seven-file live graphics pack and reject stray observer or `.gfx` files.

- [ ] **Step 6: Perform two-launch live verification**

With `UserEngine.ini` holding `VSync Off / MSAA 8x / PhysX Normal / Stereo On`:

1. Launch Batman and open Graphics Options. Require those exact values and no `Unavailable` row.
2. Change one value and Apply. Require the setting acknowledgement followed by the Apply acknowledgement and matching values in both INIs.
3. Exit normally, relaunch, and reopen Graphics Options. Require the newly persisted values again and no intermittent timeout.

- [ ] **Step 7: Commit any review-only corrections and report repository state**

Stage only files owned by this plan. Preserve the unrelated dirty project files and untracked `batma/`. Report all new commit hashes, validator results, installed hashes, and live results.
