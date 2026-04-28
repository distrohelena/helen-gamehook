# Batman Graphics Options Apply Design

## Goal

Make `Apply Changes` on `Graphics Options` save the current draft, keep the user on the same screen with working input, and disable the button again once the applied state matches the draft.

## Problem

The current custom graphics menu has two separate responsibilities mixed together:

1. applying the draft to Batman graphics state
2. navigating away from the screen or showing exit-related prompts

That coupling made the earlier `Apply/Discard` work unstable and left the plain `Apply Changes` button behavior unclear. The current result is that apply either does nothing, depends on the exit prompt path, or leaves the menu in the wrong input state.

## Scope

This change only covers the in-screen `Apply Changes` button behavior.

After a successful apply:

- the user remains on `Graphics Options`
- screen input remains active
- the draft becomes the new baseline state
- `Apply Changes` becomes disabled again

## Explicit Non-Goals

- No restart-required prompt in this change
- No `Apply/Discard` exit banner behavior changes unless required to share the same fixed apply primitive
- No redesign of the external call bridge
- No resolution mapping changes

## Candidate Approaches

### Approach 1: Fix `ApplyChanges()` as the single source of truth

Keep `ApplyChanges()` responsible for:

1. calling the native apply bridge
2. reloading the draft from live state
3. capturing that state as the new initial/baseline snapshot
4. refreshing row visuals and button enabled state

Then make the in-screen button path call only that function and stop mixing it with screen-return behavior.

### Approach 2: Split apply into a native save path plus a separate UI resync path

Create a new explicit post-apply UI resync method and keep `ApplyChanges()` as a thin bridge wrapper.

This is cleaner architecturally, but it adds another state handoff in a code path that is already fragile.

### Approach 3: Rebuild apply around the stock FE menu contract

Replace the custom apply path with the stock frontend apply conventions used by other Batman menus.

This is the cleanest long-term direction, but it is too large for the current step and would expand scope beyond the requested fix.

## Chosen Approach

Use **Approach 1**.

## Why

- It is the smallest fix that matches the requested behavior exactly.
- It isolates apply from exit/navigation state.
- It gives one place to test: success means native apply ran, draft reloaded, baseline updated, and button state reset.
- It does not depend on restart logic, which is intentionally deferred.

## Design

### UI contract

When the user activates `Apply Changes`:

1. If there are no unsaved changes, nothing happens.
2. If there are unsaved changes, call the native apply bridge.
3. If native apply fails, keep the existing failure behavior and do not fake success.
4. If native apply succeeds:
   - reload the menu draft from live state
   - overwrite the initial snapshot with the reloaded draft
   - refresh row labels and focused-row visuals
   - refresh the apply button enabled state
   - keep focus on `Graphics Options`

### Code shape

- Keep `ApplyChanges()` as the central apply primitive.
- Make the row/button activation path for `Apply Changes` call `ApplyChanges()` directly.
- Remove any dependency on exit-prompt or `ReturnFromScreen()` behavior from the in-screen apply path.
- Reuse the same `ApplyChanges()` primitive from exit flows only if needed, but do not let exit-state logic define in-screen apply behavior.

## Test Strategy

Add a failing regression test first that proves the generated `ScreenOptionsGraphics` script does all of the following for the in-screen apply button:

- calls `Helen_ApplyBatmanGraphicsDraft`
- does not call `ReturnFromScreen()` on successful in-screen apply
- reloads draft values after apply
- captures the new baseline after apply
- refreshes button/row state after apply

Then implement the minimal code change to make that test pass.

## Verification

1. Change one graphics option in the menu.
2. Press `Apply Changes`.
3. Confirm the value remains selected.
4. Confirm the user stays on `Graphics Options`.
5. Confirm input still works.
6. Confirm `Apply Changes` becomes disabled again.

## Risks

- If apply reloads from the wrong INI source, the button may disable while showing unexpected values. That is acceptable for this step because the active source path was already confirmed separately.
- If the in-screen apply and exit prompt still share hidden state, a narrow follow-up may be needed to keep those paths independent.
