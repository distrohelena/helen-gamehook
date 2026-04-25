# Graphics Panel Horizontal Retune Design

## Goal

Retune the retained translucent gray panel on the `Graphics Options` screen so it:

- stays on the same retained shell owner at depth `3` / character `307`
- keeps the taller height retune already in progress
- becomes `25%` wider
- shifts left by `15%` of the visible screen width
- leaves the header, rows, values column, and other retained shell layers unchanged

## Current Context

The graphics screen currently keeps only three retained shell layers after cloning retail `ScreenOptionsGamePC`:

- depth `1` / character `141`
- depth `3` / character `307`
- depth `26` / character `332`

The large translucent panel is still owned by depth `3` / character `307`. That is the only layer this change should touch.

The current live pass already proved two things:

1. The panel owner is correct and isolated.
2. A height-only retune on depth `3` can enlarge the panel without disturbing the header or the rows.

The remaining visual problem is horizontal fit. The panel still reads too narrow and too centered relative to the row stack, so the next pass should widen it and pull it left without moving the rows.

## Constraints

This pass must not:

- move the `GRAPHICS OPTIONS` header
- move any graphics row or right-side value placement
- resize or reposition depths `1`, `26`, `146`, or `147`
- swap to a different backing asset or add new shell layers

This pass may change only the matrix on the retained panel owner timeline at depth `3`.

## Recommended Approach

Apply a combined transform retune on the retained panel owner at depth `3` / character `307`:

- keep the current retained-panel timeline ownership check
- keep the taller panel retune in the same matrix pass
- multiply the current retained-panel `scaleX` by `1.25`
- shift the retained panel left by a delta representing `15%` of visible screen width
- leave `scaleY` and `translateY` under the in-progress taller-panel retune for the same pass

This is the smallest safe change because it preserves the working screen structure and only retunes the panel matrix that already controls the translucent background.

## Transform Strategy

The implementation should continue to treat depth `3` as the single panel owner and update four matrix values together:

- `translateX`
- `translateY`
- `scaleX`
- `scaleY`

The horizontal retune should be derived from the visible client frame rather than guessed from unrelated retail screens:

- width increase: exact `25%` relative to the current retained-panel `scaleX`
- left shift: exact `15%` of the captured visible screen width, converted into the retained panel's matrix space and then locked into regression

The final implementation should therefore:

1. measure the current retained-panel live result against the captured client width
2. compute the first-pass `translateX` delta for `15%` leftward movement
3. update the regression with the resulting exact matrix values
4. update only the retained panel constants in `GraphicsOptionsXmlPatcher.cs`

## Verification

The change is correct when all of the following hold:

- the layout regression proves depth `3` / character `307` owns the panel and locks the new `translateX`, `translateY`, `scaleX`, and `scaleY`
- the retail regression still passes unchanged
- the rebuilt package still passes package verification
- live navigator verification reaches `surface-graphics-options` through `/navigate`
- live OCR still matches `surface-graphics-options`
- the captured screen shows:
  - a visibly wider panel
  - the panel shifted left relative to the current build
  - the header still clear above the panel
  - the row stack still stationary

## Out of Scope

This design does not cover:

- moving the row stack
- moving the values column
- resizing the right-side shell line or other decorative shell layers
- changing the header anchoring logic
- replacing the retained panel asset
