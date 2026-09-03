# Batman Graphics Detail and Quality Options Design

## Summary

Activate the eight remaining detail-quality rows in Batman: Arkham Asylum GOTY's in-game Graphics Options shell while preserving the proven startup, transaction, rollback, and package-provenance architecture.

The active milestone includes Detail Level plus seven INI-backed quality toggles. Fullscreen and Resolution remain honest `Not active` placeholders and are reserved for a separate display-mode milestone.

## Goals

- Make Detail Level, Bloom, Dynamic Shadows, Motion Blur, Distortion, Fog Volumes, Spherical Harmonic Lighting, and Ambient Occlusion selectable.
- Load the seven persisted quality values from launcher-owned `UserEngine.ini` on the first menu opening.
- Derive Detail Level from the seven leaf values without creating a competing persisted authority.
- Preserve the existing serialized Apply, acknowledgement, commit, and rollback flow.
- Preserve one broad discovery scan per unresolved graphics address group per polling pass.
- Persist successful changes to both launcher-owned `UserEngine.ini` and generated `BmEngine.ini`.
- Build the package exclusively from checked-in repository sources and the verified retail input.

## Non-goals

- Do not activate Fullscreen or Resolution in this milestone.
- Do not apply display-mode changes or enumerate supported resolutions.
- Do not revive the historical full graphics controller, `Helen_*` callback bridge, or exit-prompt path.
- Do not cache absolute process memory addresses across launches.
- Do not use `batma/`, an installed pack, `F:\helenhook.7z`, or historical generated artifacts as build inputs.
- Do not change the already working VSync, MSAA, PhysX, Stereo 3D, Apply, or rollback semantics.

## User-visible behavior

### Active rows

The menu exposes these values:

| Row | Values |
| --- | --- |
| Detail Level | Low, Medium, High, Very High, Custom |
| Bloom | Off, On |
| Dynamic Shadows | Off, On |
| Motion Blur | Off, On |
| Distortion | Off, On |
| Fog Volumes | Off, On |
| Spherical Harmonic Lighting | Off, On |
| Ambient Occlusion | Off, On |

`Custom` is display-only. The user cycles Detail Level through Low, Medium, High, and Very High; Custom appears only when the leaf-toggle combination does not match a canonical preset.

### Preset definitions

| Preset | Bloom | Dynamic Shadows | Motion Blur | Distortion | Fog Volumes | Spherical Harmonic Lighting | Ambient Occlusion |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Low | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Medium | 1 | 1 | 0 | 0 | 0 | 0 | 0 |
| High | 1 | 1 | 1 | 1 | 1 | 1 | 0 |
| Very High | 1 | 1 | 1 | 1 | 1 | 1 | 1 |

Selecting a preset updates all seven leaf drafts immediately. Changing any leaf recomputes the displayed Detail Level. No menu interaction writes an INI until Apply Changes completes.

Fullscreen and Resolution continue to display `Not active` and remain unselectable.

## Authority and state model

`BatmanGraphicsConfigService::LoadIntoDispatcher` remains the startup authority and reads the sibling `UserEngine.ini`. It populates the seven leaf keys and the derived `detailLevel` key before observer polling starts.

The shell does not transmit a Detail Level value. It initializes and recomputes Detail Level from the seven leaf responses. This avoids two authorities for the same persisted state and prevents preset writes from racing individual leaf writes.

Each leaf observer maps a menu write into its existing dispatcher key and runs the existing `sync-batman-graphics-detail-level` command. During a multi-leaf transaction the derived dispatcher value may pass through intermediate states, but the last acknowledged leaf produces the final derived state before commit.

Apply continues to persist the full graphics draft through `BatmanGraphicsConfigService::ApplyFromDispatcher`. A successful apply updates both `UserEngine.ini` and `BmEngine.ini`, then reloads the authoritative launcher state.

Dual-INI persistence must behave as one logical transaction. Both edited documents are prepared and validated before publication. If publication of either file fails after the other file changed, the service restores the changed file from its exact pre-apply contents and reports failure. If restoration itself fails, the command reports a distinct hard failure rather than claiming that normal rollback restored the saved state.

## Shell architecture

The production shell remains `GraphicsOptionsShellScriptTemplates`, built through `GraphicsOptionsAssetBuilder`. The legacy `GraphicsOptionsScriptTemplates` controller is reference material only and must remain outside the production provenance graph.

The shell's transmitted setting list grows from four settings to eleven:

1. VSync
2. MSAA
3. Bloom
4. Dynamic Shadows
5. Motion Blur
6. Distortion
7. Fog Volumes
8. Spherical Harmonic Lighting
9. Ambient Occlusion
10. PhysX
11. Stereo 3D

Detail Level is an active local preset row associated with the seven quality setting records, but it is excluded from startup request, dirty-write, acknowledgement, and rollback queues.

Startup requests remain serialized under the existing global initialization deadline. Once all seven quality leaf values are available, the shell derives and renders Detail Level. If any required leaf is unavailable, Detail Level is also unavailable because a truthful preset cannot be derived.

The Apply queue contains only dirty transmitted settings and preserves screen order. Selecting a preset therefore queues the changed leaf rows, not a redundant Detail Level operation.

## Memory protocol

The seven new observers join address group `batmanFrontendControlType` and retain the proven scan range, stride, value offset, structural checks, polling interval, cache invalidation, response, acknowledgement, and failure behavior.

The compact protocol allocation is:

| Setting | Read | Responses Off/On | Writes Off/On | Acknowledgements Off/On | Failure |
| --- | ---: | --- | --- | --- | ---: |
| Bloom | 4600 | 4601 / 4602 | 4603 / 4604 | 4605 / 4606 | 4609 |
| Dynamic Shadows | 4610 | 4611 / 4612 | 4613 / 4614 | 4615 / 4616 | 4619 |
| Motion Blur | 4620 | 4621 / 4622 | 4623 / 4624 | 4625 / 4626 | 4629 |
| Distortion | 4630 | 4631 / 4632 | 4633 / 4634 | 4635 / 4636 | 4639 |
| Fog Volumes | 4640 | 4641 / 4642 | 4643 / 4644 | 4645 / 4646 | 4649 |
| Spherical Harmonic Lighting | 4650 | 4651 / 4652 | 4653 / 4654 | 4655 / 4656 | 4659 |
| Ambient Occlusion | 4660 | 4661 / 4662 | 4663 / 4664 | 4665 / 4666 | 4669 |

All codes are globally unique and contiguous where the shell uses base-plus-index mapping. Codes `4700` through `4899` remain available for the later Fullscreen and Resolution milestone. Existing `4960` through `4991` rollback/apply codes are unchanged.

Every member of the shared address group carries the complete union of accepted graphics control codes. The observer service must still perform at most one broad scan for the unresolved group in a polling pass, reuse a valid cached carrier across members, and invalidate the group atomically when its structural checks fail.

## INI mapping

The existing mappings remain authoritative:

| Config key | INI section/key | Encoding |
| --- | --- | --- |
| `bloom` | `SystemSettings.Bloom` | `False` / `True` |
| `dynamicShadows` | `SystemSettings.DynamicShadows` | `False` / `True` |
| `motionBlur` | `SystemSettings.MotionBlur` | `False` / `True` |
| `distortion` | `SystemSettings.Distortion` | `False` / `True` |
| `fogVolumes` | `SystemSettings.FogVolumes` | `False` / `True` |
| `sphericalHarmonicLighting` | `SystemSettings.DisableSphericalHarmonicLights` | Inverted: enabled is `False`, disabled is `True` |
| `ambientOcclusion` | `SystemSettings.AmbientOcclusion` | `False` / `True` |

The derived preset also controls the existing `SystemSettings.DetailMode` normalization during Apply. The launcher file remains the startup authority even when generated `BmEngine.ini` disagrees.

## Transaction and failure behavior

- Left/right input changes shell draft values only.
- Apply is enabled only when every required active setting initialized successfully, no transaction is running, and at least one transmitted draft differs from its baseline.
- Each dirty leaf write must receive its exact acknowledgement before the next write begins.
- The existing commit signal runs only after all dirty writes acknowledge.
- Commit must not leave `UserEngine.ini` and `BmEngine.ini` representing different transactions; partial dual-file publication is compensated from exact pre-apply contents.
- A write failure, timeout, or commit failure invokes the existing rollback signal.
- Rollback reloads the saved launcher state into the dispatcher. The shell may retain the user's draft for retry, matching the current Apply Failed behavior.
- Rollback failure locks the controller and reports Rollback Failed.
- Back/cancel before Apply destroys the local draft; no INI or runtime config change occurs.
- Missing or invalid startup values produce explicit Unavailable state rather than defaults or silent fallback.

## Implementation boundaries

Expected production and generated touchpoints are:

- `GraphicsOptionsShellScriptTemplates.cs` for active row definitions, derived preset behavior, request/write queues, and row rendering.
- `GraphicsOptionsAssetBuilder.cs` only where the additional active row clip actions require builder registration.
- `Rebuild-BatmanGraphicsOptionsExperiment.ps1` for the declarative observer protocol table and complete address-match union.
- The checked-in graphics pack manifests and regenerated delta produced by the validated rebuild path.
- Runtime code only if tests expose a missing generic observer capability; no Batman-specific runtime shortcut is planned.

The existing `BatmanGraphicsConfigService` already owns parsing, preset derivation, dispatcher synchronization, and dual-INI persistence. New behavior must reuse it rather than duplicate mapping logic in another native service.

## Test strategy

Implementation follows test-driven development.

### Shell tests

- All eight detail-quality rows are active with exact labels and legal values.
- Fullscreen and Resolution remain `Not active`.
- Startup requests cover the eleven transmitted settings in screen order.
- Seven quality responses populate their leaf rows and derive the correct preset.
- Each canonical combination maps to the correct preset; unmatched combinations map to Custom.
- Selecting each preset updates all seven leaf drafts.
- Custom is not directly selectable or transmitted.
- Changing one leaf recomputes Detail Level and enables Apply.
- Apply queues only dirty transmitted leaves and never queues Detail Level.
- Exact acknowledgement, timeout, commit failure, rollback success, and rollback failure behavior remain correct.

### Native tests

- All seven new observers parse and map their exact read, response, write, acknowledgement, and failure codes.
- Observer updates run `sync-batman-graphics-detail-level` after changing a leaf.
- Multiple new observers in the shared group still cause only one broad discovery scan per polling pass.
- Cache reuse, invalidation, late carrier discovery, and rearming remain correct with the expanded union.
- Config loading asserts every quality leaf from `UserEngine.ini` and ignores conflicting generated values.
- Preset and Custom derivation cover every canonical combination plus representative noncanonical combinations.
- Apply persists all seven quality fields to both INIs, including the inverted spherical-harmonic key.
- Injected failure at each dual-INI publication boundary either leaves both original files intact or reports an explicit compensation failure.

### Package and provenance tests

- Rebuild and package validators require the exact expanded observer set and collision-free protocol union.
- Shell, retail patch, layout, and package contracts agree on active versus inactive rows.
- The pack contains exactly its seven expected files and no loose GFX, observer, historical, or installed artifacts.
- Retail base size/hash, rebuilt target size/hash, and delta size/hash are regenerated and asserted.
- Production provenance continues to reject the legacy controller, old callback bridge, installed inputs, and `batma/`.

## Deployment and live verification

The Release build is deployed atomically only while Batman is closed. The installer must validate the staged package and installed retail base before activation, preserve the subtitle pack, and leave no staging or recovery directories.

Live verification proceeds in four checkpoints:

1. First launch opens Graphics Options with all eleven transmitted settings populated and no Undefined or Unavailable values.
2. Change one quality toggle, Apply, and verify the exact observer acknowledgement plus both INI writes.
3. Exercise Low, Medium, High, and Very High; verify all seven displayed leaf values before applying each selected checkpoint.
4. Fully exit and relaunch; verify the last applied values and derived Detail Level appear immediately.

## Acceptance criteria

- Rows 5 through 12 are selectable and display correct values.
- Detail Level behaves only as a derived preset controller and never becomes a competing persisted write.
- All seven quality toggles round-trip through the memory protocol and both INIs.
- First-launch initialization is reliable and performs one graphics-group scan per unresolved polling pass.
- Apply and rollback retain the already proven serialized behavior.
- Fullscreen and Resolution remain clearly inactive.
- All native, shell, package, retail, layout, and deployment validators pass.
- The committed package is reproducible from approved repository sources and verified retail input only.
