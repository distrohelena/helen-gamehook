# Batman Graphics Options Independent Settings Design

## Goal

Extend the proven Batman Arkham Asylum `Graphics Options` shell from its working VSync slice to the first tested settings group: VSync, MSAA, PhysX, and NVIDIA Stereo 3D. Each setting must load its live persisted value, edit locally, participate in one acknowledged Apply transaction, persist through Batman's launcher-owned and generated INI files, and survive a relaunch.

## Confirmed Baseline

The current Steam GOTY package has passed live verification for:

- the title-to-main-menu transition;
- opening `Graphics Options` from the Options menu;
- live VSync initialization through the stock `FE_ControlType` carrier;
- local VSync editing and Apply enablement;
- acknowledged observation of the VSync and commit carrier values in HelenHook logs;
- UTF-16LE `UserEngine.ini` and generated `BmEngine.ini` persistence;
- relaunch initialization from the persisted VSync value;
- an observer scan range that covers independently observed randomized carrier addresses near `0x10DF`, `0x1209`, and `0x2B24`.

The old full graphics controller is reference material only. It depends on unreliable custom `Helen_*` Scaleform callbacks, baked fallback values, prompt behavior that destabilized the menu, and incomplete fullscreen/resolution interaction. It must not be restored wholesale.

## Scope

This group activates four rows:

| Row | Display values | Config values |
|---|---|---|
| VSync | Off, On | `0`, `1` |
| MSAA | Off, 2x, 4x, 8x, 16x | `0`, `1`, `2`, `3`, `5` |
| PhysX | Off, Normal, High | `0`, `1`, `2` |
| NVIDIA Stereo 3D | Off, On | `0`, `1` |

The MSAA display deliberately omits 8xQ and 16xQ. Batman's current `MaxMultisamples` representation cannot distinguish those labels from 8x and 16x, so exposing them would create states that cannot round-trip honestly.

The existing Apply row becomes the transaction controller for all four settings. The remaining rows continue to display `Not active` and keep no-op input handlers.

## Non-Goals

- Activating fullscreen or resolution
- Enumerating display modes
- Activating Detail Level or its seven coupled quality toggles
- Applying renderer changes live without restarting Batman
- Restoring the historical graphics exit prompt
- Adding custom `Helen_GetInt`, `Helen_SetInt`, `Helen_RunCommand`, or direct apply callbacks to Scaleform
- Detecting whether the current GPU or driver can use PhysX High or Stereo 3D
- Exposing MSAA states that cannot round-trip through the INI format

## Considered Approaches

### Extend the VSync controller separately for every row

Add dedicated fields, timers, and branching logic for MSAA, PhysX, and Stereo 3D beside the current VSync implementation.

This would minimize the first edit but duplicate synchronization and Apply logic. Later settings groups would multiply that duplication and make carrier races more likely.

### Generalize the proven shell around declarative setting definitions

Keep the stable shell and row clips, but replace the VSync-specific state with a small controller that consumes explicit setting definitions. Each definition declares its row, display values, config values, and carrier codes. A serialized queue performs reads and writes one message at a time.

This adds a focused protocol foundation while retaining the known-good screen lifecycle and stock frontend seam.

### Restore the historical full controller and replace its bridge calls

Reuse its row tables, detail-preset logic, and Apply flow, then replace the failed `Helen_*` calls with carrier operations.

That controller also includes broken prompt state, baked fallback behavior, verbose runtime logging, and incomplete display-mode rows. Separating those concerns after restoration would be riskier than extending the stable shell.

## Chosen Approach

Use the declarative-controller approach. Preserve the stable graphics shell and selective FFDec import path. Reuse only verified value labels and config normalization knowledge from the historical controller.

## Carrier Protocol

Read responses and write requests use distinct raw values. This prevents an observer from consuming its own initialization response as a user edit.

| Setting | Read request | Read response | Write request | Success acknowledgement | Failure response |
|---|---:|---:|---:|---:|---:|
| VSync | `4200` | `4210`–`4211` | `4220`–`4221` | `4230`–`4231` | `4299` |
| MSAA | `4300` | `4310`–`4314` | `4320`–`4324` | `4330`–`4334` | `4399` |
| PhysX | `4400` | `4410`–`4412` | `4420`–`4422` | `4430`–`4432` | `4499` |
| Stereo 3D | `4500` | `4510`–`4511` | `4520`–`4521` | `4530`–`4531` | `4599` |

MSAA raw code suffixes are display indices. Its observer mappings translate write indices `0` through `4` into config values `0`, `1`, `2`, `3`, and `5`; response mappings perform the inverse translation.

Command signals remain alternating values so every transaction is observable:

| Command | Request | Success acknowledgement | Failure response |
|---|---:|---:|---:|
| Restore native draft from disk | `4970`–`4971` | `4960`–`4961` | `4969` |
| Persist graphics draft | `4990`–`4991` | `4980`–`4981` | `4989` |

Every graphics observer declares the complete protocol raw-value union in `addressMatchValues`. Read responses are absent from update mappings, write requests are absent from read-response mappings, and acknowledgement values are absent from both. This keeps directionality explicit.

## HelenRuntime Changes

### Shared carrier address

Memory observers may declare an address-group identifier. All Group 1 observers use `batmanFrontendControlType`. Once one observer resolves and structurally validates the carrier, compatible observers in that group reuse the address and validate it against their own definition before polling.

If the cached address becomes invalid, the group cache is cleared and one observer performs the broad `0x10000000..0x30000000` rescan. This avoids one full randomized-heap scan per setting while preserving the existing signature checks.

### Transaction acknowledgements

Transactional observers declare:

- input-to-success acknowledgement mappings;
- one explicit failure response;
- the existing config target and optional command.

The observer update callback returns success or failure. HelenRuntime writes a success acknowledgement only after the config update and optional command both succeed. It writes the declared failure response when either operation fails. A transactional observer rearms after writing its acknowledgement, allowing a retry of the same logical value.

Initialization requests continue to use the existing config-value callback and response mappings. Because read-response codes do not appear in update mappings, HelenRuntime does not treat its own response as a draft mutation.

### Commands and config

The pack retains `loadBatmanGraphicsDraftIntoConfig` and `applyBatmanGraphicsDraft`. It adds a rollback signal key and a rollback observer that invokes `loadBatmanGraphicsDraftIntoConfig` without persisting.

The apply observer acknowledges success only after `applyBatmanGraphicsDraft` writes both INIs and reloads the normalized draft. Existing failure behavior remains visible through logs and the failure carrier response.

`BatmanGraphicsConfigService` already supports the Group 1 values. Its complete-draft validation remains unchanged so missing or invalid settings fail instead of being replaced with defaults.

## ActionScript Controller

Each active setting definition contains:

- stable row name and label;
- display-value array;
- config-value array;
- read request and response values;
- write request and acknowledgement values;
- failure response value.

The controller owns distinct initial and draft display indices. It never sends a write while the user is merely editing.

### Initialization

On screen entry:

1. block settings interaction;
2. show `Loading...` for the four active rows;
3. request one setting at a time;
4. poll `FE_GetControlType` until the expected response arrives;
5. store the response as both initial and draft state;
6. advance to the next setting;
7. unblock input after all four values resolve.

The complete initialization sequence has one ten-second deadline. The first request may wait for carrier discovery; subsequent requests use the shared cached address and normally complete within one observer poll.

Apply starts disabled because every draft matches its initial value.

### Editing

Left, right, and the normal row action update only the local display index. Values wrap for the normal action while directional inputs respect the list boundaries. Accepted changes play Batman's existing forward or back frontend sound and refresh the affected row and Apply state.

The controller determines dirty state by comparing every active draft index with its initial index.

### Apply transaction

When Apply is activated:

1. reject activation when no setting is dirty or a transaction is already running;
2. block input and show `Applying...` on the Apply row;
3. enqueue only dirty settings in stable row order;
4. send one write request and wait for its exact success acknowledgement;
5. advance only after acknowledgement;
6. after all writes succeed, send the alternating persist command;
7. wait for the matching persist acknowledgement;
8. copy all draft indices into the initial state;
9. clear the status, refresh rows, and re-enable input.

Each write or command step has a two-second timeout. Since the carrier address is already resolved, this is forty observer intervals and is not coupled to a fixed dwell delay.

### Failure and rollback

If a write returns its failure response or times out, the controller stops the Apply queue and sends the alternating restore-from-disk command. A persist failure follows the same rollback path.

The local UI draft remains unchanged and dirty so the user can retry. A successful rollback displays `Apply Failed` and restores input. A rollback failure displays `Rollback Failed`; Apply remains disabled for that screen because native draft state is no longer known. Back remains available so relaunch can restore a known state.

If initialization fails, the four active rows display `Unavailable`, Apply remains disabled, input is restored for navigation, and Back continues to return normally. No baked value or default is substituted.

## Presentation

The existing layout, row order, labels, focus graph, title, and Back behavior remain unchanged. Active rows use their real values and normal arrow visibility. Inactive rows remain visibly `Not active` with no-op interaction handlers.

The Apply row has these states:

- blank and dim when the draft is clean;
- enabled when at least one active setting is dirty;
- `Applying...` and disabled during a transaction;
- blank and dim after success;
- `Apply Failed` and enabled after successful rollback;
- `Rollback Failed` and disabled when native state cannot be recovered.

No success modal, restart prompt, or unsaved-changes prompt is introduced in this group.

## Build and Package Generation

The rebuild continues to start from the verified retail `Frontend.umap`, generate a fresh shell GFX, apply the selective script import, validate retail compression, generate a new delta, and publish atomically.

The package manifest declares only the active Group 1 setting observers, the apply observer, and the rollback observer. No old full-controller bindings, prompt export, or generated historical file may enter the package.

Build-time INI inputs may still be validated by the asset builder, but changing Group 1 values in that input must not change the generated GFX or package hash. All visible values come from the live initialization protocol.

## Test Strategy

### HelenRuntime tests

- Parse valid address-group and acknowledgement declarations.
- Reject incomplete acknowledgement contracts and duplicate/conflicting protocol values.
- Reuse one structurally validated address across a carrier group.
- Invalidate and reacquire a stale grouped address.
- Produce exact read responses for all Group 1 config values.
- Acknowledge successful writes and command executions.
- Return failure responses for config or command failures.
- Rearm transactional observers so the same value can be retried.
- Preserve behavior for non-transactional subtitle observers.

### ActionScript and builder tests

- Emit the exact protocol table and sequential initialization state machine.
- Keep edits local until Apply.
- Map the five MSAA display indices to config values accurately.
- Send only dirty setting writes in stable order.
- Require an acknowledgement before advancing.
- Commit only after every setting acknowledgement.
- Update the initial state only after commit success.
- Exercise write failure, timeout, rollback success, and rollback failure states.
- Keep inactive rows callback-free.
- Prohibit every custom `Helen_*` call and historical prompt path.

### Package tests

- Require exact observers, mappings, command bindings, scan geometry, and address group.
- Rebuild identical GFX and package bytes from differing build-time Group 1 INI values.
- Reopen the generated GFX and patched retail package successfully.
- Preserve the retail startup sprites and title-to-main-menu transition dependencies.
- Verify deployment remains atomic and does not modify the subtitle pack.

## Live Verification

For each Group 1 setting independently:

1. record `UserEngine.ini` and `BmEngine.ini` values;
2. launch Batman and confirm the row initializes to the corresponding value;
3. change the row and confirm Apply enables without any INI change;
4. press Apply and confirm the row is blocked while acknowledgements are pending;
5. confirm Apply becomes clean only after a logged successful commit acknowledgement;
6. close Batman and compare both INIs, including sampled unrelated settings;
7. relaunch and confirm the selected value initializes correctly.

Also verify multiple simultaneous edits, retry after an injected native failure, Back with an unapplied local edit, repeated application of the same logical values, all inactive rows, and the complete title-to-main-menu-to-options navigation path.

## Acceptance Criteria

- VSync, MSAA, PhysX, and Stereo 3D display live persisted values rather than baked values or placeholders.
- The five honest MSAA choices round-trip without changing identity.
- Editing remains local until Apply.
- Apply serializes only dirty writes and advances only on exact acknowledgements.
- A successful commit updates both INIs and clears dirty state.
- A failed transaction remains visibly failed, restores native draft state when possible, and never claims success.
- Repeating the same value in a later transaction is observable and acknowledged.
- Other graphics rows remain inactive.
- Back and the title/main-menu transitions remain stable.
- Generated artifacts originate only from current source and the verified retail base.
- Native, ActionScript, package, retail round-trip, deployment, and live verification all pass before the implementation is committed.
