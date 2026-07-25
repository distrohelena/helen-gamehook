# Halo: Campaign Evolved Local Split-Screen Design

## Goal

Enable two people to play Halo: Campaign Evolved on one PC through one
`HaloCampaignEvolved.exe` process and two ordinary controllers. The mod must use
the game's existing Unreal local-player and viewport facilities; it must not
launch a second game process, depend on NucleusCoop, or emulate platform
services.

The first supported target is the installed PC build rooted at
`F:\games\Halo Campaign Evolved` on 2026-07-25. Every release is pinned to the
exact executable fingerprint for the build it supports.

## Evidence and Constraints

The PC executable is an Unreal build (`Meteorite`) and includes the engine's
local-player and split-screen primitives, including `CreatePlayer`,
`LocalPlayer`, `SplitScreen`, `bUseSplitscreen`, and two-, three-, and
four-player layout definitions. Its live user configuration likewise contains
`SplitscreenHudTextSize` and player-trait slots one through four.

This demonstrates that the engine facilities are present, but does not prove
that the PC game layer exposes them. Officially supported PC co-op is online
only. The mod therefore treats successful local-player creation, viewport
layout, and controller ownership as separate runtime checks. It must fail
closed if any expected build-specific address or state is absent.

The current HelenHook runtime is a useful foundation, but its documented
Batman-first build is Win32 and its default bootstrap is a `dinput8.dll` proxy.
Campaign Evolved requires an x64 runtime. Before making a playable mod, the
project must also verify that the game loads the selected proxy DLL; otherwise
the runtime needs an explicit, documented x64 bootstrap path. No bootstrap
mechanism is assumed in this design.

## Architecture

### Game Pack

Add a `halo-campaign-evolved-splitscreen` HelenHook pack. The pack matches only
`HaloCampaignEvolved.exe` by file name, byte size, and SHA-256. A mismatch logs
the reason and installs no game-specific hook.

The pack holds build-specific signatures, addresses, and declarative hook
definitions. Game-independent code stays in HelenHook. The pack contains no
platform-account or networking emulation.

### Split-Screen Controller

Add a game-specific controller with one responsibility: transition a verified
main-menu game state to exactly two local players and the native two-player
viewport arrangement.

The controller waits for the game's frontend/game-instance readiness point,
then resolves the relevant local-player and viewport objects. It does not run
from `DllMain` or under loader lock. A deliberately chosen hotkey triggers the
first experiment so the game is completely initialized before mutation. The
controller records the outcome of each prerequisite and makes no partial retry
after a failure.

The controller's success conditions are:

1. The game reports exactly two local players.
2. The native two-player viewport layout is active.
3. Controller 1 drives player 1 and controller 2 drives player 2 independently
   at the frontend.
4. The game remains stable while entering and leaving a campaign session.

If player creation succeeds but the viewport is absent, that is a distinct
failure state. If the viewport exists but both controllers drive the same
player, input ownership is a distinct failure state. Keeping those cases
separate makes the investigation diagnosable.

### Input

The first release is controller-only: two XInput-compatible controllers, no
keyboard/mouse player. The mod relies on the game's own controller support and
only intervenes if PC input binding sends both devices to player 1. The game
already records controller settings in `HaloGlobalGameUserSettings.ini`.

The controller-index investigation must remain inside the single process. It
must not use external input redirection, multiple Windows users, or a second
game executable.

### Bootstrap and Packaging

The deliverable is a compact HelenHook runtime directory beside the game (or a
documented equivalent bootstrap directory) containing the x64 proxy/runtime and
the selected pack. The normal launcher remains the game's own launcher.

The installer is out of scope initially. Early development uses a reversible,
manual placement procedure and a log file. The final package must include a
complete removal path that restores the original files or removes only files it
added.

## Implementation Stages

### Stage 1: x64 and Bootstrap Proof

Build HelenHook for x64 and prove that its runtime initializes in the game
without changing game state. Record the executable fingerprint and write a
single startup log entry. If the proxy is not loaded, stop and select a
documented bootstrap mechanism before investigating split-screen.

### Stage 2: Read-Only Runtime Discovery

Locate and validate the main-menu-ready state, game instance, local-player
collection, and viewport client for the pinned build. The runtime only logs
counts, state transitions, and addresses. It does not create a player.

### Stage 3: Frontend Player-Two Probe

On the explicit hotkey, create one additional local player through the game's
native path and request the two-player layout. Verify the four frontend success
conditions above. Do not enter campaign while any condition is missing.

### Stage 4: Campaign Validation

Start a campaign from the two-player frontend, test each controller's player
ownership, HUD/camera separation, pause behavior, and return to menu. Test
offline/local play first. Treat any online behavior as unsupported until it is
independently verified.

### Stage 5: Pack Hardening

Replace exploratory address resolution with tightly scoped signatures and
expected-byte validation. Add a user-facing toggle, clear diagnostics, removal
instructions, and regression tests for pack parsing and fingerprint matching.

## Error Handling and Safety

- A fingerprint mismatch means no hooks and no local-player action.
- Any failed runtime prerequisite logs the named prerequisite and leaves the
  game in its original state.
- No hook is installed until all required resolution and expected-byte checks
  succeed.
- A failed experimental player-two action is terminal for that game session;
  users restart the game instead of accumulating partial local-player state.
- Development tests use a disposable profile or backed-up settings directory.
- The mod targets local/offline split-screen only during initial validation.

## Testing

Automated tests cover x64 pack selection, executable fingerprint matching,
signature/expected-byte validation, state-machine transitions, and loggable
failure categories. They do not launch the game.

Manual acceptance testing on the pinned build covers:

1. Normal game launch with the mod disabled or absent.
2. Runtime initialization and build-match logging without any player mutation.
3. Hotkey creation of player 2 and visible two-player frontend viewport.
4. Independent controller navigation for both local players.
5. Campaign load, gameplay, pause, return to menu, and clean game exit.
6. A deliberately mismatched executable, confirming that the mod refuses to
   activate.

## Non-Goals

- NucleusCoop handler development.
- Running two copies of the game.
- Steam, Xbox, EOS, PlayFab, or other identity/network emulation.
- Online co-op compatibility.
- Keyboard/mouse plus controller support in the first release.
- More than two local players.
