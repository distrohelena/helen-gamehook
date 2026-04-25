# Graphics Background Panel Resize Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the large translucent gray background panel on the `Graphics Options` screen so it covers roughly `85-90%` of the visible screen height while leaving the header, rows, and horizontal layout unchanged.

**Architecture:** The graphics screen already keeps only three retained shell layers after cloning retail `ScreenOptionsGamePC`: depths `1`, `3`, and `26`. Exported XML shows that depth `3` with character `307` is the only retained shell placement already using the large translucent panel scale (`scaleX="2.0688171"`, `scaleY="1.2487793"`), so this plan resizes only that depth, locks depths `1` and `26` unchanged in regression, then rebuilds and verifies the live screen through the working handle-bound navigator path.

**Tech Stack:** C#, PowerShell, FFDec XML export, HelenGameHook rebuild/deploy scripts, HelenUI navigator-service, screenshot-cli, RecognitionCli

---

## File Map

- Modify: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsLayout.ps1`
  Owns the exported MainV2 regression. Extend it to prove the panel owner is depth `3` / character `307`, to lock the resized panel matrix, and to prove the other retained shell placements remain unchanged.
- Modify: `C:/dev/helenhook/games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsXmlPatcher.cs`
  Owns the graphics-screen clone patch. Add a dedicated vertical panel normalization helper plus matrix scale helpers instead of piggybacking on header or row logic.
- Verify only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanRetailGraphicsOptionsPatch.ps1`
  Guards retail `Options` and ensures the graphics-panel resize does not leak back into untouched menu screens.
- Run only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Rebuild-BatmanGraphicsOptionsExperiment.ps1`
  Rebuilds the generated `.umap`, tracked `.hgdelta`, and `files.json`.
- Run only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1`
  Verifies the rebuilt graphics-options payload.
- Run only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Deploy-BatmanGraphicsOptionsExperiment.ps1`
  Deploys the rebuilt pack into the live Batman install.
- Run only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/BatmanWindowHelpers.ps1`
  Resolves the live Batman handle used by navigator-service and screenshot-cli.
- Run only: `C:/dev/helenui/plugins/navigator-service/src/NavigatorService/NavigatorService.csproj`
  Provides the verified `/navigate` path used to reach `surface-graphics-options` without manual key sends.
- Run only: `C:/dev/helenui/plugins/screenshot-cli/src/ScreenshotCli/bin/Debug/net8.0-windows10.0.19041.0/ScreenshotCli.exe`
  Captures the live graphics screen by exact HWND after navigation completes.
- Run only: `C:/dev/helenui/plugins/recognition-cli/src/RecognitionCli/bin/Debug/net8.0/RecognitionCli.exe`
  Confirms the final live capture still matches `surface-graphics-options`.

## Verified Current Owner

The current exported XML at `C:/dev/helenhook/games/HelenBatmanAA/builder/generated/main-menu-graphics/MainV2-graphics-options.xml` already shows the retained graphics shell after the earlier cleanup:

- depth `1` / character `141`
  - `translateX="-7126"`
  - `translateY="899"`
  - `hasScale="false"`
- depth `3` / character `307`
  - `translateX="-7879"`
  - `translateY="-287"`
  - `scaleX="2.0688171"`
  - `scaleY="1.2487793"`
- depth `26` / character `332`
  - `translateX="-1641"`
  - `translateY="4609"`
  - `scaleX="-0.5350189"`
  - `scaleY="-0.5350189"`

That makes depth `3` / character `307` the panel-owning layer for this pass.

## Placement Target

Use a first-pass vertical expansion that keeps the current X placement and `scaleX`, but increases the panel height enough to cover the current row stack:

- panel owner depth `3` / character `307`
  - `translateX`: keep `-7879`
  - `translateY`: move from `-287` to `-1400`
  - `scaleX`: keep `2.0688171`
  - `scaleY`: move from `1.2487793` to `2.890625`

This is intentionally a vertical-only resize target. If the live review shows the panel still missing the bottom rows or colliding with the header by more than a small visual margin, repeat Task 1 with updated measured `translateY` / `scaleY` values on depth `3` only. Do not resize depths `1` or `26`, and do not move rows in this plan.

### Task 1: Lock the panel owner and implement the vertical-only resize with a failing regression

**Files:**
- Modify: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsLayout.ps1`
- Modify: `C:/dev/helenhook/games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsXmlPatcher.cs`
- Test: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsLayout.ps1`
- Verify: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanRetailGraphicsOptionsPatch.ps1`

- [ ] **Step 1: Extend the layout regression to prove depth `3` / character `307` owns the panel**

Add this owner-proof block after the existing retained-shell setup in `Test-BatmanGraphicsOptionsLayout.ps1` so the test fails loudly if the shell ownership changes:

```powershell
$GraphicsPanelOwnerPlacement = $ScreenOptionsGraphics.SelectSingleNode(
    "subTags/item[@type='PlaceObject2Tag' and @depth='3' and @characterId='307']"
)

if ($null -eq $GraphicsPanelOwnerPlacement) {
    throw 'Expected retained graphics panel owner at depth 3 with character 307.'
}

$GraphicsPanelOwnerMatrix = $GraphicsPanelOwnerPlacement.SelectSingleNode('matrix')
if ($null -eq $GraphicsPanelOwnerMatrix) {
    throw 'Expected retained graphics panel owner to contain a matrix node.'
}

if ($GraphicsPanelOwnerMatrix.Attributes['hasScale'].Value -ne 'true') {
    throw "Expected retained graphics panel owner at depth 3 to keep hasScale='true'."
}

$GraphicsHeaderShellPlacement = $ScreenOptionsGraphics.SelectSingleNode(
    "subTags/item[@type='PlaceObject2Tag' and @depth='1' and @characterId='141']"
)

if ($null -eq $GraphicsHeaderShellPlacement) {
    throw 'Expected retained graphics shell placement at depth 1 with character 141.'
}

$GraphicsHeaderShellMatrix = $GraphicsHeaderShellPlacement.SelectSingleNode('matrix')
if ($GraphicsHeaderShellMatrix.Attributes['hasScale'].Value -ne 'false') {
    throw "Expected retained shell depth 1 character 141 to keep hasScale='false'."
}
```

Replace the shell expectation block with explicit scale-aware expectations:

```powershell
$ExpectedGraphicsShellPlacements = @(
    @{
        Depth = '1'
        CharacterId = '141'
        TranslateX = '-7126'
        TranslateY = '899'
        HasScale = 'false'
        ScaleX = $null
        ScaleY = $null
    },
    @{
        Depth = '3'
        CharacterId = '307'
        TranslateX = '-7879'
        TranslateY = '-1400'
        HasScale = 'true'
        ScaleX = '2.0688171'
        ScaleY = '2.890625'
    },
    @{
        Depth = '26'
        CharacterId = '332'
        TranslateX = '-1641'
        TranslateY = '4609'
        HasScale = 'true'
        ScaleX = '-0.5350189'
        ScaleY = '-0.5350189'
    },
    @{
        Depth = '146'
        CharacterId = '118'
        TranslateX = '-6582'
        TranslateY = '-4780'
        HasScale = 'true'
        ScaleX = '0.7967224'
        ScaleY = '0.47787476'
    },
    @{
        Depth = '147'
        CharacterId = '340'
        TranslateX = '-8292'
        TranslateY = '-5436'
        HasScale = 'true'
        ScaleX = '0.7349243'
        ScaleY = '0.7349243'
    }
)
```

Inside the existing `foreach ($ExpectedGraphicsShellPlacement in $ExpectedGraphicsShellPlacements)` block, add scale checks after the translate checks:

```powershell
$ExpectedHasScale = $ExpectedGraphicsShellPlacement.HasScale
$ActualHasScale = $MatrixNode.Attributes['hasScale'].Value
if ($ActualHasScale -ne $ExpectedHasScale) {
    throw "Graphics shell depth $ExpectedDepth character $ExpectedCharacterId has hasScale $ActualHasScale, expected $ExpectedHasScale."
}

if ($null -ne $ExpectedGraphicsShellPlacement.ScaleX) {
    $ActualScaleX = $MatrixNode.Attributes['scaleX'].Value
    if ($ActualScaleX -ne $ExpectedGraphicsShellPlacement.ScaleX) {
        throw "Graphics shell depth $ExpectedDepth character $ExpectedCharacterId has scaleX $ActualScaleX, expected $($ExpectedGraphicsShellPlacement.ScaleX)."
    }
}

if ($null -ne $ExpectedGraphicsShellPlacement.ScaleY) {
    $ActualScaleY = $MatrixNode.Attributes['scaleY'].Value
    if ($ActualScaleY -ne $ExpectedGraphicsShellPlacement.ScaleY) {
        throw "Graphics shell depth $ExpectedDepth character $ExpectedCharacterId has scaleY $ActualScaleY, expected $($ExpectedGraphicsShellPlacement.ScaleY)."
    }
}
```

- [ ] **Step 2: Run the layout regression and verify it fails on the old panel matrix**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsLayout.ps1 -BatmanRoot C:\dev\helenhook\games\HelenBatmanAA
```

Expected: FAIL because the current implementation still emits depth `3` / character `307` at `translateY="-287"` and `scaleY="1.2487793"`, for example:

```text
Graphics shell depth 3 character 307 has translateY -287, expected -1400.
```

- [ ] **Step 3: Add dedicated panel constants and helpers in `GraphicsOptionsXmlPatcher.cs`**

Add these constants near the existing graphics header constants:

```csharp
/// <summary>
/// Depth used by the retained translucent graphics background panel.
/// </summary>
private const int GraphicsPanelDepth = 3;

/// <summary>
/// Character id used by the retained translucent graphics background panel.
/// </summary>
private const int GraphicsPanelCharacterId = 307;

/// <summary>
/// Target Y translation for the retained translucent graphics background panel.
/// </summary>
private const int GraphicsPanelTranslateY = -1400;

/// <summary>
/// Target X scale for the retained translucent graphics background panel.
/// </summary>
private const string GraphicsPanelScaleX = "2.0688171";

/// <summary>
/// Target Y scale for the retained translucent graphics background panel after extending it to cover most of the screen height.
/// </summary>
private const string GraphicsPanelScaleY = "2.890625";
```

Call the new normalizer from `PatchGraphicsScreenSprite(...)` between shell cleanup and row normalization:

```csharp
private static void PatchGraphicsScreenSprite(XmlElement graphicsSprite)
{
    XmlElement subTags = GetSingleElement(graphicsSprite, "subTags");
    RemoveGameOnlyHelperWidgets(subTags);
    RemoveInnerPanelShellPlacements(subTags);
    List<XmlElement> originalChildren = subTags.ChildNodes.OfType<XmlElement>().ToList();
    EnsureGraphicsRowDepthTimeline(originalChildren);
    RemoveLegacyGameRowDepthTimelines(subTags);
    NormalizeGraphicsHeaderPlacements(subTags);
    NormalizeGraphicsPanelPlacement(subTags);
    NameGraphicsTitlePlacement(subTags);
    NormalizeGraphicsRowPlacements(subTags);
}
```

Add the new panel normalizer beside the existing header normalizer:

```csharp
/// <summary>
/// Resizes only the retained translucent graphics background panel so the full graphics row stack fits inside it.
/// </summary>
/// <param name="subTags">The graphics screen timeline node collection.</param>
private static void NormalizeGraphicsPanelPlacement(XmlElement subTags)
{
    XmlElement placement = FindInitialPlacementAtDepth(subTags, GraphicsPanelDepth);
    if (GetAttribute(placement, "characterId") != GraphicsPanelCharacterId.ToString())
    {
        throw new InvalidOperationException(
            $"Expected retained graphics panel at depth {GraphicsPanelDepth} with character {GraphicsPanelCharacterId}.");
    }

    XmlElement matrix = GetSingleElement(placement, "matrix");
    int sourceTranslateY = ReadRequiredTranslateY(matrix);
    int translateYDelta = GraphicsPanelTranslateY - sourceTranslateY;

    foreach (XmlElement timelinePlacement in subTags.ChildNodes
                 .OfType<XmlElement>()
                 .Where(node =>
                     GetAttribute(node, "type") == "PlaceObject2Tag" &&
                     GetAttribute(node, "depth") == GraphicsPanelDepth.ToString()))
    {
        XmlElement? timelineMatrix = timelinePlacement.ChildNodes
            .OfType<XmlElement>()
            .SingleOrDefault(node => node.Name == "matrix");
        if (timelineMatrix is null)
        {
            continue;
        }

        int currentTranslateY = ReadRequiredTranslateY(timelineMatrix);
        SetTranslateY(timelineMatrix, currentTranslateY + translateYDelta);
        SetScaleX(timelineMatrix, GraphicsPanelScaleX);
        SetScaleY(timelineMatrix, GraphicsPanelScaleY);
    }
}
```

Add matrix scale helpers near the existing translate helpers:

```csharp
/// <summary>
/// Writes a matrix scaleX value while forcing the matrix to stay scaled.
/// </summary>
private static void SetScaleX(XmlElement matrix, string scaleX)
{
    matrix.SetAttribute("hasScale", "true");
    matrix.SetAttribute("scaleX", scaleX);
}

/// <summary>
/// Writes a matrix scaleY value while forcing the matrix to stay scaled.
/// </summary>
private static void SetScaleY(XmlElement matrix, string scaleY)
{
    matrix.SetAttribute("hasScale", "true");
    matrix.SetAttribute("scaleY", scaleY);
}
```

- [ ] **Step 4: Run the layout regression and verify it passes**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsLayout.ps1 -BatmanRoot C:\dev\helenhook\games\HelenBatmanAA
```

Expected: PASS.

- [ ] **Step 5: Run the retail regression and verify non-graphics screens stayed untouched**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Test-BatmanRetailGraphicsOptionsPatch.ps1 -BatmanRoot C:\dev\helenhook\games\HelenBatmanAA -Configuration Debug
```

Expected: PASS with no errors about the retail `Options` header or the clean `ScreenOptionsGamePC` clone source.

- [ ] **Step 6: Commit the code-and-test change**

Run:

```powershell
git -C C:\dev\helenhook\games\HelenBatmanAA add -- scripts/Test-BatmanGraphicsOptionsLayout.ps1 builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsXmlPatcher.cs
git -C C:\dev\helenhook\games\HelenBatmanAA commit -m "Resize graphics background panel vertically"
```

Expected:

```text
[branch] <commit> Resize graphics background panel vertically
```

### Task 2: Rebuild the graphics-options pack and deploy the resized panel

**Files:**
- Modify: `C:/dev/helenhook/games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/assets/deltas/Frontend-graphics-options.hgdelta`
- Modify: `C:/dev/helenhook/games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/files.json`
- Generate: `C:/dev/helenhook/games/HelenBatmanAA/builder/generated/graphics-options-experiment/Frontend-graphics-options.umap`
- Test: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1`

- [ ] **Step 1: Rebuild the graphics-options experiment pack**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Rebuild-BatmanGraphicsOptionsExperiment.ps1 -BatmanRoot C:\dev\helenhook\games\HelenBatmanAA -Configuration Release
```

Expected: success and updated outputs at:

```text
C:\dev\helenhook\games\HelenBatmanAA\builder\generated\graphics-options-experiment\Frontend-graphics-options.umap
C:\dev\helenhook\games\HelenBatmanAA\helengamehook\packs\batman-aa-graphics-options\builds\steam-goty-1.0\assets\deltas\Frontend-graphics-options.hgdelta
C:\dev\helenhook\games\HelenBatmanAA\helengamehook\packs\batman-aa-graphics-options\builds\steam-goty-1.0\files.json
```

- [ ] **Step 2: Run the package regression**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsPackage.ps1 -BatmanRoot C:\dev\helenhook\games\HelenBatmanAA -Configuration Release
```

Expected: PASS.

- [ ] **Step 3: Deploy the rebuilt pack into the live Batman install**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Deploy-BatmanGraphicsOptionsExperiment.ps1 -Configuration Release
```

Expected:

```text
PASS
PASS
DEPLOYED
```

- [ ] **Step 4: Commit the rebuilt tracked pack artifacts**

Run:

```powershell
git -C C:\dev\helenhook\games\HelenBatmanAA add -- helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/assets/deltas/Frontend-graphics-options.hgdelta helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/files.json
git -C C:\dev\helenhook\games\HelenBatmanAA commit -m "Rebuild graphics options pack for panel resize"
```

Expected:

```text
[branch] <commit> Rebuild graphics options pack for panel resize
```

### Task 3: Verify the live graphics screen through navigator-service and capture the resized panel

**Files:**
- Run only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/BatmanWindowHelpers.ps1`
- Run only: `C:/dev/helenui/plugins/navigator-service/src/NavigatorService/NavigatorService.csproj`
- Run only: `C:/dev/helenui/plugins/screenshot-cli/src/ScreenshotCli/bin/Debug/net8.0-windows10.0.19041.0/ScreenshotCli.exe`
- Run only: `C:/dev/helenui/plugins/recognition-cli/src/RecognitionCli/bin/Debug/net8.0/RecognitionCli.exe`
- Generate: `C:/dev/helenui/artifacts/graphics-background-panel/live-panel-final.png`
- Generate: `C:/dev/helenui/artifacts/graphics-background-panel/live-panel-summary.json`

- [ ] **Step 1: Start an elevated navigator-service instance**

Run:

```powershell
dotnet run --project C:\dev\helenui\plugins\navigator-service\src\NavigatorService\NavigatorService.csproj -- serve --port 38416
```

Expected:

```text
[startup] listening http://localhost:38416
```

- [ ] **Step 2: Resolve the live Batman handle**

Run in a separate terminal:

```powershell
. C:\dev\helenhook\games\HelenBatmanAA\scripts\BatmanWindowHelpers.ps1
$Window = Get-BatmanWindowCandidateSnapshot
$Window | Select-Object Handle,Title,ProcessId,ClassName | ConvertTo-Json -Compress
```

Expected: a visible Batman window, for example:

```json
{"Handle":"0x000000000193040E","Title":"BATMAN: ARKHAM ASYLUM","ProcessId":32108,"ClassName":"LaunchUnrealUWindowsClient"}
```

- [ ] **Step 3: Create a handle-bound navigator session and navigate to `surface-graphics-options`**

Run:

```powershell
$Handle = '0x000000000193040E'
$CreateBody = @{
    handle = $Handle
    projectPath = 'C:\dev\helenui\batman-aa.json'
    ocrConfigPath = 'C:\dev\helenui\plugins\recognition-cli\recognition-config.sample.json'
} | ConvertTo-Json

$Created = Invoke-RestMethod -Method Post -Uri http://localhost:38416/sessions -ContentType 'application/json' -Body $CreateBody
$Recognized = Invoke-RestMethod -Method Post -Uri ("http://localhost:38416/sessions/{0}/recognize" -f $Created.sessionId) -ContentType 'application/json' -Body '{}'
$NavigateBody = @{
    targetScreen = 'surface-graphics-options'
    timeoutMs = 120000
    retryLimit = 5
} | ConvertTo-Json
$Navigate = Invoke-RestMethod -Method Post -Uri ("http://localhost:38416/sessions/{0}/navigate" -f $Created.sessionId) -ContentType 'application/json' -Body $NavigateBody
$Summary = Invoke-RestMethod -Method Get -Uri ("http://localhost:38416/sessions/{0}" -f $Created.sessionId)
[pscustomobject]@{
    Created = $Created
    Recognized = $Recognized
    Navigate = $Navigate
    Summary = $Summary
} | ConvertTo-Json -Depth 10 | Set-Content C:\dev\helenui\artifacts\graphics-background-panel\live-panel-summary.json
```

Expected:

```text
Navigate.Status = completed
Summary.CurrentScreenName = GraphicsOptions
```

- [ ] **Step 4: Capture the final live graphics screen by exact handle**

Run:

```powershell
C:\dev\helenui\plugins\screenshot-cli\src\ScreenshotCli\bin\Debug\net8.0-windows10.0.19041.0\ScreenshotCli.exe capture --handle 0x000000000193040E --output C:\dev\helenui\artifacts\graphics-background-panel\live-panel-final.png
```

Expected:

```json
{
  "success": true,
  "window": {
    "title": "BATMAN: ARKHAM ASYLUM",
    "processName": "ShippingPC-BmGame"
  }
}
```

- [ ] **Step 5: Confirm the capture still matches `surface-graphics-options`**

Run:

```powershell
& 'C:\dev\helenui\plugins\recognition-cli\src\RecognitionCli\bin\Debug\net8.0\RecognitionCli.exe' analyze --project C:\dev\helenui\batman-aa.json --image C:\dev\helenui\artifacts\graphics-background-panel\live-panel-final.png --config C:\dev\helenui\plugins\recognition-cli\recognition-config.sample.json
```

Expected:

```text
screen_match.screen_id = surface-graphics-options
```

- [ ] **Step 6: Review the capture and confirm the panel fills roughly `85-90%` of screen height**

Inspect:

```text
C:\dev\helenui\artifacts\graphics-background-panel\live-panel-final.png
```

Expected visual result:

- the translucent gray panel is visibly taller than the current build
- the top sits below the moved `GRAPHICS OPTIONS` header
- the lower graphics rows now sit inside the gray panel instead of dropping below it
- row X positions and the right-side values column remain unchanged

- [ ] **Step 7: Stop and retune only depth `3` if the live panel is still visibly short**

If the panel still does not contain the lower rows, repeat Task 1 with updated depth `3` constants only:

```text
GraphicsPanelTranslateY
GraphicsPanelScaleY
```

Do not modify:

```text
depth 1 / character 141
depth 26 / character 332
GraphicsRowTranslateY
GraphicsHeaderBackingTranslateY
GraphicsHeaderTitleTranslateY
```

Expected: one more measured retune at most, still isolated to the retained panel owner.

### Task 4: Verify the tracked state is clean after the final commits

**Files:**
- Verify only: `C:/dev/helenhook/games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsXmlPatcher.cs`
- Verify only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsLayout.ps1`
- Verify only: `C:/dev/helenhook/games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/assets/deltas/Frontend-graphics-options.hgdelta`
- Verify only: `C:/dev/helenhook/games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/files.json`

- [ ] **Step 1: Check the tracked graphics-panel files for leftover uncommitted changes**

Run:

```powershell
git -C C:\dev\helenhook\games\HelenBatmanAA status --short -- builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsXmlPatcher.cs scripts/Test-BatmanGraphicsOptionsLayout.ps1 helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/assets/deltas/Frontend-graphics-options.hgdelta helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/files.json
```

Expected: no output.
