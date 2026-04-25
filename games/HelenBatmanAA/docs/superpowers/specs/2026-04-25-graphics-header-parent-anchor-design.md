# Graphics Options Header Parent Anchor Design

## Goal

Move the `Graphics Options` header to a true top-of-screen position, roughly `50-100px` from the top of the visible view, without moving the graphics options rows, the background box, or the main `Options` chooser screen.

## Scope

In scope:
- Trace the graphics header backing and title placement chain for depths `146` and `147`
- Identify the transform that actually controls the header's final screen-space Y position
- Update the patcher to move the header by correcting that controlling transform
- Update regression coverage so it asserts the real controlling placement rather than only the leaf local offsets

Out of scope:
- Moving the graphics option rows
- Moving or resizing the graphics background box
- Changing the main `Options` menu screen
- Changing controller or navigator behavior

## Current Evidence

The current patch only normalizes the local Y values on the visible graphics header pair:
- depth `146` backing at `-1400`
- depth `147` title at `-2056`

Those values are enforced by `Test-BatmanGraphicsOptionsLayout.ps1` and applied by `NormalizeGraphicsHeaderPlacements` in `GraphicsOptionsXmlPatcher.cs`.

Despite multiple local-Y tuning passes, the live header still appears around the middle of the screen instead of near the top. That pattern is the important evidence: if the visible result barely changes while the leaf local values move, then the dominant positioning is likely coming from a parent or inherited transform above those placements.

## Root Cause Hypothesis

The graphics header pair is not effectively anchored in absolute screen space. It appears to inherit a higher-level transform from the cloned graphics shell or one of its parent sprites, and that inherited transform is already centered or lowered relative to the visible screen.

Because the current patch only edits the leaf placements at depths `146` and `147`, it is fighting the symptom instead of the real anchor. That is why changing the local values has produced weak or misleading visual movement.

The real fix is to trace the placement ancestry for the header pair, find the smallest shared ancestor that determines the pair's screen-space Y, and reposition that ancestor while preserving the header backing/title relationship.

## Design

Use the smallest reliable change that affects only the header:

1. Export and inspect the current graphics screen timeline and the referenced header sprite chain.
2. Trace the placement ancestry for depths `146` and `147` until reaching the first shared parent transform that contributes to final screen-space Y.
3. Compare that ancestor against the graphics rows and background box to ensure it is header-specific or at least header-only within the modified path.
4. Update `GraphicsOptionsXmlPatcher.cs` to normalize that controlling parent transform instead of relying solely on the leaf `TranslateY` values.
5. Keep the title/backing pair aligned by preserving their existing local relationship unless inspection shows that relationship is itself wrong.
6. Leave all row and box geometry unchanged.

Preferred implementation shape:
- move one shared header ancestor if a clean header-only parent exists
- otherwise move the shallowest safe transform above depths `146/147` and keep the leaf placements as relative offsets only

## Testing

Required checks:

- Add or update layout assertions so the test verifies the transform that actually controls the header's final visual Y position
- Keep the existing checks that ensure the graphics rows and background shell do not move
- Verify the updated test fails against the current implementation before the patch change
- Run `Test-BatmanGraphicsOptionsLayout.ps1`
- Run `Test-BatmanRetailGraphicsOptionsPatch.ps1`
- Rebuild with `Rebuild-BatmanGraphicsOptionsExperiment.ps1`
- Redeploy with `Deploy-BatmanGraphicsOptionsExperiment.ps1`
- Verify in-game that the `Graphics Options` header lands near the top while the rows and box stay where they are

## Risks

- The shared ancestor may also affect another shell element that is visually tied to the header
- The SWF hierarchy may require a combination of parent and leaf adjustments instead of a single transform change
- If the wrong ancestor is moved, the header could separate from its backing or drag extra chrome with it

## Success Criteria

- The `Graphics Options` header appears roughly `50-100px` from the top of the visible screen
- The header backing and title remain aligned with each other
- The graphics option rows do not move
- The graphics background box does not move
- The main `Options` chooser remains unchanged
- The updated layout and retail regression tests pass
