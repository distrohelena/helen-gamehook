# Batman Main Menu Audio Subtitle Size Design

Date: 2026-03-29

## Goal

Add a dedicated `Subtitle Size` option to the front-end / main-menu `Options Audio` screen so it matches the pause-menu behavior more closely without disturbing the existing four vanilla audio rows.

## Scope

- Main-menu `Options Audio` screen only
- Keep the four existing audio rows visually vanilla
- Add `Subtitle Size` as a fifth row underneath `VolumeDialogue`
- Keep the existing subtitle-size storage carrier for the front end in this pass
- Show `Subtitle Size` even when subtitles are disabled
- Grey out and disable `Subtitle Size` when `Subtitles` is off, if the movie supports a stable dim state

## Non-Goals

- No pause-menu layout changes in this pass
- No redesign of the front-end audio panel art or typography
- No attempt to replace the existing front-end subtitle-size persistence carrier with a new native storage path
- No shipping of generated or decompiled Flash assets in git
- No silent raw-package fallback if the trusted-base delta path is not valid

## Current Problem

The repo already contains a partial front-end subtitle-size path, but it is structurally incomplete and historically unsafe:

- `MainMenuXmlPatcher` exists, but the current asset builder does not apply it during the front-end build
- the generated front-end script path still treats the `Subtitles` row as the old four-state subtitle-size carrier instead of adding a dedicated fifth row
- historical Batman notes show repeated `MainMenu.MainV2` FFDec rebuilds broke the press-start / save-select flow

So the current state is not a conservative five-row menu extension. It is a half-patched path that can mutate script behavior without rebuilding the front-end movie structure correctly.

## Chosen Approach

Use the same general shape that now works on the pause screen:

- make the front-end audio screen structural change in XML first
- rebuild a structural `MainV2` movie from that patched XML
- import the patched scripts into that rebuilt structural movie
- ship the result only through a trusted-base delta path

The main-menu audio screen should stay visually conservative:

- `Subtitles`
- `VolumeSFX`
- `VolumeMusic`
- `VolumeDialogue`
- `SubtitleSize`

Only `SubtitleSize` is new. The other four rows should keep their vanilla positions and ordering.

## Builder And Package Strategy

### Front-end builder base

For this pass, treat the front-end movie source as a trusted decompressed `Startup_INT.upk` builder input. This is based on the existing Batman notes and the existing local extracted `frontend/mainv2` workspace, which already carries a `MainV2` export tree outside the `BmGame.u` gameplay package flow.

The prep flow should make that builder dependency explicit instead of relying on stale local leftovers.

### Prep step

`Prepare-BatmanBuilderWorkspace.ps1` should grow a front-end preparation path that recreates:

- `builder/extracted/frontend/mainv2/frontend-mainv2.gfx`
- `builder/extracted/frontend/mainv2/frontend-mainv2.xml`
- `builder/extracted/frontend/mainv2/frontend-mainv2-export/scripts`

from a trusted front-end base package plus FFDec.

If the current package tooling cannot read the live compressed package directly, the prep flow should require the decompressed trusted front-end builder input explicitly rather than guessing or mutating a live installed file in place.

### Shipped pack

The Batman pack should ship both:

- the existing gameplay `BmGame.u` delta-backed virtual file
- a second front-end virtual file entry for the trusted front-end package that owns `MainV2`

The pack rebuild must fail if either virtual file cannot be built and validated from its trusted base.

## Structural Patch Shape

`MainMenuXmlPatcher.cs` becomes the source of truth for the front-end audio screen layout.

It should:

- identify `ScreenOptionsAudio`
- preserve the four vanilla rows in vanilla visual order
- insert a real `SubtitleSize` row underneath `VolumeDialogue`
- assign the new row its own depth instead of overloading `Subtitles`
- update any repeated placement tags for the new depth so timeline animation cannot pull the row back into the wrong vertical slot later

The intended visible order is:

1. `Subtitles`
2. `VolumeSFX`
3. `VolumeMusic`
4. `VolumeDialogue`
5. `SubtitleSize`

## ActionScript Behavior

`ScriptTemplates.cs` should stop treating the front-end `Subtitles` row as a disguised subtitle-size option.

The front-end script behavior should become:

- `Subtitles` remains a normal binary `Off / On` row
- `SubtitleSize` becomes a dedicated row with its own label and three size values
- the front-end subtitle-size row continues to read and write through the existing subtitle-size carrier for this pass
- when `Subtitles` is off:
  - `SubtitleSize` remains visible
  - left and right arrows are hidden
  - cycle prompt is not shown
  - interaction is ignored
  - label/value styling is dimmed if the movie supports a stable dim state
- if dim styling proves unstable in `MainV2`, the fallback is visible but inert, not hidden

This keeps the user-facing model simple while avoiding a deeper native settings rewrite in the same pass.

## Asset Builder Changes

`SubtitleSizeAssetBuilder.cs` should make the front-end build flow match the pause flow more closely.

The required front-end build order is:

1. copy the extracted front-end ActionScript tree into a generated working directory
2. patch the front-end XML with `MainMenuXmlPatcher`
3. rebuild a structural `MainV2` GFX from that patched XML with `-xml2swf`
4. import patched scripts into that structural movie with `-importScript`
5. emit a front-end patch manifest alongside the gameplay manifest

The current half-patched path, where front-end scripts are modified without rebuilding the structural movie from patched XML, should be removed.

## Error Handling

The builder should fail fast when:

- the trusted gameplay base is missing
- the trusted front-end base is missing
- the extracted front-end workspace is missing or stale
- the XML patcher cannot find the expected `ScreenOptionsAudio` sprite or row placements
- the structural front-end rebuild fails
- the shipped manifest cannot describe both virtual files cleanly

Do not ship a partial pack if the gameplay path succeeds but the front-end path fails.

## Verification

### Generated structure checks

Add a dedicated front-end verifier script, `Test-BatmanMainMenuAudioLayout.ps1`, that asserts:

- the generated front-end audio screen has five visible rows
- the first four rows remain in vanilla order
- `SubtitleSize` is the last row under `VolumeDialogue`
- the generated frame script registers rows in the same navigation order
- the generated `ListItem` script contains the disabled-state behavior for the `SubtitleSize` row

### Builder workspace checks

Extend the builder workspace verification so it also checks the front-end extracted inputs under `builder/extracted/frontend/mainv2/...`.

Rebuild should stop immediately if those inputs are missing or do not match the trusted builder expectation.

### Pack validation

Extend the Batman pack verification so it validates the full shipped manifest, not only the gameplay `BmGame.u` entry.

The validator should assert:

- both virtual files are present
- each entry uses `mode = delta-on-read`
- each entry points at the expected shipped delta asset
- each delta references the expected trusted base size and hash

Runtime tests should also cover a Batman pack containing multiple virtual file entries.

### Live smoke test

A successful build is not enough. The release gate must include a live front-end smoke test:

1. launch Batman
2. press start successfully
3. pass the save/profile selection flow successfully
4. open `Options -> Audio` from the main menu
5. confirm `SubtitleSize` is the last row
6. turn subtitles off and confirm `SubtitleSize` is greyed or at least inert
7. turn subtitles on and confirm `SubtitleSize` becomes active
8. back out through the save/apply flow without front-end breakage

If the press-start or save/profile path breaks again, the change does not ship.

## Rationale

The user request is narrow: add the front-end `Subtitle Size` option and make it behave like a normal dedicated menu row. The risky part is not the menu logic by itself. The risky part is the historical `MainV2` rebuild path.

So this design keeps the user-facing change conservative while raising the engineering bar around the front-end movie pipeline:

- explicit trusted bases
- structural XML rebuild before script import
- fail-fast validation
- live front-end smoke gating

That is the smallest shape that can plausibly make the main-menu option work without repeating the earlier broken distributable path.
