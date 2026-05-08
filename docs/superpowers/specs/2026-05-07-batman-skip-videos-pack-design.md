# Batman Skip Videos Pack Design

## Goal

Add a standalone Batman pack named `batman-aa-skip-videos` that makes the game behave as if selected startup video files do not exist, without deleting or renaming the real files on disk.

This must be reusable for future packs, so the behavior is not hardcoded to Batman video filenames. Instead, pack metadata will declare a list of paths that should be treated as missing by the runtime.

## Scope

In scope:

- New generic pack metadata for hidden file paths
- Runtime support for synthetic “file not found” behavior
- A standalone Batman pack that hides the five startup `.bik` files
- Tests for metadata parsing, path matching, and Batman pack contract

Out of scope:

- Any subtitle or texture functionality
- Any modification of real files on disk
- Directory write interception or rename/delete virtualization
- A UI for toggling hidden paths

## User-Facing Behavior

When `batman-aa-skip-videos` is the active pack:

- The game should believe the following files do not exist:
  - `BmGame/Movies/baa_logo_run_v5_h264.bik`
  - `BmGame/Movies/Legal.bik`
  - `BmGame/Movies/Legalus.bik`
  - `BmGame/Movies/nvidia.bik`
  - `BmGame/Movies/utlogo.bik`
- The real files remain untouched on disk.
- The behavior should apply both to direct file opens and to existence or enumeration checks.

When the pack is not active:

- Runtime behavior must remain unchanged.

## Metadata Design

Add a new optional build-level manifest field:

```json
{
  "missingPaths": [
    "BmGame/Movies/baa_logo_run_v5_h264.bik",
    "BmGame/Movies/Legal.bik"
  ]
}
```

Rules:

- Paths are relative game paths, not absolute host paths.
- Matching is case-insensitive.
- Matching uses normalized slash handling so `/` and `\` resolve to the same canonical form.
- Empty entries are invalid and should fail manifest loading.
- Duplicate normalized paths should be rejected during build-definition loading.

This field belongs in `build.json`, not `pack.json`, because hiding files is a build/runtime payload concern just like hooks and virtual files.

## Runtime Design

### Path Matcher

Add one focused runtime component responsible for hidden-path matching. It should:

- normalize incoming file paths
- convert absolute paths under the game root into relative canonical paths
- compare against the active build’s hidden-path set
- expose one clear query such as `ShouldHidePath(...)`

This logic should stay separate from the file hook detours so it can be tested independently.

### Hook Coverage

To make files appear absent in all normal discovery paths, the runtime should hide matching paths from:

- `CreateFileW`
- `CreateFileA`
- `GetFileAttributesW`
- `GetFileAttributesA`
- `FindFirstFileW`
- `FindFirstFileA`
- `FindNextFileW`
- `FindNextFileA`

Expected behavior:

- direct open attempts return invalid handle / failure with `ERROR_FILE_NOT_FOUND`
- attribute probes return invalid attributes with `ERROR_FILE_NOT_FOUND`
- directory enumeration omits matching entries entirely

The implementation should remain read-only. No real filesystem state is changed.

### Interaction With Existing Virtual File System

Hidden-path handling must be evaluated before normal virtual-file servicing. If a path is declared missing, the runtime should report it absent even if another feature could otherwise virtualize it.

This keeps the behavior deterministic and avoids contradictory pack state.

## Batman Pack Design

Create a new standalone pack directory:

- `games/HelenBatmanAA/helengamehook/packs/batman-aa-skip-videos`

It should contain one build for the retail Steam GOTY target and only the metadata needed to activate hidden-path behavior. No delta payloads, texture payloads, or native blobs are required.

The build manifest will declare the five startup movies in `missingPaths`.

## Error Handling

Fail fast when:

- `missingPaths` contains empty values
- normalized entries collide
- a hidden-path matcher receives an unusable root/config state

Do not silently ignore malformed metadata.

For runtime logging, emit concise diagnostics when the pack loads and when a hidden path is suppressed, but avoid flooding logs during directory scans.

## Testing

### Unit / runtime tests

- build-definition parsing accepts valid `missingPaths`
- build-definition parsing rejects empty or duplicate normalized entries
- hidden-path matcher normalizes case, separators, and absolute-vs-relative paths correctly
- hidden paths return “not found” semantics for open and attribute probe code paths

### Batman pack tests

- standalone pack manifest exists and is loadable
- declared hidden paths match the five expected startup movies
- pack contains no unrelated subtitle/texture payload wiring

## Risks

Primary risk:

- the game may use additional Win32 existence probes not yet hooked

Mitigation:

- start with the broad file API coverage above
- keep the matcher reusable so more hooks can be added without changing pack metadata

Secondary risk:

- directory enumeration filtering can be easy to get wrong if state is tracked too loosely

Mitigation:

- keep enumeration state ownership explicit and test both `FindFirst*` and repeated `FindNext*` flows

## Success Criteria

The work is successful when:

- `batman-aa-skip-videos` can be activated as a standalone pack
- Batman skips the startup videos because the game believes those five files are absent
- real `.bik` files remain untouched on disk
- the hidden-path feature is reusable by future packs through metadata only
