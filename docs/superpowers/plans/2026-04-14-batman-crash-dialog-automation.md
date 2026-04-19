# Batman Crash Dialog Automation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Batman graphics-menu smoke automation detect modal crash dialogs, OCR their contents, and fail with actionable evidence instead of continuing blindly.

**Architecture:** Extend `screenshot-cli` so scripts can capture a specific top-level window by handle. Add a `CrashDialog` recognition screen to the Batman HelenUI project, then update shared Batman PowerShell helpers so navigation scripts capture either the main game window or a detected dialog window and persist JSON plus OCR artifacts.

**Tech Stack:** PowerShell, .NET 8, xUnit, HelenUI recognition metadata, Windows OCR

---

### Task 1: Add exact-window capture support to screenshot-cli

**Files:**
- Modify: `../helenui/plugins/screenshot-cli/src/ScreenshotCli/Application/ScreenshotCliApplication.cs`
- Modify: `../helenui/plugins/screenshot-cli/src/ScreenshotCli/Windowing/IWindowQueryService.cs`
- Modify: `../helenui/plugins/screenshot-cli/src/ScreenshotCli/Windowing/WindowQueryService.cs`
- Modify: `../helenui/plugins/screenshot-cli/tests/ScreenshotCli.Tests/ProgramTests.cs`
- Modify: `../helenui/plugins/screenshot-cli/tests/ScreenshotCli.Tests/WindowQueryServiceTests.cs`
- Modify: `../helenui/plugins/screenshot-cli/README.md`

- [ ] Add failing tests for `capture --handle` lookup and not-found behavior.
- [ ] Implement `FindByHandle` in the query service and plumb `--handle` through CLI argument parsing.
- [ ] Update CLI docs to describe `--title` and `--handle`.
- [ ] Run the screenshot-cli test suite.

### Task 2: Add a crash dialog screen to the Batman HelenUI project

**Files:**
- Modify: `../helenui/batman-aa.json`

- [ ] Add a `CrashDialog` screen with text-driven recognition clues for common fatal/error dialog language.
- [ ] Keep the screen independent from normal in-game flows so it can be used as a terminal recognition state for dialog captures.

### Task 3: Add reusable dialog capture and OCR helpers

**Files:**
- Modify: `games/HelenBatmanAA/scripts/BatmanWindowHelpers.ps1`
- Add: `games/HelenBatmanAA/scripts/Test-BatmanDialogRecognition.ps1`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanDialogCapture.ps1`

- [ ] Add a failing PowerShell integration test that spawns a synthetic fatal-error message box and expects `CrashDialog` recognition.
- [ ] Implement helper functions to capture a window by handle, analyze it with `recognition-cli`, export artifacts, and raise a single dialog-failure summary.
- [ ] Re-run the dialog helper tests until they pass.

### Task 4: Wire both Batman navigation scripts to the shared helpers

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanNavigateToGraphicsSimple.ps1`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanNavigateToGraphics.ps1`

- [ ] Replace full-desktop screenshots with main-window capture through `screenshot-cli`.
- [ ] Check for dialogs before navigation inputs and before game-window captures.
- [ ] Persist dialog screenshot, OCR JSON, and window snapshot JSON in the existing artifacts directories.
- [ ] Fail fast when no Batman main window exists instead of continuing with blind input.

### Task 5: Verify end to end

**Files:**
- Modify: `games/HelenBatmanAA/README.md`

- [ ] Run focused `screenshot-cli` tests.
- [ ] Run `Test-BatmanDialogCapture.ps1`.
- [ ] Run `Test-BatmanDialogRecognition.ps1`.
- [ ] If Batman is available, run one live navigation smoke to confirm the new capture path records the game window and would stop on a modal.
