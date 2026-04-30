# Batman Subtitle Six-Size Restoration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore the Batman gameplay-only subtitle mod from three in-game sizes to six in-game sizes while preserving the current working live-update path and avoiding any main-menu changes.

**Architecture:** Keep the existing split of responsibilities: the pause-menu ActionScript owns FE raw-code translation, the gameplay pack owns persisted `ui.subtitleSize` metadata and startup replay, and the EXE patcher remains on the already-working `--ui-state-live` deploy mode. Make the current three-state assumptions fail first, then widen the builder, override script, pack manifests, and generated artifacts to a single six-state contract.

**Tech Stack:** PowerShell Batman build/deploy scripts, ActionScript override source, C#/.NET 8 `SubtitleSizeModBuilder`, Helen gameplay pack JSON manifests, existing Batman regression scripts, deployed Win32 EXE patched by `NativeSubtitleExePatcher`

---

## File Map

### Modified gameplay subtitle UI sources

- `games/HelenBatmanAA/patch-source/PauseRuntimeScaleListItem.as`
  - Source-of-truth gameplay pause-menu override that reads and writes `FE_GetControlType` / `FE_SetControlType`.
- `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/ScriptTemplates.cs`
  - Generated pause-menu template source that must stop trimming subtitle size back to three options.

### Modified gameplay pack sources and generated manifests

- `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/pack.json`
  - Shipped gameplay-only pack metadata that exposes `Subtitle Size` labels and persisted config defaults.
- `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/commands.json`
  - Shipped gameplay command mapping from `ui.subtitleSize` to runtime subtitle scale.
- `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/files.json`
  - Rebuilt gameplay-only delta manifest written by the pack rebuild.
- `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/assets/deltas/BmGame-subtitle-signal.hgdelta`
  - Rebuilt gameplay delta produced after the six-state pause asset changes.

### Modified Batman build and verification scripts

- `games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1`
  - Rebuilds the gameplay-only pack and must emit six labels, six persisted states, and six scale mappings.
- `games/HelenBatmanAA/scripts/Test-BatmanPauseRuntimeScaleBuilder.ps1`
  - Red/green contract for the generated pause-menu ActionScript and subtitle-size initializer.
- `games/HelenBatmanAA/scripts/Test-BatmanKnownGoodGameplayPackage.ps1`
  - Red/green contract for shipped gameplay-only pack metadata, `ConsoleFontSize` value-map entries, and command mappings.
- `games/HelenBatmanAA/scripts/Test-BatmanDeploySubtitleSignalMode.ps1`
  - Verifies deploy still uses the working `--ui-state-live` EXE patch mode.

### Verified but expected to stay unchanged

- `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/Program.cs`
  - Already carries six raw codes and the current live-update patch path; treat as verify-only unless a failing test proves a gap.
- `games/HelenBatmanAA/scripts/Deploy-Batman.ps1`
  - Expected to remain on `--ui-state-live`; only change this file if the verification test proves drift.

---

## Task 1: Make The Pause-Menu Six-State Contract Fail First

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanPauseRuntimeScaleBuilder.ps1`
- Modify: `games/HelenBatmanAA/patch-source/PauseRuntimeScaleListItem.as`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/ScriptTemplates.cs`

- [ ] **Step 1: Write the failing pause-menu regression**

Update `games/HelenBatmanAA/scripts/Test-BatmanPauseRuntimeScaleBuilder.ps1` so it rejects the current three-state initializer and requires the full six-state FE contract:

```powershell
if ($ListItemContents.IndexOf('4104', [System.StringComparison]::Ordinal) -lt 0) {
    throw "Pause runtime scale builder does not emit the Very Large raw code 4104."
}

if ($ListItemContents.IndexOf('4105', [System.StringComparison]::Ordinal) -lt 0) {
    throw "Pause runtime scale builder does not emit the Huge raw code 4105."
}

if ($ListItemContents.IndexOf('4106', [System.StringComparison]::Ordinal) -lt 0) {
    throw "Pause runtime scale builder does not emit the Massive raw code 4106."
}

$ExpectedClipAction = 'this.Init("SubtitleSize","Small","Medium","Large","Very Large","Huge","Massive");'
if ($ClipActionContents.IndexOf($ExpectedClipAction, [System.StringComparison]::Ordinal) -lt 0) {
    throw "Pause runtime scale builder does not emit the six-choice SubtitleSize initializer."
}
```

- [ ] **Step 2: Run the regression to prove the current build is wrong**

Run:

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -File ".\games\HelenBatmanAA\scripts\Test-BatmanPauseRuntimeScaleBuilder.ps1"
```

Expected: `FAIL` with a message about missing `4104`, `4105`, `4106`, or the missing six-choice initializer.

- [ ] **Step 3: Implement the six-state pause-menu mapping**

Update `games/HelenBatmanAA/patch-source/PauseRuntimeScaleListItem.as` and the mirrored template in `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/ScriptTemplates.cs` so subtitle size no longer collapses to three states:

```actionscript
function GetSubtitleDefaultState()
{
   return 1;
}

function MapStoredSubtitleCodeToState(InValue)
{
   switch(this.NormalizeIntState(InValue,4102))
   {
      case 4101:
         return 0;
      case 4102:
         return 1;
      case 4103:
         return 2;
      case 4104:
         return 3;
      case 4105:
         return 4;
      case 4106:
         return 5;
      default:
         return this.GetSubtitleDefaultState();
   }
}

function MapSubtitleStateToStoredCode(InValue)
{
   switch(this.NormalizeIntState(InValue,this.GetSubtitleDefaultState()))
   {
      case 0:
         return 4101;
      case 1:
         return 4102;
      case 2:
         return 4103;
      case 3:
         return 4104;
      case 4:
         return 4105;
      case 5:
         return 4106;
      default:
         return 4102;
   }
}
```

Also remove the three-state trimming logic from the template:

```csharp
// Delete the block that pops subtitle labels back down to three entries.
if(this.UsesSubtitleSizeStorage() && this.Names.length > 3)
{
   while(this.Names.length > 3)
   {
      this.Names.pop();
   }
}
```

And update the generated subtitle initializer string in `ScriptTemplates.cs` to:

```csharp
public const string PauseAudioSubtitleSizeClipAction = """
onClipEvent(load){
   this.Init("SubtitleSize","Small","Medium","Large","Very Large","Huge","Massive");
}
""";
```

- [ ] **Step 4: Rebuild the pause asset and verify the regression turns green**

Run:

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -File ".\games\HelenBatmanAA\scripts\Rebuild-BatmanPack.ps1" -Configuration Debug
rtk powershell -NoProfile -ExecutionPolicy Bypass -File ".\games\HelenBatmanAA\scripts\Test-BatmanPauseRuntimeScaleBuilder.ps1"
rtk powershell -NoProfile -ExecutionPolicy Bypass -File ".\games\HelenBatmanAA\scripts\Test-BatmanPauseAudioLayout.ps1"
```

Expected:

- `Test-BatmanPauseRuntimeScaleBuilder.ps1` prints `PASS`
- `Test-BatmanPauseAudioLayout.ps1` prints `PASS`

- [ ] **Step 5: Commit the pause-menu six-state contract**

```powershell
rtk git add `
  .\games\HelenBatmanAA\patch-source\PauseRuntimeScaleListItem.as `
  .\games\HelenBatmanAA\builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\ScriptTemplates.cs `
  .\games\HelenBatmanAA\scripts\Test-BatmanPauseRuntimeScaleBuilder.ps1
rtk git commit -m "Restore Batman six-state pause subtitle menu"
```

## Task 2: Make The Gameplay Pack Metadata Six-State End-To-End

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanKnownGoodGameplayPackage.ps1`
- Modify: `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/pack.json`
- Modify: `games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1`
- Modify: `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/commands.json`

- [ ] **Step 1: Write the failing gameplay-pack regression**

Update `games/HelenBatmanAA/scripts/Test-BatmanKnownGoodGameplayPackage.ps1` so it rejects the current three-option pack and requires six persisted states, six option labels, and six runtime scale mappings:

```powershell
$ExpectedOptions = @('Small', 'Medium', 'Large', 'Very Large', 'Huge', 'Massive')
if ((@($PackManifest.features[0].options) -join '|') -ne ($ExpectedOptions -join '|')) {
    throw 'Batman subtitle pack options did not match the approved six-label order.'
}

$ExpectedIniKeys = @(
    @{
        id = 'subtitleSize'
        valueMap = @(
            @{ match = 0; encodedValue = 5 },
            @{ match = 1; encodedValue = 6 },
            @{ match = 2; encodedValue = 7 },
            @{ match = 3; encodedValue = 8 },
            @{ match = 4; encodedValue = 9 },
            @{ match = 5; encodedValue = 10 }
        )
    }
)

$ExpectedSubtitleScaleMappings = @(
    @{ match = 0; value = 1.0 },
    @{ match = 1; value = 1.5 },
    @{ match = 2; value = 2.0 },
    @{ match = 3; value = 4.0 },
    @{ match = 4; value = 6.0 },
    @{ match = 5; value = 8.0 }
)
```

- [ ] **Step 2: Run the gameplay-pack regression to verify it fails**

Run:

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -File ".\games\HelenBatmanAA\scripts\Test-BatmanKnownGoodGameplayPackage.ps1"
```

Expected: `FAIL` with a mismatch on options, value-map count, or subtitle scale mapping count.

- [ ] **Step 3: Implement the six-state gameplay pack metadata**

Update `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/pack.json` and the manifest-writing section of `games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1` to emit six labels and six persisted `ConsoleFontSize` values:

```json
"options": [
  "Small",
  "Medium",
  "Large",
  "Very Large",
  "Huge",
  "Massive"
]
```

```powershell
valueMap = @(
    @{ match = 0; encodedValue = 5 }
    @{ match = 1; encodedValue = 6 }
    @{ match = 2; encodedValue = 7 }
    @{ match = 3; encodedValue = 8 }
    @{ match = 4; encodedValue = 9 }
    @{ match = 5; encodedValue = 10 }
)
```

Update the generated `applySubtitleSize` command mapping to the approved six-scale ladder:

```json
"mappings": [
  { "match": 0, "value": 1.0 },
  { "match": 1, "value": 1.5 },
  { "match": 2, "value": 2.0 },
  { "match": 3, "value": 4.0 },
  { "match": 4, "value": 6.0 },
  { "match": 5, "value": 8.0 }
]
```

- [ ] **Step 4: Rebuild the gameplay pack and verify the six-state metadata**

Run:

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -File ".\games\HelenBatmanAA\scripts\Rebuild-BatmanPack.ps1" -Configuration Debug
rtk powershell -NoProfile -ExecutionPolicy Bypass -File ".\games\HelenBatmanAA\scripts\Test-BatmanKnownGoodGameplayPackage.ps1"
```

Expected:

- rebuild completes successfully
- `Test-BatmanKnownGoodGameplayPackage.ps1` prints `PASS`

- [ ] **Step 5: Commit the six-state gameplay pack metadata**

```powershell
rtk git add `
  .\games\HelenBatmanAA\helengamehook\packs\batman-aa-subtitles\pack.json `
  .\games\HelenBatmanAA\helengamehook\packs\batman-aa-subtitles\builds\steam-goty-1.0\commands.json `
  .\games\HelenBatmanAA\helengamehook\packs\batman-aa-subtitles\builds\steam-goty-1.0\files.json `
  .\games\HelenBatmanAA\helengamehook\packs\batman-aa-subtitles\builds\steam-goty-1.0\assets\deltas\BmGame-subtitle-signal.hgdelta `
  .\games\HelenBatmanAA\scripts\Rebuild-BatmanPack.ps1 `
  .\games\HelenBatmanAA\scripts\Test-BatmanKnownGoodGameplayPackage.ps1
rtk git commit -m "Restore Batman six-state subtitle pack metadata"
```

## Task 3: Verify The Live Update Path Still Matches The Working Runtime

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanDeploySubtitleSignalMode.ps1`
- Verify only unless failing test proves otherwise:
  - `games/HelenBatmanAA/scripts/Deploy-Batman.ps1`
  - `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/Program.cs`

- [ ] **Step 1: Lock the deploy contract to the current EXE patch mode**

Make sure `games/HelenBatmanAA/scripts/Test-BatmanDeploySubtitleSignalMode.ps1` explicitly rejects any drift away from `--ui-state-live`:

```powershell
if ($DeployText.IndexOf('--ui-state-live', [System.StringComparison]::Ordinal) -lt 0) {
    throw 'Batman deploy must patch subtitle runtime with --ui-state-live.'
}

foreach ($ForbiddenMode in @('--subtitle-size-signal', '--subtitle-tail-debug-signal', '--render3d-tail-debug-signal', '--invoke-trace')) {
    if ($DeployText.IndexOf($ForbiddenMode, [System.StringComparison]::Ordinal) -ge 0) {
        throw "Batman deploy must not regress to $ForbiddenMode."
    }
}
```

- [ ] **Step 2: Run the deploy-mode regression**

Run:

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -File ".\games\HelenBatmanAA\scripts\Test-BatmanDeploySubtitleSignalMode.ps1"
```

Expected: `PASS` immediately. If it fails, fix `Deploy-Batman.ps1` only enough to restore `--ui-state-live`.

- [ ] **Step 3: Rebuild, deploy, and run the full Batman verification set**

Run:

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -File ".\games\HelenBatmanAA\scripts\Test-BatmanSubtitleSignalHookContract.ps1"
rtk powershell -NoProfile -ExecutionPolicy Bypass -File ".\games\HelenBatmanAA\scripts\Test-BatmanPauseRuntimeScaleBuilder.ps1"
rtk powershell -NoProfile -ExecutionPolicy Bypass -File ".\games\HelenBatmanAA\scripts\Test-BatmanKnownGoodGameplayPackage.ps1"
rtk powershell -NoProfile -ExecutionPolicy Bypass -File ".\games\HelenBatmanAA\scripts\Test-BatmanDeploySubtitleSignalMode.ps1"
rtk powershell -NoProfile -ExecutionPolicy Bypass -File ".\games\HelenBatmanAA\scripts\Deploy-Batman.ps1" -Configuration Debug
```

Expected:

- each verification script prints `PASS`
- deploy prints `DEPLOYED`

- [ ] **Step 4: Validate the in-game contract with one manual smoke pass**

Check these exact behaviors in the game after deployment:

```text
1. Open Pause -> Audio and confirm six options are visible:
   Small / Medium / Large / Very Large / Huge / Massive
2. Change from Medium to Massive and confirm the subtitle size changes live.
3. Leave and reopen the menu and confirm Massive remains selected.
4. Restart the game and confirm Massive is still selected and still applies.
```

If live resize breaks at this stage, stop and debug before changing architecture. Do not switch away from `--ui-state-live` without a new failing test that proves a runtime gap.

- [ ] **Step 5: Commit the verified six-state restore**

```powershell
rtk git add `
  .\games\HelenBatmanAA\scripts\Test-BatmanDeploySubtitleSignalMode.ps1 `
  .\games\HelenBatmanAA\scripts\Deploy-Batman.ps1
rtk git commit -m "Verify Batman six-state subtitle deploy path"
```

## Task 4: Final Verification And Branch Hygiene

**Files:**
- Verify only:
  - `games/HelenBatmanAA/scripts/Test-BatmanPauseAudioLayout.ps1`
  - `games/HelenBatmanAA/scripts/Test-BatmanSubtitleSignalHookContract.ps1`
  - `games/HelenBatmanAA/scripts/Test-BatmanKnownGoodGameplayPackage.ps1`
  - `games/HelenBatmanAA/scripts/Test-BatmanPauseRuntimeScaleBuilder.ps1`
  - `games/HelenBatmanAA/scripts/Test-BatmanDeploySubtitleSignalMode.ps1`

- [ ] **Step 1: Run the final verification sweep from a clean worktree**

Run:

```powershell
rtk git status --short
rtk powershell -NoProfile -ExecutionPolicy Bypass -File ".\games\HelenBatmanAA\scripts\Test-BatmanPauseAudioLayout.ps1"
rtk powershell -NoProfile -ExecutionPolicy Bypass -File ".\games\HelenBatmanAA\scripts\Test-BatmanPauseRuntimeScaleBuilder.ps1"
rtk powershell -NoProfile -ExecutionPolicy Bypass -File ".\games\HelenBatmanAA\scripts\Test-BatmanKnownGoodGameplayPackage.ps1"
rtk powershell -NoProfile -ExecutionPolicy Bypass -File ".\games\HelenBatmanAA\scripts\Test-BatmanDeploySubtitleSignalMode.ps1"
rtk powershell -NoProfile -ExecutionPolicy Bypass -File ".\games\HelenBatmanAA\scripts\Test-BatmanSubtitleSignalHookContract.ps1"
```

Expected:

- `git status --short` shows only the intended tracked changes before the final commit, then nothing after commit
- all five verification scripts print `PASS`

- [ ] **Step 2: Create the final implementation commit**

```powershell
rtk git add .\games\HelenBatmanAA .\docs\superpowers\plans\2026-04-30-batman-subtitle-six-size-restoration.md
rtk git commit -m "Expand Batman subtitle mod to six gameplay sizes"
```

- [ ] **Step 3: Record the verification evidence in the close-out**

Capture the exact commands that passed and their final status:

```text
Test-BatmanPauseAudioLayout.ps1 -> PASS
Test-BatmanPauseRuntimeScaleBuilder.ps1 -> PASS
Test-BatmanKnownGoodGameplayPackage.ps1 -> PASS
Test-BatmanDeploySubtitleSignalMode.ps1 -> PASS
Test-BatmanSubtitleSignalHookContract.ps1 -> PASS
Deploy-Batman.ps1 -Configuration Debug -> DEPLOYED
```

Use that evidence in the final summary instead of claiming success from inspection alone.
