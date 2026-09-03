# Batman Graphics Detail and Quality Options Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Activate Batman: Arkham Asylum GOTY's Detail Level and seven quality-toggle rows with reliable startup values, derived preset behavior, serialized Apply/rollback, and reproducible packaging.

**Architecture:** Keep Detail Level as a shell-local derived preset controller and transmit only the seven INI-backed quality leaves through the existing grouped memory-observer transport. Extend the declarative package generator with a compact collision-free protocol, reuse `BatmanGraphicsConfigService` as the sole configuration authority, and make its two-file publication compensating so a failed commit cannot leave the launcher and generated INIs in different transactions.

**Tech Stack:** C++20/Win32, ActionScript 2 emitted by C#/.NET 8, PowerShell package tooling, MSBuild Win32, FFDec, Unreal package delta tooling.

**Spec:** `docs/superpowers/specs/2026-09-03-batman-graphics-detail-quality-options-design.md`

## Global Constraints

- Work directly on `main`, as explicitly authorized by the user.
- Preserve unrelated existing changes in `HelenRuntime/HelenRuntime.vcxproj`, `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`, and `tests/HelenRuntime.Tests/TestMain.cpp`.
- Do not inspect or use `batma/`, the installed game files, `F:\helenhook.7z`, or historical generated artifacts as build inputs.
- Fullscreen and Resolution must remain visible, unselectable `Not active` rows.
- Detail Level is derived from seven leaves; `Custom` is display-only and must never cross the memory protocol.
- Use raw codes `4600` through `4669` exactly as allocated by the spec; leave existing `4200` through `4599` and `4960` through `4991` behavior unchanged.
- Every new or changed C++/C# member must have substantive Doxygen/XML documentation and follow the repository's same-line-brace and member-order conventions.
- Do not add tuples, nullable ownership shortcuts, runtime fallback defaults, legacy `Helen_*` callbacks, or GUI error paths.
- Use `apply_patch` for source edits. Use console-only build/test invocations through `rtk proxy powershell.exe` where environment normalization is required.
- Do not deploy while `ShippingPC-BmGame.exe` is running.
- End every task with a focused commit containing only that task's files.

---

### Task 1: Make dual-INI graphics publication compensating

**Files:**
- Modify: `HelenRuntime/BatmanGraphicsConfigService.cpp:441-543,1537-1601`
- Test: `tests/HelenRuntime.Tests/CommandExecutorTests.cpp:130-380,560-690`

**Interfaces:**
- Consumes: `BatmanGraphicsConfigService::ApplyFromDispatcher(const CommandDispatcher&) const` and the existing encoding-preserving `IniTextDocument` parser.
- Produces: the same public `bool ApplyFromDispatcher(...)` interface, with all-or-restored publication semantics for `BmEngine.ini` and sibling `UserEngine.ini`.

- [ ] **Step 1: Add byte-exact file fixture helpers and two publication-failure tests**

Add documented test helpers that read raw bytes and acquire a Win32 handle denying write/delete sharing. Add two cases around the existing Batman graphics apply fixture:

```cpp
const std::vector<std::uint8_t> original_engine_bytes = ReadAllBytes(engine_ini_path);
const std::vector<std::uint8_t> original_user_bytes = ReadAllBytes(user_ini_path);

HANDLE locked_engine = OpenFileWithoutWriteOrDeleteSharing(engine_ini_path);
Expect(locked_engine != INVALID_HANDLE_VALUE, "Generated INI failure fixture could not lock BmEngine.ini.");
Expect(!batman_graphics_config_service.ApplyFromDispatcher(batman_dispatcher),
    "Graphics apply unexpectedly succeeded while generated INI publication was blocked.");
CloseHandle(locked_engine);
Expect(ReadAllBytes(engine_ini_path) == original_engine_bytes,
    "Failed graphics apply changed the generated INI.");
Expect(ReadAllBytes(user_ini_path) == original_user_bytes,
    "Failed graphics apply did not restore the launcher INI exactly.");
```

Repeat with `UserEngine.ini` locked. That case proves a future generated-first publisher compensates the already-published generated file when launcher publication fails. Always close the handle before assertions that may throw.

- [ ] **Step 2: Run the native suite and verify the new test fails for the current publisher**

Run:

```powershell
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /t:Build /p:Configuration=Debug /p:Platform=Win32 /m:1 /nodeReuse:false /v:minimal; if (`$LASTEXITCODE -ne 0) { exit `$LASTEXITCODE }; & '.\bin\Win32\Debug\tests\HelenRuntimeTests.exe'; exit `$LASTEXITCODE"
```

Expected: `FAIL` because current code writes `UserEngine.ini` before a blocked `BmEngine.ini` write and does not restore it.

- [ ] **Step 3: Add namespace-scope staging, replacement, cleanup, and compensation helpers**

Keep implementation helpers in the anonymous namespace; do not create local functions inside `ApplyFromDispatcher`. Use exact byte snapshots and same-directory temporary files. The helper surface should express the transaction explicitly:

```cpp
bool TryReadFileBytes(const std::filesystem::path& path, std::vector<std::uint8_t>& bytes);
bool TryEncodeIniDocument(const IniTextDocument& document, std::vector<std::uint8_t>& bytes);
bool TryEncodeIniLines(const std::vector<std::string>& lines, std::vector<std::uint8_t>& bytes);
bool TryCreateSiblingTemporaryFile(
    const std::filesystem::path& target_path,
    std::filesystem::path& temporary_path);
bool TryWriteFileBytes(
    const std::filesystem::path& path,
    const std::vector<std::uint8_t>& bytes);
bool TryReplaceFileFromSibling(
    const std::filesystem::path& staged_path,
    const std::filesystem::path& target_path);
bool TryPublishBatmanGraphicsIniPair(
    const std::filesystem::path& engine_path,
    const std::vector<std::uint8_t>& engine_bytes,
    const std::vector<std::uint8_t>& original_engine_bytes,
    const std::filesystem::path& user_path,
    const std::vector<std::uint8_t>& user_bytes,
    const std::vector<std::uint8_t>& original_user_bytes);
```

`TryPublishBatmanGraphicsIniPair` must:

1. Stage and close both new files before replacing either target.
2. Replace generated `BmEngine.ini` first and launcher `UserEngine.ini` second with same-volume Win32 replacement semantics.
3. If the first replacement fails, leave both originals untouched.
4. If the second replacement fails, stage the exact original generated bytes and replace the generated target back.
5. Remove every owned stage/recovery file on all return paths.
6. Log a distinct compensation failure if the original generated file cannot be restored.

- [ ] **Step 4: Route `ApplyFromDispatcher` through the pair publisher**

Prepare and validate both edited documents before staging. Preserve original bytes before mutation, encode both outputs, call the pair publisher once, and log success only after both replacements succeed:

```cpp
if (!TryPublishBatmanGraphicsIniPair(
        ini_path_,
        encoded_engine_bytes,
        original_engine_bytes,
        user_ini_path,
        encoded_user_bytes,
        original_user_bytes)) {
    Logf(L"[graphics] Apply failed: dual-INI publication did not commit.");
    return false;
}
```

Do not change config mapping, launcher authority, or public method signatures.

- [ ] **Step 5: Run the native suite twice**

Run the command from Step 2 twice. Expected both times: `PASS`, with no MessageBoxes or orphaned stage/recovery files in the temporary fixture directories.

- [ ] **Step 6: Review and commit Task 1**

```powershell
rtk git diff --check
rtk git diff -- HelenRuntime/BatmanGraphicsConfigService.cpp tests/HelenRuntime.Tests/CommandExecutorTests.cpp
rtk git add -- HelenRuntime/BatmanGraphicsConfigService.cpp tests/HelenRuntime.Tests/CommandExecutorTests.cpp
rtk git commit -m "Make Batman graphics INI apply transactional"
```

---

### Task 2: Activate the detail-quality shell controller

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellContract.ps1:790-960,1265-1360`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellScriptTemplates.cs:85-520,593-748`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsAssetBuilder.cs:217-257,530-559`

**Interfaces:**
- Consumes: the existing shell controller `Settings` record shape and `CreateActiveRowClipAction` behavior.
- Produces: eleven transmitted setting records plus a derived row-5 controller with `GetDetailLevelDraftIndex`, `GetDetailLevelInitialIndex`, `CanEditDetailLevel`, `SetDetailPreset`, `ToggleDetailPreset`, `IncrementDetailPreset`, and `DecrementDetailPreset` ActionScript methods.

- [ ] **Step 1: Change shell-contract expectations before production templates**

Update the exact setting-definition table to include these seven records between MSAA and PhysX:

```powershell
@{ RowIndex=6;  Name='Bloom';                         Values=@('Off','On'); ConfigValues=@(0,1); Read=4600; ResponseBase=4601; WriteBase=4603; AckBase=4605; Failure=4609 },
@{ RowIndex=7;  Name='Dynamic Shadows';               Values=@('Off','On'); ConfigValues=@(0,1); Read=4610; ResponseBase=4611; WriteBase=4613; AckBase=4615; Failure=4619 },
@{ RowIndex=8;  Name='Motion Blur';                   Values=@('Off','On'); ConfigValues=@(0,1); Read=4620; ResponseBase=4621; WriteBase=4623; AckBase=4625; Failure=4629 },
@{ RowIndex=9;  Name='Distortion';                    Values=@('Off','On'); ConfigValues=@(0,1); Read=4630; ResponseBase=4631; WriteBase=4633; AckBase=4635; Failure=4639 },
@{ RowIndex=10; Name='Fog Volumes';                   Values=@('Off','On'); ConfigValues=@(0,1); Read=4640; ResponseBase=4641; WriteBase=4643; AckBase=4645; Failure=4649 },
@{ RowIndex=11; Name='Spherical Harmonic Lighting';   Values=@('Off','On'); ConfigValues=@(0,1); Read=4650; ResponseBase=4651; WriteBase=4653; AckBase=4655; Failure=4659 },
@{ RowIndex=12; Name='Ambient Occlusion';             Values=@('Off','On'); ConfigValues=@(0,1); Read=4660; ResponseBase=4661; WriteBase=4663; AckBase=4665; Failure=4669 }
```

Require row 5 values `Low`, `Medium`, `High`, `Very High`, `Custom`; rows 6-12 active `Off`/`On`; rows 1-2 still fixed `Not active`. Assert that row 5 calls only the dedicated preset methods and that `Settings` contains no row-5 record.

Require the exact four canonical leaf combinations and verify that no `FE_SetControlType`, `FE_GetControlType`, `Helen_*`, or write request occurs inside local edit/preset functions. Extend the state-machine harness to cover eleven startup requests in screen order, Detail Level becoming Unavailable when any leaf fails, preset changes queuing only dirty leaves, exact leaf acknowledgements, timeouts, commit failure, rollback success/failure, and Back discarding the local draft without sending a write.

- [ ] **Step 2: Run the shell contract and verify it fails on the inactive rows**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellContract.ps1 -Configuration Release
```

Expected: `FAIL` because the production template still declares four settings and rows 5-12 are `Not active`.

- [ ] **Step 3: Add the seven transmitted setting records**

Extend `CreateSettings()` in visual order using the exact records from Step 1. Preserve `ConfigValues:new Array(0,1)` and base-plus-index response/write/ack behavior. Do not add Detail Level to `Settings`.

- [ ] **Step 4: Add derived preset controller methods**

Implement the approved preset matrix as ActionScript controller methods. `GetDetailLevelDraftIndex()` and `GetDetailLevelInitialIndex()` must return `0..3` for canonical combinations and `4` for Custom. `SetDetailPreset(index, forward)` accepts only `0..3`, plays one navigation sound, updates all seven leaf `DraftIndex` values, and refreshes once after the full mutation.

Directional behavior is deterministic:

- Low: left disabled, right selects Medium.
- Medium/High: left/right select adjacent presets.
- Very High: right disabled, left selects High.
- Custom: right disabled, left selects Very High.
- Activate/RunAction advances Low -> Medium -> High -> Very High -> Low; activating Custom selects Low.

Use one predicate for Detail Level editability: initialization complete, no initialization failure, no blocked interaction, no rollback lock, and all seven leaf indices resolved.

- [ ] **Step 5: Emit a dedicated Detail Level row and active leaf rows**

Add a documented C# `CreateDetailLevelRowClipAction()` generator. It displays five names but delegates mutations only to the dedicated preset methods. Replace rows 6-12 with `CreateActiveRowClipAction(...)`, retain fixed rows 1-2, and update the builder XML documentation from “four active settings” to the exact derived-plus-eleven model.

- [ ] **Step 6: Run shell contract and builder tests**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellContract.ps1 -Configuration Release
```

Expected: shell contract builds the production builder and reflection harness, then prints `STATE_MACHINE_PASS` and `PASS` without launching a GUI.

- [ ] **Step 7: Review and commit Task 2**

```powershell
rtk git diff --check
rtk git add -- games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellContract.ps1 games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellScriptTemplates.cs games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsAssetBuilder.cs
rtk git commit -m "Activate Batman detail quality rows"
```

---

### Task 3: Generate and package the seven quality observer protocols

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Rebuild-BatmanGraphicsOptionsExperiment.ps1:148-296,656-778`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1:797-941`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanRetailGraphicsOptionsPatch.ps1`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsLayout.ps1`
- Modify: `games/HelenBatmanAA/scripts/Deploy-BatmanGraphicsOptionsExperiment.ps1`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellDeployment.ps1`
- Modify: `tests/HelenRuntime.Tests/PackRepositoryTests.cpp:1250-1380`
- Generated: `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/commands.json`
- Generated: `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/hooks.json`
- Generated: `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/files.json`
- Generated: `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/assets/deltas/Frontend-graphics-options.hgdelta`
- Generated: `games/HelenBatmanAA/builder/generated/graphics-options-experiment/Frontend-graphics-options.umap`

**Interfaces:**
- Consumes: `New-GraphicsCarrierObserver` and the existing `sync-batman-graphics-detail-level` command-step kind.
- Produces: protocol-table entries with a required `Command` string, a `syncBatmanGraphicsDetailLevel` command, thirteen observers total, synchronized validators, and an exact seven-file package rebuilt from approved sources.

- [ ] **Step 1: Extend package/parser expectations first**

Require three commands in order:

```json
[
  { "id": "loadBatmanGraphicsDraftIntoConfig", "steps": [{ "kind": "load-batman-graphics-draft-into-config" }] },
  { "id": "syncBatmanGraphicsDetailLevel", "steps": [{ "kind": "sync-batman-graphics-detail-level" }] },
  { "id": "applyBatmanGraphicsDraft", "steps": [{ "kind": "apply-batman-graphics-config" }, { "kind": "load-batman-graphics-draft-into-config" }] }
]
```

Require `graphicsObserverBloom -> bloom`, `graphicsObserverDynamicShadows -> dynamicShadows`, `graphicsObserverMotionBlur -> motionBlur`, `graphicsObserverDistortion -> distortion`, `graphicsObserverFogVolumes -> fogVolumes`, `graphicsObserverSphericalHarmonicLighting -> sphericalHarmonicLighting`, and `graphicsObserverAmbientOcclusion -> ambientOcclusion`, with each observer using its exact code range from the spec. Require `command: "syncBatmanGraphicsDetailLevel"` on each new leaf observer, no `command` property on VSync/MSAA/PhysX/Stereo, and the unchanged Apply/rollback commands. Require every observer's `addressMatchValues` to equal the complete sorted union.

- [ ] **Step 2: Run package verification and native tests to establish RED**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsPackage.ps1 -Configuration Release
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /t:Build /p:Configuration=Debug /p:Platform=Win32 /m:1 /nodeReuse:false /v:minimal; if (`$LASTEXITCODE -ne 0) { exit `$LASTEXITCODE }; & '.\bin\Win32\Debug\tests\HelenRuntimeTests.exe'; exit `$LASTEXITCODE"
```

Expected: both reject the old two-command/six-observer package fixture.

- [ ] **Step 3: Expand and validate the authoritative protocol table**

Add a required `Command` property to all protocol rows. Existing rows use `Command=''`; the seven new rows use `Command='syncBatmanGraphicsDetailLevel'`. Add the exact `4600..4669` allocations from the spec. Update `Assert-GraphicsProtocolTable` to require a string Command, reject whitespace-only nonempty values, and retain global ID/key/raw-code collision checks.

Pass `-Command $protocol.Command` into `New-GraphicsCarrierObserver`; its existing omission behavior must keep the property absent for empty commands.

- [ ] **Step 4: Generate the synchronization command**

Create `syncBatmanGraphicsDetailLevel` between load and apply:

```powershell
$syncGraphicsDetailLevelCommand = [ordered]@{
    id = 'syncBatmanGraphicsDetailLevel'
    name = 'Sync Batman Graphics Detail Level'
    steps = @([ordered]@{ kind = 'sync-batman-graphics-detail-level' })
}
$commands = [ordered]@{
    commands = @($loadGraphicsDraftCommand, $syncGraphicsDetailLevelCommand, $applyGraphicsDraftCommand)
}
```

- [ ] **Step 5: Align duplicate validator and deployment contracts**

Require rows 5-12 active and rows 1-2 inactive in retail/layout tests. Require thirteen observers, three commands, the complete code union, and the seven synchronization commands in the deployment validator. Remove stale assertions that all rows are inactive or that Apply is absent; replace them with the current fifteen-row shell and serialized Apply contract.

- [ ] **Step 6: Prove the new shell is deterministic and capture its concrete hash**

Run the retail graphics patch validator once while its fixed shell hash is still stale:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanRetailGraphicsOptionsPatch.ps1 -Configuration Release
```

Expected: its independent shell builds are byte-identical, but the script reports the new actual SHA-256 against the old `6CF058DA55867BE38F4A7861C877EB2D4CFD98E4510848D924B2322FCBDF65A5` expectation. If the independent builds differ, stop and fix determinism before continuing.

Set `$ExpectedGraphicsShellSha256` in both `Test-BatmanGraphicsOptionsPackage.ps1` and `Test-BatmanRetailGraphicsOptionsPatch.ps1` to the uppercase hash emitted by those identical builds. Do not predict or insert a hash before this measurement.

- [ ] **Step 7: Regenerate only through the checked-in atomic rebuild script**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Rebuild-BatmanGraphicsOptionsExperiment.ps1 -Configuration Release
```

Expected: staged verifier `PASS`, post-publication verifier `PASS`, exact retail base hash `271916B888F83374122AF0FCCC5C685804F4C8286A92A772CD71E4F48A00F2CC`, and printed new shell/target/delta hashes. Do not bypass validation, copy installed files, or hand-edit generated JSON.

- [ ] **Step 8: Run native parser tests and every package validator**

Run the native command from Step 2, followed by:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellContract.ps1 -Configuration Release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsPackage.ps1 -Configuration Release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanRetailGraphicsOptionsPatch.ps1 -Configuration Release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsLayout.ps1 -Configuration Release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellDeployment.ps1
```

Expected: native tests and every script print `PASS`; the shell contract also prints `STATE_MACHINE_PASS`. Verify `commands.json` has exactly three commands, `hooks.json` has exactly thirteen observers, all seven leaf observers carry the sync command, and the pack contains exactly seven files.

- [ ] **Step 9: Verify artifact provenance and exact inventory**

Confirm the delta reconstructs the exact target declared by `files.json`; no loose GFX, old observer manifest, backup, staging directory, recovery directory, or installed artifact exists in the pack; and the retail source, target, shell, and delta hashes are recorded for the handoff. Confirm from command inputs and rebuild provenance checks that `batma/`, installed game files, and `F:\helenhook.7z` were not generator inputs.

- [ ] **Step 10: Review and commit Task 3**

Stage only the generator, validators, parser test, and current generated package/target files belonging to this milestone. Inspect the staged list before committing; never commit a package that fails its own verifier.

```powershell
rtk git diff --check
rtk git add -- games/HelenBatmanAA/scripts/Rebuild-BatmanGraphicsOptionsExperiment.ps1 games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1 games/HelenBatmanAA/scripts/Test-BatmanRetailGraphicsOptionsPatch.ps1 games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsLayout.ps1 games/HelenBatmanAA/scripts/Deploy-BatmanGraphicsOptionsExperiment.ps1 games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellDeployment.ps1 tests/HelenRuntime.Tests/PackRepositoryTests.cpp games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options games/HelenBatmanAA/builder/generated/graphics-options-experiment/Frontend-graphics-options.umap
rtk git diff --cached --name-status
rtk git commit -m "Package Batman detail quality protocol"
```

---

### Task 4: Prove grouped runtime behavior and complete config coverage

**Files:**
- Modify: `tests/HelenRuntime.Tests/MemoryStateObserverServiceTests.cpp`
- Modify: `tests/HelenRuntime.Tests/CommandExecutorTests.cpp:560-650`

**Interfaces:**
- Consumes: the generated observer record contract, `MemoryStateObserverService::PollDueObservers`, and existing config-service methods.
- Produces: regression coverage for the eleven-setting address group, every quality mapping, derived presets/Custom, inverted spherical lighting, and dual-file persistence.

- [ ] **Step 1: Add an eleven-setting grouped-discovery fixture**

Construct the four existing and seven new setting definitions with one shared group and complete union. Seed a carrier only after the first poll. Assert:

```cpp
Expect(memory_reader.GetScanCount("graphicsObserverVsync") >= 2,
    "The group leader did not retry late graphics carrier discovery.");
Expect(memory_reader.GetScanCount("graphicsObserverMsaa") == 0,
    "A nonleader graphics observer performed a duplicate broad scan.");
Expect(memory_reader.GetScanCount("graphicsObserverAmbientOcclusion") == 0,
    "The last quality observer performed a duplicate broad scan.");
```

Drive representative read, write, acknowledgement, invalidation, and rearm values for all seven new mini-ranges. Assert each update targets the correct config key and carries command ID `syncBatmanGraphicsDetailLevel`.

- [ ] **Step 2: Add complete launcher-load assertions**

Expand the conflicting-INI fixture assertions to cover all seven leaves explicitly:

```cpp
Expect(batman_dispatcher.TryGetInt("bloom") == 1, "Launcher Bloom was not loaded.");
Expect(batman_dispatcher.TryGetInt("dynamicShadows") == 1, "Launcher Dynamic Shadows were not loaded.");
Expect(batman_dispatcher.TryGetInt("motionBlur") == 1, "Launcher Motion Blur was not loaded.");
Expect(batman_dispatcher.TryGetInt("distortion") == 1, "Launcher Distortion was not loaded.");
Expect(batman_dispatcher.TryGetInt("fogVolumes") == 1, "Launcher Fog Volumes were not loaded.");
Expect(batman_dispatcher.TryGetInt("sphericalHarmonicLighting") == 1, "Launcher spherical lighting was not inverted correctly.");
Expect(batman_dispatcher.TryGetInt("ambientOcclusion") == 1, "Launcher Ambient Occlusion was not loaded.");
```

- [ ] **Step 3: Add canonical and Custom derivation cases**

Use literal leaf tables for Low, Medium, High, Very High, and at least two noncanonical combinations. Run `sync-batman-graphics-detail-level` and assert exact `detailLevel` values `0,1,2,3,4,4`. Separately select every preset through `sync-batman-graphics-detail-preset` and assert all seven literal leaves.

- [ ] **Step 4: Assert both INIs after a custom apply**

Set a noncanonical combination, run apply, and assert all seven values plus the normalized `DetailMode` in both files. The spherical-lighting assertion must be inverted:

```cpp
Expect(saved_user_ini_text.find("DisableSphericalHarmonicLights=True") != std::string::npos,
    "Custom apply did not invert spherical harmonic lighting in UserEngine.ini.");
Expect(saved_engine_ini_text.find("DisableSphericalHarmonicLights=True") != std::string::npos,
    "Custom apply did not invert spherical harmonic lighting in BmEngine.ini.");
```

- [ ] **Step 5: Run the full native suite twice**

Use the canonical MSBuild/test command from Task 1. Expected both runs: `PASS`.

- [ ] **Step 6: Review and commit Task 4**

```powershell
rtk git diff --check
rtk git add -- tests/HelenRuntime.Tests/MemoryStateObserverServiceTests.cpp tests/HelenRuntime.Tests/CommandExecutorTests.cpp
rtk git commit -m "Cover Batman detail quality runtime flow"
```

---

### Task 5: Final verification, atomic installation, and live checkpoints

**Files:**
- Verify only: repository and installed game trees
- Do not modify source unless a checkpoint exposes a reproducible defect; any defect starts a new failing-test cycle and focused commit.

**Interfaces:**
- Consumes: the committed Release runtime and exact seven-file package.
- Produces: hash-verified installation and recorded live evidence for initialization, settings, presets, Apply, and relaunch persistence.

- [ ] **Step 1: Run fresh full native and Release builds**

```powershell
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /t:Build /p:Configuration=Debug /p:Platform=Win32 /m:1 /nodeReuse:false /v:minimal; if (`$LASTEXITCODE -ne 0) { exit `$LASTEXITCODE }; & '.\bin\Win32\Debug\tests\HelenRuntimeTests.exe'; exit `$LASTEXITCODE"
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' 'HelenGameHook.sln' /t:HelenGameHook /p:Configuration=Release /p:Platform=Win32 /m:1 /nodeReuse:false /v:minimal; exit `$LASTEXITCODE"
```

Expected: native suite `PASS`; Release build exits zero. Existing unrelated compiler warnings must be reported honestly and not attributed to this feature.

- [ ] **Step 2: Re-run Task 3 validators from the committed tree**

Run all five Task 3 scripts again. Expected: all `PASS` with the same shell/target/delta hashes recorded before commit.

- [ ] **Step 3: Confirm Batman is closed and deploy atomically**

```powershell
$process = Get-Process ShippingPC-BmGame -ErrorAction SilentlyContinue
if ($null -ne $process) { throw 'Batman must be closed before deployment.' }
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Deploy-BatmanGraphicsOptionsExperiment.ps1 -GameBin 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries' -Configuration Release
```

Expected: validators `PASS`, final `DEPLOYED`, no staging/recovery survivors.

- [ ] **Step 4: Hash-check the installed publication**

Compare repository and live SHA-256 values for `HelenGameHook.dll`, `dinput8.dll`, `helengamehook/config/packs.json`, and every relative file in `batman-aa-graphics-options`. Require source/live pack counts of exactly seven and exact ordered relative-path/hash equality.

- [ ] **Step 5: Live checkpoint A — first opening**

Ask the user to launch and open Graphics Options once. Record all visible values for VSync, MSAA, Detail Level, seven quality rows, PhysX, and Stereo. Reject any Undefined, Loading timeout, or Unavailable result.

Inspect `HelenGameHook.log`. Require one initial broad-scan log for the unresolved graphics address group, responses for all eleven transmitted settings, and no response/ack/failure errors.

- [ ] **Step 6: Live checkpoint B — one-leaf transaction**

Ask the user to change one quality leaf. Confirm Detail Level immediately becomes the literal expected preset or Custom. Apply and inspect:

- Exact write request and acknowledgement for that leaf.
- Final Apply acknowledgement.
- Both INIs contain the same intended values.
- `UserEngine.ini` retains its UTF-16LE BOM and unrelated sampled values.

- [ ] **Step 7: Live checkpoint C — preset matrix**

Ask the user to select Low, Medium, High, and Very High one at a time. Before each Apply, verify the seven displayed leaves match the literal matrix in the spec. Apply at least the final preset and inspect ordered leaf acknowledgements followed by commit acknowledgement.

- [ ] **Step 8: Live checkpoint D — relaunch persistence**

Ask the user to exit completely, relaunch, and reopen Graphics Options. Require the last applied seven leaves and derived Detail Level to appear immediately with no intermittent timeout.

- [ ] **Step 9: Final repository audit**

```powershell
rtk git diff --check
rtk git status --short
rtk git log --oneline -12
```

Expected: only the user's pre-existing project/test project edits and `batma/` remain outside the feature commits. Report all feature commit hashes, verification results, artifact hashes, deployment hashes, and live outcomes.
