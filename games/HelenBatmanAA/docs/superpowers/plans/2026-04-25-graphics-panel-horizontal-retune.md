# Graphics Panel Horizontal Retune Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Retune the retained translucent `Graphics Options` background panel so it is taller, `25%` wider, and shifted left by `15%` of the screen while leaving the header, rows, and other shell layers unchanged.

**Architecture:** The graphics screen still uses the retained shell owner at depth `3` / character `307` as the only panel layer that should move. This plan updates only that matrix, locks the new transform in the exported-layout regression, rebuilds and deploys the pack sequentially, then verifies the live screen through the working handle-bound navigator path.

**Tech Stack:** C#, PowerShell, FFDec XML export, HelenGameHook rebuild/deploy scripts, HelenUI navigator-service, screenshot-cli, RecognitionCli

---

## File Map

- Modify: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsLayout.ps1`
  Owns the exported `MainV2` regression. Update the depth-`3` expected matrix to lock the combined horizontal and vertical retune while keeping the other retained shell layers unchanged.
- Modify: `C:/dev/helenhook/games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsXmlPatcher.cs`
  Owns the graphics-screen clone patch. Extend the existing retained-panel normalizer so it adjusts X as well as Y and scale.
- Verify only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanRetailGraphicsOptionsPatch.ps1`
  Guards the untouched retail `Options` screen and proves the panel retune stays isolated to the cloned graphics screen.
- Run only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Rebuild-BatmanGraphicsOptionsExperiment.ps1`
  Rebuilds the generated `.umap`, tracked `.hgdelta`, and `files.json`.
- Run only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1`
  Verifies the rebuilt graphics-options payload. This must run by itself, not in parallel with deploy, because both commands touch the same verification export tree.
- Run only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Deploy-BatmanGraphicsOptionsExperiment.ps1`
  Deploys the rebuilt pack into the live Batman install. This must run after the package test completes.
- Run only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/BatmanWindowHelpers.ps1`
  Resolves the live Batman handle used by navigator-service and screenshot-cli.
- Run only: `C:/dev/helenui/plugins/navigator-service/src/NavigatorService/NavigatorService.csproj`
  Provides the verified `/navigate` path used to reach `surface-graphics-options` without manual key sends.
- Run only: `C:/dev/helenui/plugins/screenshot-cli/src/ScreenshotCli/bin/Debug/net8.0-windows10.0.19041.0/ScreenshotCli.exe`
  Captures the live graphics screen by exact HWND after navigation completes.
- Run only: `C:/dev/helenui/plugins/recognition-cli/src/RecognitionCli/bin/Debug/net8.0/RecognitionCli.exe`
  Confirms the final live capture still matches `surface-graphics-options`.

## Verified Screen-Width Mapping

The exported movie at `C:/dev/helenhook/games/HelenBatmanAA/builder/generated/main-menu-graphics/MainV2-graphics-options.xml` declares:

```xml
<displayRect type="RECT" Xmax="20480" Xmin="0" Ymax="15360" Ymin="0" nbits="16"/>
```

That gives an exact stage width of `20480` matrix units, so the approved left shift of `15%` of screen width is:

```text
20480 * 0.15 = 3072
```

## Placement Target

Use the current committed/deployed retained-panel baseline plus the approved combined retune:

- current committed/deployed depth `3` / character `307`
  - `translateX = -7879`
  - `translateY = -4600`
  - `scaleX = 2.0688171`
  - `scaleY = 4.625`
- target depth `3` / character `307`
  - `translateX = -10951`
    - current `-7879` minus `3072`
  - `translateY = -5500`
  - `scaleX = 2.5860214`
    - `2.0688171 * 1.25 = 2.586021375`, rounded to `7` decimal places
  - `scaleY = 5.125`

Depths `1`, `26`, `146`, and `147` must remain unchanged in this plan.

### Task 1: Lock the combined panel retune in regression and implement the retained-panel X/Y/scale normalizer

**Files:**
- Modify: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsLayout.ps1`
- Modify: `C:/dev/helenhook/games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsXmlPatcher.cs`
- Test: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsLayout.ps1`
- Verify: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanRetailGraphicsOptionsPatch.ps1`

- [ ] **Step 1: Update the layout regression with the exact depth-`3` combined target**

Replace the expected depth-`3` shell placement in `Test-BatmanGraphicsOptionsLayout.ps1` with:

```powershell
@{
    Depth = '3'
    CharacterId = '307'
    TranslateX = '-10951'
    TranslateY = '-5500'
    HasScale = 'true'
    ScaleX = '2.5860214'
    ScaleY = '5.125'
},
```

Keep the rest of the retained shell expectation block unchanged, including:

```powershell
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
    HasScale = 'false'
    ScaleX = $null
    ScaleY = $null
}
```

- [ ] **Step 2: Run the layout regression and verify it fails on the current committed implementation**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsLayout.ps1 -BatmanRoot C:\dev\helenhook\games\HelenBatmanAA
```

Expected: FAIL because the current implementation still emits the committed panel matrix, for example:

```text
Graphics shell depth 3 character 307 has translateX -7879, expected -10951.
```

or:

```text
Graphics shell depth 3 character 307 has translateY -4600, expected -5500.
```

- [ ] **Step 3: Add exact X/Y/scale constants for the retained panel in `GraphicsOptionsXmlPatcher.cs`**

Set the retained-panel constants to:

```csharp
/// <summary>
/// Target X translation for the retained translucent graphics background panel after shifting it left by 15% of screen width.
/// </summary>
private const int GraphicsPanelTranslateX = -10951;

/// <summary>
/// Target Y translation for the retained translucent graphics background panel.
/// </summary>
private const int GraphicsPanelTranslateY = -5500;

/// <summary>
/// Target X scale for the retained translucent graphics background panel after widening it by 25%.
/// </summary>
private const string GraphicsPanelScaleX = "2.5860214";

/// <summary>
/// Target Y scale for the retained translucent graphics background panel after extending it to cover more of the screen height.
/// </summary>
private const string GraphicsPanelScaleY = "5.125";
```

- [ ] **Step 4: Add the missing translate-X reader helper**

Add this helper near `ReadRequiredTranslateY(...)`:

```csharp
/// <summary>
/// Reads a matrix translateX value.
/// </summary>
/// <param name="matrix">The matrix element to inspect.</param>
/// <returns>The parsed translateX value.</returns>
private static int ReadRequiredTranslateX(XmlElement matrix)
{
    string? translateXText = GetAttribute(matrix, "translateX");
    if (translateXText is null)
    {
        throw new InvalidOperationException("Expected matrix translateX attribute.");
    }

    return int.Parse(translateXText);
}
```

- [ ] **Step 5: Extend `NormalizeGraphicsPanelPlacement(...)` so it applies the X retune as well**

Replace the retained-panel normalizer body with:

```csharp
/// <summary>
/// Retunes only the retained translucent graphics background panel so it becomes taller, wider, and shifted left without disturbing the row stack.
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
    int sourceTranslateX = ReadRequiredTranslateX(matrix);
    int sourceTranslateY = ReadRequiredTranslateY(matrix);
    int translateXDelta = GraphicsPanelTranslateX - sourceTranslateX;
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

        int currentTranslateX = ReadRequiredTranslateX(timelineMatrix);
        int currentTranslateY = ReadRequiredTranslateY(timelineMatrix);
        SetTranslateX(timelineMatrix, currentTranslateX + translateXDelta);
        SetTranslateY(timelineMatrix, currentTranslateY + translateYDelta);
        SetScaleX(timelineMatrix, GraphicsPanelScaleX);
        SetScaleY(timelineMatrix, GraphicsPanelScaleY);
    }
}
```

- [ ] **Step 6: Run the layout regression and verify it passes**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsLayout.ps1 -BatmanRoot C:\dev\helenhook\games\HelenBatmanAA
```

Expected:

```text
PASS
```

- [ ] **Step 7: Run the retail regression and verify the retune stayed isolated**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Test-BatmanRetailGraphicsOptionsPatch.ps1 -BatmanRoot C:\dev\helenhook\games\HelenBatmanAA -Configuration Debug
```

Expected:

```text
PASS
```

- [ ] **Step 8: Commit the code-and-test change**

Run:

```powershell
git -C C:\dev\helenhook\games\HelenBatmanAA add -- scripts/Test-BatmanGraphicsOptionsLayout.ps1 builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsXmlPatcher.cs
git -C C:\dev\helenhook\games\HelenBatmanAA commit -m "Retune graphics panel width and offset"
```

Expected:

```text
[branch] <commit> Retune graphics panel width and offset
```

### Task 2: Rebuild the graphics-options pack and deploy the combined retune

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

- [ ] **Step 2: Run the package regression by itself**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsPackage.ps1 -BatmanRoot C:\dev\helenhook\games\HelenBatmanAA -Configuration Release
```

Expected:

```text
PASS
```

Important: do not run deploy in parallel with this command. Both commands touch the same verification export tree and can fail with locked files or missing exports if started together.

- [ ] **Step 3: Deploy the rebuilt pack after the package test finishes**

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
git -C C:\dev\helenhook\games\HelenBatmanAA commit -m "Rebuild graphics options pack for horizontal retune"
```

Expected:

```text
[branch] <commit> Rebuild graphics options pack for horizontal retune
```

### Task 3: Verify the live graphics screen through navigator-service and handle-bound capture

**Files:**
- Run only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/BatmanWindowHelpers.ps1`
- Run only: `C:/dev/helenui/plugins/navigator-service/src/NavigatorService/NavigatorService.csproj`
- Run only: `C:/dev/helenui/plugins/screenshot-cli/src/ScreenshotCli/bin/Debug/net8.0-windows10.0.19041.0/ScreenshotCli.exe`
- Run only: `C:/dev/helenui/plugins/recognition-cli/src/RecognitionCli/bin/Debug/net8.0/RecognitionCli.exe`
- Generate: `C:/dev/helenui/artifacts/graphics-panel-horizontal-retune/live-panel-final.png`
- Generate: `C:/dev/helenui/artifacts/graphics-panel-horizontal-retune/live-panel-summary.json`

- [ ] **Step 1: Start or restart navigator-service on port `38416`**

Run:

```powershell
dotnet run --project C:\dev\helenui\plugins\navigator-service\src\NavigatorService\NavigatorService.csproj -- serve --port 38416
```

Expected:

```text
[startup] listening http://localhost:38416
```

- [ ] **Step 2: Restart Batman so it loads the newly deployed frontend pack**

Run:

```powershell
Get-Process ShippingPC-BmGame -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Process -FilePath 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe'
```

Expected: Batman relaunches and exposes a game window after the splash screen.

- [ ] **Step 3: Resolve the live Batman handle**

Run:

```powershell
. C:\dev\helenhook\games\HelenBatmanAA\scripts\BatmanWindowHelpers.ps1
$Window = Get-BatmanWindowCandidateSnapshot
$Window | Select-Object Handle,Title,ProcessId,ClassName | ConvertTo-Json -Compress
```

Expected: a visible Batman game window, for example:

```json
{"Handle":"0x00000000005C130C","Title":"BATMAN: ARKHAM ASYLUM","ProcessId":41380,"ClassName":"LaunchUnrealUWindowsClient"}
```

- [ ] **Step 4: Create a handle-bound session and navigate to `surface-graphics-options`**

Run:

```powershell
$ArtifactDir = 'C:\dev\helenui\artifacts\graphics-panel-horizontal-retune'
New-Item -ItemType Directory -Force -Path $ArtifactDir | Out-Null
$Handle = '0x00000000005C130C'
$CreateBody = @{
    handle = $Handle
    projectPath = 'C:\dev\helenui\batman-aa.json'
    ocrConfigPath = 'C:\dev\helenui\plugins\recognition-cli\recognition-config.sample.json'
} | ConvertTo-Json
$Created = Invoke-RestMethod -Method Post -Uri 'http://localhost:38416/sessions' -ContentType 'application/json' -Body $CreateBody
$Recognized = Invoke-RestMethod -Method Post -Uri ("http://localhost:38416/sessions/{0}/recognize" -f $Created.sessionId) -ContentType 'application/json' -Body '{}'
$NavigateBody = @{
    targetScreen = 'surface-graphics-options'
    timeoutMs = 120000
    retryLimit = 5
} | ConvertTo-Json
$Navigate = Invoke-RestMethod -Method Post -Uri ("http://localhost:38416/sessions/{0}/navigate" -f $Created.sessionId) -ContentType 'application/json' -Body $NavigateBody
$Summary = Invoke-RestMethod -Method Get -Uri ("http://localhost:38416/sessions/{0}" -f $Created.sessionId)
[pscustomobject]@{
    Handle = $Handle
    Created = $Created
    Recognized = $Recognized
    Navigate = $Navigate
    Summary = $Summary
} | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $ArtifactDir 'live-panel-summary.json')
```

Expected:

```text
Navigate.Status = completed
Summary.CurrentScreenName = GraphicsOptions
```

- [ ] **Step 5: Capture the final live graphics screen by exact handle**

Run:

```powershell
C:\dev\helenui\plugins\screenshot-cli\src\ScreenshotCli\bin\Debug\net8.0-windows10.0.19041.0\ScreenshotCli.exe capture --handle 0x00000000005C130C --output C:\dev\helenui\artifacts\graphics-panel-horizontal-retune\live-panel-final.png
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

- [ ] **Step 6: Confirm the capture still matches `surface-graphics-options`**

Run:

```powershell
& 'C:\dev\helenui\plugins\recognition-cli\src\RecognitionCli\bin\Debug\net8.0\RecognitionCli.exe' analyze --project C:\dev\helenui\batman-aa.json --image C:\dev\helenui\artifacts\graphics-panel-horizontal-retune\live-panel-final.png --config C:\dev\helenui\plugins\recognition-cli\recognition-config.sample.json
```

Expected:

```text
screen_match.screen_id = surface-graphics-options
```

- [ ] **Step 7: Review the capture for the approved visual changes**

Inspect:

```text
C:\dev\helenui\artifacts\graphics-panel-horizontal-retune\live-panel-final.png
```

Expected visual result:

- the translucent gray panel is visibly wider than the current build
- the panel sits further left by a noticeable amount
- the panel is still taller than the committed `-4600 / 4.625` build
- the header remains above the panel
- the graphics rows and right-side values remain stationary

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
