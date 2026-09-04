# Batman Graphics Fullscreen and Supported Resolution Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Activate Batman: Arkham Asylum GOTY's Fullscreen and Resolution rows, with Resolution populated only from modes supported by the display currently containing Batman's game window.

**Architecture:** Add a runtime-owned Win32 display-mode catalog and a bounded dynamic-scalar extension to the proven `FE_SetControlType` / `FE_GetControlType` observer carrier. Keep Fullscreen and the selected resolution index local in ActionScript until Apply; native code then resolves one finite index, revalidates the selected pair, atomically updates ResX/ResY in the dispatcher, and reuses the existing dual-INI transaction.

**Tech Stack:** C++20/Win32, ActionScript 2 emitted by C#/.NET 8, PowerShell package tooling, MSBuild Win32, FFDec, Unreal package delta tooling.

**Spec:** `docs/superpowers/specs/2026-09-04-batman-graphics-display-options-design.md`

## Global Constraints

- Work directly on `main`, as explicitly requested by the user.
- Preserve the user's existing changes in `HelenRuntime/HelenRuntime.vcxproj`, `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`, and `tests/HelenRuntime.Tests/TestMain.cpp`; patch only the entries needed by this feature and never replace those files wholesale.
- Do not inspect or use `batma/`, installed game files, `F:\helenhook.7z`, or historical generated artifacts as build inputs.
- Do not revive `GraphicsOptionsScriptTemplates.cs` or any `Helen_*` ActionScript callback.
- Keep edits local to the shell until Apply. Back must not send Fullscreen or Resolution writes.
- Resolution labels contain unique positive `WIDTH x HEIGHT` pairs only. Refresh-rate and pixel-format variants collapse to one pair.
- The catalog limit is 98 entries. Empty, oversized, ambiguous-window, unsupported-current, or stale-selected catalogs fail explicitly.
- Use scalar width and height responses; do not pack a pair into one integer.
- Keep all existing protocol codes through `4669` and `4960` through `4991` unchanged.
- Use `4670..4679` for Fullscreen, `4700..4899` for catalog/current-resolution reads, and `5000..5199` for resolution Apply requests and acknowledgements.
- Every added or changed C++/C# class, function, constructor, field, property, and enum member receives substantive Doxygen/XML documentation.
- Use same-line braces, one class per file, explicit types, no tuples, no runtime fallback defaults, and no GUI/MessageBox error paths.
- Use `apply_patch` for source edits and focused commits after each green task.
- Do not deploy while `ShippingPC-BmGame.exe` is running.

## File Structure

### New native files

- `include/HelenHook/BatmanDisplayMode.h` — one explicit immutable width/height value type.
- `include/HelenHook/BatmanDisplayModeService.h` — catalog ownership, normalization, scalar queries, exact pair lookup, and Apply-time revalidation.
- `HelenRuntime/BatmanDisplayModeService.cpp` — process-window discovery, monitor resolution, `EnumDisplaySettingsExW`, sorting, deduplication, and catalog state.
- `include/HelenHook/MemoryStateObserverDynamicResponseCallback.h` — the generic callback signature used by the observer service and coordinator.
- `tests/HelenRuntime.Tests/BatmanDisplayModeServiceTests.cpp` — deterministic service coverage through an injected enumeration callback.

### Existing native files

- `include/HelenHook/MemoryStateObserverDefinition.h` — declarative dynamic-provider ID, request values, and scalar bounds.
- `include/HelenHook/MemoryStateObserverService.h` and `HelenRuntime/MemoryStateObserverService.cpp` — bounded negative scalar responses and group-owned transient response correlation.
- `HelenRuntime/PackRepository.cpp` — strict parsing and validation of the new observer declaration.
- `include/HelenHook/BuildRuntimeCoordinator.h` and `HelenRuntime/BuildRuntimeCoordinator.cpp` — forwards dynamic queries without adding Batman behavior.
- `include/HelenHook/CommandDispatcher.h` and `HelenRuntime/CommandDispatcher.cpp` — atomic two-key integer mutation.
- `include/HelenHook/BatmanGraphicsConfigService.h` and `HelenRuntime/BatmanGraphicsConfigService.cpp` — selected-mode validation and atomic draft assignment.
- `include/HelenHook/CommandExecutor.h` and `HelenRuntime/CommandExecutor.cpp` — one `set-batman-graphics-resolution-mode` command step.
- `HelenGameHook/HelenGameHook.cpp` — display-service lifetime and dynamic-provider wiring.
- `HelenRuntime/HelenRuntime.vcxproj`, `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`, and `tests/HelenRuntime.Tests/TestMain.cpp` — append only the new compile/include/test registrations while preserving user edits.

### Shell, package, and generated files

- `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellScriptTemplates.cs` — Fullscreen record, resolution catalog state machine, local stepping, and selected-index Apply.
- `games/HelenBatmanAA/scripts/Rebuild-BatmanGraphicsOptionsExperiment.ps1` — exact protocol tables and generated observer/command/config manifests.
- `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellContract.ps1` — generated-script and Node state-machine behavior.
- `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1` — exact schema, command, observer, and seven-file package contract.
- `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsLayout.ps1` — all fourteen option rows active with preserved arrow geometry.
- `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellDeployment.ps1` and `games/HelenBatmanAA/scripts/Deploy-BatmanGraphicsOptionsExperiment.ps1` — strict staged/live protocol validation.
- `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`, `tests/HelenRuntime.Tests/MemoryStateObserverServiceTests.cpp`, `tests/HelenRuntime.Tests/BuildRuntimeCoordinatorTests.cpp`, `tests/HelenRuntime.Tests/CommandDispatcherTests.cpp`, and `tests/HelenRuntime.Tests/CommandExecutorTests.cpp` — native regressions.
- `games/HelenBatmanAA/builder/generated/graphics-options-experiment/Frontend-graphics-options.umap` — reproducible generated target.
- The checked-in pack remains exactly `pack.json`, `build.json`, `bindings.json`, `commands.json`, `files.json`, `hooks.json`, and `assets/deltas/Frontend-graphics-options.hgdelta`.

---

### Task 1: Add the Windows-supported display-mode catalog

**Files:**
- Create: `include/HelenHook/BatmanDisplayMode.h`
- Create: `include/HelenHook/BatmanDisplayModeService.h`
- Create: `HelenRuntime/BatmanDisplayModeService.cpp`
- Create: `tests/HelenRuntime.Tests/BatmanDisplayModeServiceTests.cpp`
- Modify: `HelenRuntime/HelenRuntime.vcxproj`
- Modify: `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`
- Modify: `tests/HelenRuntime.Tests/TestMain.cpp`

- [ ] **Step 1: Register a failing native test suite without disturbing existing project edits**

Append `BatmanDisplayModeServiceTests.cpp`, its runtime source/header entries, and `RunBatmanDisplayModeServiceTests()` beside the existing graphics tests. The new test must inject enumeration data and assert this exact normalized result:

```cpp
const std::vector<helen::BatmanDisplayMode> raw_modes = {
    helen::BatmanDisplayMode(1920, 1080),
    helen::BatmanDisplayMode(1280, 720),
    helen::BatmanDisplayMode(1920, 1080),
    helen::BatmanDisplayMode(3440, 1440)
};

Expect(service.Refresh(), "Supported display mode refresh failed.");
Expect(service.GetModeCount() == 3, "Display modes were not deduplicated.");
Expect(service.GetMode(0) == helen::BatmanDisplayMode(1280, 720), "Display modes were not sorted by pixel count.");
Expect(service.GetMode(2) == helen::BatmanDisplayMode(3440, 1440), "Ultrawide display mode was not retained.");
```

Also assert rejection of zero dimensions, an empty list, 99 unique pairs, an ambiguous process-window result, an exact pair not present in the catalog, and a selected pair removed by a second enumeration.

- [ ] **Step 2: Build and verify RED**

```powershell
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /t:Build /p:Configuration=Debug /p:Platform=Win32 /m:1 /nodeReuse:false /v:minimal; exit `$LASTEXITCODE"
```

Expected: compilation fails because `BatmanDisplayMode` and `BatmanDisplayModeService` do not exist.

- [ ] **Step 3: Implement the value and service interfaces**

Use this public shape, with full Doxygen on every member:

```cpp
class BatmanDisplayMode {
public:
    BatmanDisplayMode(int width, int height);
    int GetWidth() const noexcept;
    int GetHeight() const noexcept;
    bool operator==(const BatmanDisplayMode& other) const noexcept;

private:
    int width_;
    int height_;
};

class BatmanDisplayModeService {
public:
    using EnumerationCallback = std::function<bool(std::wstring&, std::vector<BatmanDisplayMode>&)>;
    static constexpr std::size_t MaximumModeCount = 98;

    BatmanDisplayModeService();
    explicit BatmanDisplayModeService(EnumerationCallback enumeration_callback);
    bool Refresh();
    std::size_t GetModeCount() const noexcept;
    const BatmanDisplayMode& GetMode(std::size_t index) const;
    std::optional<std::size_t> FindModeIndex(int width, int height) const noexcept;
    std::optional<int> QueryCatalogScalar(int raw_request_value) const;
    std::optional<BatmanDisplayMode> RevalidateMode(std::size_t index);

private:
    EnumerationCallback enumeration_callback_;
    std::wstring display_device_name_;
    std::vector<BatmanDisplayMode> modes_;
};
```

`Refresh()` calls the injected source, rejects invalid/empty/oversized results, sorts by `double(width) * double(height)`, then width and height, and removes exact duplicates. `QueryCatalogScalar(4700)` returns count; `4701 + 2*i` and `4702 + 2*i` return width and height. Requests outside the captured count return no value.

The default callback must use `EnumWindows` to collect visible, ownerless top-level windows owned by `GetCurrentProcessId()`. Require exactly one qualifying game window, resolve its monitor with `MonitorFromWindow(..., MONITOR_DEFAULTTONULL)`, obtain the device name through `GetMonitorInfoW`, and enumerate with `EnumDisplaySettingsExW`. Do not use `GetForegroundWindow` or primary-monitor fallback.

`RevalidateMode(index)` captures the selected pair, re-enumerates the same current game display, and returns that exact pair only if still present.

- [ ] **Step 4: Build and run GREEN**

```powershell
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /t:Build /p:Configuration=Debug /p:Platform=Win32 /m:1 /nodeReuse:false /v:minimal; if (`$LASTEXITCODE -ne 0) { exit `$LASTEXITCODE }; & '.\bin\Win32\Debug\tests\HelenRuntimeTests.exe'; exit `$LASTEXITCODE"
```

Expected: `PASS`, with no window creation and no MessageBox.

- [ ] **Step 5: Commit Task 1**

```powershell
rtk git diff --check
rtk git add -- include/HelenHook/BatmanDisplayMode.h include/HelenHook/BatmanDisplayModeService.h HelenRuntime/BatmanDisplayModeService.cpp tests/HelenRuntime.Tests/BatmanDisplayModeServiceTests.cpp
rtk git add -p -- HelenRuntime/HelenRuntime.vcxproj tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj tests/HelenRuntime.Tests/TestMain.cpp
rtk git diff --cached -- HelenRuntime/HelenRuntime.vcxproj tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj tests/HelenRuntime.Tests/TestMain.cpp
rtk git diff -- HelenRuntime/HelenRuntime.vcxproj tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj tests/HelenRuntime.Tests/TestMain.cpp
rtk git commit -m "Add Batman supported display mode catalog"
```

At each `git add -p` prompt, stage only the new display-mode compile/include/test-registration hunks. The cached diff must exclude every pre-existing user hunk, and the uncached diff must still contain those pre-existing changes after the commit.

---

### Task 2: Parse a bounded dynamic-scalar observer contract

**Files:**
- Create: `include/HelenHook/MemoryStateObserverDynamicResponseCallback.h`
- Modify: `include/HelenHook/MemoryStateObserverDefinition.h`
- Modify: `HelenRuntime/PackRepository.cpp`
- Test: `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`
- Modify: `HelenRuntime/HelenRuntime.vcxproj`

- [ ] **Step 1: Add failing strict parser cases**

Add a valid observer fixture containing:

```json
"dynamicResponse": {
  "provider": "batmanDisplayModes",
  "requests": [4700, 4701, 4702],
  "minimumValue": 1,
  "maximumValue": 32767
}
```

The dynamic response-only fixture omits `targetConfigKey`, `mappings`, `responseRequestValue`, `responseMappings`, and `acknowledgementMappings`. Assert exact parsed fields. Add rejection cases for an empty provider, empty or duplicate requests, nonpositive bounds, minimum greater than maximum, a request absent from `addressMatchValues`, static `responseRequestValue`/`responseMappings` mixed with `dynamicResponse`, any update mapping mixed with `dynamicResponse`, and a dynamic observer without `failureResponseValue`.

- [ ] **Step 2: Run RED**

Run the native Debug build and executable command from Task 1. Expected: parser assertions fail because `dynamicResponse` is ignored or rejected.

- [ ] **Step 3: Add the explicit definition fields and callback type**

```cpp
using MemoryStateObserverDynamicResponseCallback =
    std::function<std::optional<int>(const std::string& provider_id, int raw_request_value)>;
```

Add these documented members to `MemoryStateObserverDefinition`:

```cpp
std::optional<std::string> DynamicResponseProviderId;
std::vector<int> DynamicResponseRequestValues;
int DynamicResponseMinimumValue = 0;
int DynamicResponseMaximumValue = 0;
```

Parse the exact object above. When present, require all fields, reject unknown members, permit an empty `TargetConfigKey` only for this response-only observer shape, and enforce the test constraints. Dynamic scalar values are positive; transport encoding as a negative raw integer belongs to the observer service, not pack JSON. Existing mapped observers must still provide a nonempty target key.

- [ ] **Step 4: Run GREEN and commit**

Run the native suite command from Task 1. Expected: `PASS`.

```powershell
rtk git diff --check
rtk git add -- include/HelenHook/MemoryStateObserverDynamicResponseCallback.h include/HelenHook/MemoryStateObserverDefinition.h HelenRuntime/PackRepository.cpp tests/HelenRuntime.Tests/PackRepositoryTests.cpp
rtk git add -p -- HelenRuntime/HelenRuntime.vcxproj
rtk git diff --cached -- HelenRuntime/HelenRuntime.vcxproj
rtk git diff -- HelenRuntime/HelenRuntime.vcxproj
rtk git commit -m "Parse dynamic observer scalar responses"
```

Stage only the new callback-header project entry from `HelenRuntime.vcxproj`; leave the user's pre-existing project-file hunks unstaged.

---

### Task 3: Transport correlated dynamic scalar responses safely

**Files:**
- Modify: `include/HelenHook/MemoryStateObserverService.h`
- Modify: `HelenRuntime/MemoryStateObserverService.cpp`
- Test: `tests/HelenRuntime.Tests/MemoryStateObserverServiceTests.cpp`

- [ ] **Step 1: Add failing transport and shared-group tests**

Create one dynamic observer with requests `4700`, `4701`, and `4702`, bounds `1..32767`, failure `4899`, and callback results `3`, `1920`, and `1080`. Assert:

```cpp
Expect(ReadInt32(carrier_value_address) == -3, "Catalog count response was not encoded as a negative scalar.");
Expect(ReadInt32(carrier_value_address) == -34688, "Catalog width response was not ordinal-tagged.");
Expect(ReadInt32(carrier_value_address) == -66616, "Catalog height response was not ordinal-tagged.");
```

Add cases proving that a missing callback, absent provider result, zero, a value above 32767, or an exception writes `4899`; a negative response cannot discover a carrier; an exact pending negative response keeps the shared address cached across every grouped observer; an unrelated negative value invalidates the group; and a new declared request clears the old transient response.

- [ ] **Step 2: Run RED**

Run the native suite command from Task 1. Expected: compilation fails because the service has no dynamic callback constructor parameter.

- [ ] **Step 3: Implement the generic service behavior**

Extend the constructor without introducing Batman-specific branches:

```cpp
MemoryStateObserverService(
    std::vector<MemoryStateObserverDefinition> definitions,
    UpdateCallback update_callback,
    ConfigValueCallback config_value_callback = {},
    MemoryStateObserverDynamicResponseCallback dynamic_response_callback = {});
```

Store one transient raw response per address group and per ungrouped observer. Initial scanning continues to call only the static `IsAddressMatchValue`. Cached validation accepts the exact transient value only while its originating dynamic request is pending and all structural checks still pass.

When a declared dynamic request is read:

1. Call the callback with the declared provider and raw request.
2. Reject absence, exceptions, and values outside inclusive declared bounds.
3. Find the request's zero-based ordinal in `DynamicResponseRequestValues` and encode success as the negative magnitude `ordinal * 32768 + value`; use checked wide arithmetic, allowing `INT_MIN` only for magnitude `2147483648`, and reject an invalid ordinal or overflow. For `[4700, 4701, 4702]` with values `[3, 1920, 1080]`, the encoded responses are `-3`, `-34688`, and `-66616`.
4. Write the response at `ValueOffset`.
5. Record request, response, address, and address group before the next poll.
6. On failure, write `FailureResponseValue` and clear pending dynamic state.

Do not emit `MemoryStateObserverUpdate` for a dynamic read and do not allow another observer to consume the negative response.

- [ ] **Step 4: Run GREEN twice and commit**

Run the native suite twice. Expected both times: `PASS`, proving pending state does not leak between service instances.

```powershell
rtk git diff --check
rtk git add -- include/HelenHook/MemoryStateObserverService.h HelenRuntime/MemoryStateObserverService.cpp tests/HelenRuntime.Tests/MemoryStateObserverServiceTests.cpp
rtk git commit -m "Transport dynamic observer scalar responses"
```

---

### Task 4: Wire dynamic providers through the runtime coordinator

**Files:**
- Modify: `include/HelenHook/BuildRuntimeCoordinator.h`
- Modify: `HelenRuntime/BuildRuntimeCoordinator.cpp`
- Test: `tests/HelenRuntime.Tests/BuildRuntimeCoordinatorTests.cpp`

- [ ] **Step 1: Add failing forwarding tests**

Construct a coordinator with a callback that records provider ID and request, then drive a dynamic observer request. Assert exactly one call with `batmanDisplayModes` and `4700`, a returned `3` becoming raw `-3`, and no dispatcher key mutation or command execution. Add missing-provider and callback-exception cases expecting the observer failure response.

- [ ] **Step 2: Run RED**

Run the native suite command from Task 1. Expected: compilation fails because `BuildRuntimeCoordinator` does not accept the callback.

- [ ] **Step 3: Forward the callback unchanged**

Add the optional final constructor parameter:

```cpp
BuildRuntimeCoordinator(
    std::vector<std::string> startup_command_ids,
    std::vector<MemoryStateObserverDefinition> state_observers,
    CommandDispatcher& command_dispatcher,
    CommandExecutor& command_executor,
    MemoryStateObserverDynamicResponseCallback dynamic_response_callback = {});
```

Move it into `MemoryStateObserverService`. Do not add provider names or Batman logic to the coordinator.

- [ ] **Step 4: Run GREEN and commit**

Run the native suite. Expected: `PASS`.

```powershell
rtk git diff --check
rtk git add -- include/HelenHook/BuildRuntimeCoordinator.h HelenRuntime/BuildRuntimeCoordinator.cpp tests/HelenRuntime.Tests/BuildRuntimeCoordinatorTests.cpp
rtk git commit -m "Forward runtime observer query providers"
```

---

### Task 5: Apply one supported resolution pair atomically

**Files:**
- Modify: `include/HelenHook/CommandDispatcher.h`
- Modify: `HelenRuntime/CommandDispatcher.cpp`
- Modify: `include/HelenHook/BatmanGraphicsConfigService.h`
- Modify: `HelenRuntime/BatmanGraphicsConfigService.cpp`
- Modify: `include/HelenHook/CommandExecutor.h`
- Modify: `HelenRuntime/CommandExecutor.cpp`
- Test: `tests/HelenRuntime.Tests/CommandDispatcherTests.cpp`
- Test: `tests/HelenRuntime.Tests/CommandExecutorTests.cpp`

- [ ] **Step 1: Add failing dispatcher atomicity tests**

Specify this API:

```cpp
bool TrySetIntPair(
    const std::string& first_key,
    int first_value,
    const std::string& second_key,
    int second_value);
```

Assert both registered keys change together, either missing key leaves both old values unchanged, identical keys are rejected, and a persistent `JsonConfigStore` receives both values before one save.

- [ ] **Step 2: Add failing supported-mode command tests**

Inject catalog `1280x720`, `1920x1080`, `3440x1440`; register `resolutionModeIndex`, `resolutionWidth`, and `resolutionHeight`; run a command containing `set-batman-graphics-resolution-mode`; assert index 1 writes exactly `1920/1080`. Then change the injected enumeration so that mode disappears and assert command failure leaves the old pair exact. Add out-of-range index and missing-key cases.

- [ ] **Step 3: Run RED**

Run the native suite. Expected: compilation fails on `TrySetIntPair` and the new command-step behavior.

- [ ] **Step 4: Implement atomic dispatcher mutation**

Validate both distinct keys before changing either map entry. For a persistent store, set both store values and save once after the in-memory pair is ready. Preserve the scalar `TrySetInt` contract unchanged.

- [ ] **Step 5: Implement selected-resolution application**

Bind `BatmanGraphicsConfigService` to a required `BatmanDisplayModeService&` and add:

```cpp
bool ApplySelectedResolutionModeToDispatcher(CommandDispatcher& dispatcher);
```

Read `resolutionModeIndex`, reject negative/out-of-range values, call `RevalidateMode`, then call `TrySetIntPair("resolutionWidth", width, "resolutionHeight", height)`. The service must not select a closest mode or update the INIs here.

Add `set-batman-graphics-resolution-mode` to `CommandExecutor::ExecuteStep`; it calls only this method. Update all construction sites and fixtures to pass a required display-mode service rather than nullable ownership.

- [ ] **Step 6: Extend INI assertions and run GREEN**

In the existing graphics Apply fixture, assert both `UserEngine.ini` and `BmEngine.ini` contain the chosen `Fullscreen`, `ResX`, and `ResY`. Keep the exact-byte compensation assertions for every publication failure.

Run the native suite twice. Expected: `PASS` both times.

- [ ] **Step 7: Commit Task 5**

```powershell
rtk git diff --check
rtk git add -- include/HelenHook/CommandDispatcher.h HelenRuntime/CommandDispatcher.cpp include/HelenHook/BatmanGraphicsConfigService.h HelenRuntime/BatmanGraphicsConfigService.cpp include/HelenHook/CommandExecutor.h HelenRuntime/CommandExecutor.cpp tests/HelenRuntime.Tests/CommandDispatcherTests.cpp tests/HelenRuntime.Tests/CommandExecutorTests.cpp
rtk git commit -m "Apply Batman resolution drafts atomically"
```

---

### Task 6: Activate Fullscreen in the shell

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellContract.ps1`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellScriptTemplates.cs`

- [ ] **Step 1: Change the shell contract to require Fullscreen**

Add this exact setting record before VSync:

```powershell
@{ RowIndex=1; Name='Fullscreen'; Values=@('Windowed','Fullscreen'); ConfigValues=@(0,1); Read=4670; ResponseBase=4671; WriteBase=4673; AckBase=4675; Failure=4679 }
```

Require row 1 to use the active row clip action, participate in local dirty state, initialize first, queue first when dirty, and preserve the uniform `-12/+12` arrow widening. Require Back to emit no `4673` or `4674` write.

- [ ] **Step 2: Run RED**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellContract.ps1 -Configuration Release
```

Expected: `FAIL` because row 1 still contains `Not active`.

- [ ] **Step 3: Add Fullscreen to the production shell**

Prepend this ActionScript setting definition and replace the first row generator:

```actionscript
{RowIndex:1,Name:"Fullscreen",Values:new Array("Windowed","Fullscreen"),ConfigValues:new Array(0,1),ReadRequest:4670,ReadResponseBase:4671,WriteRequestBase:4673,WriteAcknowledgementBase:4675,FailureResponse:4679,InitialIndex:-1,DraftIndex:-1}
```

```csharp
CreateActiveRowClipAction("Fullscreen", 1, ["Windowed", "Fullscreen"])
```

Do not touch Resolution yet. The existing generic initialization, dirty queue, acknowledgement, commit, rollback, and row behavior must carry Fullscreen without special cases.

- [ ] **Step 4: Run GREEN and commit**

Run the shell contract. Expected: `STATE_MACHINE_PASS` and `PASS`.

```powershell
rtk git diff --check
rtk git add -- games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellContract.ps1 games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellScriptTemplates.cs
rtk git commit -m "Activate Batman fullscreen option"
```

---

### Task 7: Add the local supported-resolution shell controller

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellContract.ps1`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellScriptTemplates.cs`

- [ ] **Step 1: Add failing catalog and resolution state-machine tests**

Extend the Node harness with catalog responses for `1280x720`, `1920x1080`, and `3440x1440`, plus current `1920x1080`. Require exact labels and draft index 1. Assert:

- left selects `1280 x 720` and stops at the first item
- right selects `3440 x 1440` and stops at the last item
- activation advances and wraps
- selection only changes shell state before Apply
- dirty resolution queues `5000 + draftIndex`
- acknowledgement must equal `5100 + draftIndex`
- `5199`, timeout, commit failure, and rollback failure use the existing failure paths
- Back emits no `5000..5097` request
- zero, 99, duplicate, incomplete, out-of-order, positive scalar, unsupported-current, and mismatched-request responses fail initialization

- [ ] **Step 2: Run RED**

Run the shell contract. Expected: `FAIL` because Resolution remains `Not active` and no catalog state exists.

- [ ] **Step 3: Add catalog state and serialized initialization**

Add documented controller fields:

```actionscript
this.ResolutionModes = new Array();
this.ResolutionInitialIndex = -1;
this.ResolutionDraftIndex = -1;
this.ResolutionCatalogCount = 0;
this.ResolutionCatalogRequest = 4700;
this.ResolutionCatalogDeadline = undefined;
```

Initialization order is Fullscreen, catalog count, each width/height pair, current width, current height, then the remaining eleven transmitted settings. Catalog requests are `4700`, entry requests `4701 + 2*i` and `4702 + 2*i`, current width `4897`, and current height `4898`. `4899` is failure. Successful scalar responses are ordinal-tagged negatives; decode magnitude as `-rawValue`, request ordinal as `floor((magnitude - 1) / 32768)`, and scalar as `magnitude - ordinal * 32768`, requiring scalar `1..32767` and matching the pending request ordinal.

Build each explicit object only after both dimensions arrive:

```actionscript
this.ResolutionModes.push({Width:widthValue,Height:heightValue,Label:widthValue + " x " + heightValue});
```

Require count `1..98`, exact response/request correlation, unique pairs, and an exact current-pair match. Do not use bootstrap values as fallback.

- [ ] **Step 4: Add local row behavior and special Apply entry**

Replace row 2 with a dedicated `CreateResolutionRowClipAction()`. Its Update reads label and enabled state from controller methods; its left/right/activation methods mutate only `ResolutionDraftIndex` and refresh Apply state. Reuse the same once-only arrow alignment helper as every active option row.

Extend dirty detection and queue creation with one explicit resolution operation in screen order. The queued record contains selected index, request `5000 + index`, acknowledgement `5100 + index`, and failure `5199`. On commit success copy draft index to initial. On rollback success restore the last saved exact pair returned by the normal initialization reload.

- [ ] **Step 5: Run GREEN twice and commit**

Run the shell contract twice. Expected both times: `STATE_MACHINE_PASS` and `PASS`, proving catalog state does not drift between generated-controller instances.

```powershell
rtk git diff --check
rtk git add -- games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellContract.ps1 games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellScriptTemplates.cs
rtk git commit -m "Activate Batman supported resolution option"
```

---

### Task 8: Wire the runtime and generate the exact pack protocol

**Files:**
- Modify: `HelenGameHook/HelenGameHook.cpp`
- Modify: `games/HelenBatmanAA/scripts/Rebuild-BatmanGraphicsOptionsExperiment.ps1`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1`
- Modify: `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`

- [ ] **Step 1: Make package and repository tests fail on the new exact protocol**

Require config key `resolutionModeIndex` with default `-1`, command `setBatmanGraphicsResolutionMode` with one `set-batman-graphics-resolution-mode` step, and exactly sixteen graphics observers including Apply and Rollback. The sixteen are twelve finite scalar-setting observers, one dynamic catalog/current-resolution observer, one resolution-index observer, Apply, and Rollback. Require these allocations:

```text
Fullscreen: read 4670; responses 4671/4672; writes 4673/4674; acks 4675/4676; failure 4679
Catalog/current reads: requests 4700..4898; negative dynamic scalars; failure 4899
Resolution Apply: writes 5000..5097 -> indices 0..97; acks 5100..5197; failure 5199
```

The catalog observer declares provider `batmanDisplayModes`, requests `4700..4898`, and scalar bounds `1..32767`. All grouped observers contain the complete positive static union; negative scalar responses are transient and never discovery values.

- [ ] **Step 2: Run RED**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsPackage.ps1 -Configuration Release
```

Expected: `FAIL` because generated manifests still omit Fullscreen, catalog, and resolution selection.

- [ ] **Step 3: Wire the provider and selected-mode command**

Create `BatmanDisplayModeService` before `BatmanGraphicsConfigService` in `HelenGameHook.cpp`, pass its required reference into graphics-service construction, and pass this dynamic callback into `BuildRuntimeCoordinator`:

```cpp
[&display_mode_service, &command_dispatcher](const std::string& provider_id, int raw_request) -> std::optional<int> {
    if (provider_id != "batmanDisplayModes") {
        return std::nullopt;
    }
    if (raw_request == 4897) {
        return command_dispatcher.TryGetInt("resolutionWidth");
    }
    if (raw_request == 4898) {
        return command_dispatcher.TryGetInt("resolutionHeight");
    }
    if (raw_request == 4700 && !display_mode_service.Refresh()) {
        return std::nullopt;
    }
    return display_mode_service.QueryCatalogScalar(raw_request);
}
```

Use owned runtime lifetime rather than references to temporaries. Reset the display service only after coordinator shutdown.

- [ ] **Step 4: Generate exact declarative records**

Extend the rebuild protocol table with Fullscreen and one special resolution selection definition. Generate all 98 index mappings rather than hand-maintaining checked-in JSON. Add `resolutionModeIndex` and the resolution command. Generate the catalog dynamic-response object with exact requests and bounds. Recompute the complete address union and reject duplicate positive codes.

- [ ] **Step 5: Run native GREEN and confirm the checked-in package is RED**

Run the native suite, then the package validator. Expected: native `PASS`; package validator fails because the checked-in manifests and generated asset have not yet been regenerated from the changed source contract. Confirm that the failure names the stale package rather than a native protocol defect.

- [ ] **Step 6: Commit runtime and protocol source before binary regeneration**

```powershell
rtk git diff --check
rtk git add -- HelenGameHook/HelenGameHook.cpp games/HelenBatmanAA/scripts/Rebuild-BatmanGraphicsOptionsExperiment.ps1 games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1 tests/HelenRuntime.Tests/PackRepositoryTests.cpp
rtk git commit -m "Wire Batman display option protocol"
```

---

### Task 9: Regenerate, validate, and commit the seven-file package

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsLayout.ps1`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellDeployment.ps1`
- Modify: `games/HelenBatmanAA/scripts/Deploy-BatmanGraphicsOptionsExperiment.ps1`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsRetailBaseContract.ps1` only for new approved script hashes/allowlists
- Modify: `games/HelenBatmanAA/builder/generated/graphics-options-experiment/Frontend-graphics-options.umap`
- Modify: `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/pack.json`
- Modify: `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/build.json`
- Modify: `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/bindings.json`
- Modify: `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/commands.json`
- Modify: `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/files.json`
- Modify: `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/hooks.json`
- Modify: `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/assets/deltas/Frontend-graphics-options.hgdelta`

- [ ] **Step 1: Update validator expectations first**

Require all fourteen option rows active, rows 1 and 2 widened once, exact new script/target/delta hashes, the complete dynamic/static protocol, exact seven-file inventory, delta round trip, and retail preservation outside the existing explicit sprite/script allowlists. Add mutation tests that remove one catalog request, duplicate one code, change a scalar bound, corrupt one resolution acknowledgement, or restore either row to `Not active`; each mutation must fail.

- [ ] **Step 2: Run validators and verify RED**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsLayout.ps1 -Configuration Release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellDeployment.ps1
```

Expected: `FAIL` on stale generated target/package hashes.

- [ ] **Step 3: Rebuild only from the verified retail base and repository sources**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Rebuild-BatmanGraphicsOptionsExperiment.ps1 -Configuration Release
```

Expected: one verified target UMAP, one regenerated HGDL delta, updated `files.json`, exact seven pack files, and no use of installed or historical assets.

- [ ] **Step 4: Run the complete automated gate**

```powershell
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /t:Build /p:Configuration=Debug /p:Platform=Win32 /m:1 /nodeReuse:false /v:minimal; if (`$LASTEXITCODE -ne 0) { exit `$LASTEXITCODE }; & '.\bin\Win32\Debug\tests\HelenRuntimeTests.exe'; exit `$LASTEXITCODE"
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' 'HelenHook.sln' /t:Build /p:Configuration=Release /p:Platform=Win32 /m:1 /nodeReuse:false /v:minimal; exit `$LASTEXITCODE"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellContract.ps1 -Configuration Release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsPackage.ps1 -Configuration Release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsRetailBaseContract.ps1 -Configuration Release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsLayout.ps1 -Configuration Release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellDeployment.ps1
```

Expected: native `PASS`, Release build with zero errors, shell `STATE_MACHINE_PASS` and `PASS`, then every package/retail/layout/deployment validator `PASS`.

- [ ] **Step 5: Verify provenance and exact inventory**

Confirm `files.json` target size/hash matches the generated UMAP; HGDL header target hash matches both; applying the delta to the declared retail base reproduces the target byte-for-byte; and the pack has exactly the seven paths listed in File Structure. Search production sources and generated scripts for `Helen_GetInt`, `Helen_SetInt`, `Helen_RunCommand`, installed paths, `batma`, and `F:\helenhook.7z`; all must be absent from the production provenance graph.

- [ ] **Step 6: Commit generated package and validators**

```powershell
rtk git diff --check
rtk git add -- games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsLayout.ps1 games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellDeployment.ps1 games/HelenBatmanAA/scripts/Deploy-BatmanGraphicsOptionsExperiment.ps1 games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsRetailBaseContract.ps1 games/HelenBatmanAA/builder/generated/graphics-options-experiment/Frontend-graphics-options.umap games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/pack.json games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/build.json games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/bindings.json games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/commands.json games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/files.json games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/hooks.json games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/assets/deltas/Frontend-graphics-options.hgdelta
rtk git commit -m "Package Batman supported display options"
```

---

### Task 10: Deploy and run live checkpoints

**Files:**
- No source edits expected.

- [ ] **Step 1: Confirm Batman is closed**

```powershell
if (Get-Process -Name 'ShippingPC-BmGame' -ErrorAction SilentlyContinue) { throw 'Close Batman before deployment.' }
```

Expected: no output.

- [ ] **Step 2: Deploy the validated Release package atomically**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Deploy-BatmanGraphicsOptionsExperiment.ps1 -GameBin 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries' -Configuration Release
```

Expected: validator `PASS`, validator `PASS`, then `DEPLOYED`.

- [ ] **Step 3: Verify installed provenance**

Compare SHA-256 for source/live `HelenGameHook.dll`, `dinput8.dll`, `packs.json`, and every relative file in `batman-aa-graphics-options`. Require source count 7, live count 7, exact ordered path/length/hash equality, and no `.helengamehook-staging-*` or `.helengamehook-recovery-*` directory.

- [ ] **Step 4: Run live checkpoint A**

Ask the user to open Graphics Options and report:

- Fullscreen shows Windowed or Fullscreen immediately.
- Resolution shows the current `WIDTH x HEIGHT` immediately.
- Resolution contains only unique modes supported by the display Batman occupies.
- Both rows are selectable and their arrows retain the widened alignment.

Read `helengamehook/logs/HelenGameHook.log` and require the catalog monitor/count plus successful Fullscreen and resolution initialization responses, with no observer failure, rollback, or virtualization error.

- [ ] **Step 5: Run live checkpoint B**

Ask the user to change Fullscreen and Resolution, press Back, reopen, and confirm the original values return. Then change them again, press Apply Changes, confirm Apply disables, fully exit, relaunch, and confirm the applied values return.

Verify both `UserEngine.ini` and `BmEngine.ini` contain the same applied `Fullscreen`, `ResX`, and `ResY`. Confirm the log contains the exact selected-index acknowledgement and successful dual-INI transaction.

- [ ] **Step 6: Final repository audit**

```powershell
rtk git diff --check
rtk git status --short
rtk git log -12 --oneline
```

Expected: only the user's known unrelated dirty paths remain; every feature task is represented by a focused commit on `main`.
