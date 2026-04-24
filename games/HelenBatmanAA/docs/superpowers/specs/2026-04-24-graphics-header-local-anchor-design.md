# Graphics Options Header Local Anchor Design

## Goal

Move the `Graphics Options` header pair higher on the graphics screen by applying a large negative local-space Y offset, around `-2000`, to the header backing and title text. The intent is to compensate for the inherited parent transform that is already anchored partway down the screen.

## Scope

In scope:
- Reposition the graphics header backing timeline on depth `146`
- Reposition the graphics header title timeline on depth `147`
- Update layout regression coverage to assert the new local-space placement strategy

Out of scope:
- Moving the graphics options rows
- Moving or resizing the background box
- Changing the main `Options` chooser screen
- Changing input, focus, or navigation behavior

## Root Cause Hypothesis

The current graphics header move is too conservative because it treats the header placements as if they were anchored directly in screen space. In practice, the inherited graphics header appears to live under a parent transform that is already positioned roughly midway down the screen. Because of that extra inherited offset, a small upward local translation does not lift the header far enough visually.

The safest correction is to keep the same depths and timelines but change the local-space Y target to a much larger negative value so the final composed position lands near the top of the screen.

## Design

Use the smallest possible change:

1. Keep the existing graphics header depths:
   - `GraphicsHeaderBackingDepth = 146`
   - `GraphicsHeaderTitleDepth = 147`
2. Change only the local Y targets used for those two header placements
3. Target a local Y around `-2000` for both header elements so the pair moves together
4. Leave all other graphics screen placements unchanged

This preserves the current patch structure while adapting to the actual inherited transform stack instead of trying to relocate more of the screen.

## Testing

Required checks:

- Update `Test-BatmanGraphicsOptionsLayout.ps1` to expect the new large negative local Y values for depths `146` and `147`
- Verify that the updated test fails against the old implementation
- Update `GraphicsOptionsXmlPatcher.cs` to satisfy the new assertions
- Run `Test-BatmanGraphicsOptionsLayout.ps1`
- Run `Test-BatmanRetailGraphicsOptionsPatch.ps1`
- Rebuild with `Rebuild-BatmanGraphicsOptionsExperiment.ps1`
- Redeploy with `Deploy-BatmanGraphicsOptionsExperiment.ps1`
- Verify in-game that only the `Graphics Options` header pair moved

## Risks

- `-2000` may overshoot and push the header too high if the inherited transform is not as large as expected
- The title and backing could drift apart if only one depth is updated correctly
- A second tuning pass may still be needed after in-game verification

## Success Criteria

- The `Graphics Options` title and header backing appear together near the top of the graphics screen
- The background box and graphics option rows do not move
- The main `Options` chooser remains unchanged
- Layout and retail regression tests still pass
