# Batman Subtitle Six-Size Restoration Design

## Goal

Restore the Batman gameplay-only subtitle size feature from three presets to six presets without reintroducing any main-menu or frontend modifications, and keep the live in-game update path on the EXE patch flow that is working now.

## Scope

This change applies only to the gameplay subtitle pack, the pause-menu subtitle-size row, the gameplay pack rebuild flow, and the deployed EXE subtitle live-update patch mode.

This change does not modify:

- `Frontend.umap`
- main-menu subtitle UI
- graphics-options work
- unrelated runtime hook paths

## User-Facing Behavior

The in-game pause Audio menu exposes exactly these six subtitle size options in this order:

- `Small`
- `Medium`
- `Large`
- `Very Large`
- `Huge`
- `Massive`

The runtime config default remains `ui.subtitleSize = 1`, so fresh installs start at `Medium`.

The persisted integer states map to subtitle scale values as follows:

- `0` -> `1.0`
- `1` -> `1.5`
- `2` -> `2.0`
- `3` -> `4.0`
- `4` -> `6.0`
- `5` -> `8.0`

The pause-menu row continues to use the FE raw subtitle control codes:

- `4101` -> `Small`
- `4102` -> `Medium`
- `4103` -> `Large`
- `4104` -> `Very Large`
- `4105` -> `Huge`
- `4106` -> `Massive`

Changing the option in the in-game pause Audio menu must:

1. update the visible row value immediately
2. update the live subtitle size in-game
3. persist through menu reopen
4. persist through game restart

## Current Problem

The current repository is split across incompatible assumptions:

- the EXE patcher already recognizes six raw subtitle codes and six scale values
- the gameplay pack metadata still exposes only three persisted states
- the subtitle-size builder template still trims the pause-menu row to three labels
- the regression tests still enforce the three-state layout

That mismatch is the root cause to avoid. The feature must be six-state end-to-end or the tests should fail.

## Chosen Approach

Keep the working architecture from the current gameplay-only implementation and widen it back to six states.

The implementation keeps these responsibilities:

- pause-menu ActionScript owns the in-game row and FE raw-code translation
- gameplay pack metadata owns persisted `ui.subtitleSize` state and startup/apply mapping
- the EXE patcher owns the live subtitle scale update path

The deployed EXE patch mode remains `--ui-state-live`. No main-menu payload, frontend carrier, observer-only live path, or alternate signal-hook path is reintroduced by this restoration.

## Components

### Pause Menu Subtitle Row

`games/HelenBatmanAA/patch-source/PauseRuntimeScaleListItem.as` remains the gameplay pause-menu subtitle-size storage backend.

It must:

- expose six labels in the approved order
- map pause-menu state `0..5` to raw FE control codes `4101..4106`
- map raw FE control codes `4101..4106` back to pause-menu state `0..5`
- preserve the existing live-refresh call pattern that works with the current EXE patch

### Builder Templates

`games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/ScriptTemplates.cs` remains the source for generated pause-menu scripts.

It must stop trimming subtitle-size state to three entries and must emit the six approved labels and six-state FE mapping in the generated pause-menu asset output.

### Gameplay Pack Metadata

The gameplay pack remains the single persisted source of truth for `ui.subtitleSize`.

The following metadata stays gameplay-only and is updated to six states:

- `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/pack.json`
- `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/commands.json`
- generated artifacts written by `games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1`

The pack must:

- keep `ui.subtitleSize` as the config key
- keep default value `1`
- map persisted states `0..5` to the approved six scale values
- keep the pack gameplay-only

### EXE Live Update Path

`games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/Program.cs` already contains the six-code scale path and the `--ui-state-live` scanner path that currently works.

This restoration uses that path as-is unless a failing test proves a gap. The EXE patch mode should remain aligned with the gameplay pause-menu FE raw-code contract and should not switch back to older experimental hook modes during this work.

## Data Flow

1. The gameplay subtitle pack loads.
2. The pack resolves persisted `ui.subtitleSize`, defaulting to `1` when no saved value exists.
3. The pack applies the six-entry startup mapping to establish the current subtitle scale.
4. The player opens the in-game pause Audio menu.
5. The pause-menu row reads the current FE raw code and maps it to one of the six visible labels.
6. The player changes the subtitle size.
7. The pause-menu row writes the matching FE raw code and triggers the existing live-refresh path.
8. The EXE `ui-state-live` patch observes the active UI state and applies the matching live subtitle scale.
9. The saved `ui.subtitleSize` value remains available on later menu opens and future launches.

## Error Handling

This change does not add fallback behavior.

If any layer still assumes three states, verification must fail. The implementation must not silently:

- trim the label list
- collapse states `3..5` back to lower values
- invent default scales for missing mappings
- restore main-menu assets as a workaround

## Testing

Verification must fail unless all of the following are true:

- the generated pause-menu clip action emits the six approved labels in order
- the generated pause-menu `ListItem` emits FE raw codes `4101` through `4106`
- the pause-menu `ListItem` preserves the working live-refresh call path
- the gameplay pack manifest exposes six subtitle size options with default `1`
- the gameplay pack command mapping resolves `0..5` to `1.0`, `1.5`, `2.0`, `4.0`, `6.0`, `8.0`
- the gameplay pack remains gameplay-only and does not introduce frontend payloads
- deployment still patches the EXE with `--ui-state-live`

At minimum, the existing Batman subtitle verification scripts should be updated so they enforce the six-state contract instead of the current three-state contract.

## Non-Goals

This change does not:

- restore or modify main-menu subtitle size UI
- change the subtitle feature into a graphics-options feature
- replace the current EXE live-update route with a different transport
- add resolution-aware subtitle scaling
- change unrelated Batman pack contents

## Implementation Boundaries

Keep implementation limited to the current gameplay subtitle pack, pause-menu builder/template sources, pause-menu override script, rebuild/deploy scripts, and the verification scripts that define this contract.
