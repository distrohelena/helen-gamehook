# Batman Graphics Options VSync Slice Design

## Goal

Turn the proven callback-free Batman `Graphics Options` shell into one complete, testable vertical slice: show the live persisted VSync value, let the user edit it, persist the change through Batman's launcher and generated INI files, and keep the title-to-main-menu transition stable.

## Confirmed Baseline

The current shell has passed a live test on the Steam GOTY build:

- `Click to Start` reaches the main menu.
- `Graphics Options` appears in the Options menu.
- Activating it opens the graphics screen.
- The rows are intentionally inert and currently show placeholder values.

That result depends on the current selective FFDec script import and retail-compatible MiniLZO recompression path. This slice must preserve that build route and must not restore the dormant full-screen patch wholesale.

## Scope

This slice activates only the `VSync` row and the final `Apply Changes` row.

The user can:

1. open `Graphics Options`
2. see VSync as `Off` or `On`
3. change the local VSync draft with left, right, or the normal row action
4. apply the changed draft
5. return to the Options menu normally

The change is persisted through the existing Batman graphics configuration service and takes effect according to Batman's normal graphics restart behavior.

## Non-Goals

- Activating any of the other fourteen graphics rows
- Restoring the old full graphics controller
- Restoring unsaved-changes or restart prompts
- Applying VSync immediately on every arrow press
- Adding a new custom `Helen_*` Scaleform callback path
- Redesigning the screen layout
- Changing subtitle-size behavior or its carrier codes
- Applying graphics changes live inside the running renderer

## Considered Approaches

### Approach 1: Extend the stable shell with a VSync-only stock frontend carrier

Keep the current shell scripts and patch structure. Add a small local VSync draft controller, then dispatch the already reserved stock `FE_SetControlType` carrier codes only when `Apply Changes` is activated.

This approach preserves the known-good menu transition and introduces the smallest possible interactive surface.

### Approach 2: Restore the dormant full controller and disable the other rows

Reuse the historical controller, prompts, draft model, and full XML patch while hiding or disabling fourteen rows.

Although this reuses more code, it also restores known defects in prompt handling, unproven `Helen_*` frontend calls, and significantly more ActionScript state than the VSync slice requires.

### Approach 3: Write through `Helen_SetInt` from the row

Call the custom setter whenever VSync changes and invoke the graphics apply command through another custom callback.

Live investigation already showed that the custom `Helen_*` calls were not reliably reached through Batman's frontend GFx boundary. This approach would repeat the failed integration seam.

## Chosen Approach

Use **Approach 1**: extend the stable shell with a VSync-only local draft and the existing stock frontend carrier.

## UI Behavior

### Screen initialization

The shell initializes one controller with:

- an initial VSync state
- a local draft VSync state
- an apply-signal toggle used to make repeated apply events observable
- an apply-in-progress guard
- an apply timer used to separate the value signal from the commit signal

The generated frontend asset does not bake a VSync value. On entry, the controller writes request code `4200` through `FE_SetControlType`, polls `FE_GetControlType`, and waits for HelenHook to replace the carrier value with `4210` for Off or `4211` for On. The VSync row shows `Loading...` during this handshake and `Unavailable` if no response arrives within ten seconds.

All rows except `VSync` and `Apply Changes` remain non-interactive. Their existing placeholder presentation remains unchanged in this slice.

### VSync interaction

The VSync row supports the same user inputs as a normal two-state Batman option row:

- left selects the previous state
- right selects the next state
- the normal row action toggles the state

Because there are only two values, navigation wraps between `Off` and `On`. Each accepted change updates the row immediately and plays the normal forward or back frontend sound.

Changing the row only mutates the local ActionScript draft. It does not write native configuration or the INI file.

### Apply Changes interaction

`Apply Changes` is visible but disabled while the local VSync draft matches the initial state. It becomes enabled as soon as they differ.

When activated:

1. reject duplicate activation while an apply is already in progress
2. block screen input
3. emit `4210` for VSync Off or `4211` for VSync On through `FE_SetControlType`
4. wait one second so the VSync carrier remains observable despite live scan and render-thread scheduling load
5. emit an alternating commit signal, `4990` or `4991`, through the same stock frontend setter
6. let HelenHook's state observers update the graphics draft and invoke the existing apply command
7. clear the apply-in-progress guard and re-enable input after carrier dispatch finishes
8. promote the dispatched draft to the local initial value so Apply remains disabled until another edit

The user stays on `Graphics Options` after apply. The apply action must not call `ReturnFromScreen()`.

`Apply Changes` is enabled only while the local draft differs from the initial value. It remains disabled after dispatch until the user edits VSync again. Runtime logs and the INI comparison remain the authoritative persistence evidence because the stock carrier does not return a command result to GFx.

### Back behavior

Back continues to return directly to the Options menu. This slice does not introduce an unsaved-changes prompt. If the user backs out before applying, the local draft is discarded when the screen is reconstructed.

## Runtime and Package Data

The graphics pack declares the complete existing graphics draft schema because `BatmanGraphicsConfigService::ApplyFromDispatcher` validates and writes a complete graphics state. A startup command loads every current value from `BmEngine.ini` before an apply signal can be handled. Consequently, applying VSync preserves the other graphics values rather than substituting defaults.

Only two state observers are enabled for this slice:

- the VSync observer answers request `4200` from the current `vsync` config value and maps `4210` and `4211` back into that key
- the apply observer maps `4990` and `4991` and invokes `applyBatmanGraphicsDraft`

No observer for another graphics row is included. The carrier codes are distinct from the subtitle-size range `4101` through `4106`.

Both observers scan the structurally validated frontend carrier across `0x10000000..0x30000000`. This range covers the independently observed randomized heap locations near `0x10DF`, `0x1209`, and `0x2B24`; the resolved address is cached for normal polling.

The apply command reuses the existing sequence:

1. `apply-batman-graphics-config`
2. `load-batman-graphics-draft-into-config`

This preserves established failure behavior. A missing or invalid required graphics value causes the operation to fail rather than silently writing defaults.

## Code Shape

- Keep `GraphicsOptionsAssetBuilder.BuildShell(...)` and `GraphicsOptionsXmlPatcher.PatchShell(...)` as the active build path.
- Extend the shell script templates with a dedicated VSync controller instead of importing `BatmanGraphicsOptionsController`.
- Request the current VSync state at screen entry instead of embedding a build-time INI snapshot in the package.
- Make the third fixed row interactive and bind it to the shell controller.
- Make the fifteenth fixed row the visible Apply action.
- Keep other fixed-row actions callback-free and no-op.
- Generate the minimal pack config, commands, bindings, and two-observer hook manifest from the current rebuild script.
- Do not copy historical generated package files into the active pack. Historical commits may be used only as a reference for the proven carrier constants and manifest shape.

## Failure Handling

- Invalid or missing required graphics INI data fails package generation explicitly.
- A malformed package manifest fails validation.
- Missing startup graphics configuration prevents apply rather than creating defaults.
- Unknown carrier values are ignored by exact mapping.
- Duplicate apply activation is rejected while input is blocked.
- A missing initial-state response leaves the setting explicitly unavailable rather than substituting a baked or default value.

The implementation does not show a success banner. It updates the local baseline after dispatch for conventional dirty-state behavior, while the live test confirms the native result independently.

## Test Strategy

Add failing regression coverage before implementation for the following behavior:

### Generated ActionScript

- the shell requests and polls for the live VSync value without embedding a build-time state
- the VSync row shows `Off` and `On`
- only the VSync row has working increment, decrement, and action handlers
- the VSync edit remains local until Apply
- Apply dispatches only `4210` or `4211`, followed by `4990` or `4991`
- Apply separates the value and commit signals by one second
- Apply clears the local dirty state after dispatch and stays disabled until another edit
- no graphics shell script calls `Helen_GetInt`, `Helen_SetInt`, `Helen_RunCommand`, or the historical direct apply export
- Apply does not return from the screen
- Back retains the proven shell return path

### Package manifests

- all required graphics config keys are declared and loaded at startup
- only the VSync and apply state observers are present
- VSync answers `4200` with `4210` or `4211` and maps those response values back into config
- Apply maps only `4990` and `4991`
- the apply command writes and reloads the graphics draft
- no historical unused bindings are restored

### Generated package

- rebuild starts from the verified retail `Frontend.umap`
- the target package and delta hashes are reproducible from current sources
- the selective script import preserves the retail sprites required for title-to-main-menu transition
- package validation succeeds without staging or historical package inputs

## Live Verification Checkpoints

### Checkpoint 1: Menu interaction

1. Install the newly generated package.
2. Start Batman and pass `Click to Start`.
3. Open Options, then `Graphics Options`.
4. Confirm VSync shows `Off` or `On` rather than `undefined` or `Not active`.
5. Confirm left, right, and the normal row action toggle VSync.
6. Confirm `Apply Changes` enables and the other rows remain inactive.
7. Back out and reopen the screen to confirm the transition remains stable.

### Checkpoint 2: Persistence

1. Record the original `UseVsync` value in `UserEngine.ini` and `BmEngine.ini`.
2. Toggle VSync and activate `Apply Changes`.
3. Close the game normally.
4. Confirm runtime logs observed the expected VSync and apply carrier codes.
5. Confirm `UseVsync` changed in both INIs while unrelated sampled graphics values remained unchanged.
6. Relaunch and confirm Batman regenerates `BmEngine.ini` from the launcher-owned `UserEngine.ini` value and the menu reports that live state.
7. Restore the original VSync value if the test is not intended to keep the change.

## Risks and Mitigations

- **Frontend regression:** Preserve the exact stable shell patch and retail recompression route; test the title transition again before persistence testing.
- **Carrier collision:** Use the existing graphics-only `4210/4211` and `4990/4991` ranges with exact observer mappings, separate from subtitle codes.
- **Unrelated INI changes:** Load the complete current graphics state at startup and compare sampled unrelated keys during the live test.
- **Asynchronous false success:** Treat logs and the INI as authoritative; do not add a fake success callback or banner.
- **Scope creep from historical code:** Extend the shell directly and prohibit importing the old full controller, prompt sprite, or all-row observer manifest.
- **Randomized carrier address:** Scan the full observed heap span and validate the complete carrier structure before accepting an address.
- **Launcher overwrite:** Persist the authoritative UTF-16LE `UserEngine.ini` as well as generated `BmEngine.ini`, preserving each file's encoding and unrelated content.

## Acceptance Criteria

The slice is successful when:

- Batman still reaches the main menu after `Click to Start`.
- `Graphics Options` opens reliably.
- VSync alone displays a real value and is editable.
- Apply emits only the VSync and commit carrier signals.
- `UserEngine.ini` and `BmEngine.ini` persist the selected `UseVsync` value across relaunch.
- unrelated sampled graphics values are unchanged.
- the other graphics rows remain inactive.
- Back works without a prompt or stuck input.
- tests and package validation pass using artifacts generated only from current source and the verified retail base.
