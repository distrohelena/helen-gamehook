# Graphics Background Panel Resize Design

## Goal

Increase the height of the large translucent gray background panel on the `Graphics Options` screen so it covers roughly `85-90%` of the visible screen height and contains the full graphics row stack.

## Current Context

The graphics screen is the cloned sprite `600` created from retail `ScreenOptionsGamePC` (`356`) in `GraphicsOptionsXmlPatcher.cs`.

The current graphics patch already:

- removes the inherited inner panel shell depths `21/22/23/25`
- keeps the retained outer shell placements at depths `1/3/26`
- moves the graphics header to the top band at depths `146/147`
- stretches the row stack vertically across the screen with fixed row Y placements

Live captures show that the row stack is now much taller than the remaining gray panel. The panel stops too early, so the lower rows extend below it.

## User-Facing Requirement

The gray panel behind the graphics rows should be tall enough that the visible options list sits inside it, targeting approximately `85-90%` of the screen height.

## Scope

This pass changes only the background panel height on the graphics screen.

It must not:

- move the graphics header again
- move the graphics row Y positions
- change the row count or row order
- change the panel width
- change the left/right text column layout
- alter the retail main `Options` chooser

## Design Options

### 1. Recommended: resize the retained graphics panel vertically

Identify which retained shell placement currently draws the translucent gray panel and change only that placement's vertical geometry.

Why this is preferred:

- smallest diff
- preserves the current screen composition
- keeps the later row-position pass independent

### 2. Restore a removed inner shell layer and resize that instead

This would reintroduce one of the removed inner panel depths and use it as the tall panel.

Why this is not preferred:

- reopens layers intentionally removed from the clone
- increases risk of restoring retail Game Options overlays we do not want

### 3. Inject a brand-new custom panel

This would add a new background layer for the graphics screen.

Why this is not preferred:

- broadest change
- unnecessary while a retained panel layer already exists

## Chosen Design

Resize the existing retained translucent panel vertically, and only vertically.

The implementation must first prove which retained shell placement owns the visible gray panel. The current retained shell depths are `1`, `3`, and `26`; only the panel-owning layer should be resized.

Once the owning layer is confirmed:

- add explicit patch constants for the panel's vertical resize behavior
- apply a deterministic matrix normalization for that layer
- preserve its current horizontal size and X placement
- retune Y so the top of the panel still sits below the moved header while the bottom reaches far enough to contain the lower rows

This keeps the visual change isolated to the background panel and avoids mixing sizing with the later row/box layout pass.

## Implementation Constraints

The patch must not rely on ad hoc manual edits inside exported XML.

The behavior should live in `GraphicsOptionsXmlPatcher.cs` as named constants plus a dedicated normalization helper, following the same pattern already used for:

- button depth rebalancing
- graphics header relocation
- graphics row normalization

The regression must prove both:

- which retained placement is being treated as the panel owner
- the exact matrix values emitted for that placement after resizing

If the retained shell layer cannot be resized cleanly without corrupting the surrounding art, implementation should stop and revise the design before restoring removed shell layers or adding a custom panel.

## Verification

Required verification for this change:

1. `Test-BatmanGraphicsOptionsLayout.ps1`
   It should lock the resized panel-owning shell placement and confirm row/header placements stayed unchanged.

2. `Test-BatmanRetailGraphicsOptionsPatch.ps1`
   It should confirm retail `Options` layout remains untouched.

3. `Test-BatmanGraphicsOptionsPackage.ps1`
   It should confirm the rebuilt pack contains the intended output.

4. Live in-game verification
   The graphics screen should show the gray panel extended to roughly `85-90%` of screen height, with the graphics rows visually inside it.

## Non-Goals

This design does not:

- reposition the panel horizontally
- move or respread the graphics rows
- resize the header backing strip
- retune button text alignment
- change any non-graphics screen
