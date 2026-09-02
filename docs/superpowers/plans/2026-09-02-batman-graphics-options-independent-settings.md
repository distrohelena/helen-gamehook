# Batman Graphics Options Independent Settings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the verified Batman Arkham Asylum Graphics Options shell so VSync, MSAA, PhysX, and NVIDIA Stereo 3D load live values, edit locally, apply through acknowledged serialized carrier transactions, persist to both Batman INIs, and survive relaunch.

**Architecture:** Generalize the existing VSync-only `FE_ControlType` seam into a declarative ActionScript settings controller. HelenRuntime will share one structurally validated carrier address across all graphics observers and will acknowledge each write only after the config mutation and optional command succeed. The checked-in pack remains generated from current source and the verified retail base; inactive graphics rows remain presentation-only.

**Tech Stack:** C++17/Win32, HelenRuntime JSON pack model, native HelenRuntime test executable, C#/FFDec ActionScript patch builder, PowerShell package/build validators, Batman Arkham Asylum GOTY Scaleform frontend.

---

## Working Rules and File Map

Execute this plan directly on `main`, matching the user's established workflow. Preserve all unrelated changes, especially:

- `HelenRuntime/HelenRuntime.vcxproj`
- `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`
- `tests/HelenRuntime.Tests/TestMain.cpp`
- untracked `batma/`

Do not add new C++ compilation units: grouped-address state belongs as explicitly named fields and private methods on the existing observer service. This avoids touching the dirty project files while keeping ownership inside the service that uses it.

Primary responsibilities:

- `include/HelenHook/MemoryStateObserverDefinition.h`: declarative address-group and transactional acknowledgement fields.
- `include/HelenHook/MemoryStateObserverService.h`: boolean update callback, grouped-address cache, and cache helper declarations.
- `include/HelenHook/BuildRuntimeCoordinator.h`: observer update success contract.
- `HelenRuntime/PackRepository.cpp`: parse and validate the new observer schema.
- `HelenRuntime/MemoryStateObserverService.cpp`: grouped address reuse, read responses, transactional success/failure acknowledgement, and retry rearming.
- `HelenRuntime/BuildRuntimeCoordinator.cpp`: return the combined config-write and optional-command result.
- `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`: schema and checked-in Batman protocol tests.
- `tests/HelenRuntime.Tests/MemoryStateObserverServiceTests.cpp`: address reuse, acknowledgement, failure, retry, and legacy behavior tests.
- `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellScriptTemplates.cs`: declarative four-setting ActionScript controller.
- `games/HelenBatmanAA/scripts/Rebuild-BatmanGraphicsOptionsExperiment.ps1`: authoritative protocol/config/observer generation.
- `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/pack.json`: generated pack identity and config schema.
- `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/hooks.json`: generated observer protocol.
- `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/commands.json`: generated load/apply commands.
- `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/build.json`: generated startup-command declaration.
- `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/files.json`: generated retail delta installation declaration.
- `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/bindings.json`: generated empty bindings declaration.
- `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/assets/deltas/Frontend-graphics-options.hgdelta`: generated retail frontend delta.
- `games/HelenBatmanAA/builder/generated/graphics-options-experiment/Frontend-graphics-options.umap`: ignored rebuild output used for standalone validation; verify it but never stage it.
- `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellContract.ps1`: controller state-machine and forbidden-bridge contracts.
- `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1`: exact pack protocol and artifact provenance.
- `games/HelenBatmanAA/scripts/Test-BatmanRetailGraphicsOptionsPatch.ps1`: retail patch/open/export round trip.
- `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellDeployment.ps1`: atomic deployment and subtitle-pack isolation.
- `games/HelenBatmanAA/scripts/Deploy-BatmanGraphicsOptionsExperiment.ps1`: only if its embedded verification assumptions must be expanded for the six observers; do not change deployment semantics.

## Task 1: Add and Validate the Observer Protocol Schema

**Files:**

- Modify: `include/HelenHook/MemoryStateObserverDefinition.h`
- Modify: `HelenRuntime/PackRepository.cpp`
- Test: `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`

- [ ] Add failing parser tests for a valid grouped transactional observer.

Create a JSON fixture containing these exact fields:

```json
{
  "id": "graphicsObserverMsaa",
  "addressGroup": "batmanFrontendControlType",
  "scanStartAddress": "0x10000000",
  "scanEndAddress": "0x30000000",
  "scanStride": 4,
  "valueOffset": 12,
  "pollIntervalMs": 50,
  "targetConfigKey": "msaa",
  "addressMatchValues": [4300, 4310, 4311, 4312, 4313, 4314, 4320, 4321, 4322, 4323, 4324, 4330, 4331, 4332, 4333, 4334, 4399],
  "checks": [{"comparison": "equals-constant", "offset": 0, "expectedValue": 4102}],
  "mappings": [
    {"match": 4320, "value": 0},
    {"match": 4321, "value": 1},
    {"match": 4322, "value": 2},
    {"match": 4323, "value": 3},
    {"match": 4324, "value": 5}
  ],
  "responseRequestValue": 4300,
  "responseMappings": [
    {"match": 0, "value": 4310},
    {"match": 1, "value": 4311},
    {"match": 2, "value": 4312},
    {"match": 3, "value": 4313},
    {"match": 5, "value": 4314}
  ],
  "acknowledgementMappings": [
    {"match": 4320, "value": 4330},
    {"match": 4321, "value": 4331},
    {"match": 4322, "value": 4332},
    {"match": 4323, "value": 4333},
    {"match": 4324, "value": 4334}
  ],
  "failureResponseValue": 4399
}
```

Assert the parsed `AddressGroup`, five acknowledgement mappings, and failure response exactly.

- [ ] Add failing rejection tests covering each malformed contract.

Reject:

- an empty `addressGroup`;
- `acknowledgementMappings` without `failureResponseValue`;
- `failureResponseValue` without acknowledgement mappings;
- an acknowledgement input that is absent from `mappings`;
- an acknowledgement output absent from `addressMatchValues`;
- a failure response absent from `addressMatchValues`;
- duplicate acknowledgement inputs;
- an acknowledgement mapping whose input maps to no config value;
- response mappings without `responseRequestValue`, preserving the current rule.

- [ ] Run the parser tests and confirm they fail for missing fields.

```powershell
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' '.\tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /t:Build /p:Configuration=Debug /p:Platform=Win32 /m:1 /v:minimal; if (`$LASTEXITCODE -eq 0) { & '.\bin\Win32\Debug\tests\HelenRuntimeTests.exe'; exit `$LASTEXITCODE } else { exit `$LASTEXITCODE }"
```

Expected: build succeeds, then at least one new parser assertion fails because the schema has not been implemented.

- [ ] Add the definition fields with substantive Doxygen comments immediately before the existing address matching and response fields.

```cpp
/** @brief Optional identifier that lets compatible observers reuse one structurally validated carrier base address. */
std::optional<std::string> AddressGroup;
/** @brief Raw request-to-success-response mappings written only after a mapped update completes successfully. */
std::vector<MemoryStateObserverMapEntryDefinition> AcknowledgementMappings;
/** @brief Optional raw response written when a transactional config update or follow-up command fails. */
std::optional<int> FailureResponseValue;
```

- [ ] Extend `ParseStateObserver` to parse and validate the contract before returning.

Use `TryGetString` for `addressGroup`, `TryGetInt` for `failureResponseValue`, and `ParseStateObserverMapping` for each acknowledgement mapping. Validate with explicit loops and `std::set<int>` so duplicate inputs cannot pass. A transaction is valid only when acknowledgement mappings and failure response are both present, every acknowledgement `Match` appears in `definition.Mappings`, and every acknowledgement output plus the failure value appears in `AddressMatchValues`.

- [ ] Run the complete native test executable.

Use the command above. Expected: `HelenRuntime tests passed.` and exit code `0`.

- [ ] Commit the schema checkpoint without staging unrelated files.

```powershell
git add -- include/HelenHook/MemoryStateObserverDefinition.h HelenRuntime/PackRepository.cpp tests/HelenRuntime.Tests/PackRepositoryTests.cpp
git commit -m "Add transactional observer protocol schema"
```

## Task 2: Share and Invalidate Carrier Addresses

**Files:**

- Modify: `include/HelenHook/MemoryStateObserverService.h`
- Modify: `HelenRuntime/MemoryStateObserverService.cpp`
- Test: `tests/HelenRuntime.Tests/MemoryStateObserverServiceTests.cpp`

- [ ] Add a failing test proving two observers in the same group use one resolved address.

Allocate one verified carrier block, create VSync and MSAA definitions with `AddressGroup = "batmanFrontendControlType"`, and poll VSync first. Then place an MSAA request in that same block and poll all observers. Assert both debug views report the same `CachedAddress`, and the second observer's `RescanCount` stays `0` because it reused the group cache.

- [ ] Add a failing stale-group test.

Resolve both observers to carrier A, invalidate one structural check in A, create carrier B with the same valid signature and a recognized raw value, then poll. Assert the group cache is cleared, a rescan resolves B, and all group members eventually report B rather than retaining A.

- [ ] Run the native tests and confirm the new grouped-address assertions fail.

Expected: existing tests remain green; new tests fail because observers still cache independently.

- [ ] Add the grouped cache and private helpers to `MemoryStateObserverService`.

Add `#include <unordered_map>` and this field:

```cpp
/** @brief Structurally validated carrier base addresses keyed by declarative address-group identifier. */
std::unordered_map<std::string, std::uintptr_t> grouped_addresses_;
```

Add private methods with full Doxygen comments:

```cpp
std::uintptr_t GetCachedAddress(std::size_t observer_index) const;
void CacheResolvedAddress(std::size_t observer_index, std::uintptr_t address);
void ClearCachedAddress(std::size_t observer_index);
```

Their behavior must be:

- ungrouped observer: read/write only its debug view cache;
- grouped observer: read/write `grouped_addresses_` and mirror the chosen address into every matching debug view;
- stale grouped observer: erase the group entry and clear every matching debug view before rescanning.

- [ ] Refactor `PollObserver` to use the helpers around the existing structural validation and scan code.

Do not weaken `MatchesObserverChecks`, `IsAddressMatchValue`, range validation, or exception-safe reads. A reused group address must pass the current observer's own checks and current raw-value union before it is accepted.

- [ ] Run the full native test executable.

Expected: `HelenRuntime tests passed.`; the shared-address test reports one group address and no second full scan.

- [ ] Commit the grouped-address checkpoint.

```powershell
git add -- include/HelenHook/MemoryStateObserverService.h HelenRuntime/MemoryStateObserverService.cpp tests/HelenRuntime.Tests/MemoryStateObserverServiceTests.cpp
git commit -m "Share graphics observer carrier addresses"
```

## Task 3: Acknowledge Transactional Observer Updates

**Files:**

- Modify: `include/HelenHook/MemoryStateObserverService.h`
- Modify: `include/HelenHook/BuildRuntimeCoordinator.h`
- Modify: `HelenRuntime/MemoryStateObserverService.cpp`
- Modify: `HelenRuntime/BuildRuntimeCoordinator.cpp`
- Test: `tests/HelenRuntime.Tests/MemoryStateObserverServiceTests.cpp`
- Test: `tests/HelenRuntime.Tests/BuildRuntimeCoordinatorTests.cpp` if present; otherwise extend the existing coordinator assertions in their current test file.

- [ ] Add failing service tests for success, failure, retry, and legacy observers.

Cover these exact cases:

1. Raw `4324` maps to config value `5`; callback returns `true`; carrier becomes `4334` only after the callback runs.
2. Raw `4324`; callback returns `false`; carrier becomes `4399`.
3. After either response, writing `4324` again emits a second update and a second acknowledgement.
4. A read request `4300` returns `4314` for config value `5` and emits no update.
5. A legacy subtitle observer without acknowledgement fields retains change-suppression behavior and never writes a transaction response.
6. An acknowledgement memory write failure makes `PollOnce()` return `false` instead of claiming success.

- [ ] Add failing coordinator tests that distinguish config and command failure.

Assert `HandleObserverUpdate` returns:

- `false` when `TrySetInt` fails and does not run the command;
- `false` when config succeeds but the optional command fails;
- `true` when config and command both succeed;
- `true` for a successful config-only update.

- [ ] Change the service update callback contract.

```cpp
/**
 * @brief Applies a mapped observer update and reports whether every required native operation succeeded.
 */
using UpdateCallback = std::function<bool(const MemoryStateObserverUpdate&)>;
```

Change the coordinator private method to:

```cpp
/**
 * @brief Applies one mapped observer update through the generic config and command surfaces.
 * @param update Observer update emitted by the live-state scanner.
 * @return True when the config mutation and optional command both succeed; otherwise false.
 */
bool HandleObserverUpdate(const MemoryStateObserverUpdate& update);
```

The constructor callback must `return HandleObserverUpdate(update);`.

- [ ] Make `BuildRuntimeCoordinator::HandleObserverUpdate` return the exact combined result.

Do not run the optional command after a failed config write. Log both result fields as today, then return `set_succeeded && command_succeeded`.

- [ ] Add a response-mapping helper for transactional acknowledgements.

Use a clearly named namespace function:

```cpp
std::optional<int> TryMapAcknowledgementValue(
    const helen::MemoryStateObserverDefinition& definition,
    int raw_request_value);
```

- [ ] Refactor transaction processing so the callback precedes the acknowledgement write.

For a mapped request:

1. capture the update under the mutex;
2. release the mutex;
3. call `update_callback_` and retain its boolean result;
4. select the exact success acknowledgement or failure response;
5. write it to `resolved_address + ValueOffset`;
6. update `LastRawValue` to the response and increment `UpdateCount` only for the emitted request;
7. log `[observer] acknowledgement ... result=... response=...`.

For transactional observers, suppress only an unchanged raw request that is still present in memory. Because HelenRuntime replaces every processed request with an acknowledgement/failure code, writing the same request later must be recognized as a new transition. Do not use `LastMappedValue` as the sole transactional deduplication key.

- [ ] Run the full native tests twice.

Expected both times: `HelenRuntime tests passed.` The second run catches state leakage between tests.

- [ ] Commit the acknowledgement checkpoint.

```powershell
git add -- include/HelenHook/MemoryStateObserverService.h include/HelenHook/BuildRuntimeCoordinator.h HelenRuntime/MemoryStateObserverService.cpp HelenRuntime/BuildRuntimeCoordinator.cpp tests/HelenRuntime.Tests/MemoryStateObserverServiceTests.cpp tests/HelenRuntime.Tests/BuildRuntimeCoordinatorTests.cpp
git commit -m "Acknowledge graphics observer transactions"
```

If `BuildRuntimeCoordinatorTests.cpp` does not exist, remove only that nonexistent path from `git add`; do not stage `TestMain.cpp`.

## Task 4: Generate the Exact Group 1 Pack Protocol

**Files:**

- Modify: `games/HelenBatmanAA/scripts/Rebuild-BatmanGraphicsOptionsExperiment.ps1`
- Modify: generated files under `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/`
- Rebuild/verify: `games/HelenBatmanAA/builder/generated/graphics-options-experiment/Frontend-graphics-options.umap` (ignored output)
- Test: `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`
- Test: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1`

- [ ] Extend checked-in pack tests first and verify they fail against the VSync-only pack.

Require exactly six observers, in this order:

1. `graphicsObserverVsync`
2. `graphicsObserverMsaa`
3. `graphicsObserverPhysx`
4. `graphicsObserverStereo`
5. `graphicsObserverApplySignal`
6. `graphicsObserverRollbackSignal`

Require `AddressGroup == "batmanFrontendControlType"` for all six and require the complete raw-value union in every `AddressMatchValues` list.

- [ ] Define the protocol in one authoritative PowerShell data table.

Use these exact values:

```powershell
$graphicsProtocol = @(
    [ordered]@{ Id='graphicsObserverVsync'; Key='vsync'; Read=4200; Responses=@(4210,4211); Writes=@(4220,4221); Acks=@(4230,4231); Failure=4299; ConfigValues=@(0,1) },
    [ordered]@{ Id='graphicsObserverMsaa'; Key='msaa'; Read=4300; Responses=@(4310,4311,4312,4313,4314); Writes=@(4320,4321,4322,4323,4324); Acks=@(4330,4331,4332,4333,4334); Failure=4399; ConfigValues=@(0,1,2,3,5) },
    [ordered]@{ Id='graphicsObserverPhysx'; Key='physx'; Read=4400; Responses=@(4410,4411,4412); Writes=@(4420,4421,4422); Acks=@(4430,4431,4432); Failure=4499; ConfigValues=@(0,1,2) },
    [ordered]@{ Id='graphicsObserverStereo'; Key='stereo'; Read=4500; Responses=@(4510,4511); Writes=@(4520,4521); Acks=@(4530,4531); Failure=4599; ConfigValues=@(0,1) }
)
```

Build the complete address union from the four setting entries plus:

```powershell
@(4970,4971,4960,4961,4969,4990,4991,4980,4981,4989)
```

Sort and de-duplicate once, then pass the same integer array to every observer. Do not retain `4210/4211` as VSync write requests; VSync writes are `4220/4221`.

- [ ] Extend `New-GraphicsCarrierObserver` parameters and emitted JSON.

Add mandatory `AddressMatchValues`, optional `AddressGroup`, `AcknowledgementMappings`, and `FailureResponseValue` parameters. Enforce acknowledgement/failure pairing in PowerShell before writing JSON. Every generated graphics observer must emit:

```powershell
addressGroup = 'batmanFrontendControlType'
```

- [ ] Add `rollbackSignal` to the config keys and generate the command observers.

The config list must contain the existing 16 keys plus `rollbackSignal`, for 17 total.

Apply observer:

- mappings `4990 -> 0`, `4991 -> 1`;
- acknowledgements `4990 -> 4980`, `4991 -> 4981`;
- failure `4989`;
- command `applyBatmanGraphicsDraft`.

Rollback observer:

- mappings `4970 -> 0`, `4971 -> 1`;
- acknowledgements `4970 -> 4960`, `4971 -> 4961`;
- failure `4969`;
- command `loadBatmanGraphicsDraftIntoConfig`.

- [ ] Rename the pack presentation from VSync-only to Group 1 graphics options.

Set the pack name to `Batman Graphics Options` while retaining pack ID `batman-aa-graphics-options`.

- [ ] Rebuild the package from current source.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Rebuild-BatmanGraphicsOptionsExperiment.ps1 -Configuration Release
```

Expected: staged validation and atomic publication succeed; checked-in `hooks.json`, `pack.json`, and related generated artifacts change. No file from `batma/` is consumed.

- [ ] Run native and package tests.

```powershell
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' '.\tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /t:Build /p:Configuration=Debug /p:Platform=Win32 /m:1 /v:minimal; if (`$LASTEXITCODE -eq 0) { & '.\bin\Win32\Debug\tests\HelenRuntimeTests.exe'; exit `$LASTEXITCODE } else { exit `$LASTEXITCODE }"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsPackage.ps1 -Configuration Release
```

Expected: native tests pass and package validator reports a valid six-observer package.

- [ ] Commit authoritative protocol generation and generated metadata.

Inspect `git status --short`, then stage the rebuild script, exact generated pack files, and test files only.

```powershell
git add -- games/HelenBatmanAA/scripts/Rebuild-BatmanGraphicsOptionsExperiment.ps1 games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1 tests/HelenRuntime.Tests/PackRepositoryTests.cpp games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options
git commit -m "Generate Batman independent graphics protocol"
```

## Task 5: Replace the VSync Shell with a Declarative Four-Setting Controller

**Files:**

- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellScriptTemplates.cs`
- Test: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellContract.ps1`

- [ ] Expand shell contract tests before editing the template.

Require these literal value lists and protocol constants in the generated ActionScript:

```actionscript
["Off","On"]
["Off","2x","4x","8x","16x"]
["Off","Normal","High"]
[0,1,2,3,5]
4200, 4300, 4400, 4500
4220, 4320, 4420, 4520
4230, 4330, 4430, 4530
4299, 4399, 4499, 4599
4970, 4960, 4969
4990, 4980, 4989
```

Require active rows 3, 4, 13, and 14. Require other setting rows to remain `Not active` with no mutation callback. Prohibit `Helen_GetInt`, `Helen_SetInt`, `Helen_RunCommand`, historical prompt symbols, and a direct callback from the Apply row.

Add static state-machine assertions that:

- initialization advances one setting only after an expected read response;
- one overall initialization deadline is ten seconds;
- edits change only draft indices;
- dirty writes are enqueued in row order;
- each apply step waits for exact acknowledgement with a two-second timeout;
- commit follows setting acknowledgements;
- baseline indices update only after commit acknowledgement;
- failure enters rollback;
- rollback success shows `Apply Failed` and preserves the UI draft;
- rollback failure shows `Rollback Failed` and disables Apply.

- [ ] Run the shell contract and confirm it fails on the VSync-only controller.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellContract.ps1
```

- [ ] Introduce one declarative setting object shape inside the ActionScript template.

Each setting entry must include:

```actionscript
{
   RowIndex: 4,
   Name: "MSAA",
   Values: ["Off","2x","4x","8x","16x"],
   ConfigValues: [0,1,2,3,5],
   ReadRequest: 4300,
   ReadResponseBase: 4310,
   WriteRequestBase: 4320,
   WriteAcknowledgementBase: 4330,
   FailureResponse: 4399,
   InitialIndex: -1,
   DraftIndex: -1
}
```

Use analogous entries for VSync row 3, PhysX row 13, and Stereo row 14. Do not create local helper functions inside methods in the C# builder; keep template helper methods as named class members where needed.

- [ ] Implement serialized initialization using the existing frontend calls.

Only these bridges may carry state:

```actionscript
flash.external.ExternalInterface.call("FE_SetControlType",rawValue,"");
int(flash.external.ExternalInterface.call("FE_GetControlType"));
```

Set all active rows to `Loading...`, send one read request, poll until the exact response range arrives, then set both `InitialIndex` and `DraftIndex`. Start one ten-second deadline before the first request; do not reset it between settings. On timeout, mark all four active rows `Unavailable`, disable Apply, restore Back/navigation input, and never invent an index.

- [ ] Implement local editing and dirty-state presentation.

Left/right respect boundaries. Normal row action wraps. Accepted changes update only `DraftIndex`, refresh arrows/value text, play the existing stock sound, and recompute Apply enabled state by comparing every active setting's `DraftIndex` to `InitialIndex`.

- [ ] Implement the acknowledged Apply queue.

Build the queue from dirty settings in ascending `RowIndex`. For each entry, send `WriteRequestBase + DraftIndex`, wait up to two seconds for `WriteAcknowledgementBase + DraftIndex`, then advance. A setting failure response or timeout must stop normal application immediately.

After all dirty writes acknowledge, alternate the apply signal between `4990` and `4991`, wait for matching `4980` or `4981`, and only then copy all draft indices to initial indices and clear dirty state.

- [ ] Implement rollback and visible failure states.

On a write or commit failure, alternate `4970/4971` and wait for `4960/4961`. Preserve the ActionScript draft indices. After rollback success show `Apply Failed`, re-enable editing, and keep Apply enabled. After `4969` or rollback timeout show `Rollback Failed`, keep Apply disabled for that screen, and leave Back available.

- [ ] Remove VSync-specific controller names and stale protocol writes.

Search for and eliminate `GraphicsVsyncController`, direct `4210 + DraftVsync` writes, VSync-only timers, and any assumption that Apply has one setting.

- [ ] Run shell contract tests.

Expected: `Batman graphics-options shell contract validation passed.`

- [ ] Commit the controller checkpoint.

```powershell
git add -- games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellScriptTemplates.cs games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellContract.ps1
git commit -m "Activate independent Batman graphics settings"
```

## Task 6: Rebuild and Prove Artifact Provenance

**Files:**

- Modify: generated files under `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/`
- Rebuild/verify: `games/HelenBatmanAA/builder/generated/graphics-options-experiment/Frontend-graphics-options.umap` (ignored output)
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanRetailGraphicsOptionsPatch.ps1`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellDeployment.ps1`
- Modify only if required: `games/HelenBatmanAA/scripts/Deploy-BatmanGraphicsOptionsExperiment.ps1`

- [ ] Add or strengthen reproducibility tests before rebuilding.

Build twice from temporary Batman user INIs whose Group 1 values are deliberately opposite:

- INI A: VSync off, MSAA off, PhysX off, Stereo off.
- INI B: VSync on, MSAA 16x/config `5`, PhysX high/config `2`, Stereo on.

Assert the generated GFX SHA-256 and final package delta SHA-256 are identical. This proves no live display value is baked at build time.

- [ ] Require an exact generated source set.

The package validator must reject:

- historical full-controller exports;
- `Helen_*` calls;
- prompt exports;
- unexpected generated files;
- input from `F:\helenhook.7z`, `batma/`, or any previously installed live package;
- observer/config/command counts that differ from the current rebuild declaration.

- [ ] Rebuild from the verified retail base.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Rebuild-BatmanGraphicsOptionsExperiment.ps1 -Configuration Release
```

- [ ] Run all static/package/retail/deployment validators.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellContract.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsPackage.ps1 -Configuration Release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanRetailGraphicsOptionsPatch.ps1 -Configuration Release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellDeployment.ps1
```

Expected: all four scripts exit `0`; retail target reopens and exports successfully; deployment test confirms atomic replacement and unchanged subtitle pack.

- [ ] Inspect generated diffs and commit only current-source artifacts and validators.

```powershell
git diff --check
git status --short
git diff --stat
```

Stage exact changed files only, then:

```powershell
git commit -m "Validate Batman graphics package provenance"
```

## Task 7: Build, Install, and Perform Automated End-to-End Verification

**Files:**

- Verify: all files changed by Tasks 1-6
- Install target: `C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries`

- [ ] Run the complete native test suite twice.

```powershell
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' '.\tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /t:Build /p:Configuration=Debug /p:Platform=Win32 /m:1 /v:minimal; if (`$LASTEXITCODE -eq 0) { & '.\bin\Win32\Debug\tests\HelenRuntimeTests.exe'; exit `$LASTEXITCODE } else { exit `$LASTEXITCODE }"
rtk proxy powershell.exe -NoProfile -Command "& '.\bin\Win32\Debug\tests\HelenRuntimeTests.exe'; exit `$LASTEXITCODE"
```

- [ ] Build the Release hook without node reuse or GUI error reporting.

```powershell
rtk proxy powershell.exe -NoProfile -Command "`$env:MSBUILDDISABLENODEREUSE='1'; & 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' '.\HelenGameHook.sln' /t:HelenGameHook /p:Configuration=Release /p:Platform=Win32 /m:1 /nodeReuse:false /v:minimal; exit `$LASTEXITCODE"
```

Expected: exit `0`, no MessageBoxes, and a fresh Release hook timestamp.

- [ ] Re-run all four Batman validators from Task 6.

Expected: all exit `0` on the same source revision and Release binary.

- [ ] Install only the graphics-options package and current hook.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Deploy-BatmanGraphicsOptionsExperiment.ps1 -GameBin 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries' -Configuration Release
```

This Program Files write requires the normal escalation approval if the current session does not already have it. Do not install the subtitle package alongside it for this test.

- [ ] Verify installed bytes match repository Release/package bytes.

Use `Get-FileHash -Algorithm SHA256` on the installed hook, installed pack metadata, installed delta, and their source counterparts. Expected: every corresponding hash matches exactly. Also confirm no obsolete graphics observer or old generated GFX remains in the installed pack directory.

## Task 8: Live Batman Verification

**Files:**

- Read/compare: Batman `UserEngine.ini`
- Read/compare: Batman generated `BmEngine.ini`
- Read: HelenHook runtime log

- [ ] Record the pre-test state without changing it.

Capture file hashes, encodings, and the exact Group 1 keys from both INIs. Sample unrelated keys from each file so later comparisons detect collateral rewrites. Record the current log length or timestamp before launch.

- [ ] Ask the user to launch Batman and navigate Title -> Main Menu -> Options -> Graphics Options.

Confirm:

- no title/menu stall;
- VSync, MSAA, PhysX, and Stereo show the persisted values;
- inactive rows show `Not active`;
- Apply begins dim/disabled;
- Back works.

- [ ] Verify local edits do not write either INI.

Change one active row but do not Apply. Confirm Apply enables and hashes of both INIs remain unchanged.

- [ ] Verify each setting independently.

For VSync, MSAA, PhysX, and Stereo in turn:

1. choose a different honest value;
2. press Apply;
3. confirm interaction is blocked while `Applying...` is visible;
4. inspect new log lines for the exact write request, success acknowledgement, apply request, and apply acknowledgement;
5. confirm both INIs contain the normalized value;
6. relaunch and confirm the row initializes to the same display label.

For MSAA, explicitly exercise `Off`, `2x`, `4x`, `8x`, and `16x` across the test sequence and verify config values `0`, `1`, `2`, `3`, and `5` retain identity.

- [ ] Verify multi-setting serialization and repeated values.

Change all four rows before one Apply. Confirm log order is VSync, MSAA, PhysX, Stereo for dirty rows only, followed by apply. Then move one value away and back in a later transaction and confirm the same logical write code is observed and acknowledged again.

- [ ] Verify unapplied Back behavior.

Make a local edit, use Back, reopen Graphics Options, and confirm initialization returns to the persisted value rather than the abandoned draft.

- [ ] Exercise failure and rollback with an isolated temporary test declaration.

Create no production fallback. In a temporary pack copy used only by the automated/live test, point one observer command to a deliberately failing declarative command while retaining the same carrier protocol. Deploy that isolated test pack atomically, attempt Apply, and confirm:

- setting failure does not advance to commit;
- rollback request is logged;
- rollback acknowledgement produces `Apply Failed`;
- UI draft remains dirty and retryable;
- INIs remain at the pre-transaction values.

Then restore and hash-verify the production Group 1 pack. Separately exercise a rollback failure in the native service/controller test harness; do not leave a deliberately broken live pack installed.

- [ ] Compare post-test collateral state.

Confirm only intended Group 1 keys changed. Preserve UTF-16LE for `UserEngine.ini`, confirm generated `BmEngine.ini` remains parseable, and ensure sampled unrelated keys are unchanged.

## Task 9: Final Verification and Completion Commit

**Files:**

- Verify: complete tracked diff since `9526cb9`
- Modify only if tests expose defects: files already owned by this plan

- [ ] Run the full verification matrix one final time on the exact installed commit candidate.

Run:

1. native tests twice;
2. shell contract;
3. package validation;
4. retail patch round trip;
5. deployment transaction test;
6. `git diff --check`;
7. generated artifact hash comparison;
8. installed-source hash comparison.

Every command must exit `0`. Do not claim completion from earlier output if any source or generated artifact changed afterward.

- [ ] Audit the final source and generated package.

```powershell
rg -n "Helen_(GetInt|SetInt|RunCommand)|GraphicsVsyncController|Prompt|TODO|TBD|FIXME|XXX" games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellScriptTemplates.cs games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options games/HelenBatmanAA/scripts
git status --short
git log --oneline -8
```

Expected: no forbidden bridge/controller/prompt hits in the graphics shell or pack; no task placeholders; only the known unrelated dirty files remain unstaged.

- [ ] Review type and ownership consistency.

Confirm:

- every new C++ class member and function has substantive Doxygen;
- no new local helper functions were placed inside methods;
- callback success is propagated, not inferred;
- grouped addresses are owned and synchronized by `MemoryStateObserverService`;
- no default value is substituted for failed live initialization;
- no old asset or installed package is an input to rebuilding;
- the complete protocol code union is identical for all six observers.

- [ ] Commit any final test-driven corrections using exact path staging.

If the working tree already contains all implementation commits and no tracked task changes remain, do not create an empty commit. Otherwise stage only plan-owned files and use:

```powershell
git commit -m "Finish Batman independent graphics settings"
```

- [ ] Report the final commit sequence, exact verification results, installed hash match, and the live values the user confirmed.

Do not report unrelated dirty project files as implementation changes, and do not remove or overwrite them.
