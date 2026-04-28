# Batman Graphics Options Apply Design

## Goal

Make `Apply Changes` on `Graphics Options` save the current draft, keep the user on the same screen with working input, and disable the button again once the applied state matches the draft.

## Problem

The current custom graphics menu has two separate problems:

1. it mixes applying the draft with screen-exit behavior
2. it depends on a custom `Helen_*` frontend callback route that is not wired through a runtime hook for this pack

The first issue made the earlier `Apply/Discard` work unstable and left the plain `Apply Changes` button behavior unclear.

The second issue is the immediate blocker for row editing and apply:

- the graphics screen reads and writes draft state through `Helen_GetInt`, `Helen_SetInt`, and `Helen_RunCommand`
- the graphics-options pack explicitly ships without `hooks.json`
- known-good frontend customization in this repository uses stock `FE_*` callbacks instead of a custom `Helen_*` route

The current result is predictable:

- menu-open defaults can be baked from the INI and therefore appear correct
- row edits fail because `Helen_SetInt` never reaches a live callback path
- `Apply Changes` cannot be trusted because it also depends on the dead custom route

## Scope

This change covers the runtime path needed to make the in-screen graphics editor functional again.

After a successful apply:

- the user remains on `Graphics Options`
- screen input remains active
- the draft becomes the new baseline state
- `Apply Changes` becomes disabled again

During editing before apply:

- row changes update the local draft immediately
- the screen no longer depends on `Helen_SetInt` for per-row interaction

## Explicit Non-Goals

- No restart-required prompt in this change
- No `Apply/Discard` exit banner behavior changes unless required to share the same fixed apply primitive
- No new custom `Helen_*` bridge
- No resolution mapping changes

## Candidate Approaches

### Approach 1: Keep row edits local and send apply through one proven `FE_*` carrier

Keep draft edits local inside the graphics controller.

Use one proven stock `FE_*` callback route only when the user presses `Apply Changes`.

`ApplyChanges()` remains the single apply primitive and becomes responsible for:

1. dispatching the current draft through a stock `FE_*` carrier event that Batman already executes
2. letting the native side decode that carrier event and write the graphics draft
3. reloading the draft from live state
4. capturing that state as the new initial/baseline snapshot
5. refreshing row visuals and button enabled state

Then make the in-screen button path call only that function and stop mixing it with screen-return behavior.

### Approach 2: Rebuild every row onto the stock `ListItem` `FE_Get/FE_Set` contract

Replace the custom graphics row controller with a stock-style row implementation that reads and writes through `FE_Get...` and `FE_Set...` exactly like vanilla frontend rows.

This is a clean long-term direction, but it is more invasive because every row must be reshaped around the stock screen contract.

### Approach 3: Rebuild the custom `Helen_*` route with a dedicated runtime hook layer

Add a new runtime hook system so `flash.external.ExternalInterface.call("Helen_*", ...)` becomes a real live callback path for this pack.

This is not recommended because it doubles down on the route that already failed and increases runtime complexity for no gain over proven `FE_*` paths.

## Chosen Approach

Use **Approach 1**.

## Why

- It removes the dead per-row bridge dependency immediately.
- It keeps the requested user contract exactly the same.
- It uses a frontend callback family that the game already executes in production.
- It limits native work to one apply-time carrier instead of many row-level callbacks.
- It does not depend on restart logic, which is intentionally deferred.

## Design

### UI contract

When the screen opens:

1. Graphics values are loaded from the active INI-backed draft source.
2. Those values populate the local draft state inside the graphics controller.
3. Row interaction reads and writes only that local draft state.

When the user edits a row:

1. The local draft changes immediately.
2. The row refreshes immediately.
3. The `Apply Changes` row enables whenever the draft differs from the baseline.
4. No native write happens yet.

When the user activates `Apply Changes`:

1. If there are no unsaved changes, nothing happens.
2. If there are unsaved changes, send the full draft through one proven `FE_*` carrier event.
3. The native side decodes that carrier event and applies the Batman graphics draft.
4. If native apply fails, keep the existing failure behavior and do not fake success.
5. If native apply succeeds:
   - reload the menu draft from live state
   - overwrite the initial snapshot with the reloaded draft
   - refresh row labels and focused-row visuals
   - refresh the apply button enabled state
   - keep focus on `Graphics Options`

### Code shape

- Keep `ApplyChanges()` as the central apply primitive.
- Change `SetDraftRowState()` so it only mutates local draft state and UI state. It must stop calling `Helen_SetInt` per row.
- Keep `LoadDraftValues()` for screen-open and post-apply resync only.
- Introduce one explicit apply-time carrier encoder in ActionScript.
- Reuse a proven `FE_*` callback name already known to cross the GFx boundary in Batman.
- Add native decoding for that carrier on the existing proven FE interception route.
- Remove any dependency on exit-prompt or `ReturnFromScreen()` behavior from the in-screen apply path.
- Reuse the same `ApplyChanges()` primitive from exit flows only if needed, but do not let exit-state logic define in-screen apply behavior.

### Carrier shape

The carrier must satisfy two constraints:

1. it must execute through a stock `FE_*` callback path that already works in this game
2. it must encode enough information to apply the full graphics draft deterministically

The recommended carrier is the same family already proven by the subtitle-size work:

- use a stock `FE_*` setter route
- encode graphics draft state into a sentinel payload that native code can recognize unambiguously
- ignore ordinary stock traffic unless it matches the graphics sentinel contract exactly

The exact carrier field layout is an implementation detail. The design requirement is that the payload is deterministic, versionable, and rejected on partial or malformed data.

## Test Strategy

Add a failing regression test first that proves the generated `ScreenOptionsGraphics` script does all of the following:

- row editing does not call `Helen_SetInt`
- `Apply Changes` does not call `ReturnFromScreen()` on successful in-screen apply
- `Apply Changes` uses the chosen stock `FE_*` carrier instead of `Helen_RunCommand`
- the script reloads draft values after apply
- the script captures the new baseline after apply
- the script refreshes button and row state after apply

Add a native or packaging regression check that proves the chosen carrier hook is present and rejects non-graphics traffic.

Then implement the minimal code change to make that test pass.

## Verification

1. Change one graphics option in the menu.
2. Press `Apply Changes`.
3. Confirm the value remains selected.
4. Confirm the user stays on `Graphics Options`.
5. Confirm input still works.
6. Confirm `Apply Changes` becomes disabled again.

## Risks

- If the chosen `FE_*` carrier shares a stock backend side effect, the sentinel filter must prevent collateral writes.
- If apply reloads from the wrong INI source, the button may disable while showing unexpected values. That is acceptable for this step because the active source path was already confirmed separately.
- If the in-screen apply and exit prompt still share hidden state, a narrow follow-up may be needed to keep those paths independent.
