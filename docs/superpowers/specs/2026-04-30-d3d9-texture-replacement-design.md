# D3D9 Texture Replacement Design

## Goal

Add a reusable runtime feature to `Helen GameHook` that can replace one or more Direct3D 9 textures at runtime without modifying the original game files on disk.

The first target scenario is Batman subtitle and prompt font sharpening:

- the game already lays out the text correctly
- the game renders text through generated triangles and a shared glyph atlas
- the user goal is only to increase glyph source resolution
- text behavior, wrapping, placement, and scale should stay unchanged

This feature should generalize to other games that render UI text or sprites through stable `D3D9` textures.

## Current Runtime Fit

`Helen GameHook` currently provides:

- proxy bootstrap through `dinput8.dll`
- pack discovery and build matching
- virtual file replacement
- declarative command execution
- inline native hook installation

It does not currently provide:

- graphics API interception
- `D3D9` device wrapping
- runtime texture replacement

This work therefore adds a new runtime subsystem. It must remain optional and pack-driven so existing file and code hook workflows continue unchanged.

## Non-Goals

- no `D3D11`, `OpenGL`, or `Vulkan` support in the first pass
- no generic shader editing
- no generic vertex rewriting
- no attempt to fix text layout, scale, clipping, or wrapping
- no requirement to patch original package files

## Chosen First Slice

Implement a minimal `D3D9` texture replacement pipeline with these properties:

- packs can declare one or more replacement textures
- runtime identifies matching textures from the live resource lifecycle, not the draw loop
- runtime substitutes replacement texture content for matched textures
- matching can use texture dimensions, format, hash, and optional upload metadata
- replacement is limited to `2D` textures used through `IDirect3DDevice9`

The first version should optimize for stability and inspectability rather than maximal flexibility.

## Why This Approach Fits Batman

The Batman subtitle investigation established that:

- the visible target text is not a simple HUD `GFx` image export
- the renderer generates triangles for text and samples a shared font texture
- the user wants sharper glyphs, not different layout behavior

That makes runtime atlas replacement a better fit than repeated package edits to `BmGame.u`, `Startup_INT.upk`, or `HUD-extracted.gfx`.

## Architecture

### 1. Import Hook Entry

The runtime should install an optional import hook for:

- `d3d9.dll!Direct3DCreate9`

When present, the detour calls the original function, receives the real `IDirect3D9*`, and returns a wrapped proxy object instead.

If the import is not present or wrapping fails, the runtime must leave graphics behavior unchanged.

### 2. COM Wrapper Layer

Add proxy wrappers for:

- `IDirect3D9`
- `IDirect3DDevice9`
- `IDirect3DTexture9`

Batman already showed that device-level discovery in `SetTexture` is too aggressive. Even metadata-only observation there still sits in the render hot path. The first stable slice should therefore intercept only lifecycle methods:

- `IDirect3D9::CreateDevice`
- `IDirect3DDevice9::CreateTexture`
- `IDirect3DDevice9::UpdateTexture`
- `IDirect3DDevice9::Reset`
- `IDirect3DTexture9::LockRect`
- `IDirect3DTexture9::UnlockRect`
- `IUnknown::Release` on wrapped interfaces

All other calls should forward directly to the real interface.

### 3. Texture Tracking

When the game creates a texture, the runtime records:

- texture pointer identity
- width
- height
- mip level count
- usage flags
- format
- pool

The runtime must also detect when texture content becomes stable enough to fingerprint.

For the first version, the simplest acceptable route is:

- intercept `IDirect3DTexture9::LockRect` and `UnlockRect` through the wrapped texture object
- mark the texture dirty on `LockRect`
- compute a hash of level `0` after `UnlockRect` when the texture was written
- propagate fingerprints through `IDirect3DDevice9::UpdateTexture` when the game uploads from one tracked texture into another

This avoids guessing texture contents solely from creation parameters and keeps heavy work out of the draw loop.

### 4. Replacement Matching

Each pack-defined replacement entry should match on:

- required width
- required height
- required `D3DFORMAT`
- required level-0 content hash

Optional narrowing fields should include:

- draw-stage tag or user label
- expected sampler slot
- expected primitive-count range
- expected screen-region heuristic

The mandatory stable key for the first version should be the texture content hash plus dimensions and format.

### 5. Replacement Application

When a texture first matches an entry:

- load the replacement image bytes from pack assets
- convert them to the target runtime format if necessary
- upload the replacement data into a runtime-owned `IDirect3DTexture9`
- associate the original texture identity with the replacement texture

At draw time:

- if the runtime later adds a bind-time substitution point, it should only swap already matched textures
- otherwise forward the original texture unchanged

This approach avoids mutating original game resource memory directly and makes reset or lifetime handling clearer.

### 6. Special K Comparison

Special K’s texture workflow is built around first-load dumping, persistent texture hashes, and later injection or reload of already identified textures. That behavior implies resource-lifecycle interception rather than per-draw discovery.

The equivalent `Helen GameHook` pattern should be:

- observe texture creation
- observe texture uploads until one texture becomes stable enough to fingerprint
- cache the identity and replacement decision
- keep any later bind-time work trivial or optional

`SetTexture` can still become a narrow substitution point later, but it is the wrong place to do first-load discovery.

## Pack Schema Sketch

Add an optional document such as `textures.json` under one build directory.

Example shape:

```json
{
  "replacements": [
    {
      "id": "batman-subtitle-font-atlas",
      "api": "d3d9",
      "match": {
        "width": 1024,
        "height": 1024,
        "format": "A8R8G8B8",
        "hash": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
      },
      "replacement": {
        "path": "assets/textures/batman-subtitle-font-atlas.png"
      },
      "scope": {
        "samplerStage": 0
      }
    }
  ]
}
```

First-pass constraints:

- one replacement image per matched texture
- replacement image dimensions must equal the original dimensions
- runtime rejects mismatched dimensions or unsupported formats

## Runtime Types

Suggested native types:

- `D3d9HookSet`
  - owns import hook installation for `Direct3DCreate9`
- `D3d9ProxyDirect3D9`
  - wraps `IDirect3D9`
- `D3d9ProxyDevice9`
  - wraps `IDirect3DDevice9`
- `D3d9ProxyTexture9`
  - wraps `IDirect3DTexture9`
- `TextureReplacementDefinition`
  - parsed pack metadata for one replacement
- `TextureReplacementRepository`
  - owns validated replacement definitions and source image bytes
- `TextureFingerprint`
  - width, height, format, hash
- `TrackedTextureRecord`
  - live runtime metadata for one original texture
- `TextureReplacementService`
  - matches tracked textures, creates replacement resources, emits diagnostics

This keeps pack parsing, live device logic, and replacement policy separate.

## Logging And Diagnostics

The feature is only practical if it is easy to inspect.

Add debug logging for:

- `Direct3DCreate9` hook installed or skipped
- each tracked texture creation
- each tracked upload path such as `LockRect` or `UpdateTexture`
- each post-upload computed fingerprint
- each successful or failed replacement match
- each replacement association between original and runtime-owned textures
- each `Reset` rebuild

Add an optional debug mode that writes candidate texture fingerprints to a log file so users can discover match keys from live runs.

## Failure Behavior

Do not silently invent fallbacks.

If a pack replacement entry is invalid:

- log the exact reason
- ignore that entry
- leave the game texture untouched

If replacement texture upload fails:

- log the exact `HRESULT`
- keep the original texture bound

If `D3D9` hooks cannot be installed:

- log that the subsystem is disabled for the current process

## Verification Strategy

### Unit Coverage

Add coverage for:

- texture replacement definition parsing
- format parsing
- hash parsing
- replacement asset validation
- repository lookup by fingerprint

### Live Validation

For Batman:

1. identify the live subtitle atlas fingerprint from a `D3D9` trace
2. create a replacement atlas with the same dimensions
3. launch the game with a debug pack that logs texture creation and upload matches
4. confirm only subtitles and prompt text are affected
5. confirm text remains positioned and wrapped correctly

## Incremental Delivery Plan

### Phase 1

- metadata schema
- asset loading
- `Direct3DCreate9` import hook
- device and texture wrappers
- creation and upload fingerprint logging only

### Phase 2

- replacement texture creation
- minimal substitution point for already matched textures
- reset handling

### Phase 3

- optional draw-scope narrowing for shared atlases
- editor support for replacement metadata

## Batman Trace Plan

To extract the correct subtitle atlas for Batman:

1. capture the intro subtitle draw using `apitrace` on the `D3D9` path
2. find the `DrawIndexedPrimitive` calls that render the subtitle quads
3. inspect the bound stage-0 texture for those draws
4. export the texture image from the trace viewer
5. fingerprint that texture by dimensions and raw bytes
6. use that fingerprint as the first replacement rule

This plan assumes the subtitle and prompt path share one stable atlas, which matches the current user observation.

## Batman Correction

The Batman runtime experiments established one concrete rule for this subsystem:

- no discovery work belongs in `SetTexture`

That path runs in the draw loop and was unstable even for metadata-only observation. The current Batman build therefore keeps the `Direct3DCreate9` entry hook but disables device-level `SetTexture` interception unless real replacement rules exist. The next implementation slice should start from wrapped `CreateTexture`, `LockRect`, `UnlockRect`, and `UpdateTexture`.
