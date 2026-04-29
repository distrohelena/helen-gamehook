# Batman Subtitle Six-State Signal Hook Design

## Goal

Make the existing gameplay-only Batman subtitle size feature work with six presets by extending the proven live update path instead of relying on the current observer-only route.

The user-visible result must be:

- six pause-menu subtitle size options are shown
- changing the option updates the live subtitle scale
- the selected value persists through menu reopen and game restart
- no main-menu or frontend payload is reintroduced

## Current Failure

The current build mixes two incompatible implementations:

- the pause menu ActionScript writes six `FE_SetControlType` values: `4101` through `4106`
- the deployed gameplay pack persists and reapplies through a `hooks.json` state observer
- the historical implementation that actually worked used the executable subtitle signal hook in `NativeSubtitleExePatcher`
- that native signal hook still only recognizes three codes: `4101`, `4102`, and `4103`

This means the pack currently exposes six states at the UI layer while the proven native route still only defines three states.

## Requirements

### Functional

- keep the feature gameplay-only
- keep the six labels:
  - `Small`
  - `Medium`
  - `Large`
  - `Very Large`
  - `Huge`
  - `Massive`
- keep the six scale mappings:
  - `4101` -> `1.0`
  - `4102` -> `1.5`
  - `4103` -> `2.0`
  - `4104` -> `4.0`
  - `4105` -> `6.0`
  - `4106` -> `8.0`
- keep default persisted value at `Medium`
- opening the pause menu must read the saved value and show the correct label
- changing the value must update the live subtitle scale without restart
- saved value must still apply on next launch

### Non-Functional

- do not add any main-menu or frontend package payload
- do not rely on best-effort observer scanning for the live change path
- do not introduce fallback logic that silently hides a missing signal route
- preserve the current compressed retail `BmGame.u` patching flow

## Chosen Approach

Use the proven executable subtitle signal hook as the only live change transport and extend it from three subtitle codes to six subtitle codes.

The observer-based route remains unnecessary for this feature and should not be the source of truth for live subtitle size changes.

## Design

### 1. ActionScript Contract

`PauseRuntimeScaleListItem.as` remains the pause-menu storage backend.

It continues to:

- read the current value through `FE_GetControlType`
- write the current value through `FE_SetControlType`
- use raw stored codes `4101` through `4106`

No new storage mechanism is introduced here.

### 2. Native Signal Hook Contract

The executable signal hook in `builder/tools/NativeSubtitleExePatcher/Program.cs` is extended from three recognized subtitle codes to six.

The hook must:

- recognize `4101`, `4102`, `4103`, `4104`, `4105`, and `4106`
- map each code directly to the corresponding live subtitle scale
- ignore unrelated `FE_SetControlType` traffic exactly as before
- keep the same proven hook shape and patching strategy unless a failing test proves otherwise

This restores a single direct native boundary between the pause menu write and the live scale update.

### 3. Persistence Contract

Persistence remains tied to the same subtitle size config key already used by the gameplay-only pack:

- config key: `ui.subtitleSize`
- default value: `1`

The saved integer state is still mapped to the six scales through pack metadata and command execution on launch.

The distinction is:

- live change path: direct native subtitle signal hook
- startup replay path: config read plus subtitle scale apply command

Both paths must use the same six-state mapping.

### 4. Pack Metadata

The gameplay-only subtitle pack metadata stays aligned with the six-state contract:

- `pack.json` keeps the six labels and `Medium` default
- `commands.json` keeps the six integer-to-scale mappings

`hooks.json` must not be the mechanism that makes the feature work. If the file remains present for other runtime assets, its subtitle observer content must not be treated as the required live-update path.

### 5. Verification Strategy

This fix is test-first.

Before code changes:

- add a failing verification that rejects a subtitle signal hook build when only `4101..4103` are recognized
- add a failing verification that the generated build contract exposes six live subtitle signal codes

After implementation:

- rebuild the gameplay-only pack
- verify the compressed retail `BmGame.u` patch still succeeds
- verify the gameplay package tests pass
- verify installed-base compatibility still passes
- deploy and validate against the live game

## Out of Scope

- any graphics options work
- any main-menu subtitle option work
- changing subtitle labels beyond the current six approved labels
- introducing resolution-based naming
- redesigning the hook architecture beyond extending the known working subtitle signal path

## Risks

### Risk: Hook implementation assumptions are still hardcoded to three states

Mitigation:

- add explicit failing tests against the native signal mapping
- update the mapping in one place and verify generated artifacts reflect all six codes

### Risk: Observer-based logic still conflicts with the direct signal path

Mitigation:

- keep the direct signal path as the source of truth
- do not depend on observer state changes for correctness

### Risk: Rebuild scripts drift from deployed runtime expectations

Mitigation:

- verify the rebuilt pack contents
- verify installed base compatibility before deployment

## Success Criteria

The work is complete only when all of the following are true:

- the pause menu shows six subtitle size options
- selecting each option changes the live subtitle scale
- leaving and reopening the pause menu preserves the selected value
- restarting the game preserves the selected value
- no main-menu behavior is modified
- the game launches cleanly with the subtitle pack installed
