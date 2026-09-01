# Batman Skip-Videos File-Path Fix Design

## Status

This design supersedes the runtime strategy in
`2026-07-25-batman-bink-startup-video-hiding-design.md`. Live diagnostics proved that Batman
passes in-memory Bink data, rather than a filename, to `_BinkOpen@8` for the startup movies.

## Goal

Make the standalone `batman-aa-skip-videos` pack hide exactly its five declared startup movies
without changing the real `.bik` files and without requiring the subtitle pack.

## Evidence and Root Cause

An isolated live run loaded one pack and five `missingPaths`. The runtime installed both the file
API hooks and the experimental Bink hook. The log then recorded these relevant requests:

- `CreateFileW` received the Steam installation path ending in
  `Binaries\..\BmGame\Movies\baa_logo_run_v5_h264.bik` with `hide=false`.
- `CreateFileW` received the Steam installation paths for `UTlogo.bik` and `Legal.bik` with
  `hide=false`.
- The same filenames were probed below the user's Documents directory and correctly remained
  available to normal fallback behavior.
- `_BinkOpen@8` received buffers beginning with the `BIK` file signature after the files had
  already been read.

`RuntimeLayout::GameRoot` currently identifies Batman's `Binaries` directory. `FileApiHookSet`
passes that directory to `HiddenPathMatcher` as though it were the installation root. As a result,
an installation path normalized from `Binaries\..\BmGame\Movies\Legal.bik` cannot become the pack's
canonical `BmGame/Movies/Legal.bik` path and does not match.

## Selected Approach

Keep suppression at the Win32 file-open boundary, where the request still has a filename. Give the
file hook an explicit Binaries request base and an installation-root matcher:

1. Normalize incoming paths lexically so `.` and `..` components are resolved before comparison.
2. Relativize absolute candidates against the Batman installation root, which is the parent of the
   Binaries directory.
3. Preserve direct canonical relative-path matching for pack-facing paths.
4. Report only declared `missingPaths` as absent with `ERROR_FILE_NOT_FOUND`.
5. Remove the experimental `BinkMovieHookSet`; it cannot identify filenames when
   `BINKFROMMEMORY` is used and must not inspect binary movie data as text.

The alternative of suppressing Bink memory buffers by content hash is rejected because it couples
the pack to complete movie payloads and identifies requests too late in the loading pipeline. Native
patching of Batman's startup sequence is also rejected because it is more build-specific than the
existing declarative `missingPaths` contract.

## Path-Matching Behavior

The matcher must hide:

- canonical relative paths such as `BmGame/Movies/Legal.bik`;
- absolute installation paths such as
  `C:\Game\Batman Arkham Asylum GOTY\BmGame\Movies\Legal.bik`;
- observed Binaries-relative absolute paths such as
  `C:\Game\Batman Arkham Asylum GOTY\Binaries\..\BmGame\Movies\Legal.bik`.

Matching remains case-insensitive and separator-insensitive.

The matcher must not hide:

- a corresponding path below the user's Documents directory;
- undeclared movies such as `BmGame/Movies/Black.bik`;
- gameplay cinematics or any non-movie path.

## Runtime Changes

`HiddenPathMatcher` will lexically normalize candidates before relativizing absolute paths.
`FileApiHookSet` will bind hidden-path matching to the Batman installation root while continuing to
use the existing virtual-file service unchanged. `HelenGameHook` will no longer construct or own a
Bink movie hook.

Temporary observation logging will be removed after the live behavior is proven. Normal runtime
logging should retain initialization failures and may log actual hidden-path suppression events,
but it must not log binary buffers as strings.

## Error Handling

Invalid roots and hidden-path declarations continue to fail fast. A hidden match returns the normal
Win32 absence result and sets `ERROR_FILE_NOT_FOUND`. Non-matching requests call the original API
with all arguments unchanged. No fallback may silently re-open a path after it has matched a
declared missing path.

## Testing

The automated regression will use the exact observed Steam-style
`Binaries\..\BmGame\Movies\Legal.bik` path and must fail before the matcher correction. Additional
assertions will prove that the Documents fallback and undeclared `Black.bik` remain visible.

The complete runtime test executable and Batman skip-pack contract test must pass. A Win32 Release
build will then be installed with only `batman-aa-skip-videos` enabled. Live verification requires:

- startup logs showing one active pack and five hidden paths;
- declared movie opens returning `hide=true` or equivalent suppression records;
- no `_BinkOpen@8` interception;
- startup logos and legal movies not playing;
- all real movie files remaining unchanged on disk.

## Success Criteria

The fix is complete when an isolated cold launch skips the two observed startup videos, the log
shows file-layer suppression for declared paths only, the subtitle pack remains inactive, and all
automated verification passes.
