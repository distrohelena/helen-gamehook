# Batman Main Menu Checkpoint Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fail Batman menu navigation immediately when the game never reaches the main menu instead of continuing to send blind inputs from the title screen.

**Architecture:** Add shared recognition helpers that can evaluate one analyzed screenshot against an expected Batman screen and summarize the mismatch. Reuse those helpers in the simple graphics navigator to wait for the main menu and options menu before continuing, with screenshots and candidate scores attached to failures.

**Tech Stack:** PowerShell, HelenUI recognition CLI, screenshot CLI, Batman HelenUI project JSON

---

### Task 1: Add the failing regression

**Files:**
- Create: `games/HelenBatmanAA/scripts/Test-BatmanMainMenuCheckpointRecognition.ps1`
- Test data: `C:\dev\helenhook\artifacts\live-main-menu-probe\01_main_before.png`
- Test data: `C:\dev\helenhook\artifacts\nav-graphics\05_main_menu.png`

- [ ] **Step 1: Write the failing test**

```powershell
Assert-BatmanRecognitionExpectedScreen -Recognition $MainRecognition -ExpectedScreenName 'Main'
Assert-BatmanRecognitionExpectedScreen -Recognition $TitleRecognition -ExpectedScreenName 'Main'
```

- [ ] **Step 2: Run test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File '.\games\HelenBatmanAA\scripts\Test-BatmanMainMenuCheckpointRecognition.ps1'`
Expected: FAIL because `Assert-BatmanRecognitionExpectedScreen` does not exist yet.

- [ ] **Step 3: Commit**

```bash
git add docs/superpowers/plans/2026-04-14-batman-main-menu-checkpoint.md games/HelenBatmanAA/scripts/Test-BatmanMainMenuCheckpointRecognition.ps1
git commit -m "test: add batman main-menu checkpoint regression"
```

### Task 2: Add shared recognition checkpoint helpers

**Files:**
- Modify: `games/HelenBatmanAA/scripts/BatmanWindowHelpers.ps1`
- Test: `games/HelenBatmanAA/scripts/Test-BatmanMainMenuCheckpointRecognition.ps1`

- [ ] **Step 1: Write the minimal helper surface**

```powershell
function Get-BatmanRecognitionScreenScore { }
function Get-BatmanRecognitionMatchedScreenName { }
function Get-BatmanRecognitionTopCandidateSummary { }
function Assert-BatmanRecognitionExpectedScreen { }
function Wait-ForBatmanMainWindowExpectedScreen { }
```

- [ ] **Step 2: Run the regression to verify the helper still fails correctly**

Run: `powershell -ExecutionPolicy Bypass -File '.\games\HelenBatmanAA\scripts\Test-BatmanMainMenuCheckpointRecognition.ps1'`
Expected: FAIL until the helper logic is implemented.

- [ ] **Step 3: Implement the minimal recognition helper logic**

```powershell
$Score = Get-BatmanRecognitionScreenScore -Recognition $Recognition -ScreenName $ExpectedScreenName
if ($Score -lt $MinimumScore) {
    throw "$Context expected '$ExpectedScreenName' but found '$MatchedScreen'. Top candidates: $TopCandidates"
}
```

- [ ] **Step 4: Run the regression to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File '.\games\HelenBatmanAA\scripts\Test-BatmanMainMenuCheckpointRecognition.ps1'`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add games/HelenBatmanAA/scripts/BatmanWindowHelpers.ps1 games/HelenBatmanAA/scripts/Test-BatmanMainMenuCheckpointRecognition.ps1
git commit -m "feat: add batman recognition checkpoint helpers"
```

### Task 3: Gate the simple navigator on real menu checkpoints

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanNavigateToGraphicsSimple.ps1`
- Test: `games/HelenBatmanAA/scripts/Test-BatmanMainMenuCheckpointRecognition.ps1`

- [ ] **Step 1: Replace blind post-title progress with checkpointed waits**

```powershell
Wait-ForBatmanMainWindowExpectedScreen -ExpectedScreenName 'Main' -Context 'Main menu did not load after leaving title'
Wait-ForBatmanMainWindowExpectedScreen -ExpectedScreenName 'Options' -Context 'Options menu did not load from the main menu'
```

- [ ] **Step 2: Run the focused regression**

Run: `powershell -ExecutionPolicy Bypass -File '.\games\HelenBatmanAA\scripts\Test-BatmanMainMenuCheckpointRecognition.ps1'`
Expected: PASS

- [ ] **Step 3: Run the live simple navigator**

Run: `powershell -ExecutionPolicy Bypass -File '.\games\HelenBatmanAA\scripts\Test-BatmanNavigateToGraphicsSimple.ps1' -StepDelayMs 700 -MenuDelayMs 3000`
Expected: either
- `Main menu did not load...` with a screenshot that still shows `Title`, or
- successful progression into `Graphics Options`, or
- another explicit checkpoint failure with a screenshot.

- [ ] **Step 4: Commit**

```bash
git add games/HelenBatmanAA/scripts/Test-BatmanNavigateToGraphicsSimple.ps1
git commit -m "feat: gate batman graphics navigation on menu checkpoints"
```
