# Helen Binary Delta Design

Date: 2026-03-29

## Goal

Add a generic binary delta format and runtime path so Helen packs can ship either:

- full replacement files
- binary deltas against an exact base file

The design must work for any binary asset type, not just Batman packages. It must remain engine-agnostic and preserve the current Helen boundary:

- runtime knows generic file virtualization and binary reconstruction only
- developer tooling may know game or engine formats

## Requirements

- Support both `replace-on-read` and `delta-on-read` virtual files.
- Delta files must validate against one exact base file size and SHA-256.
- Delta files must fail hard on base mismatch.
- Runtime must avoid reconstructing huge files fully in RAM.
- Runtime must support both normal streamed reads and `CreateFileMapping` consumers.
- The format must be safe to commit to git even when the target file would be very large.
- The runtime must stay generic and reusable across games.

## Non-Goals

- No engine-aware parsing in the runtime.
- No best-effort fallback to the original file on validation failure.
- No external runtime dependency on `xdelta`, `bsdiff`, or similar tools.
- No v1 chunk compression.
- No v1 intra-chunk edit script language.

## Chosen Approach

Use a Helen-native chunked delta container named `.hgdelta`.

Each target file is represented as a sequence of fixed-size target chunks:

- unchanged chunks reference the installed base file at the same offset
- changed chunks store full target chunk bytes in the delta container

This favors predictable random access and a simple runtime over maximal patch compactness.

## Virtual File JSON Model

`files.json` keeps one `virtualFiles` array, but each entry now has an explicit source kind.

### Full replacement example

```json
{
  "id": "bmgameGameplayPackage",
  "path": "BmGame/CookedPC/BmGame.u",
  "mode": "replace-on-read",
  "source": {
    "kind": "full-file",
    "path": "assets/packages/BmGame-subtitle-signal.u"
  }
}
```

### Delta-backed example

```json
{
  "id": "bmgameGameplayPackageDelta",
  "path": "BmGame/CookedPC/BmGame.u",
  "mode": "delta-on-read",
  "source": {
    "kind": "delta-file",
    "path": "assets/deltas/BmGame-subtitle-signal.hgdelta",
    "base": {
      "size": 101403981,
      "sha256": "..."
    },
    "target": {
      "size": 101405329,
      "sha256": "..."
    },
    "chunkSize": 65536
  }
}
```

### Rules

- `replace-on-read` requires `source.kind = "full-file"`.
- `delta-on-read` requires `source.kind = "delta-file"`.
- `delta-file` requires exact `base` and `target` metadata.
- `chunkSize` in JSON must match the `.hgdelta` header.

## `.hgdelta` Container Format

Format name: `Helen Game Delta v1`

### Header

- magic: `HGDL`
- version: `1`
- flags
- chunk size
- base file size
- target file size
- base file SHA-256
- target file SHA-256
- chunk count
- chunk table offset
- payload offset

### Chunk table

One fixed-size entry per target chunk.

Each entry contains:

- chunk kind
  - `0 = base-copy`
  - `1 = delta-bytes`
- target chunk size
- payload offset
- payload size

### Semantics

- `base-copy`
  - payload fields are zero
  - runtime reads the corresponding byte range from the installed base file
- `delta-bytes`
  - payload stores the full target bytes for that chunk
- final chunk may be shorter than the nominal chunk size

### Important constraint

Chunk addressing is positional. Target chunk `N` maps to offset `N * chunkSize`. For `base-copy`, runtime reads the same offset from the base file.

This means v1 handles shifts by marking shifted target chunks as changed chunks. That is less compact than a global copy/insert delta, but much better for runtime random access.

## Runtime Read Behavior

The runtime should refactor virtualized file serving behind a generic file-source abstraction with two backends:

- full replacement source
- delta source

### Normal read path

For `CreateFile` plus `ReadFile` style access:

1. Open the real on-disk base file.
2. Validate exact base size and SHA-256 once for the file definition.
3. Serve reads by translating requested ranges into target chunks.
4. For each chunk:
   - `base-copy`: read from the installed file at the same offset
   - `delta-bytes`: read from the `.hgdelta` payload
5. Keep a small in-memory cache for hot chunks.

Steady-state behavior must stay bounded in memory and must not materialize the whole target file.

### Memory-mapped path

For `CreateFileMapping` and `MapViewOfFile`:

1. Validate exact base size and SHA-256.
2. Materialize the fully reconstructed target file into:
   - `helengamehook\\cache\\resolved\\<pack>\\<build>\\<file-id>\\...`
3. Verify the reconstructed target size and SHA-256.
4. Reuse that resolved cache file for later mappings until invalidated.

This is the practical generic solution for mapped files. It avoids trying to emulate sparse patched views directly through the mapping API.

## Cache Design

Two cache layers are required:

- in-memory chunk cache for normal delta-backed reads
- on-disk resolved-file cache for mapped-file consumers

Resolved cache keys must include:

- pack id
- build id
- virtual file id
- base file size and SHA-256
- delta file identity

If any input changes, the resolved cache file is rebuilt.

## Failure Behavior

The runtime must fail hard for any declared delta-backed file when:

- the base file is missing
- the base file size mismatches
- the base file SHA-256 mismatches
- the `.hgdelta` header is malformed
- the chunk table is malformed
- the reconstructed target size mismatches
- the reconstructed target SHA-256 mismatches

No silent fallback to the original game file is allowed.

## Runtime Boundary

Generic C++ runtime owns:

- delta container parsing
- base file validation
- random-access mixed reads from base and delta payload
- resolved cache materialization
- virtual file dispatch

Developer tooling owns:

- building the final patched binary target
- comparing base and target
- generating `.hgdelta`
- selecting chunk size
- emitting pack JSON that references either a full file or a delta file

The runtime must not understand Unreal packages, Batman assets, or any other game-specific format.

## Batman Rollout

Batman is the proving target, but not the design boundary.

Recommended rollout:

1. Add `.hgdelta` reader and tests.
2. Extend `VirtualFileDefinition` and `files.json` parsing to support source kinds.
3. Add delta-backed streamed reads.
4. Add resolved-cache materialization for mapping consumers.
5. Add a Batman-local builder step that generates:
   - patched `BmGame.u`
   - `BmGame-subtitle-signal.hgdelta`
6. Switch the Batman gameplay package declaration from full file to delta.
7. Keep native hook blobs and other small assets as normal files for now.

## Testing

### Unit tests

- valid `.hgdelta` parse
- malformed header rejection
- malformed chunk table rejection
- base mismatch rejection
- target hash mismatch rejection
- random-access read correctness across mixed chunk types
- resolved cache rebuild and reuse
- `files.json` parse for both full and delta sources

### Integration tests

- virtualized `ReadFile` path with a small synthetic base plus delta pair
- `CreateFileMapping` path materializing a resolved cache file
- Batman pack load using `delta-on-read`

## Rationale

This design gives Helen both:

- simple full-file replacement for small assets
- scalable delta-backed delivery for very large binaries

It keeps the runtime generic, preserves exact-build safety, and avoids storing giant rebuilt outputs in git.
