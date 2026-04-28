# Batman Graphics Options INI Load Design

## Goal

Make `Graphics Options` open with values that reflect the current live `BmEngine.ini` graphics settings. This change only covers menu load/default state. It does not change apply, save, or prompt behavior.

## Problem

The current custom graphics menu reads its startup state through `Helen_RunCommand` and `Helen_GetInt`. Live evidence shows that opening `Graphics Options` does not execute that callback path, so the menu does not refresh from the INI and instead shows stale or default values.

## Constraints

- Scope is limited to loading values into the menu on open.
- Existing apply/save behavior is intentionally left untouched.
- The fix must avoid adding another fragile runtime callback dependency.

## Chosen Approach

Bootstrap the graphics menu startup state from a native INI snapshot injected into the served frontend asset.

### Flow

1. Read the live Batman graphics state from the user INI using the existing native graphics config service.
2. Convert that state into the exact row-state values used by `ScreenOptionsGraphics`.
3. Inject those values into the generated graphics screen startup script so `DraftState` is initialized from the snapshot before any row binding or refresh runs.
4. Leave the existing runtime `Helen_*` path in place for now, but make menu-open correctness independent from it.

## Why This Approach

- It solves the exact user-visible problem: changing the INI outside the game should change what the menu shows on open.
- It removes dependence on the currently unproven `Helen_*` menu-open bridge.
- It is smaller and lower-risk than redesigning the screen around new `FE_*` callbacks.

## Non-Goals

- Fixing `Apply Changes`
- Fixing the `Apply/Discard` banner flow
- Adding new runtime FE hooks
- Refactoring the full graphics options architecture

## Verification

1. Set `Bloom=True` and other known values in the live `BmEngine.ini`.
2. Rebuild and deploy the graphics frontend asset.
3. Open `Graphics Options`.
4. Confirm the rows match the INI immediately on first open.

## Risks

- If the injected startup snapshot and the row-state mapping disagree, the menu could show the wrong labels. This is mitigated by reusing the existing row-state semantics already implemented in `ScreenOptionsGraphics`.
