# Graphics Options Header Top Position Design

## Goal

Move the `Graphics Options` title text and its header backing upward so the pair sits near the top of the graphics screen, around 5% from the top edge. This step must not move the graphics option rows, the large background box, or any other shell elements.

## Scope

In scope:
- Reposition the graphics screen header backing at depth `146`
- Reposition the graphics screen title text at depth `147`
- Preserve the existing `Title` instance naming and ActionScript binding
- Update regression tests to assert the new header placement

Out of scope:
- Moving the graphics option rows
- Moving or resizing the main background box
- Changing row spacing, button order, or controller behavior
- Changing the main `Options` chooser screen

## Current Structure

The graphics screen is cloned from `ScreenOptionsGamePC` and then normalized by `GraphicsOptionsXmlPatcher.cs`. Header placement is currently controlled by:

- `GraphicsHeaderBackingDepth = 146`
- `GraphicsHeaderTitleDepth = 147`
- `GraphicsHeaderBackingTranslateY`
- `GraphicsHeaderTitleTranslateY`
- `NormalizeGraphicsHeaderPlacements(...)`

The layout test `Test-BatmanGraphicsOptionsLayout.ps1` currently locks those two placements to the existing Y coordinates. That test should change first so the old placement fails before production code is updated.

## Design

Use the smallest possible layout change:

1. Keep the graphics header backing and title on depths `146` and `147`
2. Change only the target Y translations used by `NormalizeGraphicsHeaderPlacements(...)`
3. Leave all row depths and row placement logic unchanged
4. Rebuild the graphics-options pack and redeploy it after the code change

This keeps the change isolated to the graphics header pair and reduces the risk of repeating the earlier regression where the wrong header was moved or hidden.

## Testing

Required checks:

- Update `Test-BatmanGraphicsOptionsLayout.ps1` to expect the new header Y values
- Run that test and confirm it fails against the old implementation
- Update `GraphicsOptionsXmlPatcher.cs` to satisfy the new assertions
- Run `Test-BatmanGraphicsOptionsLayout.ps1`
- Run `Test-BatmanRetailGraphicsOptionsPatch.ps1`
- Rebuild with `Rebuild-BatmanGraphicsOptionsExperiment.ps1`
- Redeploy with `Deploy-BatmanGraphicsOptionsExperiment.ps1`

## Risks

- The header may move high enough to clip against the safe-area or overlap the top shell art
- The backing and title could become vertically misaligned if only one depth is updated
- The absolute Y values may need a second adjustment after visual verification in-game

## Success Criteria

- The `Graphics Options` title text and backing appear together near the top of the graphics screen
- The graphics rows remain in their current positions
- The main `Options` chooser remains unchanged
- Layout and retail-regression tests pass after the update
