# Batman Retail Gameplay Delta Design

## Goal

Make the Batman gameplay subtitle-size pack work against a true retail vanilla `BmGame.u` without requiring any unpacked package file in the installed game directory.

This pass is intentionally limited to gameplay only. `Frontend.umap` stays vanilla and out of the shipped pack.

## Problem Statement

The current Batman pack is not retail-vanilla safe.

- The shipped `BmGame.u` delta in `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/files.json` expects an unpacked base:
  - size `100365345`
  - SHA-256 `621a5c8d99c9f7c7283531d05a4a6d56bdf15ad93ede0d5bf2f5d3e45117ff36`
- The local retail-style backup file is:
  - size `59857525`
  - SHA-256 `4306148e7627ec2c0de4144fd6ab45521b3b7e090d1028a0b685cadafafb89e6`
- The current `BmGameGfxPatcher` explicitly documents that it supports unpacked Unreal packages only and does not rebuild chunk compression.
- Direct tests on the retail compressed package fail in the current parser:
  - retail `BmGame.u` load fails with `Arithmetic operation resulted in an overflow`
  - retail `Frontend.umap` load fails with `Index and count must refer to a location within the buffer`

This means the current Batman gameplay delta cannot honestly be described as "drop in Helen DLLs plus the pack on untouched Batman."

## Non-Goals

- No frontend `MainMenu.MainV2` work in this pass.
- No `Frontend.umap` delta in the shipped runtime pack.
- No changes to the generic runtime `hgdelta` format.
- No support for encrypted packages.
- No broad asset-tool cleanup outside what is required for retail `BmGame.u`.

## Current Format Findings

The current parser already reads the package header and logical table offsets correctly for both retail and unpacked files. The divergence is in chunk compression support.

Observed header facts:

- Retail `BmGame.u.original-backup`
  - package size field: `5749076`
  - compression flags: `0x00020057`
  - compression chunk count: `2`
- Unpacked `builder/extracted/bmgame-unpacked/BmGame.u`
  - same logical package fields
  - same compression flags field
  - compression chunk count: `0`

The retail file therefore appears to be the same logical package content with chunk-compressed storage and an on-disk compression chunk table. The current parser skips the chunk table, then incorrectly treats compressed physical offsets as if they were direct readable object offsets.

## Chosen Approach

Extend `BmGameGfxPatcher` so it can read and rewrite retail compressed `BmGame.u` directly.

This is preferred over an external unpack-repack bridge because:

- it preserves the actual shipping contract inside this repo
- it keeps the Batman workflow reproducible
- it avoids introducing another opaque packer dependency
- it allows the existing manifest-driven patch flow to stay the single source of truth

The shipped Batman pack will revert to gameplay-only while this work is completed.

## Architecture

### 1. Logical Package Model Over Physical Storage

`UnrealPackage` should stop exposing only raw on-disk bytes. It needs a logical package view that works for both:

- unpacked packages where object data can be sliced directly from the file
- compressed packages where the file must first be read through chunk decompression

The key boundary is:

- physical package file bytes are a storage detail
- logical package bytes are what the export table offsets and sizes refer to

Everything that inspects exports should operate on logical bytes.

### 2. Compression Chunk Support

Add explicit compression chunk parsing and decoding support inside `BmGameGfxPatcher`.

Required responsibilities:

- parse the retail chunk table from the package header trailer
- map logical offsets to compressed physical chunk ranges
- decompress chunk payloads into a logical byte image
- preserve enough metadata to rebuild a valid compressed output package after patching

These responsibilities should live in focused types under `BmGameGfxPatcher`, one class per file.

### 3. Patch Application On Logical Bytes

`GfxPatchApplier` currently:

- copies the input file
- appends patched export objects to the end of the physical file
- rewrites export offsets and lengths in place

That approach is only valid for unpacked files.

For retail compressed packages, the new flow must be:

1. load the logical package image
2. apply export replacements against logical package bytes
3. rebuild the package storage image with valid compressed chunks
4. write the rebuilt package file
5. reopen it through the same parser and verify the patched exports

Unpacked files may keep the current fast path if it remains compatible with the new abstractions.

### 4. Gameplay-Only Batman Shipping

This pass removes frontend shipping from the Batman pack.

`files.json` should contain exactly one virtual file:

- `bmgameGameplayPackage`
  - path `BmGame/CookedPC/BmGame.u`
  - `mode = "delta-on-read"`
  - `kind = "delta-file"`
  - base fingerprint set to the true retail `BmGame.u`

There should be no `frontendMapPackage` entry in the shipped pack for this pass.

## File Boundaries

### `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/UnrealPackage.cs`

Modify to:

- parse retail compressed package metadata instead of only unpacked storage
- read export objects from a logical package image
- preserve header/chunk metadata needed for rebuild

### New `BmGameGfxPatcher` support files

Expected new focused classes:

- compression chunk record parser
- compressed package logical reader
- compressed package writer or rebuilder

Exact filenames can be chosen during implementation, but each file must have one clear responsibility.

### `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/GfxPatchApplier.cs`

Modify to:

- patch logical package bytes
- branch between unpacked and compressed output write paths
- verify patched compressed output by reopening through the same parser

### `games/HelenBatmanAA/scripts/Prepare-BatmanBuilderWorkspace.ps1`

Modify to:

- keep using a trusted retail `BmGame.u` as the gameplay base source of truth
- extract pause/HUD source assets from the retail gameplay package through the upgraded patcher
- stop implying that an unpacked gameplay package is the shipped base

### `games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1`

Modify to:

- rebuild only the gameplay pack in this pass
- remove frontend package generation and frontend delta output
- write a gameplay-only `files.json`
- pin the gameplay base fingerprint to the retail `BmGame.u`

### `games/HelenBatmanAA/scripts/Deploy-Batman.ps1`

Keep and adapt:

- installed-base preflight must remain mandatory
- expected manifest shape becomes gameplay-only

### `games/HelenBatmanAA/scripts/Test-BatmanKnownGoodGameplayPackage.ps1`

Modify to:

- assert exactly one virtual file in the shipped Batman pack
- assert retail gameplay base fingerprint:
  - size `59857525`
  - SHA-256 `4306148e7627ec2c0de4144fd6ab45521b3b7e090d1028a0b685cadafafb89e6`

### `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`

Modify to:

- revert checked-in Batman expectations to gameplay-only
- pin the rebuilt retail-safe gameplay pack metadata

### `games/HelenBatmanAA/README.md`

Modify to:

- describe this pass as gameplay-only
- remove any statement that the frontend delta is currently shipped
- explain that retail-safe shipping means direct compatibility with compressed vanilla `BmGame.u`

## Testing Strategy

### Parser-Level Tests

Add focused tests that prove:

- retail compressed `BmGame.u` header parsing reads chunk metadata correctly
- export lookup for `PauseMenu.Pause` and `GameHUD.HUD` works on the retail file
- logical object reads from the retail file return stable bytes

### Rebuild Red/Green Test

Add a focused gameplay build regression that currently fails on retail `BmGame.u`, then passes after the patcher work:

- input: retail `BmGame.u.original-backup`
- action: build the patched gameplay package
- assertions:
  - output package is readable by the upgraded patcher
  - patched exports exist and match expected replacement content
  - output file remains a valid compressed retail-style package

### Batman Pack Verification

`Test-BatmanKnownGoodGameplayPackage.ps1` must pass only when:

- the pack contains one gameplay virtual file
- the base fingerprint matches the retail backup
- the generated gameplay target matches the checked-in manifest
- the shipped `.hgdelta` matches the checked-in file

### Runtime Verification

Required verification before calling this pass done:

1. rebuild gameplay-only Batman pack from retail `BmGame.u`
2. run `MSBuild tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32 /m`
3. run `bin\Win32\Debug\tests\HelenRuntimeTests.exe`
4. run `powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Deploy-Batman.ps1 -Configuration Debug`
5. verify deploy succeeds on a restored retail `BmGame.u`
6. launch Batman
7. verify the hook log registers and serves only `BmGame.u`
8. verify pause `Options -> Audio` still shows `Subtitle Size`
9. verify no frontend virtual file is registered or served

## Acceptance Criteria

This pass is complete only if all of the following are true:

- the shipped Batman pack contains only a gameplay delta entry
- the shipped gameplay delta base fingerprint matches the retail backup `BmGame.u`
- the upgraded `BmGameGfxPatcher` can read and patch retail compressed `BmGame.u`
- deploy succeeds against the restored retail gameplay file without replacing installed package files
- the pause menu `Subtitle Size` feature still works in-game
- the main menu remains completely vanilla

If any step still depends on an unpacked gameplay package in the installed game directory, the pass is not done.
