# Batman Graphics Fullscreen and Supported Resolution Design

## Summary

Activate the final two placeholder rows in Batman: Arkham Asylum GOTY's in-game Graphics Options screen. Fullscreen becomes a normal Windowed/Fullscreen setting. Resolution is populated at runtime from the display modes Windows reports for the monitor containing Batman's process-owned game window.

The shell keeps resolution edits local until Apply Changes. Native code owns display-mode enumeration and validates the selected width/height pair as one unit before updating the graphics draft. Existing transactional persistence then writes Fullscreen, ResX, and ResY to both launcher-owned `UserEngine.ini` and generated `BmEngine.ini`.

## Goals

- Make Fullscreen and Resolution display their current persisted values when Graphics Options opens.
- Make Fullscreen selectable between Windowed and Fullscreen.
- Make Resolution selectable only among width/height pairs supported by the display Batman is using.
- Deduplicate modes that differ only by refresh rate or pixel format.
- Preserve the working shell, focus, arrow alignment, initialization, Apply, rollback, and Back behavior.
- Keep all user edits local until Apply Changes begins.
- Update resolution width and height atomically so the dispatcher and INI transaction never observe a half-selected mode.
- Re-enumerate and validate display support at Apply time so a monitor change cannot persist a mode that is no longer available.
- Preserve reproducible package generation from checked-in sources and the verified retail base.

## Non-goals

- Do not expose refresh rate, color depth, monitor selection, borderless-windowed mode, or an arbitrary custom width/height editor.
- Do not apply a display mode immediately when an arrow is pressed.
- Do not silently choose a closest resolution when the persisted or selected pair is unsupported.
- Do not generate a machine-specific package during deployment.
- Do not revive the legacy `GraphicsOptionsScriptTemplates` controller or its `Helen_*` callback route.
- Do not use `batma/`, the installed game, `F:\helenhook.7z`, or historical generated artifacts as build inputs.

## Considered Approaches

### Runtime-owned supported-mode catalog

Native code enumerates the monitor in use whenever the Graphics Options screen requests its catalog. The shell receives the catalog through the existing integer carrier, builds labels locally, and stages one selected catalog index.

This is the chosen approach. It remains portable across monitors, GPUs, drivers, and reboots while retaining a reproducible package.

### Deployment-time catalog generation

The deployment script could enumerate the current machine and bake those modes into the generated frontend asset. This is simpler at runtime, but it makes the checked-in package differ by machine and become stale after display changes.

### Fixed common modes filtered at runtime

The shell could contain a table of familiar resolutions and ask native code which ones are supported. This avoids arbitrary scalar responses, but it can omit valid ultrawide, high-DPI, or unusual display modes without providing a meaningful safety advantage.

## User-visible Behavior

### Fullscreen

The first row exposes:

- Windowed
- Fullscreen

Left and right stop at the corresponding ends. Activating the row advances and wraps, matching the existing active rows. Changing the value enables Apply Changes but does not touch either INI before Apply.

### Resolution

The second row displays labels in `WIDTH x HEIGHT` form, for example `1920 x 1080`.

The list contains every unique positive width/height pair returned by Windows for the display containing Batman's process-owned top-level game window. Modes that differ only by refresh rate, orientation metadata, or pixel format collapse into one entry. Entries are sorted by pixel count, then width, then height for deterministic navigation.

Left and right stop at the list boundaries. Activating the row advances and wraps. Changing the selected entry enables Apply Changes but remains entirely inside the shell until Apply begins.

The runtime supports at most 98 unique width/height pairs. More than 98 is an explicit catalog failure rather than silent truncation. This bound keeps the request protocol finite and well below the practical unique-mode count of one Windows display.

## Display Selection and Enumeration

A dedicated `BatmanDisplayModeService` owns Windows display discovery and catalog normalization.

The service locates Batman's process-owned top-level game window and resolves the monitor containing that window. It uses the monitor device name with `EnumDisplaySettingsExW`; it does not use the foreground window or assume that the primary display owns the game.

Enumeration happens when the graphics shell requests the catalog, when Batman's game window is available. The service rejects these states explicitly:

- no unambiguous process-owned game window
- no monitor for that window
- display-mode enumeration failure
- no valid positive width/height pairs
- more than 98 unique pairs
- persisted ResX/ResY absent from the supported catalog

No closest-mode or primary-display fallback is permitted.

## Shell State Model

The production shell remains `GraphicsOptionsShellScriptTemplates`. The legacy full controller remains reference-only and outside the production provenance graph.

Initialization becomes:

1. Load the existing fourteen INI-backed scalar values through the established graphics initialization flow, including Fullscreen, ResX, and ResY.
2. Request the supported resolution catalog from native code.
3. Build local resolution labels from the returned width and height scalars.
4. Find the exact persisted ResX/ResY pair in that catalog.
5. Capture Fullscreen and the resolution index in the shell baseline and draft state.
6. Enable interaction only after every required value and catalog entry has been validated.

Fullscreen joins the normal transmitted-setting collection. Resolution is one special shell setting with a local initial index and draft index. Its catalog is immutable for that screen instance. Reopening the screen requests a fresh catalog.

The existing uniform arrow widening applies to both newly active rows exactly once on load. The Apply row remains fixed.

## Carrier Protocol

The existing `FE_SetControlType` / `FE_GetControlType` carrier remains the only ActionScript/native boundary.

### Fullscreen protocol

Fullscreen uses the same finite read, response, write, acknowledgement, and failure pattern as VSync. Its observer maps only normalized values 0 and 1 and joins the shared `batmanFrontendControlType` address group.

### Resolution catalog protocol

Codes `4700` through `4899` remain reserved for catalog requests:

- one request for catalog count
- one request for persisted width
- one request for persisted height
- two requests per catalog entry, width followed by height
- one explicit failure response

The maximum of 98 entries fits this allocation exactly.

The generic observer service gains a bounded dynamic scalar-response contract. A declared request invokes a supplied query callback with the observer identity and request value. The callback returns one checked positive integer. The service writes an unambiguous encoded scalar response; the shell decodes and validates it against the pending request.

Dynamic responses are never eligible to discover an address. Initial discovery still requires a declared protocol request or sentinel plus all existing structural checks. A cached shared carrier may retain a response only while that exact dynamic request is pending and the structure remains valid. Other members of the address group must recognize that group-owned pending response without emitting updates or clearing the shared cache.

Arbitrary packed width/height values are not used. Width and height travel as separate scalar responses, are bounds-checked independently, and become a pair only after both responses for an entry succeed.

### Resolution Apply protocol

Each possible local mode index has one finite write request. During Apply, the shell sends only the selected index. Native code resolves that index against the catalog snapshot captured for this screen, re-enumerates the same monitor, confirms that the selected pair is still supported, and then updates `resolutionWidth` and `resolutionHeight` as one atomic dispatcher operation.

The operation returns one acknowledgement only after both values changed. Any lookup, monitor, validation, or atomic-update failure returns the resolution failure response and enters the existing rollback flow.

All new static request, response, acknowledgement, and failure codes are globally unique and become members of the complete shared carrier protocol union. Existing codes through the quality settings and `4960` through `4991` rollback/apply codes remain unchanged.

## Runtime Boundaries

`BatmanDisplayModeService` has three responsibilities:

- locate the game display and build the sorted unique catalog
- answer catalog scalar queries for the current screen session
- resolve and revalidate one selected mode during Apply

`BatmanGraphicsConfigService` remains the sole owner of graphics draft and INI persistence. It gains one explicit operation that accepts a validated resolution pair and updates both registered dispatcher keys atomically. Fullscreen continues through the existing scalar dispatcher path.

The generic dispatcher must either provide an atomic two-key mutation or the Batman graphics service must validate both registered keys before changing either and restore the exact old pair if the second mutation cannot complete. A partially updated pair is never reported as success.

The observer service remains transport infrastructure. It does not enumerate displays or interpret resolution semantics.

## Apply, Rollback, and Back

- Arrow and row actions change shell-local Fullscreen and Resolution drafts only.
- Apply queues Fullscreen in screen order when dirty.
- Apply queues Resolution as one selected-index operation when dirty.
- The normal commit signal runs only after every dirty setting, including the atomic resolution pair, acknowledges.
- The existing graphics transaction persists the complete draft to both INIs and reloads launcher-owned state after success.
- Apply success copies the reloaded Fullscreen and exact resolution pair into the shell baseline and disables Apply Changes.
- A setting failure, catalog change, timeout, or commit failure enters the existing rollback protocol.
- Rollback reloads Fullscreen, ResX, and ResY from launcher-owned `UserEngine.ini` together with every other graphics value.
- Back before Apply destroys only shell-local state and performs no native draft or INI write.
- Rollback failure retains the existing locked `Rollback Failed` behavior.

## Error Handling

Initialization does not display a fabricated resolution. Catalog or current-pair failure leaves the graphics screen in its explicit initialization-failed state and logs the exact stage, monitor identity when available, request, and reason.

The shell rejects:

- nonpositive counts or dimensions
- counts above 98
- duplicate pairs after native normalization
- incomplete width/height responses
- responses that do not belong to the pending request
- a persisted pair not present in the catalog
- a selected index outside the captured catalog

Native code rejects stale monitor identity, changed catalog identity, unsupported selected pairs, missing dispatcher keys, and any non-atomic pair update. None of these conditions selects a default or reports a false acknowledgement.

## Implementation Boundaries

Expected production touchpoints are:

- a new display-mode value type and `BatmanDisplayModeService`, one class per file
- `BatmanGraphicsConfigService` for atomic resolution draft assignment and Apply-time validation integration
- the memory observer definition, parser, service, and coordinator callback wiring for bounded dynamic scalar responses
- `GraphicsOptionsShellScriptTemplates.cs` for the two active rows, catalog initialization, local stepping, and resolution Apply operation
- `Rebuild-BatmanGraphicsOptionsExperiment.ps1` for the Fullscreen, catalog, and resolution-write protocol definitions and complete address-group union
- checked-in pack manifests plus the regenerated delta produced by the validated rebuild path
- project files only as required to compile the new source files

The implementation must not introduce runtime patch-up helpers, hidden defaults, an installed-file input, or a second graphics authority.

## Test Strategy

Implementation follows strict red-green-refactor TDD.

### Display-mode service tests

- chooses the monitor containing a process-owned game window
- rejects missing or ambiguous game-window ownership
- deduplicates refresh-rate and pixel-format variants by width/height
- sorts by pixel count, width, and height
- rejects zero dimensions, empty catalogs, and more than 98 unique pairs
- retains unusual supported aspect ratios
- resolves an exact persisted pair and rejects an unsupported pair
- re-enumerates and rejects a selected mode removed before Apply

Windows API access is placed behind an injected display-mode source so tests use deterministic real service logic without controlling the desktop or opening GUI windows.

### Observer and coordinator tests

- parses the bounded dynamic scalar-response declaration
- invokes the query callback only for a declared request
- rejects missing, nonpositive, malformed, or out-of-bounds callback results
- correlates one response with its exact pending request
- preserves the shared carrier cache while a valid dynamic response is pending
- does not let another grouped observer consume that response
- never uses a dynamic response for initial address discovery
- writes the explicit failure response when the query callback fails
- preserves one broad unresolved-group scan per polling pass

### Graphics service tests

- loads Fullscreen, ResX, and ResY from launcher-owned `UserEngine.ini`
- updates both resolution dispatcher keys atomically
- leaves the old pair unchanged on missing-key or second-write failure
- validates the selected pair against a fresh supported-mode enumeration
- persists Fullscreen, ResX, and ResY to both INIs
- restores exact original bytes at every dual-INI publication failure boundary

### Shell tests

- Fullscreen and Resolution replace the two `Not active` rows
- all thirteen persisted option rows plus derived Detail Level initialize in screen order
- catalog responses build exact `WIDTH x HEIGHT` labels
- persisted ResX/ResY selects the matching local index
- Fullscreen and Resolution edits are local before Apply
- left/right boundaries and activation wrapping match the established row behavior
- dirty resolution queues exactly one selected-index operation
- Back emits no resolution or Fullscreen write
- exact acknowledgement, timeout, failure, commit, and rollback behavior remains intact
- all fourteen editable rows receive the uniform arrow widening once without drift

### Package and provenance tests

- pack schema and generated hooks contain the exact new config keys and observers
- the shared protocol union is complete and collision-free
- shell, package, retail, layout, and deployment validators agree that all fourteen option rows are active
- the pack remains exactly seven files with no loose generated asset or historical input
- delta application recreates the generated frontend target byte-for-byte
- retail XML, scripts, images, shapes, and sprites remain unchanged outside the explicit allowlists
- production provenance rejects the legacy controller, installed game inputs, `batma/`, and `F:\helenhook.7z`

## Deployment and Live Verification

After all automated validators pass, deploy atomically while Batman is closed and verify exact source/live hashes plus the absence of staging and recovery directories.

Live checkpoints are:

1. Open Graphics Options and confirm Fullscreen and Resolution load without Undefined or Unavailable.
2. Confirm Resolution contains only modes supported by the display Batman is using and has no refresh-rate duplicates.
3. Change Fullscreen and Resolution, press Back, reopen, and confirm neither draft persisted.
4. Change Fullscreen and Resolution, Apply, and confirm Apply disables after acknowledgement.
5. Exit and relaunch; confirm the applied values reload and both INIs contain the same Fullscreen, ResX, and ResY values.
6. Review the runtime log for the exact catalog, selected monitor, observer acknowledgements, and absence of rollback or virtualization failures.

## Acceptance Criteria

- Fullscreen and Resolution are active and selectable.
- Resolution exposes every unique width/height pair reported for Batman's current display and no unsupported pair.
- Monitor or driver changes are reflected the next time the screen opens without rebuilding the package.
- Fullscreen and Resolution remain shell-local until Apply.
- Resolution width and height update atomically and persist transactionally to both INIs.
- Back remains immediate and side-effect-free.
- Existing settings, Detail Level, Apply, rollback, focus, and arrow alignment retain their proven behavior.
- Native, shell, package, retail, layout, and deployment validators pass.
- The committed seven-file package is reproducible only from approved checked-in sources and the verified retail input.
