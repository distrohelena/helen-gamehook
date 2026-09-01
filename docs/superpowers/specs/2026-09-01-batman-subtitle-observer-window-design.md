# Batman Subtitle Observer Window Design

## Goal

Make the gameplay-only Batman subtitle-size option reliably detect its live UI state after the game allocates the pause-menu control block at a high heap address.

## Current Failure

The native subtitle UI-state scanner reserves and validates the address window `0x10000000..0x12000000`, while the gameplay subtitle pack declares an observer ending at `0x11000000`. The observer can therefore miss a valid control block even though the pause option is present and the replacement texture is active.

## Design

Extend the subtitle observer’s declared scan end to `0x12000000` in both the checked-in build manifest and the rebuild script. Keep the observer stride, signature checks, raw-code mappings, polling interval, runtime slot, native text-scale hook, and gameplay-only package contents unchanged.

Update the package verification contract so a rebuilt or checked-in pack must use the widened window. No executable patching, video pack activation, fallback scan behavior, or unrelated runtime changes are introduced.

## Verification

- Run the Batman gameplay-package verifier and the repository tests that cover pack parsing and observer behavior.
- Rebuild the subtitle package and confirm its manifest, delta, native blob, and texture assets are valid.
- Confirm the installed `packs.json` still enables only `batman-aa-subtitles` and compare installed files with the rebuilt source package.
- Launch the game and verify the log resolves `subtitleUiStateObserver` and emits an update when the subtitle-size option changes.
