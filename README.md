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

## Live game navigation tools

Do not drive Batman or any other game with ad-hoc OS-global keyboard automation such as PowerShell `SendKeys`, `System.Windows.Forms.SendKeys`, or scripts that only call `SetForegroundWindow` and then type. Those inputs can land in the wrong foreground app.

Use the HelenUI navigation tooling instead:

- `C:\dev\helenui\plugins\navigator-service`: the session-based navigation plugin/service for capture, recognition, targeted keyboard input, and project-driven navigation.
- `C:\dev\helenui\plugins\screenshot-cli`: window capture helper used by the navigation service and diagnostics.
- `C:\dev\helenui\plugins\recognition-cli`: recognition pipeline for matching screenshots against HelenUI project JSON.

For Batman live tests, start a navigator session against the `ShippingPC-BmGame.exe` window and let `navigator-service` send inputs through its session API or `navigateToScreen` flow. If a one-off diagnostic script is needed, it must call the navigator service or a targeted input primitive, not global `SendKeys`.

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
