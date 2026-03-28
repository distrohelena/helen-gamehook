# Helen GameHook

`Helen GameHook` is a generic Windows game-hook runtime intended to be dropped next to a game executable through a proxy DLL such as `dinput8.dll`.

This repository currently contains:

- `HelenRuntime`: shared utility/static library
- `HelenGameHook.dll`: generic runtime/dispatcher
- `dinput8.dll`: proxy bootstrap DLL
- `games/HelenBatmanAA/helengamehook/packs`: the current Batman pack source
- `tools/HelenGameHook.Editor`: C# GUI/editor skeleton

## Intended architecture

1. The game loads `dinput8.dll`.
2. The proxy forwards `DirectInput8Create` to the real system `dinput8.dll`.
3. The proxy also loads `HelenGameHook.dll`.
4. `HelenGameHook.dll` discovers patch packs in `helengamehook\packs\` next to the game executable.
5. The runtime selects the first pack whose executable list matches the current process.
6. The runtime exposes generic commands, patch primitives, and config plumbing to that pack.

## Current status

This is a working Batman-first vertical slice, not a finished general-purpose framework. It includes:

- proxy bootstrap
- runtime logging
- JSON pack loading
- pack discovery and build matching
- virtual file replacement in RAM
- declarative command execution
- native blob-backed hook installation
- bounded state observation for Batman subtitle size

It does not yet include:

- a finished developer GUI workflow
- broader multi-game tooling
- a polished public modding SDK

## Build

Open [HelenGameHook.sln](C:\dev\helenhook\HelenGameHook.sln) in Visual Studio 2022 and build `Release|Win32`.

Output layout:

- `bin\Win32\Release\dinput8.dll`
- `bin\Win32\Release\HelenGameHook.dll`
- `games\HelenBatmanAA\helengamehook\packs\batman-aa-subtitles`
- `tools\HelenGameHook.Editor`

## Next steps

1. Clean up the Batman-first runtime slice.
2. Normalize the Batman pack/tooling layout under `games\`.
3. Continue generalizing pack authoring without reintroducing per-game DLL logic.
