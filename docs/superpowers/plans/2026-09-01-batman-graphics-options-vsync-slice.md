# Batman Graphics Options VSync Slice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the stable Batman graphics-options shell display, edit, and persist VSync through a minimal two-signal frontend carrier without activating the other graphics rows.

**Architecture:** Keep `BuildShell`/`PatchShell` and the selective FFDec import that passed the live title transition. Inject the current VSync value from `BmEngine.ini`, manage one local ActionScript draft, then dispatch exact `FE_SetControlType` codes to two HelenHook state observers: one for VSync and one for commit. Generate and validate the package from current sources before atomic deployment.

**Tech Stack:** C#/.NET 9 builder, ActionScript 2 emitted as C# raw strings, PowerShell packaging and integration checks, FFDec CLI, Unreal package patching, HGDL delta packaging, HelenHook JSON manifests.

---

## File Structure

- Modify `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellScriptTemplates.cs`: own the VSync-only ActionScript controller plus inert, VSync, and Apply row templates.
- Modify `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellBuildPaths.cs`: add the required user `BmEngine.ini` input to the shell path model.
- Modify `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsAssetBuilder.cs`: load the typed INI snapshot and inject it into the shell scripts while preserving scoped script staging.
- Modify `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/Program.cs`: accept `--ini` for `build-main-menu-graphics-shell` and pass it into the path model.
- Modify `games/HelenBatmanAA/scripts/Test-BatmanRetailGraphicsOptionsPatch.ps1`: verify the reopened retail-patched GFX has only VSync and Apply interaction.
- Modify `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1`: verify the generated scripts, package schema, commands, and two exact carrier observers.
- Modify `games/HelenBatmanAA/scripts/Rebuild-BatmanGraphicsOptionsExperiment.ps1`: pass the INI to the builder and generate minimal config/command/hook manifests.
- Regenerate `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/**`: publish current-source manifests and delta.
- Regenerate `games/HelenBatmanAA/builder/generated/graphics-options-experiment/Frontend-graphics-options.umap`: publish the byte-verified current-source target package.

### Task 1: Specify the VSync-only exported ActionScript contract

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanRetailGraphicsOptionsPatch.ps1:28-56`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1:140-205`

- [ ] **Step 1: Replace the all-inert row assertion with an exact VSync-slice assertion**

In both scripts, keep the existing path discovery and add this contract over the fifteen row scripts:

```powershell
function Get-VsyncSliceRowActionBody {
    param([Parameter(Mandatory = $true)] [string]$Text, [Parameter(Mandatory = $true)] [string]$FunctionAssignment)
    $match = [regex]::Match($Text, [regex]::Escape($FunctionAssignment) + '\s*=\s*function\s*\(\s*\)\s*\{(?<body>.*?)\}', [Text.RegularExpressions.RegexOptions]::Singleline)
    if (-not $match.Success) { throw "Missing row action $FunctionAssignment." }
    return $match.Groups['body'].Value.Trim()
}

function Assert-NoOpVsyncSliceRowActions {
    param([Parameter(Mandatory = $true)] [string]$Text, [Parameter(Mandatory = $true)] [string]$Context)
    foreach ($action in @('RunAction', 'Increment', 'Decrement', 'ShowPrompt')) {
        if ((Get-VsyncSliceRowActionBody -Text $Text -FunctionAssignment "this.$action") -ne '') { throw "$Context action $action must be a no-op." }
    }
}

function Assert-VsyncSliceRowContract {
    param([Parameter(Mandatory = $true)] [string]$ScreenDirectory)

    $depths = @('141', '133', '125', '117', '109', '101', '93', '85', '77', '69', '61', '53', '45', '37', '29')
    $labels = @('Fullscreen', 'Resolution', 'VSync', 'MSAA', 'Detail Level', 'Bloom', 'Dynamic Shadows', 'Motion Blur', 'Distortion', 'Fog Volumes', 'Spherical Harmonic Lighting', 'Ambient Occlusion', 'PhysX', 'Stereo 3D', 'Apply Changes')
    for ($index = 0; $index -lt $depths.Count; $index++) {
        $rowPath = Join-Path $ScreenDirectory "frame_1\PlaceObject2_290_List_Template_$($depths[$index])\CLIPACTIONRECORD onClipEvent(load).as"
        if (-not (Test-Path -LiteralPath $rowPath)) { throw "Missing known row script $rowPath." }
        $rowText = Get-Content -LiteralPath $rowPath -Raw
        Assert-ContainsOrdinal -Text $rowText -Token $labels[$index] -Context "graphics row $($index + 1) label"
        Assert-ContainsOrdinal -Text $rowText -Token 'this._visible = true;' -Context "graphics row $($index + 1) visibility"

        if ($index -eq 2) {
            Assert-ContainsOrdinal -Text $rowText -Token 'this.Names = new Array("Off","On");' -Context 'VSync row values'
            foreach ($token in @('GraphicsVsyncController.ToggleVsync()', 'GraphicsVsyncController.IncrementVsync()', 'GraphicsVsyncController.DecrementVsync()')) {
                Assert-ContainsOrdinal -Text $rowText -Token $token -Context 'VSync row interaction'
            }
        } elseif ($index -eq 14) {
            Assert-ContainsOrdinal -Text $rowText -Token 'GraphicsVsyncController.ApplyChanges()' -Context 'Apply row action'
            foreach ($action in @('Increment', 'Decrement')) {
                if ((Get-VsyncSliceRowActionBody -Text $rowText -FunctionAssignment "this.$action") -ne '') { throw "Apply row action $action must be a no-op." }
            }
        } else {
            Assert-ContainsOrdinal -Text $rowText -Token 'this.Names = new Array("Not active");' -Context "inactive graphics row $($index + 1)"
            Assert-NoOpVsyncSliceRowActions -Text $rowText -Context "inactive graphics row $($index + 1)"
        }
    }
}
```

Change both callers from `Assert-RowShellContract` or the inline all-inert loop to `Assert-VsyncSliceRowContract`.

- [ ] **Step 2: Assert the screen controller and forbidden callback contract**

Immediately after reading the exported screen frame script, require these tokens:

```powershell
foreach ($required in @(
    'class rs.ui.BatmanGraphicsVsyncController',
    'this.InitialVsync = this.NormalizeVsync',
    'this.DraftVsync = this.InitialVsync',
    'this.ApplyWasDispatched = false',
    'FE_SetControlType",4210 + this.DraftVsync',
    'setInterval',
    '},100)',
    'FE_SetControlType",4990 + this.ApplySignalToggle',
    'this.Screen.BlockInput(true)',
    'this.Screen.BlockInput(false)'
)) {
    Assert-ContainsOrdinal -Text $screenText -Token $required -Context 'Options Graphics VSync controller'
}
foreach ($forbidden in @('Helen_GetInt', 'Helen_SetInt', 'Helen_RunCommand', 'Helen_ApplyBatmanGraphicsDraft', 'GraphicsExitPrompt')) {
    Assert-NotContainsOrdinal -Text $screenText -Token $forbidden -Context 'Options Graphics VSync controller'
}
```

Also assert that `CaptureInitialState` is absent so asynchronous dispatch cannot clear the dirty state.

Replace the exported-script forbidden list in both tests with the following list. `ApplyChanges` is now required and must no longer be globally forbidden.

```powershell
@('Helen_', 'GraphicsExitPrompt', 'loadBatmanGraphicsDraftIntoConfig', 'applyBatmanGraphicsDraft', 'Unsaved graphics changes', 'Some changes require a restart')
```

- [ ] **Step 3: Run the retail integration test and observe the intended failure**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanRetailGraphicsOptionsPatch.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder -Configuration Release
```

Expected: FAIL because row 3 still contains `Not active`, row 15 is hidden, and the VSync controller is absent.

### Task 2: Implement the INI-bootstrapped VSync shell controller

**Files:**
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellScriptTemplates.cs`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellBuildPaths.cs`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsAssetBuilder.cs:110-120,222-255,285-305`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/Program.cs:150-166,279`

- [ ] **Step 1: Add the required INI path to the shell build model**

Add `BatmanUserIniPath` to the record after `FfdecPath`, document it, and change the factory signature to:

```csharp
public static GraphicsOptionsShellBuildPaths FromRoot(
    string root,
    string ffdecPath,
    string outputDirectory,
    string batmanUserIniPath)
```

Populate the record with:

```csharp
BatmanUserIniPath: Path.GetFullPath(batmanUserIniPath),
```

Update the class summary so the INI is described as the required VSync bootstrap input rather than saying the shell has no INI dependency.

- [ ] **Step 2: Wire `--ini` through the shell command**

In `RunBuildMainMenuGraphicsShell`, parse the exact default used by the full graphics builder:

```csharp
string batmanUserIniPath = Path.GetFullPath(
    options.GetValue("--ini") ?? BatmanGraphicsIniBootstrapLoader.GetDefaultIniPath());
```

Call:

```csharp
GraphicsOptionsShellBuildPaths paths = GraphicsOptionsShellBuildPaths.FromRoot(
    root,
    ffdecPath,
    outputDirectory,
    batmanUserIniPath);
```

Add `[--ini <BmEngine.ini>]` to the usage line and update the method Doxygen comments to describe the accepted INI argument.

- [ ] **Step 3: Load the snapshot before patching shell scripts**

Change `BuildShell` to fail fast on the required INI and inject the typed snapshot:

```csharp
ValidateShellInputs(paths);
PrepareOutputDirectories(paths.OutputDirectory, paths.TempDirectory);
BatmanGraphicsIniBootstrapSnapshot bootstrapSnapshot =
    BatmanGraphicsIniBootstrapLoader.Load(paths.BatmanUserIniPath);

PatchFrontendShellScripts(paths.FrontendWorkingScriptsPath, bootstrapSnapshot);
ValidateShellPatchedScriptSet(paths.FrontendWorkingScriptsPath);
```

Add `paths.BatmanUserIniPath` to `ValidateShellInputs`, and change `PatchFrontendShellScripts` to accept the snapshot.

- [ ] **Step 4: Generate the VSync controller and fifteen row scripts**

Replace the fixed `ScreenFrame1` constant with a template and a typed factory. The emitted controller must use this state and dispatch implementation:

```actionscript
class rs.ui.BatmanGraphicsVsyncController
{
   var Screen;
   var InitialVsync;
   var DraftVsync;
   var ApplySignalToggle;
   var ApplyInProgress;
   var ApplyWasDispatched;
   var ApplyTimerId;
   function BatmanGraphicsVsyncController(screen,initialVsync)
   {
      this.Screen = screen;
      this.InitialVsync = this.NormalizeVsync(initialVsync);
      this.DraftVsync = this.InitialVsync;
      this.ApplySignalToggle = 0;
      this.ApplyInProgress = false;
      this.ApplyWasDispatched = false;
      this.ApplyTimerId = undefined;
   }
   function NormalizeVsync(value)
   {
      return Number(value) == 0 ? 0 : 1;
   }
   function IsDirty()
   {
      return this.DraftVsync != this.InitialVsync;
   }
   function CanApply()
   {
      return !this.ApplyInProgress && (this.IsDirty() || this.ApplyWasDispatched);
   }
   function SetVsync(value,forward)
   {
      if(this.ApplyInProgress)
      {
         return undefined;
      }
      this.DraftVsync = this.NormalizeVsync(value);
      flash.external.ExternalInterface.call("FE_PlaySoundFromString",forward ? "UI_FrontEndSFX.UI_Forward" : "UI_FrontEndSFX.UI_Back");
      this.RefreshRows();
   }
   function ToggleVsync()
   {
      this.SetVsync(this.DraftVsync == 0 ? 1 : 0,true);
   }
   function IncrementVsync()
   {
      this.SetVsync(this.DraftVsync == 0 ? 1 : 0,true);
   }
   function DecrementVsync()
   {
      this.SetVsync(this.DraftVsync == 0 ? 1 : 0,false);
   }
   function RefreshRows()
   {
      this.Screen.GraphicsRow3.State = this.DraftVsync;
      this.Screen.GraphicsRow3.Update();
      this.Screen.GraphicsRow15.Update();
      this.Screen.ReUpdate();
   }
   function ApplyChanges()
   {
      if(!this.CanApply())
      {
         return undefined;
      }
      this.ApplyInProgress = true;
      this.Screen.BlockInput(true);
      this.RefreshRows();
      flash.external.ExternalInterface.call("FE_SetControlType",4210 + this.DraftVsync,"");
      var controller = this;
      this.ApplyTimerId = setInterval(function()
      {
         clearInterval(controller.ApplyTimerId);
         controller.ApplyTimerId = undefined;
         controller.ApplySignalToggle = controller.ApplySignalToggle == 0 ? 1 : 0;
         flash.external.ExternalInterface.call("FE_SetControlType",4990 + controller.ApplySignalToggle,"");
         controller.ApplyWasDispatched = true;
         controller.ApplyInProgress = false;
         controller.Screen.BlockInput(false);
         controller.RefreshRows();
      },100);
   }
   function Destroy()
   {
      if(this.ApplyTimerId != undefined)
      {
         clearInterval(this.ApplyTimerId);
         this.ApplyTimerId = undefined;
      }
   }
}
```

After `this.Init()` and before `AddItem`, construct it with `__BOOTSTRAP_VSYNC__`. Add all fifteen items in a closed loop: row 1 up to index 14, row 14 down to index 14, and row 15 between indices 13 and 0. Make `CancelScreen()` call `GraphicsVsyncController.Destroy()` before `ReturnFromScreen()`.

Implement `CreateScreenFrame1(BatmanGraphicsIniBootstrapSnapshot snapshot)` with culture-invariant replacement of `__BOOTSTRAP_VSYNC__`.

Replace the static `RowClipActions` field with:

```csharp
public static string[] CreateRowClipActions(BatmanGraphicsIniBootstrapSnapshot bootstrapSnapshot)
```

Return thirteen existing callback-free `Not active` scripts plus:

- row 3: `Names = new Array("Off","On")`, `State = bootstrapSnapshot.Vsync`, visible clickers, and `RunAction`/`Increment`/`Decrement` calls into `_parent.GraphicsVsyncController`
- row 15: label `Apply Changes`, blank value, visible, `RunAction` calling `_parent.GraphicsVsyncController.ApplyChanges()`, alpha 100 only when `CanApply()` returns true, no-op increment/decrement

The first edit enables Apply through `IsDirty()`. After any dispatch, `ApplyWasDispatched` keeps Apply enabled even if the user toggles back to the original bootstrap value; this permits either VSync state to be resent or retried without claiming the previous dispatch persisted.

Keep string escaping in the existing class methods and add substantive Doxygen to every new factory.

- [ ] **Step 5: Pass the snapshot into frame and row generation**

In `PatchFrontendShellScripts`, write:

```csharp
GraphicsOptionsShellScriptTemplates.CreateScreenFrame1(bootstrapSnapshot)
```

Change `WriteGraphicsShellRowClipActions` to accept the snapshot and obtain:

```csharp
string[] rowClipActions = GraphicsOptionsShellScriptTemplates.CreateRowClipActions(bootstrapSnapshot);
```

- [ ] **Step 6: Run the focused retail integration test**

Run the Task 1 command again.

Expected: PASS, including FFDec reopen/export, VSync/Apply row behavior, absence of `Helen_*`, and retained compressed retail output.

- [ ] **Step 7: Build the builder project**

Run:

```powershell
dotnet build .\games\HelenBatmanAA\builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj -c Release
```

Expected: build succeeds with zero errors.

- [ ] **Step 8: Commit the shell implementation**

```powershell
git add games/HelenBatmanAA/scripts/Test-BatmanRetailGraphicsOptionsPatch.ps1 games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1 games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellScriptTemplates.cs games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellBuildPaths.cs games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsAssetBuilder.cs games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/Program.cs
git commit -m "Add VSync interaction to Batman graphics shell"
```

### Task 3: Specify and generate the minimal runtime carrier package

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1:70-105,392-470`
- Modify: `games/HelenBatmanAA/scripts/Rebuild-BatmanGraphicsOptionsExperiment.ps1:1-30,270-380`

- [ ] **Step 1: Change the package test to require hooks and runtime schema**

Add `hooks.json` to `Assert-ExactPackFileSet`, require it in the input list, and replace the zero-runtime assertions with:

```powershell
Assert-ExactProperties -Object $pack -Names @('schemaVersion', 'id', 'name', 'targets', 'config', 'builds') -Context 'pack.json'
if ($pack.name -ne 'Batman Graphics Options VSync') { throw 'pack.json VSync identity drifted.' }
$configKeys = @($pack.config | ForEach-Object { $_.key })
$expectedConfigKeys = @('fullscreen', 'resolutionWidth', 'resolutionHeight', 'vsync', 'msaa', 'detailLevel', 'bloom', 'dynamicShadows', 'motionBlur', 'distortion', 'fogVolumes', 'sphericalHarmonicLighting', 'ambientOcclusion', 'physx', 'stereo', 'applySignal')
if (($configKeys -join '|') -cne ($expectedConfigKeys -join '|')) { throw 'Graphics draft config schema drifted.' }

Assert-ExactProperties -Object $build -Names @('id', 'executable', 'match', 'startupCommands') -Context 'build.json'
if (@($build.startupCommands).Count -ne 1 -or $build.startupCommands[0] -ne 'loadBatmanGraphicsDraftIntoConfig') { throw 'Graphics startup command drifted.' }

if (@($bindings.bindings).Count -ne 0) { throw 'VSync slice must not expose custom external bindings.' }
if (@($commands.commands).Count -ne 2) { throw 'VSync slice must contain exactly load and apply commands.' }
if (($commands.commands.id -join '|') -cne 'loadBatmanGraphicsDraftIntoConfig|applyBatmanGraphicsDraft') { throw 'Graphics command ids drifted.' }

$hooks = Get-Content -LiteralPath $hooksJsonPath -Raw | ConvertFrom-Json
Assert-ExactProperties -Object $hooks -Names @('runtimeSlots', 'stateObservers', 'hooks') -Context 'hooks.json'
if (@($hooks.runtimeSlots).Count -ne 0 -or @($hooks.hooks).Count -ne 0 -or @($hooks.stateObservers).Count -ne 2) { throw 'VSync slice hook counts drifted.' }
$vsyncObserver = @($hooks.stateObservers | Where-Object id -eq 'graphicsObserverVsync')
$applyObserver = @($hooks.stateObservers | Where-Object id -eq 'graphicsObserverApplySignal')
if ($vsyncObserver.Count -ne 1 -or $applyObserver.Count -ne 1) { throw 'Required VSync/apply observers were not unique.' }
if (($vsyncObserver[0].mappings.match -join '|') -cne '4210|4211' -or ($vsyncObserver[0].mappings.value -join '|') -cne '0|1' -or $vsyncObserver[0].targetConfigKey -ne 'vsync') { throw 'VSync observer mapping drifted.' }
if (($applyObserver[0].mappings.match -join '|') -cne '4990|4991' -or $applyObserver[0].command -ne 'applyBatmanGraphicsDraft') { throw 'Apply observer mapping drifted.' }
```

- [ ] **Step 2: Run the package test and observe the intended failure**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsPackage.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder -Configuration Release
```

Expected: FAIL because the current pack has no `hooks.json`, config schema, startup command, or commands.

- [ ] **Step 3: Add deterministic carrier observer factories to the rebuild script**

Add top-level PowerShell functions that return the exact observed stock option structure:

```powershell
function New-GraphicsCarrierChecks {
    return @(
        [ordered]@{ comparison = 'equals-constant'; offset = -16; expectedValue = 50 },
        [ordered]@{ comparison = 'equals-constant'; offset = -12; expectedValue = 100 },
        [ordered]@{ comparison = 'equals-constant'; offset = -8; expectedValue = 100 },
        [ordered]@{ comparison = 'equals-constant'; offset = -4; expectedValue = 100 },
        [ordered]@{ comparison = 'equals-constant'; offset = 4; expectedValue = 1 },
        [ordered]@{ comparison = 'equals-constant'; offset = 8; expectedValue = 0 },
        [ordered]@{ comparison = 'equals-constant'; offset = 12; expectedValue = 1 },
        [ordered]@{ comparison = 'equals-value-at-offset'; offset = 16; compareOffset = 0 },
        [ordered]@{ comparison = 'equals-constant'; offset = 20; expectedValue = 2 },
        [ordered]@{ comparison = 'equals-constant'; offset = 28; expectedValue = 3 },
        [ordered]@{ comparison = 'equals-constant'; offset = 32; expectedValue = 3 }
    )
}

function New-GraphicsCarrierObserver {
    param([string]$Id, [string]$TargetConfigKey, [object[]]$Mappings, [string]$Command = '')
    $observer = [ordered]@{
        id = $Id
        scanStartAddress = '0x2B000000'
        scanEndAddress = '0x30000000'
        scanStride = 4
        valueOffset = 0
        pollIntervalMs = 50
        targetConfigKey = $TargetConfigKey
        checks = @(New-GraphicsCarrierChecks)
        mappings = $Mappings
    }
    if (-not [string]::IsNullOrWhiteSpace($Command)) { $observer.command = $Command }
    return $observer
}
```

- [ ] **Step 4: Generate config, build, commands, and hooks in staging**

Add `$stagedHooksJsonPath` and resolve the exact default INI before invoking the builder:

```powershell
$documentsPath = [Environment]::GetFolderPath([Environment+SpecialFolder]::MyDocuments)
$batmanUserIniPath = Join-Path $documentsPath 'Square Enix\Batman Arkham Asylum GOTY\BmGame\Config\BmEngine.ini'
if (-not (Test-Path -LiteralPath $batmanUserIniPath -PathType Leaf)) { throw "Batman graphics INI was not found: $batmanUserIniPath" }
```

Append `--ini`, `$batmanUserIniPath` to the `build-main-menu-graphics-shell` argument array, then generate:

```powershell
$graphicsConfig = @(
    'fullscreen', 'resolutionWidth', 'resolutionHeight', 'vsync', 'msaa', 'detailLevel',
    'bloom', 'dynamicShadows', 'motionBlur', 'distortion', 'fogVolumes',
    'sphericalHarmonicLighting', 'ambientOcclusion', 'physx', 'stereo', 'applySignal'
) | ForEach-Object { [ordered]@{ key = $_; type = 'int'; defaultValue = 0 } }

$pack = [ordered]@{
    schemaVersion = 1
    id = 'batman-aa-graphics-options'
    name = 'Batman Graphics Options VSync'
    targets = @([ordered]@{ gameId = 'batman-arkham-asylum'; executables = @($buildMatch.Executable) })
    config = @($graphicsConfig)
    builds = @($buildMatch.BuildId)
}
$build = [ordered]@{
    id = $buildMatch.BuildId
    executable = $buildMatch.Executable
    match = [ordered]@{ fileSize = $buildMatch.FileSize; sha256 = $buildMatch.Sha256 }
    startupCommands = @('loadBatmanGraphicsDraftIntoConfig')
}
$commands = [ordered]@{ commands = @(
    [ordered]@{ id = 'loadBatmanGraphicsDraftIntoConfig'; name = 'Load Batman Graphics Draft Into Config'; steps = @([ordered]@{ kind = 'load-batman-graphics-draft-into-config' }) },
    [ordered]@{ id = 'applyBatmanGraphicsDraft'; name = 'Apply Batman Graphics Draft'; steps = @([ordered]@{ kind = 'apply-batman-graphics-config' }, [ordered]@{ kind = 'load-batman-graphics-draft-into-config' }) }
) }
$hooks = [ordered]@{
    runtimeSlots = @()
    stateObservers = @(
        (New-GraphicsCarrierObserver -Id 'graphicsObserverVsync' -TargetConfigKey 'vsync' -Mappings @([ordered]@{ match = 4210; value = 0 }, [ordered]@{ match = 4211; value = 1 })),
        (New-GraphicsCarrierObserver -Id 'graphicsObserverApplySignal' -TargetConfigKey 'applySignal' -Command 'applyBatmanGraphicsDraft' -Mappings @([ordered]@{ match = 4990; value = 0 }, [ordered]@{ match = 4991; value = 1 }))
    )
    hooks = @()
}
```

Write empty `bindings`, both commands, and hooks using `Write-Utf8TextFile`. Do not copy any historical manifest into the staging tree.

- [ ] **Step 5: Rebuild atomically from the verified retail base**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Rebuild-BatmanGraphicsOptionsExperiment.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder -Configuration Release
```

Expected: the script builds the C# tools, creates a fresh GFX, patches verified retail `Frontend.umap`, creates a new HGDL delta, validates staging, atomically publishes the pack and target, and prints their SHA-256 hashes.

- [ ] **Step 6: Run package and retail verification**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsPackage.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder -Configuration Release
powershell -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanRetailGraphicsOptionsPatch.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder -Configuration Release
```

Expected: both print `PASS`.

- [ ] **Step 7: Commit runtime manifests and generated artifacts**

Review `git status --short` and stage only the graphics source, tests, pack, and generated frontend target. Preserve the unrelated project/test changes and `batma/` directory.

```powershell
git add games/HelenBatmanAA/scripts/Rebuild-BatmanGraphicsOptionsExperiment.ps1 games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1 games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options games/HelenBatmanAA/builder/generated/graphics-options-experiment/Frontend-graphics-options.umap
git commit -m "Add Batman VSync apply carrier"
```

### Task 4: Verify, deploy, and perform the live checkpoints

**Files:**
- Read: `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/**`
- Read: `games/HelenBatmanAA/builder/generated/graphics-options-experiment/Frontend-graphics-options.umap`
- Execute: `games/HelenBatmanAA/scripts/Deploy-BatmanGraphicsOptionsExperiment.ps1`
- Inspect: `C:/Program Files (x86)/Steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/helengamehook/logs/HelenGameHook.log`
- Inspect: the resolved user `BmEngine.ini`

- [ ] **Step 1: Run final source and package verification from a clean graphics diff**

Run:

```powershell
git diff --check HEAD~2..HEAD
dotnet build .\games\HelenBatmanAA\builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj -c Release
powershell -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsPackage.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder -Configuration Release
```

Expected: no whitespace errors, build succeeds, package test prints `PASS`.

- [ ] **Step 2: Record pre-deployment identities and current VSync**

Run read-only hashes over every graphics pack file and the generated target. Resolve the default INI path through `[Environment]::GetFolderPath('MyDocuments')`, then record the exact `[SystemSettings] UseVsync` line and two unrelated sampled lines such as `Bloom` and `DynamicShadows`.

Expected: all paths exist and exactly one value for each sampled key is recorded.

- [ ] **Step 3: Deploy only the subtitle and graphics packs through the official script**

Run with elevated filesystem approval:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Deploy-BatmanGraphicsOptionsExperiment.ps1 -GameBin 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries' -BuilderRoot .\games\HelenBatmanAA\builder -Configuration Release
```

Expected: deployment validates the game build, installs the current graphics pack alongside the subtitle pack, writes pack order `batman-aa-subtitles` then `batman-aa-graphics-options`, and verifies installed hashes.

- [ ] **Step 4: Ask for live menu interaction verification**

Have the user start Batman and confirm:

1. `Click to Start` still reaches the main menu.
2. `Graphics Options` opens.
3. VSync shows `Off` or `On`.
4. Left/right and Enter toggle it.
5. `Apply Changes` enables; the other rows remain inactive.
6. Back returns normally.

- [ ] **Step 5: Perform persistence verification after the user applies once**

After the game is closed, inspect `HelenGameHook.log` for observer registration, `graphicsObserverVsync`, `graphicsObserverApplySignal`, and successful `applyBatmanGraphicsDraft`. Compare `UseVsync`, `Bloom`, and `DynamicShadows` against the pre-test snapshot.

Expected: `UseVsync` matches the selected menu state, the two unrelated sampled values are byte-for-byte unchanged, and the log records the expected `4210/4211` mapping followed by `4990/4991` apply dispatch.

- [ ] **Step 6: Report the exact committed and deployed result**

Report the source commit IDs, generated target/delta sizes and SHA-256 hashes, installed pack verification result, live menu result, and persistence result. Mention that Apply intentionally stays enabled because this slice has no trustworthy native-to-GFx acknowledgement.
