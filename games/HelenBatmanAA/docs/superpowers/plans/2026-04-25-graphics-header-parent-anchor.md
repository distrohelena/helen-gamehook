# Graphics Header Parent Anchor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move the `Graphics Options` header into the top `50-100px` band of the graphics screen without moving the graphics rows, the background box, or the main `Options` chooser.

**Architecture:** The approved spec assumed a shared parent transform might be pinning the header in the middle of the screen. Current XML inspection shows the opposite: character `118` is a `DefineShapeTag` and character `340` is a `DefineEditTextTag`, both placed directly on sprite `600` at depths `146/147`. The implementation therefore needs to lock that fact in the regression, retune the actual controlling depth-`146/147` matrices to a top-of-view slot, then rebuild, deploy, and verify the live menu shape.

**Tech Stack:** C#, PowerShell, FFDec XML export, HelenGameHook pack rebuild/deploy scripts, Batman PC retail install

---

## File Map

- Modify: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsLayout.ps1`
  Owns the generated XML regression for sprite `600`. Extend it to prove what controls the header position and to lock the new target Y values.
- Modify: `C:/dev/helenhook/games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsXmlPatcher.cs`
  Owns the constants that currently normalize the graphics header at depths `146/147`.
- Verify only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanRetailGraphicsOptionsPatch.ps1`
  Guards the clean retail `Options` chooser and ensures the main menu header remains unchanged.
- Run only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsPackage.ps1`
  Verifies the rebuilt pack payload and script output.
- Run only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Rebuild-BatmanGraphicsOptionsExperiment.ps1`
  Rebuilds the generated `.umap`, tracked `.hgdelta`, and `files.json`.
- Run only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Deploy-BatmanGraphicsOptionsExperiment.ps1`
  Deploys the rebuilt graphics-options pack into the live Batman install.
- Run only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanNavigateToGraphicsSimple.ps1`
  Navigates the running game to `Graphics Options` for live verification.
- Run only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/BatmanWindowHelpers.ps1`
  Provides the screenshot helper used for the final visual check.

## Placement Target

Use the current live capture as the calibration point. The latest verified `Graphics Options` frame places the title around `214px` from the top of the captured client area. To land in the middle of the requested `50-100px` band, move the header roughly `134px` upward, which is `2680` twips.

Apply that delta to the currently deployed header pair while preserving the `656` twip gap between backing and title:

- Header backing depth `146`: `-1400 -> -4080`
- Header title depth `147`: `-2056 -> -4736`

That is the smallest next target that matches the user request and the current measured header position.

### Task 1: Prove the controlling placements and lock the top-slot target with a failing regression

**Files:**
- Modify: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsLayout.ps1`
- Modify: `C:/dev/helenhook/games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsXmlPatcher.cs`
- Test: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsLayout.ps1`
- Verify: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanRetailGraphicsOptionsPatch.ps1`

- [ ] **Step 1: Write the failing regression additions in `Test-BatmanGraphicsOptionsLayout.ps1`**

Add these assertions after the script loads `$Document` and resolves sprite `600` so the test proves the header controller is the direct `146/147` placements, not a hidden sprite parent:

```powershell
$GraphicsHeaderBackingDefinition = $Document.SelectSingleNode("/swf/tags/item[@type='DefineShapeTag' and @shapeId='118']")
if ($null -eq $GraphicsHeaderBackingDefinition) {
    throw 'Expected character 118 to remain a direct DefineShapeTag backing asset.'
}

$GraphicsHeaderBackingSpriteDefinitions = @(
    $Document.SelectNodes("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='118']")
)
if ($GraphicsHeaderBackingSpriteDefinitions.Count -ne 0) {
    throw 'Character 118 unexpectedly became a nested sprite parent; update the graphics-header plan before patching.'
}

$GraphicsHeaderTitleDefinition = $Document.SelectSingleNode("/swf/tags/item[@type='DefineEditTextTag' and @characterID='340']")
if ($null -eq $GraphicsHeaderTitleDefinition) {
    throw 'Expected character 340 to remain the direct DefineEditTextTag title asset.'
}

$GraphicsHeaderTitleSpriteDefinitions = @(
    $Document.SelectNodes("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='340']")
)
if ($GraphicsHeaderTitleSpriteDefinitions.Count -ne 0) {
    throw 'Character 340 unexpectedly became a nested sprite parent; update the graphics-header plan before patching.'
}
```

Replace the current graphics-shell expectation block with the new target slot:

```powershell
$ExpectedGraphicsShellPlacements = @(
    @{ Depth = '1'; CharacterId = '141'; TranslateX = '-7126'; TranslateY = '899' },
    @{ Depth = '3'; CharacterId = '307'; TranslateX = '-7879'; TranslateY = '-287' },
    @{ Depth = '26'; CharacterId = '332'; TranslateX = '-1641'; TranslateY = '4609' },
    @{ Depth = '146'; CharacterId = '118'; TranslateX = '-6582'; TranslateY = '-4080' },
    @{ Depth = '147'; CharacterId = '340'; TranslateX = '-8292'; TranslateY = '-4736' }
)
```

- [ ] **Step 2: Run the layout regression to verify it fails on the old constants**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsLayout.ps1 -BatmanRoot C:\dev\helenhook\games\HelenBatmanAA
```

Expected: FAIL because the current implementation still emits `-1400 / -2056`, for example:

```text
Graphics shell depth 146 character 118 has translateY -1400, expected -4080.
```

- [ ] **Step 3: Write the minimal implementation in `GraphicsOptionsXmlPatcher.cs`**

Change only the two constants that feed `NormalizeGraphicsHeaderPlacements(...)`:

```csharp
/// <summary>
/// Target Y translation for the graphics header backing block after moving it near the top of the graphics screen.
/// </summary>
private const int GraphicsHeaderBackingTranslateY = -4080;

/// <summary>
/// Target Y translation for the graphics title text after moving it near the top of the graphics screen.
/// </summary>
private const int GraphicsHeaderTitleTranslateY = -4736;
```

Do not change row constants, shell depths, or `NormalizeGraphicsHeaderPlacements(...)` itself in this pass.

- [ ] **Step 4: Run the layout regression to verify it passes**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsLayout.ps1 -BatmanRoot C:\dev\helenhook\games\HelenBatmanAA
```

Expected: PASS.

- [ ] **Step 5: Run the retail regression to verify the main `Options` screen is still untouched**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Test-BatmanRetailGraphicsOptionsPatch.ps1 -BatmanRoot C:\dev\helenhook\games\HelenBatmanAA -Configuration Debug
```

Expected: PASS with no errors about sprite `333`, depths `39/40`, or injected submenu headers.

- [ ] **Step 6: Commit the code-and-test change**

Run:

```powershell
git -C C:\dev\helenhook\games\HelenBatmanAA add -- scripts/Test-BatmanGraphicsOptionsLayout.ps1 builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsXmlPatcher.cs
git -C C:\dev\helenhook\games\HelenBatmanAA commit -m "Retune graphics header top slot"
```

Expected:

```text
[branch] <commit> Retune graphics header top slot
```

### Task 2: Rebuild the graphics-options pack and deploy the tracked artifacts

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

Expected: deployment completes successfully and the install-side verification passes.

- [ ] **Step 4: Commit the rebuilt tracked pack artifacts**

Run:

```powershell
git -C C:\dev\helenhook\games\HelenBatmanAA add -- helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/assets/deltas/Frontend-graphics-options.hgdelta helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/files.json
git -C C:\dev\helenhook\games\HelenBatmanAA commit -m "Rebuild graphics options pack for header slot fix"
```

Expected:

```text
[branch] <commit> Rebuild graphics options pack for header slot fix
```

### Task 3: Verify the live graphics screen shape in-game

**Files:**
- Run only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanNavigateToGraphicsSimple.ps1`
- Run only: `C:/dev/helenhook/games/HelenBatmanAA/scripts/BatmanWindowHelpers.ps1`
- Generate: `C:/dev/helenhook/games/HelenBatmanAA/artifacts/graphics-header-parent-anchor.png`

- [ ] **Step 1: Launch Batman if it is not already running**

Run:

```powershell
Start-Process 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe'
```

Expected: the Batman game window opens.

- [ ] **Step 2: Navigate to the `Graphics Options` screen**

If Batman is already running, use `-NoLaunch`:

```powershell
powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Test-BatmanNavigateToGraphicsSimple.ps1 -NoLaunch
```

If Batman is not running yet, omit `-NoLaunch`:

```powershell
powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Test-BatmanNavigateToGraphicsSimple.ps1
```

Expected: the script reaches the `Graphics Options` menu.

- [ ] **Step 3: Capture the live graphics screen for review**

Run:

```powershell
. C:\dev\helenhook\games\HelenBatmanAA\scripts\BatmanWindowHelpers.ps1
Capture-BatmanMainWindowImage -OutputPath C:\dev\helenhook\games\HelenBatmanAA\artifacts\graphics-header-parent-anchor.png -ScreenshotCliProject C:\dev\helenui\plugins\screenshot-cli | Out-Null
```

Expected: `C:\dev\helenhook\games\HelenBatmanAA\artifacts\graphics-header-parent-anchor.png` exists and shows the live `Graphics Options` screen.

- [ ] **Step 4: Verify the visual result manually**

Confirm these three conditions in the live window or the captured frame:

```text
1. The `Graphics Options` title sits roughly 50-100px from the top of the view.
2. The header backing remains aligned with the title.
3. The graphics rows and large background box have not moved.
```

Expected: only the header pair moved.

- [ ] **Step 5: Confirm the tracked source and pack files remain clean after live verification**

Run:

```powershell
git -C C:\dev\helenhook\games\HelenBatmanAA status --short -- builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsXmlPatcher.cs scripts/Test-BatmanGraphicsOptionsLayout.ps1 helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/assets/deltas/Frontend-graphics-options.hgdelta helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/files.json
```

Expected: no additional tracked changes beyond the two commits created in Tasks 1 and 2.
