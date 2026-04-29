# Batman Subtitle Six-Preset Design

## Goal
Expand the gameplay-only Batman subtitle size setting from three presets to six presets without reintroducing any main-menu modifications.

## Scope
This change applies only to the gameplay subtitle pack. It does not change `Frontend.umap`, main-menu assets, or any graphics-options work.

## User-Facing Behavior
The subtitle size setting exposes six neutral labels so the feature feels native to the game:

- `Small`
- `Medium`
- `Large`
- `Very Large`
- `Huge`
- `Massive`

The labels map to runtime subtitle scale values as follows:

- `0` -> `1.0`
- `1` -> `1.5`
- `2` -> `2.0`
- `3` -> `4.0`
- `4` -> `6.0`
- `5` -> `8.0`

`Medium` remains the default preset, which means the default persisted `ui.subtitleSize` value is `1`.

## Architecture
The existing gameplay-only subtitle pack remains the single source of truth for subtitle scale behavior. The implementation updates the pack metadata that translates `ui.subtitleSize` into the runtime subtitle scale. No new runtime bridge, observer, or frontend carrier is introduced.

## Components
### Subtitle Pack Command Mapping
The pack `commands.json` file currently contains the preset ladder for subtitle size. This file will be updated to define the six approved presets and their exact scale values.

### Subtitle Pack Labels
The pack metadata that exposes user-facing preset names will be updated from the current three-option set to the six approved neutral labels.

### Runtime Config Default
The default runtime configuration should use `ui.subtitleSize = 1` so fresh installs start at `Medium`. Existing persisted values are preserved; this change only affects the default when no saved value exists.

## Data Flow
1. The game loads the gameplay-only subtitle pack.
2. The pack reads persisted `ui.subtitleSize` from runtime configuration.
3. If no persisted value exists, the runtime default supplies `1`.
4. The pack resolves that integer through the six-entry preset mapping.
5. The resolved scale is applied to gameplay subtitles exactly as the current working subtitle pack does today.

## Error Handling
This feature does not add fallback behavior. The preset mapping must be explicit and complete. If a required preset entry is missing from pack metadata or verification fixtures, tests should fail rather than silently inventing a substitute mapping.

## Testing
Add focused verification that asserts:

- the subtitle pack remains gameplay-only
- the pack contains six subtitle size labels in the approved order
- the pack maps `0..5` to `1.0`, `1.5`, `2.0`, `4.0`, `6.0`, `8.0`
- the default runtime config value for `ui.subtitleSize` is `1`
- no frontend payload is introduced by this change

## Non-Goals
This change does not:

- add or restore a main-menu subtitle UI
- change subtitle persistence mechanics beyond the default value
- change graphics options or any Batman frontend patching
- make subtitle size resolution-aware at runtime

## Implementation Boundaries
Keep the implementation limited to the existing gameplay subtitle pack, its default runtime config, and its verification scripts. Do not mix this work with graphics-options code or other experimental pack paths.
