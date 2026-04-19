# Batman Main Menu Audio Subtitle Size Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a dedicated `Subtitle Size` row to Batman's main-menu `Options Audio` screen, ship it through the existing Batman delta-pack workflow, and keep the front end stable through the press-start and save/profile flows.

**Architecture:** Rebuild the front-end `MainV2` movie the same way the pause movie is now handled: prepare a reproducible extracted workspace from a trusted decompressed `Startup_INT.upk`, patch the `ScreenOptionsAudio` XML structurally, import a dedicated front-end `ListItem` script with disabled-row behavior, then patch the trusted front-end package and ship it as a second `delta-on-read` virtual file alongside `BmGame.u`. Verification stays layered: workspace prep, generated-movie structure, shipped-pack manifest shape, runtime pack loading, and live front-end smoke flow.

**Tech Stack:** C#/.NET `SubtitleSizeModBuilder`, PowerShell Batman scripts, FFDec CLI, `BmGameGfxPatcher`, existing Helen runtime pack format/tests, Win32 Batman deployment flow

---

## File Map

### New files

- `docs/superpowers/plans/2026-03-29-batman-main-menu-audio-subtitle-size.md`
  - This implementation plan.
- `games/HelenBatmanAA/scripts/Test-BatmanMainMenuAudioLayout.ps1`
  - Verifies the generated `MainV2` movie structure, front-end row order, and disabled-row script behavior.

### Modified builder source

- `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/MainMenuXmlPatcher.cs`
  - Inserts the dedicated `SubtitleSize` row at the bottom of `ScreenOptionsAudio` and keeps all depth-61 placements pinned to the same Y coordinate.
- `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/ScriptTemplates.cs`
  - Replaces the old front-end four-state subtitles hack with a dedicated `SubtitleSize` row and disabled/greyed behavior.
- `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/SubtitleSizeAssetBuilder.cs`
  - Rebuilds a structural front-end `MainV2` movie from patched XML before importing scripts and writes the front-end manifest.
- `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/Program.cs`
  - Adds a dedicated `build-main-menu-audio` entry point that emits only the front-end build outputs needed by the Batman pack pipeline.

### Modified Batman scripts and docs

- `games/HelenBatmanAA/scripts/Prepare-BatmanBuilderWorkspace.ps1`
  - Accepts a trusted decompressed `Startup_INT.upk`, extracts `MainMenu.MainV2`, and writes the front-end extracted workspace under `builder/extracted/frontend/mainv2`.
- `games/HelenBatmanAA/scripts/Test-BatmanBuilderWorkspacePreparation.ps1`
  - Fails if the front-end builder base or extracted `MainV2` workspace is missing.
- `games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1`
  - Builds front-end `MainV2`, patches the trusted front-end base package, produces a second `.hgdelta`, and writes a two-entry `files.json`.
- `games/HelenBatmanAA/scripts/Test-BatmanKnownGoodGameplayPackage.ps1`
  - Verifies the shipped Batman pack contains both delta-backed virtual files and that the manifest matches the generated package artifacts.
- `games/HelenBatmanAA/scripts/Deploy-Batman.ps1`
  - Validates, stages, and deploys both Batman delta assets instead of assuming a single-entry pack.
- `games/HelenBatmanAA/README.md`
  - Documents the new front-end prep input and the required live smoke test path.

### Modified runtime tests

- `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`
  - Expects the checked-in Batman pack to contain both the gameplay and front-end virtual file entries.

### Generated/updated shipped outputs

- `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/files.json`
  - Two virtual files: gameplay `BmGame.u` and front-end `Startup_INT.upk`.
- `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/assets/deltas/BmGame-subtitle-signal.hgdelta`
  - Existing gameplay delta rebuilt if gameplay package bytes move.
- `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/assets/deltas/Startup_INT-main-menu-subtitle-size.hgdelta`
  - New front-end delta for the trusted `Startup_INT.upk` base.

---

## Task 1: Make Front-End Builder Prep Reproducible

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Prepare-BatmanBuilderWorkspace.ps1`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanBuilderWorkspacePreparation.ps1`

- [ ] **Step 1: Extend the prep verifier so it fails on missing front-end workspace files**

Add these variables and checks to `games/HelenBatmanAA/scripts/Test-BatmanBuilderWorkspacePreparation.ps1`:

```powershell
$FrontendBasePackagePath = Join-Path $BatmanRoot 'builder\extracted\frontend\startup-int-unpacked\Startup_INT.upk'
$PreparedFrontendBasePackagePath = Join-Path $BuilderRoot 'extracted\frontend\startup-int-unpacked\Startup_INT.upk'
$PreparedFrontendGfxPath = Join-Path $BuilderRoot 'extracted\frontend\mainv2\frontend-mainv2.gfx'
$PreparedFrontendXmlPath = Join-Path $BuilderRoot 'extracted\frontend\mainv2\frontend-mainv2.xml'
$PreparedFrontendScriptsRoot = Join-Path $BuilderRoot 'extracted\frontend\mainv2\frontend-mainv2-export\scripts'
$FrontendAudioActionPath = Join-Path $PreparedFrontendScriptsRoot 'DefineSprite_359_ScreenOptionsAudio\frame_1\DoAction.as'

& powershell -ExecutionPolicy Bypass -File $PrepareScriptPath `
    -BatmanRoot $BatmanRoot `
    -BuilderRoot $BuilderRoot `
    -BasePackagePath $BasePackagePath `
    -FrontendBasePackagePath $FrontendBasePackagePath `
    -FfdecCliPath $FfdecCliPath `
    -Configuration $Configuration

$requiredPaths = @(
    $PreparedFrontendBasePackagePath,
    $PreparedFrontendGfxPath,
    $PreparedFrontendXmlPath,
    $PreparedFrontendScriptsRoot,
    $FrontendAudioActionPath
)

foreach ($requiredPath in $requiredPaths) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Prepared builder workspace is missing expected frontend path: $requiredPath"
    }
}

$frontendXmlContents = Get-Content -LiteralPath $PreparedFrontendXmlPath -Raw
if ($frontendXmlContents.IndexOf('swfName="MainV2"', [System.StringComparison]::Ordinal) -lt 0) {
    throw 'Prepared frontend MainV2.xml is missing the MainV2 movie marker.'
}

$frontendActionContents = Get-Content -LiteralPath $FrontendAudioActionPath -Raw
if ($frontendActionContents.IndexOf('Options Audio', [System.StringComparison]::Ordinal) -lt 0) {
    throw 'Prepared frontend FFDec export does not contain the main-menu audio action script.'
}
```

- [ ] **Step 2: Run the prep verifier to confirm it fails before the prep script is updated**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanBuilderWorkspacePreparation.ps1 -BatmanRoot .\games\HelenBatmanAA -Configuration Debug
```

Expected:

- FAIL because `Prepare-BatmanBuilderWorkspace.ps1` does not accept `-FrontendBasePackagePath` yet, or
- FAIL because the temp builder root does not contain `builder\extracted\frontend\startup-int-unpacked\Startup_INT.upk` and the extracted `MainV2` files

- [ ] **Step 3: Update the prep script to copy the trusted front-end base and extract `MainMenu.MainV2`**

Modify `games/HelenBatmanAA/scripts/Prepare-BatmanBuilderWorkspace.ps1` so it accepts a required `-FrontendBasePackagePath` and prepares these front-end paths:

```powershell
param(
    [string]$BatmanRoot,
    [string]$BuilderRoot,
    [Parameter(Mandatory = $true)]
    [string]$BasePackagePath,
    [Parameter(Mandatory = $true)]
    [string]$FrontendBasePackagePath,
    [Parameter(Mandatory = $true)]
    [string]$FfdecCliPath,
    [string]$Configuration = 'Release'
)

$FrontendBasePackagePath = (Resolve-Path $FrontendBasePackagePath).Path
$PreparedFrontendBasePackagePath = Join-Path $ExtractedRoot 'frontend\startup-int-unpacked\Startup_INT.upk'
$PreparedFrontendGfxPath = Join-Path $ExtractedRoot 'frontend\mainv2\frontend-mainv2.gfx'
$PreparedFrontendXmlPath = Join-Path $ExtractedRoot 'frontend\mainv2\frontend-mainv2.xml'
$PreparedFrontendExportRoot = Join-Path $ExtractedRoot 'frontend\mainv2\frontend-mainv2-export'

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $PreparedFrontendBasePackagePath) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $PreparedFrontendGfxPath) | Out-Null

if (-not [System.StringComparer]::OrdinalIgnoreCase.Equals($FrontendBasePackagePath, $PreparedFrontendBasePackagePath)) {
    Copy-Item -LiteralPath $FrontendBasePackagePath -Destination $PreparedFrontendBasePackagePath -Force
}

Reset-Directory -Path $PreparedFrontendExportRoot

Invoke-CheckedCommand `
    -FilePath 'dotnet' `
    -Arguments @(
        'run',
        '--project', $BmGameGfxPatcherProjectPath,
        '-c', $Configuration,
        '--',
        'extract-gfx',
        '--package', $PreparedFrontendBasePackagePath,
        '--owner', 'MainMenu',
        '--name', 'MainV2',
        '--output', $PreparedFrontendGfxPath
    ) `
    -FailureMessage 'Failed to extract MainMenu.MainV2 from the trusted Startup_INT builder package.'

Invoke-CheckedCommand `
    -FilePath $PreparedFfdecCliPath `
    -Arguments @('-swf2xml', $PreparedFrontendGfxPath, $PreparedFrontendXmlPath) `
    -FailureMessage 'FFDec failed to convert frontend-mainv2.gfx into frontend-mainv2.xml.'

Invoke-CheckedCommand `
    -FilePath $PreparedFfdecCliPath `
    -Arguments @('-export', 'script', $PreparedFrontendExportRoot, $PreparedFrontendGfxPath) `
    -FailureMessage 'FFDec failed to export the MainV2 ActionScript tree.'
```

- [ ] **Step 4: Re-run the prep verifier to confirm the temp builder workspace is complete**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanBuilderWorkspacePreparation.ps1 -BatmanRoot .\games\HelenBatmanAA -Configuration Debug
```

Expected:

- `PASS`

- [ ] **Step 5: Commit the prep-path change**

Run:

```powershell
git add games/HelenBatmanAA/scripts/Prepare-BatmanBuilderWorkspace.ps1 games/HelenBatmanAA/scripts/Test-BatmanBuilderWorkspacePreparation.ps1
git commit -m "Add Batman frontend builder workspace prep"
```

---

## Task 2: Rebuild MainMenu Audio Structurally Instead Of Script-Only

**Files:**
- Create: `games/HelenBatmanAA/scripts/Test-BatmanMainMenuAudioLayout.ps1`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/MainMenuXmlPatcher.cs`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/SubtitleSizeAssetBuilder.cs`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/Program.cs`

- [ ] **Step 1: Add a generated-movie verifier that expects a real fifth row**

Create `games/HelenBatmanAA/scripts/Test-BatmanMainMenuAudioLayout.ps1` with these assertions:

```powershell
param(
    [string]$BatmanRoot
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}

$GeneratedRoot = Join-Path $BatmanRoot 'builder\generated\main-menu-audio'
$FrontendXmlPath = Join-Path $GeneratedRoot '_build\MainV2-subtitle-size.xml'
$Frame1ScriptPath = Join-Path $GeneratedRoot '_build\frontend-scripts\DefineSprite_359_ScreenOptionsAudio\frame_1\DoAction.as'
$SubtitleSizeClipActionPath = Join-Path $GeneratedRoot '_build\frontend-scripts\DefineSprite_359_ScreenOptionsAudio\frame_1\PlaceObject2_290_List_Template_61\CLIPACTIONRECORD onClipEvent(load).as'

[xml]$Document = Get-Content -LiteralPath $FrontendXmlPath -Raw
$AudioScreen = $Document.SelectSingleNode("//item[@type='DefineSpriteTag' and @spriteId='359']")
$SubTags = $AudioScreen.SelectSingleNode('subTags')

$ExpectedRows = @(
    @{ Name = 'Subtitles';      Depth = '53'; TranslateY = 1885 },
    @{ Name = 'VolumeSFX';      Depth = '45'; TranslateY = 2785 },
    @{ Name = 'VolumeMusic';    Depth = '37'; TranslateY = 3622 },
    @{ Name = 'VolumeDialogue'; Depth = '29'; TranslateY = 4466 },
    @{ Name = 'SubtitleSize';   Depth = '61'; TranslateY = 5310 }
)

$ExpectedAddItemLines = @(
    'this.AddItem(Subtitles,4,1,-1,-1);',
    'this.AddItem(VolumeSFX,0,2,-1,-1);',
    'this.AddItem(VolumeMusic,1,3,-1,-1);',
    'this.AddItem(VolumeDialogue,2,4,-1,-1);',
    'this.AddItem(SubtitleSize,3,0,-1,-1);'
)

foreach ($ExpectedRow in $ExpectedRows) {
    $Row = $SubTags.SelectSingleNode("item[@type='PlaceObject2Tag' and @name='$($ExpectedRow.Name)' and @depth='$($ExpectedRow.Depth)']")
    if ($null -eq $Row) {
        throw "Expected main-menu audio row was not found: $($ExpectedRow.Name)"
    }

    $TranslateY = [int]$Row.SelectSingleNode('matrix').Attributes['translateY'].Value
    if ($TranslateY -ne $ExpectedRow.TranslateY) {
        throw "Main-menu row $($ExpectedRow.Name) had translateY=$TranslateY, expected $($ExpectedRow.TranslateY)."
    }
}

$ActualAddItemLines = @(
    Get-Content -LiteralPath $Frame1ScriptPath |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -like 'this.AddItem(*' }
)

if (($ActualAddItemLines -join "`n") -cne ($ExpectedAddItemLines -join "`n")) {
    throw "Main-menu AddItem order mismatch."
}

if (-not (Test-Path -LiteralPath $SubtitleSizeClipActionPath)) {
    throw "Main-menu SubtitleSize clip action was not generated at depth 61."
}

$AnimatedSubtitleRows = @($SubTags.SelectNodes("item[@type='PlaceObject2Tag' and @depth='61']"))
foreach ($AnimatedSubtitleRow in $AnimatedSubtitleRows) {
    $Matrix = $AnimatedSubtitleRow.SelectSingleNode('matrix')
    if ($null -ne $Matrix -and [int]$Matrix.Attributes['translateY'].Value -ne 5310) {
        throw "Animated SubtitleSize depth 61 placement drifted from translateY 5310."
    }
}

Write-Output 'PASS'
```

- [ ] **Step 2: Build the current front-end movie and confirm the verifier fails**

Run:

```powershell
dotnet build .\games\HelenBatmanAA\builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj -c Debug
dotnet run --project .\games\HelenBatmanAA\builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj -c Debug -- build-assets --root .\games\HelenBatmanAA\builder --output-dir .\games\HelenBatmanAA\builder\generated\main-menu-audio --ffdec .\games\HelenBatmanAA\builder\extracted\ffdec\ffdec-cli.exe
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanMainMenuAudioLayout.ps1 -BatmanRoot .\games\HelenBatmanAA
```

Expected:

- builder completes, but
- `Test-BatmanMainMenuAudioLayout.ps1` FAILS because there is no dedicated `SubtitleSize` row at depth `61` and the generated `DoAction.as` still contains only four `AddItem(...)` lines

- [ ] **Step 3: Patch the XML and add a dedicated front-end build command**

Update `MainMenuXmlPatcher.cs` so it adds only the fifth row and does not move the existing four rows:

```csharp
private const int ScreenOptionsAudioSpriteId = 359;
private const int SubtitleDepth = 53;
private const int SubtitleSizeDepth = 61;
private const int SubtitleTranslateY = 1885;
private const int SubtitleSizeTranslateY = 5310;

private static void PatchAudioScreen(XmlElement audioScreen)
{
    XmlElement subTags = GetSingleElement(audioScreen, "subTags");
    List<XmlElement> originalChildren = subTags.ChildNodes.OfType<XmlElement>().ToList();

    XmlElement subtitlesInitial = originalChildren.Single(node =>
        node.Name == "item" &&
        GetAttribute(node, "type") == "PlaceObject2Tag" &&
        GetAttribute(node, "depth") == SubtitleDepth.ToString() &&
        GetAttribute(node, "name") == "Subtitles");

    XmlElement subtitleSizeInitial = (XmlElement)subtitlesInitial.CloneNode(deep: true);
    SetAttribute(subtitleSizeInitial, "depth", SubtitleSizeDepth.ToString());
    SetAttribute(subtitleSizeInitial, "name", "SubtitleSize");
    SetTranslateY(GetSingleElement(subtitleSizeInitial, "matrix"), SubtitleSizeTranslateY);
    subtitlesInitial.ParentNode!.InsertAfter(subtitleSizeInitial, subtitlesInitial);

    foreach (XmlElement original in originalChildren)
    {
        string? type = GetAttribute(original, "type");
        string? depth = GetAttribute(original, "depth");
        if (depth != SubtitleDepth.ToString() || type is not ("PlaceObject2Tag" or "RemoveObject2Tag") || ReferenceEquals(original, subtitlesInitial))
        {
            continue;
        }

        XmlElement clone = (XmlElement)original.CloneNode(deep: true);
        SetAttribute(clone, "depth", SubtitleSizeDepth.ToString());
        if (type == "PlaceObject2Tag" && clone["matrix"] is XmlElement matrix)
        {
            SetTranslateY(matrix, SubtitleSizeTranslateY);
        }

        original.ParentNode!.InsertAfter(clone, original);
    }
}

private static void SetTranslateY(XmlElement matrix, int value)
{
    matrix.SetAttribute("translateY", value.ToString());
}
```

Update `SubtitleSizeAssetBuilder.cs` so the front-end path rebuilds XML structurally:

```csharp
public static void BuildFrontend(BuildPaths paths)
{
    ValidateFrontendInputs(paths);

    RecreateDirectory(paths.OutputDirectory);
    Directory.CreateDirectory(paths.TempDirectory);

    CopyDirectory(paths.FrontendScriptsPath, paths.FrontendWorkingScriptsPath);
    PatchFrontendScripts(paths.FrontendWorkingScriptsPath, paths.BuildVersion);
    MainMenuXmlPatcher.Patch(paths.FrontendXmlPath, paths.FrontendPatchedXmlPath);

    RunProcess(paths.FfdecPath, "-xml2swf", paths.FrontendPatchedXmlPath, paths.FrontendStructuralGfxPath);
    RunProcess(paths.FfdecPath, "-importScript", paths.FrontendStructuralGfxPath, paths.FrontendOutputGfxPath, paths.FrontendWorkingScriptsPath);

    WriteFrontendManifest(paths.FrontendManifestPath);
}

private static void ValidateFrontendInputs(BuildPaths paths)
{
    string[] requiredPaths =
    {
        paths.FrontendXmlPath,
        paths.FrontendSourceGfxPath,
        paths.FrontendScriptsPath,
        paths.FfdecPath
    };

    foreach (string requiredPath in requiredPaths)
    {
        if (!File.Exists(requiredPath) && !Directory.Exists(requiredPath))
        {
            throw new InvalidOperationException($"Required front-end path not found: {requiredPath}");
        }
    }
}
```

Update `Program.cs` with a dedicated command:

```csharp
"build-main-menu-audio" => RunBuildMainMenuAudio(tail),
```

and:

```csharp
private static int RunBuildMainMenuAudio(string[] args)
{
    var options = new ArgumentReader(args);
    string root = Path.GetFullPath(options.RequireValue("--root"));
    string outputDirectory = Path.GetFullPath(options.GetValue("--output-dir") ?? Path.Combine(root, "generated", "main-menu-audio"));
    string ffdecPath = Path.GetFullPath(options.GetValue("--ffdec") ?? Path.Combine(root, "extracted", "ffdec", "ffdec-cli.exe"));
    string buildVersion = BuildVersionManager.Resolve(root, options.GetValue("--build-version"));
    options.ThrowIfAnyUnknown();

    BuildPaths paths = BuildPaths.FromRoot(root, ffdecPath, outputDirectory, buildVersion);
    SubtitleSizeAssetBuilder.BuildFrontend(paths);

    Console.WriteLine($"Built Frontend:    {paths.FrontendOutputGfxPath}");
    Console.WriteLine($"Frontend manifest: {paths.FrontendManifestPath}");
    return 0;
}
```

and add the help text line:

```csharp
Console.WriteLine("  build-main-menu-audio --root <batman-builder-root> [--output-dir <generated\\main-menu-audio>] [--ffdec <extracted\\ffdec\\ffdec-cli.exe>] [--build-version <label>]");
```

- [ ] **Step 4: Run the dedicated front-end build and verify the generated structure passes**

Run:

```powershell
dotnet build .\games\HelenBatmanAA\builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj -c Debug
dotnet run --project .\games\HelenBatmanAA\builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj -c Debug -- build-main-menu-audio --root .\games\HelenBatmanAA\builder --output-dir .\games\HelenBatmanAA\builder\generated\main-menu-audio --ffdec .\games\HelenBatmanAA\builder\extracted\ffdec\ffdec-cli.exe
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanMainMenuAudioLayout.ps1 -BatmanRoot .\games\HelenBatmanAA
```

Expected:

- `dotnet build` succeeds
- `build-main-menu-audio` prints the built front-end GFX and manifest paths
- `Test-BatmanMainMenuAudioLayout.ps1` prints `PASS`

- [ ] **Step 5: Commit the structural front-end build change**

Run:

```powershell
git add games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/MainMenuXmlPatcher.cs games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/SubtitleSizeAssetBuilder.cs games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/Program.cs games/HelenBatmanAA/scripts/Test-BatmanMainMenuAudioLayout.ps1
git commit -m "Rebuild Batman main-menu audio structurally"
```

---

## Task 3: Replace the Old Front-End Four-State Hack With a Dedicated Disabled Row

**Files:**
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/ScriptTemplates.cs`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/SubtitleSizeAssetBuilder.cs`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanMainMenuAudioLayout.ps1`

- [ ] **Step 1: Extend the front-end verifier so it fails until the dedicated row behavior exists**

Add these checks to `games/HelenBatmanAA/scripts/Test-BatmanMainMenuAudioLayout.ps1`:

```powershell
$FrontendListItemPath = Join-Path $GeneratedRoot '_build\frontend-scripts\__Packages\rs\ui\ListItem.as'
$FrontendClipActionPath = Join-Path $GeneratedRoot '_build\frontend-scripts\DefineSprite_359_ScreenOptionsAudio\frame_1\PlaceObject2_290_List_Template_61\CLIPACTIONRECORD onClipEvent(load).as'

$ListItemContents = Get-Content -LiteralPath $FrontendListItemPath -Raw
$RequiredTokens = @(
    'this.GameVariable == "SubtitleSize"',
    'function AreSubtitlesEnabled()',
    'function IsDisabled()',
    'this.ItemText._alpha = _loc2_ ? 40 : 100;',
    'this.Label._alpha = _loc2_ ? 40 : 100;',
    'if(this.IsDisabled())',
    'FE_GetSubtitles'
)

foreach ($RequiredToken in $RequiredTokens) {
    if ($ListItemContents.IndexOf($RequiredToken, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Generated frontend ListItem is missing required token: $RequiredToken"
    }
}

$ClipActionContents = Get-Content -LiteralPath $FrontendClipActionPath -Raw
if ($ClipActionContents.IndexOf('this.Init("SubtitleSize","Small","Normal","Large");', [System.StringComparison]::Ordinal) -lt 0) {
    throw 'Generated frontend SubtitleSize clip action does not initialize the dedicated row.'
}
```

- [ ] **Step 2: Rebuild the front-end movie and confirm the verifier still fails on script behavior**

Run:

```powershell
dotnet run --project .\games\HelenBatmanAA\builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj -c Debug -- build-main-menu-audio --root .\games\HelenBatmanAA\builder --output-dir .\games\HelenBatmanAA\builder\generated\main-menu-audio --ffdec .\games\HelenBatmanAA\builder\extracted\ffdec\ffdec-cli.exe
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanMainMenuAudioLayout.ps1 -BatmanRoot .\games\HelenBatmanAA
```

Expected:

- FAIL because the generated front-end script still uses the old row hack on `PlaceObject2_290_List_Template_53` and does not write a dedicated disabled-state `ListItem.as`

- [ ] **Step 3: Patch the front-end list-item template, frame script, and depth-61 clip action**

Add a dedicated front-end list-item template to `ScriptTemplates.cs` and write it from `PatchFrontendScripts(...)`.

Use this front-end behavior in `ScriptTemplates.cs`:

```csharp
public const string FrontendListItem = """
class rs.ui.ListItem extends MovieClip
{
   var ItemText;
   var Label;
   var LabelName;
   var LeftClicker;
   var Names;
   var RightClicker;
   var GameVariable = "?name?";
   var State = 0;
   var Initial = 0;
   var Default = 0;
   function IsSubtitleSizeOption()
   {
      return this.GameVariable == "SubtitleSize";
   }
   function NormalizeBoolState(InValue, DefaultValue)
   {
      if(InValue == undefined)
      {
         return DefaultValue;
      }
      return InValue != 0 ? 1 : 0;
   }
   function AreSubtitlesEnabled()
   {
      return this.NormalizeBoolState(flash.external.ExternalInterface.call("FE_GetSubtitles"),1) != 0;
   }
   function IsDisabled()
   {
      return this.IsSubtitleSizeOption() && !this.AreSubtitlesEnabled();
   }
   function UpdateDisabledVisualState()
   {
      var _loc2_ = this.IsDisabled();
      this.ItemText._alpha = _loc2_ ? 40 : 100;
      this.Label._alpha = _loc2_ ? 40 : 100;
      this.LeftClicker._visible = !_loc2_ && this.State > 0;
      this.RightClicker._visible = !_loc2_ && this.State < this.Names.length - 1;
   }
   function ShowPrompt()
   {
      var _loc3_ = _root.PromptManager;
      if(this._parent.BackScreen != "")
      {
         _loc3_.SetPrompt(_loc3_.CI_B,"$UI.Cancel",this._parent.myListener.onPromptClick,100,100);
      }
      if(this.Names.length > 1 && !this.IsDisabled())
      {
         _loc3_.SetPrompt(_loc3_.CI_Interact,"$UI.Cycle",this._parent.myListener.onPromptClick,100,100);
      }
   }
   function RunAction(bMouse)
   {
      if(this.IsDisabled())
      {
         return undefined;
      }
      if(this.Names.length < 3)
      {
         this.Cycle();
      }
      else if(bMouse)
      {
         if(this._xmouse < 0)
         {
            this.Decrement();
         }
         else
         {
            this.Increment();
         }
      }
      else
      {
         this.Cycle();
      }
   }
}
""";
```

Update the front-end frame and clip action constants in `ScriptTemplates.cs`:

```csharp
public const string FrontendAudioFrame1 = """
function CancelScreen()
{
   if(HaveOptionsChanged() == true)
   {
      flash.external.ExternalInterface.call("FE_TriggerOptionsSave");
   }
   ReturnFromScreen();
}
flash.external.ExternalInterface.call("FE_SetActiveScreenName","Options Audio");
this.BackScreen = "OptionsMenu";
this.BackScreenIndex = 1;
this.FocusIndex = 0;
this.Flags = this.FLAG_OPTIONS;
this.Init();
_root.TriggerEvent("Options");
this.AddItem(Subtitles,4,1,-1,-1);
this.AddItem(VolumeSFX,0,2,-1,-1);
this.AddItem(VolumeMusic,1,3,-1,-1);
this.AddItem(VolumeDialogue,2,4,-1,-1);
this.AddItem(SubtitleSize,3,0,-1,-1);
_rotation = -2;
""";

public const string FrontendAudioSubtitleSizeClipAction = """
onClipEvent(load){
   this.Init("SubtitleSize","Small","Normal","Large");
}
""";
```

Update `PatchFrontendScripts(...)` in `SubtitleSizeAssetBuilder.cs` to write the dedicated list item and the depth-61 clip action:

```csharp
WriteAllText(
    Path.Combine(scriptsRoot, "__Packages", "rs", "ui", "ListItem.as"),
    ScriptTemplates.FrontendListItem);

WriteAllText(
    Path.Combine(
        scriptsRoot,
        "DefineSprite_359_ScreenOptionsAudio",
        "frame_1",
        "PlaceObject2_290_List_Template_61",
        "CLIPACTIONRECORD onClipEvent(load).as"),
    ScriptTemplates.FrontendAudioSubtitleSizeClipAction);
```

- [ ] **Step 4: Rebuild the front-end movie and verify the dedicated row behavior passes**

Run:

```powershell
dotnet build .\games\HelenBatmanAA\builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj -c Debug
dotnet run --project .\games\HelenBatmanAA\builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj -c Debug -- build-main-menu-audio --root .\games\HelenBatmanAA\builder --output-dir .\games\HelenBatmanAA\builder\generated\main-menu-audio --ffdec .\games\HelenBatmanAA\builder\extracted\ffdec\ffdec-cli.exe
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanMainMenuAudioLayout.ps1 -BatmanRoot .\games\HelenBatmanAA
```

Expected:

- `PASS`

- [ ] **Step 5: Commit the dedicated-row script change**

Run:

```powershell
git add games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/ScriptTemplates.cs games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/SubtitleSizeAssetBuilder.cs games/HelenBatmanAA/scripts/Test-BatmanMainMenuAudioLayout.ps1
git commit -m "Add Batman main-menu subtitle-size row behavior"
```

---

## Task 4: Ship the Front-End Movie Through the Batman Delta Pack

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanKnownGoodGameplayPackage.ps1`
- Modify: `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/files.json`

- [ ] **Step 1: Expand the shipped-pack verifier so it fails until the front-end delta exists**

Modify `games/HelenBatmanAA/scripts/Test-BatmanKnownGoodGameplayPackage.ps1` so it validates both virtual files and computes the exact expected front-end base/target fingerprints from the local trusted builder base and generated package:

```powershell
$FrontendBuildRoot = Join-Path $BatmanRoot 'builder\generated\main-menu-audio'
$TrustedFrontendBasePath = Join-Path $BatmanRoot 'builder\extracted\frontend\startup-int-unpacked\Startup_INT.upk'
$GeneratedFrontendPackagePath = Join-Path $FrontendBuildRoot 'Startup_INT-main-menu-subtitle-size.upk'
$FrontendDeltaPath = Join-Path $PackBuildRoot 'assets\deltas\Startup_INT-main-menu-subtitle-size.hgdelta'

$ExpectedFrontendVirtualFileId = 'startupFrontendPackage'
$ExpectedFrontendVirtualFilePath = 'BmGame/CookedPC/Startup_INT.upk'
$ExpectedFrontendVirtualFileMode = 'delta-on-read'
$ExpectedFrontendVirtualFileKind = 'delta-file'
$ExpectedFrontendDeltaPath = 'assets/deltas/Startup_INT-main-menu-subtitle-size.hgdelta'
$ExpectedFrontendBaseSize = (Get-Item -LiteralPath $TrustedFrontendBasePath).Length
$ExpectedFrontendBaseSha256 = (Get-FileHash -LiteralPath $TrustedFrontendBasePath -Algorithm SHA256).Hash.ToLowerInvariant()
$ExpectedFrontendTargetSize = (Get-Item -LiteralPath $GeneratedFrontendPackagePath).Length
$ExpectedFrontendTargetSha256 = (Get-FileHash -LiteralPath $GeneratedFrontendPackagePath -Algorithm SHA256).Hash.ToLowerInvariant()
$ExpectedFrontendDeltaHash = (Get-FileHash -LiteralPath $FrontendDeltaPath -Algorithm SHA256).Hash

if ($VirtualFiles.Count -ne 2) {
    throw "Batman pack manifest expected exactly two virtual files, found $($VirtualFiles.Count)."
}

$FrontendFile = @($VirtualFiles | Where-Object { $_.id -eq $ExpectedFrontendVirtualFileId })[0]
if ($null -eq $FrontendFile) {
    throw "Batman pack manifest is missing the frontend virtual file id $ExpectedFrontendVirtualFileId."
}

if ($FrontendFile.path -ne $ExpectedFrontendVirtualFilePath) {
    throw "Batman frontend package path mismatch. Expected $ExpectedFrontendVirtualFilePath but found $($FrontendFile.path)."
}

if ($FrontendFile.mode -ne $ExpectedFrontendVirtualFileMode) {
    throw "Batman frontend package mode mismatch. Expected $ExpectedFrontendVirtualFileMode but found $($FrontendFile.mode)."
}

if ($FrontendFile.source.kind -ne $ExpectedFrontendVirtualFileKind) {
    throw "Batman frontend package source kind mismatch. Expected $ExpectedFrontendVirtualFileKind but found $($FrontendFile.source.kind)."
}

if ($FrontendFile.source.path -ne $ExpectedFrontendDeltaPath) {
    throw "Batman frontend package delta path mismatch. Expected $ExpectedFrontendDeltaPath but found $($FrontendFile.source.path)."
}

if ([int64]$FrontendFile.source.base.size -ne $ExpectedFrontendBaseSize) {
    throw "Batman frontend base size mismatch."
}

if (-not [string]::Equals($FrontendFile.source.base.sha256, $ExpectedFrontendBaseSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Batman frontend base hash mismatch."
}

if ([int64]$FrontendFile.source.target.size -ne $ExpectedFrontendTargetSize) {
    throw "Batman frontend target size mismatch."
}

if (-not [string]::Equals($FrontendFile.source.target.sha256, $ExpectedFrontendTargetSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Batman frontend target hash mismatch."
}
```

- [ ] **Step 2: Run the shipped-pack verifier to confirm it fails before the rebuild pipeline is updated**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanKnownGoodGameplayPackage.ps1 -BatmanRoot .\games\HelenBatmanAA
```

Expected:

- FAIL because `files.json` still contains only the gameplay entry and `assets\deltas\Startup_INT-main-menu-subtitle-size.hgdelta` does not exist yet

- [ ] **Step 3: Update the rebuild pipeline to patch the trusted front-end package and emit a second delta**

Modify `games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1` with these paths and commands:

```powershell
$FrontendBasePackagePath = Join-Path $ExtractedRoot 'frontend\startup-int-unpacked\Startup_INT.upk'
$FrontendBuildRoot = Join-Path $GeneratedRoot 'main-menu-audio'
$FrontendManifestPath = Join-Path $FrontendBuildRoot 'subtitle-size-frontend.manifest.jsonc'
$GeneratedFrontendPackagePath = Join-Path $FrontendBuildRoot 'Startup_INT-main-menu-subtitle-size.upk'
$FrontendDeltaPath = Join-Path $PackBuildRoot 'assets\deltas\Startup_INT-main-menu-subtitle-size.hgdelta'
$FrontendAudioLayoutVerifierPath = Join-Path $PSScriptRoot 'Test-BatmanMainMenuAudioLayout.ps1'

$builderWorkspacePrerequisites = @(
    $FrontendBasePackagePath,
    (Join-Path $ExtractedRoot 'frontend\mainv2\frontend-mainv2.gfx'),
    (Join-Path $ExtractedRoot 'frontend\mainv2\frontend-mainv2.xml'),
    (Join-Path $ExtractedRoot 'frontend\mainv2\frontend-mainv2-export\scripts')
)

& dotnet run --project $SubtitleSizeModBuilderProjectPath -c $Configuration -- `
    build-main-menu-audio `
    --root $BuilderRoot `
    --output-dir $FrontendBuildRoot `
    --ffdec $FfdecPath
if ($LASTEXITCODE -ne 0) {
    throw "Main-menu audio asset build failed."
}

& powershell -ExecutionPolicy Bypass -File $FrontendAudioLayoutVerifierPath -BatmanRoot $BatmanRoot
if ($LASTEXITCODE -ne 0) {
    throw "Main-menu audio layout verification failed."
}

& dotnet run --project $BmGameGfxPatcherProjectPath -c $Configuration -- `
    patch `
    --package $FrontendBasePackagePath `
    --manifest $FrontendManifestPath `
    --output $GeneratedFrontendPackagePath
if ($LASTEXITCODE -ne 0) {
    throw "Startup_INT front-end package patch build failed."
}

$frontendDeltaInfo = & $BuildHgdeltaScriptPath `
    -BaseFile $FrontendBasePackagePath `
    -TargetFile $GeneratedFrontendPackagePath `
    -OutputFile $FrontendDeltaPath `
    -ChunkSize 65536
if ($LASTEXITCODE -ne 0) {
    throw "Batman frontend hgdelta build failed."
}

$filesManifest = @{
    virtualFiles = @(
        @{
            id = 'bmgameGameplayPackage'
            path = 'BmGame/CookedPC/BmGame.u'
            mode = 'delta-on-read'
            source = @{
                kind = 'delta-file'
                path = 'assets/deltas/BmGame-subtitle-signal.hgdelta'
                base = @{
                    size = $deltaInfo.BaseSize
                    sha256 = $deltaInfo.BaseSha256
                }
                target = @{
                    size = $deltaInfo.TargetSize
                    sha256 = $deltaInfo.TargetSha256
                }
                chunkSize = $deltaInfo.ChunkSize
            }
        },
        @{
            id = 'startupFrontendPackage'
            path = 'BmGame/CookedPC/Startup_INT.upk'
            mode = 'delta-on-read'
            source = @{
                kind = 'delta-file'
                path = 'assets/deltas/Startup_INT-main-menu-subtitle-size.hgdelta'
                base = @{
                    size = $frontendDeltaInfo.BaseSize
                    sha256 = $frontendDeltaInfo.BaseSha256
                }
                target = @{
                    size = $frontendDeltaInfo.TargetSize
                    sha256 = $frontendDeltaInfo.TargetSha256
                }
                chunkSize = $frontendDeltaInfo.ChunkSize
            }
        }
    )
}
```

- [ ] **Step 4: Rebuild the Batman pack and verify the two-entry manifest passes**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Rebuild-BatmanPack.ps1 -Configuration Debug
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanKnownGoodGameplayPackage.ps1 -BatmanRoot .\games\HelenBatmanAA
```

Expected:

- `Rebuild-BatmanPack.ps1` succeeds and prints both delta outputs
- `Test-BatmanKnownGoodGameplayPackage.ps1` prints `PASS`

- [ ] **Step 5: Commit the two-delta Batman pack change**

Run:

```powershell
git add games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1 games/HelenBatmanAA/scripts/Test-BatmanKnownGoodGameplayPackage.ps1 games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/files.json games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/assets/deltas/BmGame-subtitle-signal.hgdelta games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/assets/deltas/Startup_INT-main-menu-subtitle-size.hgdelta
git commit -m "Ship Batman frontend subtitle-size delta"
```

---

## Task 5: Align Runtime Tests, Deployment, And Live Smoke Flow

**Files:**
- Modify: `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`
- Modify: `games/HelenBatmanAA/scripts/Deploy-Batman.ps1`
- Modify: `games/HelenBatmanAA/README.md`

- [ ] **Step 1: Update the runtime and deployment expectations so they fail until both entries are handled**

Modify `tests/HelenRuntime.Tests/PackRepositoryTests.cpp` so the checked-in Batman pack expects both entries:

```cpp
Expect(loaded_batman_pack->Build.VirtualFiles.size() == 2, "Checked-in Batman pack virtual file count mismatch.");

const helen::VirtualFileDefinition* gameplay_file = nullptr;
const helen::VirtualFileDefinition* frontend_file = nullptr;
for (const helen::VirtualFileDefinition& virtual_file : loaded_batman_pack->Build.VirtualFiles)
{
    if (virtual_file.Id == "bmgameGameplayPackage")
    {
        gameplay_file = &virtual_file;
    }
    else if (virtual_file.Id == "startupFrontendPackage")
    {
        frontend_file = &virtual_file;
    }
}

Expect(gameplay_file != nullptr, "Checked-in Batman gameplay virtual file was not found.");
Expect(frontend_file != nullptr, "Checked-in Batman frontend virtual file was not found.");
Expect(frontend_file->Mode == "delta-on-read", "Checked-in Batman frontend package is not delta-backed.");
Expect(frontend_file->Source.Kind == helen::VirtualFileSourceKind::DeltaFile, "Checked-in Batman frontend package source kind mismatch.");
Expect(frontend_file->Path == std::filesystem::path("BmGame/CookedPC/Startup_INT.upk"), "Checked-in Batman frontend package path mismatch.");
Expect(frontend_file->Source.Path == std::filesystem::path("assets/deltas/Startup_INT-main-menu-subtitle-size.hgdelta"), "Checked-in Batman frontend delta path mismatch.");
```

Modify `games/HelenBatmanAA/scripts/Deploy-Batman.ps1` so it validates both staged and deployed deltas:

```powershell
$SourceGameplayDeltaPath = Join-Path $PackSource 'builds\steam-goty-1.0\assets\deltas\BmGame-subtitle-signal.hgdelta'
$SourceFrontendDeltaPath = Join-Path $PackSource 'builds\steam-goty-1.0\assets\deltas\Startup_INT-main-menu-subtitle-size.hgdelta'
$ExpectedVirtualFiles = @(
    @{
        Id = 'bmgameGameplayPackage'
        Path = 'BmGame/CookedPC/BmGame.u'
        Mode = 'delta-on-read'
        Kind = 'delta-file'
        DeltaPath = 'assets/deltas/BmGame-subtitle-signal.hgdelta'
        DeltaHash = (Get-FileHash -LiteralPath $SourceGameplayDeltaPath -Algorithm SHA256).Hash
    },
    @{
        Id = 'startupFrontendPackage'
        Path = 'BmGame/CookedPC/Startup_INT.upk'
        Mode = 'delta-on-read'
        Kind = 'delta-file'
        DeltaPath = 'assets/deltas/Startup_INT-main-menu-subtitle-size.hgdelta'
        DeltaHash = (Get-FileHash -LiteralPath $SourceFrontendDeltaPath -Algorithm SHA256).Hash
    }
)
```

- [ ] **Step 2: Run the runtime test binary and deployment script to confirm the old single-entry assumptions fail**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32 /m
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Deploy-Batman.ps1 -Configuration Debug
```

Expected:

- `HelenRuntimeTests.exe` FAILS until `PackRepositoryTests.cpp` matches the new two-entry pack shape
- `Deploy-Batman.ps1` FAILS until it validates and stages both delta files

- [ ] **Step 3: Finish the deploy/runtime alignment and document the smoke path**

Complete the PowerShell deployment loops so both staged and deployed manifests use the same `$ExpectedVirtualFiles` array, then update `games/HelenBatmanAA/README.md`.

Add this prep command to `README.md`:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Prepare-BatmanBuilderWorkspace.ps1 `
  -BasePackagePath D:\trusted\BmGame.u `
  -FrontendBasePackagePath D:\trusted\Startup_INT.upk `
  -FfdecCliPath C:\tools\ffdec\ffdec-cli.exe
```

Add this live verification checklist to `README.md`:

```text
After deployment, verify both menu paths:
1. Pause `Options -> Audio` still shows `Subtitle Size`.
2. Main menu `Options -> Audio` shows `Subtitle Size` as the fifth row.
3. Turn `Subtitles` off and confirm `Subtitle Size` is greyed or inert.
4. Turn `Subtitles` on and confirm `Subtitle Size` becomes active again.
5. Pass press-start and save/profile selection without front-end breakage.
```

- [ ] **Step 4: Run the full automated suite, then do the live front-end smoke flow**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32 /m
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanBuilderWorkspacePreparation.ps1 -BatmanRoot .\games\HelenBatmanAA -Configuration Debug
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanMainMenuAudioLayout.ps1 -BatmanRoot .\games\HelenBatmanAA
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanKnownGoodGameplayPackage.ps1 -BatmanRoot .\games\HelenBatmanAA
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Deploy-Batman.ps1 -Configuration Debug
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Launch-Check-Batman.ps1
```

Expected:

- MSBuild succeeds
- `HelenRuntimeTests.exe` prints `PASS`
- each Batman verifier prints `PASS`
- deploy prints `DEPLOYED`
- launch check prints `PROCESS_STARTED`

Then perform the live smoke flow manually:

1. Press Start from the title screen.
2. Reach the save/profile selection flow without hangs or broken prompts.
3. Open `Options -> Audio` from the main menu.
4. Confirm `Subtitle Size` appears below `Dialogue Volume`.
5. Turn subtitles off and confirm `Subtitle Size` becomes greyed or at least inert.
6. Turn subtitles on and confirm the row becomes interactive again.
7. Back out through the save/apply path and make sure the front end remains stable.

- [ ] **Step 5: Commit the runtime/deploy alignment**

Run:

```powershell
git add tests/HelenRuntime.Tests/PackRepositoryTests.cpp games/HelenBatmanAA/scripts/Deploy-Batman.ps1 games/HelenBatmanAA/README.md
git commit -m "Validate Batman frontend subtitle-size deployment"
```
