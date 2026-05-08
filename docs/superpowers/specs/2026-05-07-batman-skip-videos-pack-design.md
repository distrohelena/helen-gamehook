# Batman Multi-Pack Skip Videos Design

## Goal

Add explicit ordered pack selection so Batman can enable both the existing subtitle pack and a new `batman-aa-skip-videos` pack at the same time.

The new skip-videos pack must make the game behave as if selected startup video files do not exist, without deleting or renaming the real files on disk.

## Scope

In scope:

- explicit runtime config for enabling multiple packs in a defined order
- runtime loading of one ordered pack set instead of one first-match pack
- deterministic validation rules for enabled packs
- generic build metadata for hidden file paths
- runtime support for synthetic "file not found" behavior
- a standalone Batman skip-videos pack that hides the five startup `.bik` files
- tests for config parsing, pack loading, hidden-path matching, and Batman pack contracts

Out of scope:

- UI for toggling packs
- deleting, renaming, or patching the real movie files on disk
- write interception for filesystem mutation APIs
- automatic loading of every compatible pack found on disk

## User-Facing Behavior

Runtime pack selection is controlled by a Helen-owned config file:

```json
{
  "enabledPacksByExecutable": {
    "ShippingPC-BmGame.exe": [
      "batman-aa-subtitles",
      "batman-aa-skip-videos"
    ]
  }
}
```

When the config enables both Batman packs:

- the subtitle pack keeps working normally
- the skip-videos pack adds hidden-path behavior for the startup movies
- the game believes the following files do not exist:
  - `BmGame/Movies/baa_logo_run_v5_h264.bik`
  - `BmGame/Movies/Legal.bik`
  - `BmGame/Movies/Legalus.bik`
  - `BmGame/Movies/nvidia.bik`
  - `BmGame/Movies/utlogo.bik`
- the real files remain untouched on disk

When `packs.json` does not contain an entry for the executable, runtime behavior should preserve the current single-pack fallback so existing installs do not break.

## Pack Selection Design

### Config Format

Add a dedicated JSON file at:

- `helengamehook/config/packs.json`

This file is separate from `runtime.json`. The current runtime config store is intentionally a flat integer store, and pack selection needs structured string arrays. A dedicated config reader keeps the existing store stable.

Rules:

- pack ids are explicit strings, not inferred from directory order
- pack order in the list is meaningful
- unknown pack ids are a hard failure for that executable's pack-set initialization
- duplicate ids in the same executable list are invalid

### Repository Loading

Extend pack discovery so the repository can:

- enumerate all compatible pack/build pairs for an executable fingerprint
- load a specific ordered subset by pack id
- return a `LoadedBuildPackSet` instead of a single `LoadedBuildPack`

The pack set should preserve config order so runtime startup commands and diagnostics remain deterministic.

Current implementation constraint:

- the first enabled pack is the primary full-featured pack
- later enabled packs must be hidden-path-only addon packs
- addon packs may not declare config entries, features, startup commands, virtual files, bindings, observers, hooks, texture replacements, or commands

This narrower rule is intentional for the first slice. It solves the Batman subtitles plus skip-videos use case without silently mixing unrelated pack systems that still assume single-pack ownership.

## Merge Design

### Additive fields

These merge across enabled packs in the current implementation:

- `missingPaths`: union with deduplication
- `enableD3d9TextureReplacementHooks`: true if any enabled pack requires it
- `enableD3d9TextureHashLogging`: true if any enabled pack requires it
- `enableD3d9TextureImageDumping`: true if any enabled pack requires it

Non-additive declarations are not merged yet. They remain owned by the primary pack, and addon packs are rejected if they declare them.

## Runtime Design

### Primary-Pack Ownership

The current runtime still assumes one primary pack owns virtual files, bindings, hooks, commands, and texture replacements. Ordered pack selection only broadens the hidden-path and boolean build-flag cases for now.

### Hidden Path Metadata

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

- paths are relative game paths, not absolute host paths
- matching is case-insensitive
- matching uses normalized slash handling so `/` and `\` resolve to the same canonical form
- empty entries are invalid
- duplicate normalized paths are rejected while loading build metadata

This field belongs in `build.json`, not `pack.json`, because it is build-scoped runtime behavior.

### Hidden Path Matcher

Add one focused runtime component responsible for hidden-path matching. It should:

- normalize incoming file paths
- convert absolute paths under the game root into relative canonical paths
- compare against the merged hidden-path set
- expose one clear query such as `ShouldHidePath(...)`

Keep it separate from Win32 detours so the matching logic is directly unit-testable.

### Hook Coverage

Hide matching paths from:

- `CreateFileW`
- `CreateFileA`
- `GetFileAttributesW`
- `GetFileAttributesA`

Expected behavior:

- direct open attempts return invalid handle / failure with `ERROR_FILE_NOT_FOUND`
- attribute probes return invalid attributes with `ERROR_FILE_NOT_FOUND`

Hidden-path handling must run before virtual-file servicing. If a path is declared missing, the runtime reports it absent even if another pack could otherwise virtualize it.

## Batman Skip Videos Pack Design

Create a new standalone pack directory:

- `games/HelenBatmanAA/helengamehook/packs/batman-aa-skip-videos`

It contains one Steam GOTY build and only the metadata needed to activate hidden-path behavior. No delta payloads, texture payloads, or native blobs are required.

Its build manifest declares the five startup movie paths in `missingPaths`.

## Error Handling

Fail fast when:

- `packs.json` is malformed
- one executable list contains duplicate pack ids
- an enabled pack id does not exist or does not match the executable fingerprint
- merged named declarations collide
- `missingPaths` contains empty values
- normalized hidden-path entries collide
- a hidden-path matcher receives unusable root/config state

Do not hide invalid configuration with best-effort fallbacks.

## Testing

### Config and repository tests

- `packs.json` parsing accepts a valid ordered enabled-pack list
- malformed or duplicate enabled-pack config fails
- repository loading returns the configured Batman pack set in the declared order
- repository fallback without config still returns the existing single-pack behavior
- addon pack validation rejects non-hidden-path declarations

### Hidden-path tests

- build metadata parsing accepts valid `missingPaths`
- build metadata parsing rejects empty or duplicate normalized paths
- hidden-path matcher normalizes case, separators, and absolute-vs-relative paths correctly
- hidden paths return "not found" semantics for open and attribute probe code paths

### Batman pack tests

- skip-videos pack manifest exists and is loadable
- declared hidden paths match the five expected startup movies
- installed config can enable subtitles plus skip-videos together for Batman

## Risks

Primary risk:

- later work may broaden pack-set behavior without first introducing explicit ownership for pack-local assets

Mitigation:

- keep non-hidden-path systems owned by the primary pack until pack-local ownership is designed explicitly
- fail fast on addon-pack violations instead of silently merging incompatible state

Secondary risk:

- the game may use additional Win32 existence probes not yet hooked

Mitigation:

- start with broad open/attribute/enumeration coverage
- keep the matcher reusable so more hooks can be added without changing pack metadata

## Success Criteria

The work is successful when:

- Batman can explicitly enable both `batman-aa-subtitles` and `batman-aa-skip-videos`
- the subtitle pack keeps working while the startup videos are skipped
- real `.bik` files remain untouched on disk
- hidden-path behavior is reusable by future packs through metadata only
- runtime pack selection is explicit and deterministic instead of relying on first-match directory order
