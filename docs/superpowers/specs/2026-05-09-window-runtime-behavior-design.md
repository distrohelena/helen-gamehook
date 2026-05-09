# Window Runtime Behavior Design

## Goal

Add a generic window and input behavior layer to Helen GameHook so packs can opt into focus spoofing, background input, cursor control, and window management without embedding low-level Win32 hacks into per-game build hooks.

The first slice must stay fully flag-driven:

- nothing is enabled by default
- packs choose which raw config keys they declare
- packs may expose curated feature toggles for those keys
- the runtime remains capable of exposing every raw key later through Helen Gaming Platform

This design must also leave a clean path for later higher-level options such as borderless windowed mode, which can be expressed as curated pack behavior built on the same generic runtime service.

## Scope

In scope:

- one generic runtime-owned hook service for window and input behavior
- flag-driven focus spoofing through selected Win32 APIs
- flag-driven background input support through controlled WndProc filtering
- optional suppression of raw input registration
- optional cursor clipping
- optional forced window position, size, topmost state, and frame removal
- pack-defined config entries and features that expose the raw runtime keys
- tests for flag resolution, hook installation, and no-op behavior when flags are off

Out of scope:

- a Batman-only one-off implementation
- declarative arbitrary Win32 detours inside pack JSON
- a finished Helen Gaming Platform UI
- higher-level borderless-windowed presets
- per-game heuristics that guess which combination of flags should be enabled

## Approaches Considered

### Recommended: Generic Runtime Service Plus Pack-Declared Features

Add one runtime-owned `WindowBehaviorHookSet` that reads ordinary config keys from the existing Helen config store. Packs declare whichever keys they want and optionally surface them as curated features.

Why this is the right choice:

- it matches the current runtime architecture, where `HelenGameHook.dll` owns generic services and packs own declarative metadata
- it keeps all Win32 detour logic in one place instead of duplicating it per game
- it preserves a future raw-config UI because the config keys remain normal Helen runtime keys
- it gives Batman a usable path now without locking future games into Batman-specific hook blobs

### Rejected: Pack-Specific Native Blob Hooks

This would put the feature directly into `hooks.json` or game-specific native payloads.

Why not:

- too brittle for focus and window management
- hard to reuse across games
- poor observability and testing
- wrong abstraction level for a core runtime capability

### Rejected: Fully Declarative API Detour Metadata

This would let packs declare arbitrary Win32 API hooks in manifest files.

Why not:

- far too much surface area for a first pass
- unsafe and difficult to validate
- unnecessary when the actual needed behaviors are already known

## User-Facing Behavior

The runtime should expose independent raw config keys under a generic `window.*` namespace. Packs may choose to expose some of those keys as user-facing feature toggles, while later advanced tooling may expose all raw keys directly.

The first-pass keys should be:

- `window.focusSpoofEnabled`
- `window.activeWindowSpoofEnabled`
- `window.backgroundInputEnabled`
- `window.blockRawInputRegistrationEnabled`
- `window.clipCursorEnabled`
- `window.removeWindowFrameEnabled`
- `window.forcePositionEnabled`
- `window.forceSizeEnabled`
- `window.forceTopmostEnabled`
- `window.positionX`
- `window.positionY`
- `window.width`
- `window.height`

Rules:

- every key defaults to an inactive or neutral value
- packs opt in by declaring the keys they need
- curated pack features should be independent toggles rather than bundled presets
- later higher-level options may write several of these raw keys together, but that composition is not part of this first slice

## Runtime Design

### Service Ownership

Add one dedicated runtime component, tentatively `WindowBehaviorHookSet`, beside the existing runtime services such as file virtualization, module-load routing, and D3D9 texture replacement.

The service owns:

- low-level Win32 hook installation and removal
- main-window discovery for the current process
- WndProc subclassing
- per-flag decision logic
- logging and diagnostics for installed or failed sub-features

Packs do not own any of that logic. Packs only declare config entries and optional editor-visible features.

### Hook Installation Model

The service should install only the hooks required by the currently enabled flags. It must not patch everything unconditionally just because one related feature is enabled.

This should follow the existing runtime hook style:

- use the existing IAT hook and inline hook facilities already present in `HelenRuntime`
- keep the hook set process-local
- install during runtime initialization after config is available
- remove on normal teardown, while preserving the existing loader-lock detach precautions already used by the runtime

### Main Window Discovery

The service needs a reliable current game window handle before focus spoofing and WndProc filtering can work.

The initial rule should be simple:

- enumerate top-level windows owned by the current process
- choose the first visible main candidate that belongs to the process and is not a child tool window

This should stay centralized inside the service rather than being encoded in pack metadata. If later games need a stronger selection strategy, the runtime can evolve the discovery logic without changing the external config contract.

### Focus Spoofing

When enabled, the service should return the captured game window handle from selected focus-related APIs so the game behaves as though it is still the active foreground window.

Initial targets:

- `GetForegroundWindow`
- `GetFocus`
- `GetActiveWindow`

The config split should allow selective enablement:

- `window.focusSpoofEnabled` controls `GetForegroundWindow`
- `window.activeWindowSpoofEnabled` controls `GetFocus` and `GetActiveWindow`

This keeps the behavior explicit and avoids silently broadening scope when only one piece is desired.

### Background Input

Games often stop accepting input because their message loop reacts to deactivation or inactive-window input transitions. The runtime should support a controlled WndProc bridge that filters only the messages required to keep input alive while the game is unfocused.

When `window.backgroundInputEnabled` is enabled, the service should subclass the discovered main game window and selectively suppress or normalize the messages that cause input loss.

The initial candidate message set should cover the focus-transition and input-loss paths observed in the `ncoop` branch:

- `WM_ACTIVATE`
- `WM_NCACTIVATE`
- `WM_INPUT`

The exact filter list should stay in runtime code, not in pack metadata. The implementation must keep the original WndProc and forward messages that are not explicitly handled by the enabled feature.

### Raw Input Registration Suppression

Some games rely on raw input registration in ways that interfere with background control. When `window.blockRawInputRegistrationEnabled` is enabled, the runtime should intercept `RegisterRawInputDevices` and suppress registration instead of letting the game reconfigure raw input normally.

This should remain independent from `window.backgroundInputEnabled` because a game may need one without the other.

The runtime should not invent a best-effort fallback path. If the flag is on, the hook blocks the registration request. If the hook cannot be installed, the runtime logs the failure and leaves only that sub-feature inactive.

### Cursor Control

When `window.clipCursorEnabled` is enabled, the runtime should intercept `ClipCursor` and enforce the requested behavior against the configured game window rectangle rather than blindly trusting the game's own calls.

This gives the runtime a reusable base for future multi-window or borderless behavior without baking cursor policy into per-game code.

### Window Management

The service should support optional control over size, position, topmost state, and frame removal through a combination of API hooks and WndProc-time enforcement.

The initial low-level targets should be:

- `SetWindowPos`
- `SetWindowLongW`
- `SetWindowLongPtrW`

The corresponding flags should behave as follows:

- `window.forcePositionEnabled` uses `window.positionX` and `window.positionY`
- `window.forceSizeEnabled` uses `window.width` and `window.height`
- `window.forceTopmostEnabled` keeps the game window in the topmost band
- `window.removeWindowFrameEnabled` strips the standard caption and frame styles

The service should treat these as independent controls even if some future curated option chooses to flip several together.

## Integration With HelenGameHook

The new service should be initialized by `HelenGameHook.dll` after the runtime config store and command dispatcher exist, because the hook set depends on resolved config values.

The intended order is:

1. initialize the config store and dispatcher
2. load and merge the active pack set
3. register declared config entries
4. initialize optional runtime services, including window behavior, based on the current config keys
5. continue normal pack runtime initialization

The service should be treated as optional infrastructure rather than a pack-specific subsystem. That means:

- no Batman-specific type names in the runtime service
- no dependence on Batman INI logic
- no assumption that the pack has a UI asset calling into the runtime

## Pack Authoring Model

Packs should expose this behavior the same way they expose any other Helen-owned config:

- declare the raw `window.*` config keys they want in `pack.json`
- optionally declare user-facing `features` entries that point at those config keys

For the current pack model, that means independent `toggle` features are the right surface for boolean-style options. Numeric rectangle values such as `positionX`, `positionY`, `width`, and `height` may remain raw config only until the tooling grows a richer numeric editor.

This gives the future platform two layers:

- curated pack features for normal users
- full raw config editing for advanced users

## Failure Handling

This service should fail narrowly, not abort the entire runtime for optional behavior.

If a requested sub-feature cannot be installed:

- log the exact hook or window operation that failed
- leave only that sub-feature inactive
- keep unrelated runtime services active
- do not silently claim that the feature is active

Examples:

- if `GetForegroundWindow` cannot be hooked, focus spoofing is inactive but file virtualization still works
- if WndProc subclassing fails, background input is inactive but size or clip behavior may still work if their hooks installed successfully
- if all window behavior flags are off, the service should behave as a complete no-op

The runtime should continue to reserve hard-fail behavior for foundational startup problems rather than optional window features.

## Testing

### Flag Resolution Tests

- all flags default to inactive values
- enabling one flag installs only the hooks required by that flag
- enabling multiple flags installs the union of required hooks
- rectangle-dependent behavior reads the exact configured numeric keys

### Main Window Discovery Tests

- process-owned visible top-level windows are considered
- child or irrelevant windows are excluded
- no candidate window produces a clean inactive result rather than a fabricated handle

### Hook Set Tests

- focus spoof APIs return the discovered game window only when their flags are enabled
- disabled focus flags preserve original API behavior
- WndProc subclassing forwards unrelated messages unchanged
- background-input filtering only affects the configured message set
- raw input registration suppression blocks calls only when enabled
- cursor clipping is inactive when disabled

### Regression Baseline Tests

- with all `window.*` flags off, the hook set is a no-op
- existing runtime services remain unaffected when the window behavior service is present but inactive

These no-op tests are critical because the requested design is explicitly flag-based and must never enable behavior by accident.

## Risks

Primary risk:

- main-window discovery may not identify the correct gameplay window in every title

Mitigation:

- keep discovery logic centralized and testable
- log which window handle was selected
- evolve the selection rule in runtime code rather than exposing unstable pack-side heuristics too early

Secondary risk:

- message filtering for background input can accidentally suppress unrelated behavior if the filter becomes too broad

Mitigation:

- start with the smallest known message set
- keep each message decision explicit in code
- verify that disabled flags preserve the original behavior path

Tertiary risk:

- future borderless-windowed presets may tempt the runtime to hide raw keys behind opaque one-off behavior

Mitigation:

- keep raw `window.*` keys as the stable primitive layer
- let higher-level presets compose those keys rather than replace them
