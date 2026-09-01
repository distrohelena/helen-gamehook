# Batman Graphics Options Shell Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Generate, validate, package, and install a callback-free Batman `Graphics Options` shell that opens from the frontend Options menu and returns safely while the working subtitle pack remains enabled.

**Architecture:** Add a separate shell ActionScript contract and an explicit `build-main-menu-graphics-shell` command while leaving the full experimental editor dormant in source. Rebuild the graphics pack from the trusted retail `Frontend.umap` in a clean temporary output directory, emit no configuration or runtime behavior, and join it with subtitles against the installed `4DAC…` executable build.

**Tech Stack:** C#/.NET 8, ActionScript 2 through FFDec, UE3 package patching/compression, hgdelta virtual files, PowerShell, C++20, Visual Studio 2022/MSBuild.

---

## File structure and constraints

Execute on `main`, as requested. Preserve the unrelated working-tree entries in `HelenRuntime/HelenRuntime.vcxproj`, `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`, `tests/HelenRuntime.Tests/TestMain.cpp`, and `batma/`.

Create:

- `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellBuildPaths.cs`: valid shell path model with no INI field.
- `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellScriptTemplates.cs`: inert menu, screen, Back, and fixed-row scripts.
- `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellContract.ps1`: fast shell source/emission contract.
- `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellDeployment.ps1`: deployment coexistence contract.

Modify:

- `Program.cs`, `GraphicsOptionsAssetBuilder.cs`, and `GraphicsOptionsXmlPatcher.cs`: explicit shell pipeline that creates sprite 600 but no prompt sprite 601.
- `Get-BatmanSteamBuildMatch.ps1` and `Rebuild-BatmanGraphicsOptionsExperiment.ps1`: correct executable identity, clean output, base verification, and inert manifests.
- Graphics package/layout/retail verification scripts: require navigation and prohibit editor behavior.
- `PackRepositoryTests.cpp` and `helengamehook/config/packs.json`: prove subtitles plus graphics merge.
- `Deploy-BatmanGraphicsOptionsExperiment.ps1`: rebuild, preserve subtitles, and atomically install the active pack config.
- Checked-in `batman-aa-graphics-options` metadata and delta: fresh generated output only.

Do not modify the subtitle pack contents.

### Task 1: Define the callback-free ActionScript shell

**Files:**

- Create: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellContract.ps1`
- Create: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellScriptTemplates.cs`

- [ ] **Step 1: Write the failing shell contract**

Create a PowerShell test that resolves `BatmanRoot`, fails when the template is absent, then scans it for these exact contracts:

```powershell
$RequiredTokens = @(
    'Object.registerClass("ScreenOptionsGraphics",rs.ui.Screen)',
    'flash.external.ExternalInterface.call("FE_SetActiveScreenName","Options Menu")',
    'this.ButtonName = "Graphics Options"',
    '_parent.GotoScreen("OptionsGraphics")',
    'flash.external.ExternalInterface.call("FE_SetActiveScreenName","Graphics Options")',
    'function CancelScreen()',
    'ReturnFromScreen();',
    'this.BackScreen = "OptionsMenu"',
    'this.Title.text = "Graphics Options"',
    'this.RunAction = function()',
    'this.LeftClicker._visible = false;',
    'this.RightClicker._visible = false;'
)
$ForbiddenTokens = @('Helen_', 'ApplyChanges', 'GraphicsExitPrompt', 'Unsaved', 'RestartRequired', 'BmEngine.ini')
```

For each required token, throw when `IndexOf(..., Ordinal) -lt 0`; for each forbidden token, throw when it is `-ge 0`; print `PASS` last.

- [ ] **Step 2: Verify the test fails for the missing template**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellContract.ps1
```

Expected: FAIL with `Graphics-options shell template was not found`.

- [ ] **Step 3: Implement `GraphicsOptionsShellScriptTemplates`**

Create one documented static class. Every field and method receives substantive Doxygen. Provide:

```csharp
public const string ScreenRegistration = """
Object.registerClass("ScreenOptionsGraphics",rs.ui.Screen);
""";

public const string OptionsMenuFrame1 = """
flash.external.ExternalInterface.call("FE_SetActiveScreenName","Options Menu");
this.BackScreen = "Main*";
this.BackScreenIndex = 5;
this.State = 0;
this.Init();
this.AddItem(Game,4,1,-1,-1);
this.AddItem(Graphics,0,2,-1,-1);
this.AddItem(Audio,1,3,-1,-1);
this.AddItem(Controls,2,4,-1,-1);
this.AddItem(Credits,3,0,-1,-1);
_rotation = -2;
""";

public const string OptionsMenuGraphicsButtonClipAction = """
onClipEvent(load){
   function RunAction(){ _parent.GotoScreen("OptionsGraphics"); }
   function Update(){ Label.Text.text = this.ButtonName; }
   this.ButtonName = "Graphics Options";
   this.Update();
}
""";

public const string ScreenFrame1 = """
function CancelScreen(){ ReturnFromScreen(); }
flash.external.ExternalInterface.call("FE_SetActiveScreenName","Graphics Options");
this.BackScreen = "OptionsMenu";
this.BackScreenIndex = 1;
this.FocusIndex = 0;
this.Flags = this.FLAG_OPTIONS;
this.Init();
_root.TriggerEvent("Options");
if(this.Title != undefined){ this.Title.text = "Graphics Options"; }
this.AddItem(GraphicsRow1,13,1,-1,-1);
this.AddItem(GraphicsRow2,0,2,-1,-1);
this.AddItem(GraphicsRow3,1,3,-1,-1);
this.AddItem(GraphicsRow4,2,4,-1,-1);
this.AddItem(GraphicsRow5,3,5,-1,-1);
this.AddItem(GraphicsRow6,4,6,-1,-1);
this.AddItem(GraphicsRow7,5,7,-1,-1);
this.AddItem(GraphicsRow8,6,8,-1,-1);
this.AddItem(GraphicsRow9,7,9,-1,-1);
this.AddItem(GraphicsRow10,8,10,-1,-1);
this.AddItem(GraphicsRow11,9,11,-1,-1);
this.AddItem(GraphicsRow12,10,12,-1,-1);
this.AddItem(GraphicsRow13,11,13,-1,-1);
this.AddItem(GraphicsRow14,12,0,-1,-1);
GraphicsRow15._visible = false;
_rotation = -2;
""";

public const string ScreenFrame15 = "stop();";
```

Expose `RowClipActions` as 15 generated strings: fourteen labels (`Fullscreen`, `Resolution`, `VSync`, `MSAA`, `Detail Level`, `Bloom`, `Dynamic Shadows`, `Motion Blur`, `Distortion`, `Fog Volumes`, `Spherical Harmonic Lighting`, `Ambient Occlusion`, `PhysX`, `Stereo 3D`) with value `Not active`, followed by one hidden row. `CreateRowClipAction(label, value, visible)` must write the label/value, hide both clickers, define no-op `RunAction`, `Increment`, and `Decrement`, and set `_visible` from the argument. Escape backslashes and quotes in a separate `EscapeActionScriptString` class method; do not create a local helper.

- [ ] **Step 4: Run the contract and compile**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellContract.ps1
dotnet build .\games\HelenBatmanAA\builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj -c Debug
```

Expected: `PASS` and `Build succeeded`.

- [ ] **Step 5: Commit**

```powershell
git add -- games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellContract.ps1 games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellScriptTemplates.cs
git commit -m "Add Batman graphics options shell scripts"
```

### Task 2: Add an explicit shell builder path

**Files:**

- Create: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellBuildPaths.cs`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/Program.cs`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsAssetBuilder.cs`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsXmlPatcher.cs`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellContract.ps1`

- [ ] **Step 1: Extend the test before implementation**

Require these exact source tokens:

```powershell
$RequiredProgramTokens = @(
    '"build-main-menu-graphics-shell" => RunBuildMainMenuGraphicsShell(tail)',
    'GraphicsOptionsShellBuildPaths.FromRoot(root, ffdecPath, outputDirectory)',
    'GraphicsOptionsAssetBuilder.BuildShell(paths)'
)
$RequiredBuilderTokens = @(
    'public static void BuildShell(GraphicsOptionsShellBuildPaths paths)',
    'PatchFrontendShellScripts(paths.FrontendWorkingScriptsPath)',
    'GraphicsOptionsXmlPatcher.PatchShell(paths.FrontendXmlPath, paths.FrontendPatchedXmlPath)',
    'GraphicsOptionsShellScriptTemplates.RowClipActions'
)
$RequiredXmlTokens = @(
    'public static void PatchShell(string inputXmlPath, string outputXmlPath)',
    'AppendGraphicsShellSpriteAndExport(tags, optionsGamePcSprite)'
)
```

Run the test and expect failure on the missing command.

- [ ] **Step 2: Create the shell path record**

Model the same root, output, temp, frontend XML/GFX/scripts, FFDec, staged scripts, patched XML, structural GFX, and final GFX members as `GraphicsOptionsBuildPaths`, but omit `BatmanUserIniPath`. `FromRoot` must emit `MainV2-graphics-options-shell.xml`, `MainV2-graphics-options-shell-structural.gfx`, and `MainV2-graphics-options.gfx`. Document the record, every positional member, and `FromRoot`.

- [ ] **Step 3: Add `build-main-menu-graphics-shell`**

In `Program.cs`, add the dispatch case, help text, and this method with full Doxygen:

```csharp
private static int RunBuildMainMenuGraphicsShell(string[] args)
{
    var options = new ArgumentReader(args);
    string root = Path.GetFullPath(options.RequireValue("--root"));
    string outputDirectory = Path.GetFullPath(options.GetValue("--output-dir") ?? Path.Combine(root, "generated", "main-menu-graphics-shell"));
    string ffdecPath = Path.GetFullPath(options.GetValue("--ffdec") ?? Path.Combine(root, "extracted", "ffdec", "ffdec-cli.exe"));
    options.ThrowIfAnyUnknown();
    GraphicsOptionsShellBuildPaths paths = GraphicsOptionsShellBuildPaths.FromRoot(root, ffdecPath, outputDirectory);
    GraphicsOptionsAssetBuilder.BuildShell(paths);
    Console.WriteLine($"Built shell Frontend: {paths.FrontendOutputGfxPath}");
    return 0;
}
```

There is deliberately no `--ini` option.

- [ ] **Step 4: Stage only shell scripts**

Add `BuildShell`, `ValidateShellInputs`, `PatchFrontendShellScripts`, and `WriteGraphicsShellRowClipActions` to `GraphicsOptionsAssetBuilder`. Reuse its existing copy/process/write helpers. Write only the shell registration, options-menu frame/button, sprite-600 frame 1/frame 15, and 15 fixed row clip actions. Do not write any `GraphicsExitPrompt` file or call `BatmanGraphicsIniBootstrapLoader`.

- [ ] **Step 5: Patch only sprite 600**

Add `PatchShell` to `GraphicsOptionsXmlPatcher`. It loads the XML, reserves/remaps only id 600, patches the Options menu, and calls:

```csharp
private static void AppendGraphicsShellSpriteAndExport(XmlElement tags, XmlElement optionsGamePcSprite)
{
    XmlElement graphicsSprite = CloneSpriteWithNewId(optionsGamePcSprite, ScreenOptionsGraphicsSpriteId);
    PatchGraphicsScreenSprite(graphicsSprite);
    tags.AppendChild(graphicsSprite);
    tags.AppendChild(CreateExportAssetsTag(tags, ScreenOptionsGraphicsSpriteId, "ScreenOptionsGraphics"));
    tags.AppendChild(CloneDoInitActionTagForSprite(tags, ScreenOptionsGamePcSpriteId, ScreenOptionsGraphicsSpriteId));
}
```

Use the existing UTF-8 XML writer. Do not find, clone, export, remap, or register id 601.

- [ ] **Step 6: Verify command behavior**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellContract.ps1
dotnet build .\games\HelenBatmanAA\builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj -c Debug
dotnet run --project .\games\HelenBatmanAA\builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj -c Debug -- build-main-menu-graphics-shell --root .\games\HelenBatmanAA\builder --ini fake.ini
```

Expected: contract `PASS`, build succeeds, and the last command rejects `--ini` as unknown.

- [ ] **Step 7: Commit**

```powershell
git add -- games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellContract.ps1
git commit -m "Build Batman graphics options shell"
```

### Task 3: Generate an inert pack from verified retail inputs

**Files:**

- Modify: `games/HelenBatmanAA/scripts/Get-BatmanSteamBuildMatch.ps1`
- Modify: `games/HelenBatmanAA/scripts/Rebuild-BatmanGraphicsOptionsExperiment.ps1`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanRetailGraphicsOptionsPatch.ps1`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsLayout.ps1`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsRetailBaseContract.ps1`
- Modify: `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/**`

- [ ] **Step 1: Make package verification demand the shell first**

Replace editor assertions in the package, retail, and layout tests with:

```powershell
if (@($PackManifest.config).Count -ne 0) { throw 'Graphics shell must not declare config.' }
if (@($PackManifest.features).Count -ne 0) { throw 'Graphics shell must not declare features.' }
if (@($BuildManifest.startupCommands).Count -ne 0) { throw 'Graphics shell must not declare startup commands.' }
if ($BuildManifest.enableD3d9TextureReplacementHooks -eq $true) { throw 'Graphics shell must not enable texture hooks.' }
if (@($BindingsManifest.bindings).Count -ne 0) { throw 'Graphics shell must not declare bindings.' }
if (@($CommandsManifest.commands).Count -ne 0) { throw 'Graphics shell must not declare commands.' }
```

After FFDec export, require `Graphics Options`, `GotoScreen("OptionsGraphics")`, `CancelScreen`, `ReturnFromScreen`, and `Not active`. Search every exported `.as` file and reject `Helen_`, `ApplyChanges`, `GraphicsExitPrompt`, `loadBatmanGraphicsDraftIntoConfig`, `applyBatmanGraphicsDraft`, `Unsaved graphics changes`, and `Some changes require a restart`. Require exactly one `DefineSprite_600_ScreenOptionsGraphics` and no sprite-601 prompt directory. Keep compression, package reopening, and `Assert-HgdeltaVirtualFileContract` coverage.

- [ ] **Step 2: Run the package test and see the current editor fail**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsPackage.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder
```

Expected: FAIL on existing config/startup/binding declarations.

- [ ] **Step 3: Align the build identity with the installed working executable**

Set `Get-BatmanSteamBuildMatch.ps1` to file size `38758728` and SHA-256 `4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028`. This is the actual installed hash and the subtitle pack's build identity.

- [ ] **Step 4: Rework the rebuild script**

Before building, require retail `Frontend.umap` size `2988548` and SHA-256 `271916B888F83374122AF0FCCC5C685804F4C8286A92A772CD71E4F48A00F2CC`. Create `HelenBatmanGraphicsShell-<guid>` beneath the system temp directory and place prototype GFX, patch manifest, and generated target there. Invoke `build-main-menu-graphics-shell`, patch only the verified base, build the delta into a freshly recreated pack build directory, copy the verified current-run target to `builder/generated/graphics-options-experiment/Frontend-graphics-options.umap`, and remove the temp directory in `finally`.

Emit exactly:

```powershell
$PackJsonObject = [ordered]@{
    schemaVersion = 1
    id = 'batman-aa-graphics-options'
    name = 'Batman Graphics Options Shell'
    targets = @([ordered]@{ gameId = 'batman-arkham-asylum'; executables = @($BuildMatch.Executable) })
    builds = @($BuildMatch.BuildId)
}
$BuildJsonObject = [ordered]@{
    id = $BuildMatch.BuildId
    executable = $BuildMatch.Executable
    match = [ordered]@{ fileSize = $BuildMatch.FileSize; sha256 = $BuildMatch.Sha256 }
}
$BindingsObject = [ordered]@{ bindings = @() }
$CommandsObject = [ordered]@{ commands = @() }
```

Keep one delta-on-read `Frontend.umap` entry in `files.json`.

- [ ] **Step 5: Rebuild and run all asset verifiers**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Rebuild-BatmanGraphicsOptionsExperiment.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder -Configuration Release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellContract.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsPackage.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanRetailGraphicsOptionsPatch.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsLayout.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsRetailBaseContract.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder
```

Expected: every verifier prints `PASS`; delta reconstruction matches the manifest target byte-for-byte.

- [ ] **Step 6: Commit**

```powershell
git add -- games/HelenBatmanAA/scripts/Get-BatmanSteamBuildMatch.ps1 games/HelenBatmanAA/scripts/Rebuild-BatmanGraphicsOptionsExperiment.ps1 games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1 games/HelenBatmanAA/scripts/Test-BatmanRetailGraphicsOptionsPatch.ps1 games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsLayout.ps1 games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsRetailBaseContract.ps1 games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options
git commit -m "Package Batman graphics options shell"
```

### Task 4: Prove subtitle/graphics multi-pack compatibility

**Files:**

- Modify: `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`
- Modify: `games/HelenBatmanAA/helengamehook/config/packs.json`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanSkipVideosPack.ps1`

- [ ] **Step 1: Change C++ expectations before config**

Load the checked-in graphics pack with `4dac…`. Assert one virtual file and empty startup commands, hooks, texture replacements, commands, bindings, runtime slots, observers, config entries, and features. Assert texture hooks are disabled. Keep the frontend base hash/size assertions; validate generated target size is positive and target hash length is 64 rather than hardcoding newly generated values.

Change the explicit pack-set input to:

```cpp
{ "batman-aa-subtitles", "batman-aa-graphics-options" }
```

Assert two packs, two virtual files, no missing paths, one subtitle startup command, and the existing subtitle counts: one hook, one texture replacement, two commands, three bindings, one observer, and one runtime slot.

- [ ] **Step 2: Run C++ tests and verify the pre-alignment failure**

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=Win32 /m:1 /v:minimal
& .\bin\Win32\Debug\HelenRuntime.Tests.exe
```

Expected: the new checked-in shell or pack-set assertion fails until config/metadata are aligned.

- [ ] **Step 3: Activate subtitles plus graphics only**

Write:

```json
{
  "enabledPacksByExecutable": {
    "ShippingPC-BmGame.exe": [
      "batman-aa-subtitles",
      "batman-aa-graphics-options"
    ]
  }
}
```

Update `Test-BatmanSkipVideosPack.ps1` to keep validating the skip pack's five paths and `4DAC…` identity while asserting it is not in this checkpoint's active list.

- [ ] **Step 4: Verify and commit**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanSkipVideosPack.ps1
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=Win32 /m:1 /v:minimal
& .\bin\Win32\Debug\HelenRuntime.Tests.exe
git add -- tests/HelenRuntime.Tests/PackRepositoryTests.cpp games/HelenBatmanAA/helengamehook/config/packs.json games/HelenBatmanAA/scripts/Test-BatmanSkipVideosPack.ps1
git commit -m "Enable Batman subtitle and graphics shell packs"
```

Expected: PowerShell `PASS`; C++ runner exits 0; commit succeeds.

### Task 5: Make deployment preserve subtitles

**Files:**

- Create: `games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellDeployment.ps1`
- Modify: `games/HelenBatmanAA/scripts/Deploy-BatmanGraphicsOptionsExperiment.ps1`

- [ ] **Step 1: Write a failing deploy-source contract**

Require `$RebuildScriptPath`, `& $RebuildScriptPath`, `$ConfigSourcePath`, `$ConfigDestinationPath`, `$ConfigStagingPath`, `$ConfigBackupPath`, both pack ids, and atomic `Move-Item` activation. Reject `$ConflictingPackDestination` and any removal of the subtitle pack. Parse repository config and require exactly subtitles then graphics.

- [ ] **Step 2: Run it and confirm failure**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellDeployment.ps1
```

Expected: FAIL because the current deploy removes subtitles and does not rebuild/install config.

- [ ] **Step 3: Rebuild and transact the pack plus config**

Invoke `Rebuild-BatmanGraphicsOptionsExperiment.ps1` before source verification. Remove all conflicting-pack logic. Stage the graphics directory and repository `packs.json` separately; backup any installed graphics pack and config; activate both; verify the installed subtitle directory still exists and installed config contains exactly subtitles then graphics; only then delete backups. On failure, restore both prior graphics pack and prior config. Keep installed-base hash verification and clear logs only after success.

- [ ] **Step 4: Verify and commit**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellDeployment.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsDeployBaseCompatibility.ps1 -BatmanRoot .\games\HelenBatmanAA
git add -- games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsShellDeployment.ps1 games/HelenBatmanAA/scripts/Deploy-BatmanGraphicsOptionsExperiment.ps1
git commit -m "Deploy Batman graphics shell beside subtitles"
```

Expected: both tests print `PASS`; commit succeeds.

### Task 6: Final verification, installation, and live checkpoint

**Files:**

- Verify Tasks 1–5.
- Install to `C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries`.

- [ ] **Step 1: Run fresh rebuild plus the complete suite**

Run the complete command list rather than trusting prior generated output:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Rebuild-BatmanGraphicsOptionsExperiment.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder -Configuration Release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellContract.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsPackage.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanRetailGraphicsOptionsPatch.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsLayout.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsRetailBaseContract.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsShellDeployment.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanSkipVideosPack.ps1
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\HelenGameHook.sln /t:Build /p:Configuration=Release /p:Platform=Win32 /m:1 /v:minimal
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=Win32 /m:1 /v:minimal
& .\bin\Win32\Debug\HelenRuntime.Tests.exe
git diff --check
git status --short
```

Expected: all scripts `PASS`, builds succeed, test runner exits 0, and no unintended files are staged.

- [ ] **Step 2: Commit final generated adjustments if present**

Inspect `git status --short`. If the fresh rebuild changed tracked output, run:

```powershell
git add -- games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder games/HelenBatmanAA/scripts games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options games/HelenBatmanAA/helengamehook/config/packs.json tests/HelenRuntime.Tests/PackRepositoryTests.cpp
git commit -m "Verify Batman graphics options shell"
```

Do not create an empty commit and do not stage the pre-existing unrelated files.

- [ ] **Step 3: Install with Program Files approval**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Deploy-BatmanGraphicsOptionsExperiment.ps1 -GameBin 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries' -BuilderRoot .\games\HelenBatmanAA\builder -Configuration Release
```

Expected: `DEPLOYED` after rebuild, verification, and transactional activation.

- [ ] **Step 4: Verify installed provenance**

Require executable SHA-256 `4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028`, repository/installed graphics delta hashes equal, empty installed bindings/commands, subtitle directory present, and installed config exactly subtitles then graphics. Skip Videos remains inactive.

- [ ] **Step 5: Pause for live test**

Ask the user to open Options, confirm `Graphics Options`, open it, confirm fixed `Not active` rows, press Back, confirm Options remains responsive, then repeat open/Back once. Do not implement editable settings or Apply in this checkpoint.

- [ ] **Step 6: Inspect logs after exit**

Read the newest logs under `Binaries/helengamehook/logs`. Confirm both pack ids loaded, `Frontend.umap` reconstructed/served, subtitle `BmGame.u` remained active, and there was no delta, hash, or file-serving error. If the menu failed, preserve the installed artifact and diagnose the first runtime failure before changing code.

## Self-review

- Spec coverage: Tasks 1–2 cover menu, shell, inert rows, Back, and prompt omission; Task 3 covers clean provenance, base/target/delta validation, package reopening, and callback absence; Task 4 covers executable identity and subtitle coexistence; Task 5 covers safe deployment; Task 6 covers automated/live verification.
- Placeholder scan: every edit step names concrete behavior, commands, and failure expectations; generated hashes are validated from the emitted manifest.
- Type consistency: `GraphicsOptionsShellBuildPaths`, `BuildShell`, `PatchShell`, and `RowClipActions` use consistent names throughout.
