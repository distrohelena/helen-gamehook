# Batman Audio Menu Layout Design

Date: 2026-03-29

## Goal

Improve the in-game pause `Options Audio` screen so the five option rows fit the existing background square more naturally, with less crowding around `Subtitle Size`.

## Scope

- Pause-menu `Options Audio` screen only
- Conservative layout rebalance
- Keep the current visual language and row widget style
- Preserve the existing `Subtitle Size` behavior and storage/runtime wiring

## Non-Goals

- No front-end / main-menu audio layout changes
- No redesign of the row widget art or typography
- No changes to subtitle runtime logic
- No new preview or helper UI on the pause audio screen

## Current Problem

The current patch inserts a new `Subtitle Size` row by duplicating the `Subtitles` row and shifting it downward. The same screen also injects a help and preview area. That leaves the five audio rows fighting for the same vertical space, so `Subtitle Size` appears cramped even though the background square has enough room for a cleaner distribution.

## Chosen Approach

Remove the injected preview/help area from the pause `Options Audio` screen and redistribute the five existing rows more evenly inside the current panel bounds.

This keeps the change conservative:

- the screen keeps its existing background square
- the row clips keep their current visuals and behavior
- only the screen composition and row positions change

## Implementation Shape

### Pause XML patching

Update the pause-screen XML patcher so it:

- still duplicates the `Subtitles` row into a `SubtitleSize` row
- no longer injects `SubtitleSizeHelp`
- no longer injects `SubtitlePreviewLabel`
- no longer injects `SubtitlePreviewText`
- repositions the five audio rows to a more even vertical distribution inside the square

The intended rows are:

- `Subtitles`
- `SubtitleSize`
- `VolumeSFX`
- `VolumeMusic`
- `VolumeDialogue`

### ActionScript

Leave the existing row behavior untouched unless a minimal follow-up alignment tweak is required after testing.

In particular:

- `rs.ui.ListItem` should continue to own `Subtitle Size` state and runtime apply behavior
- the pause audio screen actions should continue to register the same five menu items in the same order
- no logic change should be bundled with the layout work

## Layout Intent

The updated screen should:

- keep all five rows inside the current square with visibly more breathing room
- avoid a bottom-heavy stack where `Subtitle Size` feels jammed between `Subtitles` and the volume controls
- preserve the existing visual hierarchy and cursor behavior
- feel like the menu was originally authored to contain five rows

## Error Handling

If the XML patcher can no longer find the expected pause-screen nodes or row depths, the builder should fail immediately rather than emitting a malformed pause package.

## Verification

1. Rebuild the Batman pack successfully.
2. Verify the Batman known-good package script still passes.
3. Deploy and launch Batman.
4. Open pause `Options Audio`.
5. Confirm all five rows fit comfortably inside the existing square.
6. Confirm `Subtitle Size` is no longer visually cramped.
7. Confirm `Subtitle Size` still changes live and still persists.

## Rationale

The problem is layout pressure, not row behavior. Removing the preview area returns space to the menu itself and avoids unnecessary widget redesign. That gives the safest path to a cleaner `Options Audio` screen while preserving the working subtitle-size runtime path.
