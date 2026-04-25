# Graphics Options Row Stack Center Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move the fixed `Graphics Options` row stack `5%` of screen width left, `20%` of screen height up, and widen row spacing so the options fill more of the enlarged panel without colliding with the header.

**Architecture:** Keep the existing row normalization pipeline. Change only the row X anchor and row Y constants, then lock those emitted matrix values in the layout regression.

**Tech Stack:** C#/.NET 8 patcher, PowerShell regression scripts, existing Batman graphics-options rebuild/deploy scripts.

---

### Task 1: Lock The New Row Placement

**Files:**
- Modify: `C:/dev/helenhook/games/HelenBatmanAA/scripts/Test-BatmanGraphicsOptionsLayout.ps1`

- [ ] **Step 1: Add row X expectation and update row Y expectations**

Change `$ExpectedGraphicsRowTranslateYByDepth` to these values:

```powershell
$ExpectedGraphicsRowTranslateYByDepth = @{
    '141' = '-4452'
    '133' = '-3912'
    '125' = '-3372'
    '117' = '-2832'
    '109' = '-2292'
    '101' = '-1752'
    '93' = '-1212'
    '85' = '-672'
    '77' = '-132'
    '69' = '408'
    '61' = '948'
    '53' = '1488'
    '45' = '2028'
    '37' = '2568'
    '29' = '3108'
}
```

Add this expected row X anchor after that hash:

```powershell
$ExpectedGraphicsRowTranslateX = '-1805'
```

In the row placement loop, assert `translateX` equals `$ExpectedGraphicsRowTranslateX` before asserting `translateY`.

- [ ] **Step 2: Run the layout regression and confirm it fails**

Run:

```powershell
rtk powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsLayout.ps1
```

Expected: failure showing an old row `translateX` or `translateY`.

### Task 2: Move The Row Stack

**Files:**
- Modify: `C:/dev/helenhook/games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsXmlPatcher.cs`

- [ ] **Step 1: Update the row X anchor**

Rename or replace `ListTemplateTranslateX` with this value:

```csharp
private const int ListTemplateTranslateX = -1805;
```

- [ ] **Step 2: Update the row Y coordinates**

Replace `GraphicsRowTranslateY` with:

```csharp
private static readonly int[] GraphicsRowTranslateY =
[
    -4452,
    -3912,
    -3372,
    -2832,
    -2292,
    -1752,
    -1212,
    -672,
    -132,
    408,
    948,
    1488,
    2028,
    2568,
    3108
];
```

- [ ] **Step 3: Run the layout regression and confirm it passes**

Run:

```powershell
rtk powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsLayout.ps1
```

Expected: `PASS`.

### Task 3: Rebuild, Deploy, Verify, Commit

**Files:**
- Modify: `C:/dev/helenhook/games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/assets/deltas/Frontend-graphics-options.hgdelta`
- Modify: `C:/dev/helenhook/games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/files.json`

- [ ] **Step 1: Rebuild the graphics-options pack**

Run:

```powershell
rtk powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Rebuild-BatmanGraphicsOptionsExperiment.ps1
```

Expected: rebuilt prototype, frontend package, delta, and manifest.

- [ ] **Step 2: Run package verification**

Run:

```powershell
rtk powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Test-BatmanGraphicsOptionsPackage.ps1
```

Expected: `PASS`.

- [ ] **Step 3: Deploy the graphics-options pack**

Run:

```powershell
rtk powershell -ExecutionPolicy Bypass -File C:\dev\helenhook\games\HelenBatmanAA\scripts\Deploy-BatmanGraphicsOptionsExperiment.ps1
```

Expected: `DEPLOYED`.

- [ ] **Step 4: Commit only the row-stack files**

Run:

```powershell
rtk git add -- builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsXmlPatcher.cs scripts/Test-BatmanGraphicsOptionsLayout.ps1 helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/assets/deltas/Frontend-graphics-options.hgdelta helengamehook/packs/batman-aa-graphics-options/builds/steam-goty-1.0/files.json docs/superpowers/plans/2026-04-25-graphics-options-row-stack-center.md
rtk git commit -m "Center graphics options row stack"
```
