# Helen True Multi-Pack Runtime Design

## Goal

Replace the temporary primary-pack restriction with true multi-pack runtime support so multiple enabled packs can contribute functional runtime behavior at the same time.

This work must preserve deterministic startup, fail fast on any conflict, and keep pack-local asset resolution correct for every declaration that loads files from disk.

## Scope

In scope:

- explicit ordered pack selection from `helengamehook/config/packs.json`
- loading multiple compatible packs into one runtime pack set
- merging non-conflicting declarations across enabled packs
- fail-fast validation for every conflicting declaration class
- pack-local asset resolution for virtual files, native hook blobs, and texture replacement assets
- shared runtime registries for commands, bindings, config entries, features, runtime slots, startup commands, and observers
- preserving current single-pack fallback when no explicit pack set is configured

Out of scope:

- UI for enabling or disabling packs
- best-effort conflict recovery
- partial activation of a pack set with invalid or conflicting packs
- automatic loading of every compatible pack on disk

## User-Facing Behavior

Runtime pack selection remains explicit:

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

When an executable has an enabled pack list:

- the runtime loads every listed pack whose build matches the executable fingerprint
- the runtime validates the entire set before installing any runtime behavior
- if validation succeeds, all non-conflicting declarations become active together
- if validation fails, the whole pack set is refused and no listed pack is activated

When no explicit pack list exists for the executable:

- the current single-pack first-match behavior remains in place

## Runtime Model

### Active Pack Set

The runtime should stop treating one `LoadedBuildPack` as the single source of truth.

Instead, it should build one merged active pack-set view that contains:

- the ordered list of loaded packs for diagnostics and startup command sequencing
- merged declaration registries used by runtime services
- pack-local source context for any declaration that needs asset resolution

The active pack set is one validated runtime snapshot. Services consume that snapshot instead of reading directly from one pack.

### Pack-Local Source Context

Every asset-backed declaration must retain enough information to resolve assets from its source pack and build directories.

Examples:

- a virtual file delta declared by pack A must load its `hgdelta` from pack A's build directory
- a texture replacement declared by pack B must load its DDS or PNG from pack B's asset root
- a native hook blob declared by pack C must load its binary blob from pack C's asset root

This means the merged runtime view cannot flatten declarations into raw metadata only. Asset-backed declarations need pack-aware context attached to them.

## Ordering Rules

Pack order is meaningful only for startup command sequencing.

All other declarations are additive when they do not conflict. The runtime must not use pack order as an implicit override rule for:

- config entries
- features
- commands
- bindings
- hooks
- virtual files
- textures
- observers

This keeps pack order from becoming hidden behavior.

## Conflict Rules

The runtime must fail fast when any enabled packs conflict.

### Conflict classes

Reject the entire enabled pack set when any of these collide:

- config entries by `key`
- features by `id`
- commands by `id`
- runtime slots by `id`
- state observers by `id`
- hooks by `id`
- virtual files by normalized `gamePath`
- texture replacements by `id`

### External bindings

External bindings are keyed by their effective callback contract, not only by any optional local id.

Reject duplicates when two bindings resolve the same external callback meaningfully, such as the same:

- external callback name
- binding mode
- config key for config-backed bindings
- command id for command-backed bindings

This avoids ambiguous `Helen_GetInt`, `Helen_SetInt`, or `Helen_RunCommand` behavior.

### Texture replacement match conflicts

Also reject the pack set when two texture replacements target the same effective texture match signature:

- API
- width
- height
- format
- hash
- sampler-stage scope when present

This prevents two packs from racing to replace the same texture.

## Failure Behavior

When any enabled pack:

- is missing
- is malformed
- fails executable fingerprint matching
- or conflicts with another enabled pack

the runtime must refuse the whole set.

The runtime log must include:

- the executable name
- the pack ids involved
- the specific conflicting declaration key, id, or game path

No best-effort fallback should silently activate a subset of the requested pack set.

## Subsystem Design

### Repository and Validation Layer

`PackRepository` still loads individual `LoadedBuildPack` values from disk, but pack-set loading must become the entry point for explicit multi-pack startup.

Validation should happen before any runtime services install hooks or allocate asset-backed state.

The validator should:

- ensure every requested pack id is present and fingerprint-compatible
- build merged registries for declaration classes
- emit precise conflict diagnostics
- preserve pack-local source context for asset-backed declarations

### Virtual Files

`VirtualFileService` currently assumes one `PackAssetResolver`.

It must become pack-aware so each registered virtual file carries the resolver or pack-local source context needed to load its declared source assets. This allows multiple packs to register disjoint virtual files safely.

Conflict rule:

- duplicate normalized game-relative virtual file paths reject the whole set

### Native Hooks

`BuildHookInstaller` currently assumes one asset resolver for all blob-backed hooks.

It must accept hook entries that retain the source pack context for the blob asset path. Hooks remain one combined installed set after validation.

Conflict rule:

- duplicate hook ids reject the whole set

### Texture Replacements

`D3d9TextureReplacementHookSet` currently assumes one asset resolver plus one replacement vector.

It must accept one merged list of pack-aware replacement entries. Each entry must carry its source resolver or equivalent source-pack context so replacement assets resolve correctly.

Conflict rules:

- duplicate replacement ids reject the whole set
- duplicate effective texture match signatures reject the whole set

### Commands, Config Entries, Features, and Runtime Slots

These declarations merge into shared registries after validation.

Runtime services still expose one unified surface:

- one command dispatcher
- one command executor
- one runtime value store

But their registered declarations come from the merged pack set rather than one primary pack.

Conflict rules:

- duplicate config keys reject the set
- duplicate feature ids reject the set
- duplicate command ids reject the set
- duplicate runtime slot ids reject the set

### External Bindings

`ExternalBindingService` remains one shared service, but it must register bindings from all enabled packs after conflict validation.

This allows multiple packs to patch different assets and still route callbacks through one runtime surface.

### Startup Commands and Observers

`BuildRuntimeCoordinator` must become pack-set aware.

Behavior:

- startup commands concatenate in enabled-pack order
- observer declarations merge after duplicate-id validation
- observer follow-up commands resolve through the merged command registry

Conflict rule:

- duplicate observer ids reject the whole set

### Hidden Paths

`missingPaths` remains additive and deduplicated across enabled packs.

This existing behavior should remain as part of the true multi-pack model, not a special-case addon path.

## Bootstrap Flow

When `packs.json` provides enabled packs for the executable:

1. fingerprint the host executable
2. load the requested `LoadedBuildPackSet`
3. validate and merge the entire set into one active runtime pack-set view
4. create merged runtime services from that view
5. install hooks and startup services once using merged declarations

When `packs.json` has no entry for the executable:

1. use existing single-pack repository loading
2. build a single-pack runtime view compatible with the same runtime services

This keeps the runtime model unified while preserving backward compatibility.

## Testing

### Repository and config tests

- valid ordered pack sets load successfully
- missing or unknown pack ids fail
- duplicate pack ids in `packs.json` fail
- single-pack fallback still works when no explicit pack list exists

### Merge validation tests

Add positive and negative tests for:

- duplicate config keys
- duplicate feature ids
- duplicate command ids
- duplicate runtime slot ids
- duplicate observer ids
- duplicate hook ids
- duplicate virtual file game paths
- duplicate texture replacement ids
- duplicate texture match signatures
- duplicate effective binding contracts

### Positive coexistence tests

Add tests proving two packs can coexist when they declare disjoint:

- virtual files
- commands and bindings
- hook blobs
- texture replacements
- hidden paths

### Checked-in Batman contract tests

Keep checked-in pack coverage for:

- subtitles plus skip-videos

Extend it later as more Batman packs are added.

## Risks

Primary risk:

- flattening declarations too early and losing pack-local asset ownership

Mitigation:

- represent asset-backed merged entries with explicit source-pack context

Secondary risk:

- broadening merge support without strong conflict validation

Mitigation:

- validate and build merged registries before runtime service initialization starts
- refuse the whole set on any ambiguity

## Success Criteria

The work is successful when:

- multiple functional packs can be enabled together for one executable
- no primary-pack restriction remains
- asset-backed declarations load from the correct source pack
- startup command order is deterministic
- all non-order conflicts fail fast with specific diagnostics
- single-pack fallback still works unchanged
