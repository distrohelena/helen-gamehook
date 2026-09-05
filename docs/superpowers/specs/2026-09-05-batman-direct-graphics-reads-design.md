# Batman direct graphics reads

## Objective and scope

Replace graphics-menu initialization reads with the native ExternalInterface dispatch path proven by the Fullscreen experiment. Helena confirmed immediate Windowed results after reopening the menu and restarting Batman. This is evidence for the boolean callback and those lifecycle cases, not proof of numeric arguments, other values, or resolution transport.

Include Fullscreen, VSync, MSAA, the individual detail toggles, PhysX, Stereo 3D, persisted width/height, both existing resolution catalogs, and desktop dimensions. Detail Level remains derived from its existing leaf settings. Preserve subtitle behavior and the existing menu layout, directional arrows, local drafts, Back, and Apply/rollback behavior. Fullscreen editing returns to its established pre-probe behavior once valid display data is available.

Do not migrate Apply's write/acknowledgement protocol in this change. Consequently, remaining write-side polling must not be described as eliminated. Do not add video skip or unrelated packs.

## Chosen approach

Create a native snapshot and expose its fields through direct primitive getters. Capture the launcher INI once per screen initialization; getters do not reopen files. Preserve the current windowed and fullscreen mode-selection rules through the existing display service rather than creating a parallel resolution implementation.

This approach builds on the verified primitive-return boundary. Returning a whole Scaleform object would reduce call count but require additional unverified object/string ownership machinery. Repeating independent disk reads for each getter would avoid a snapshot but could produce mixed values and redundant work. Neither alternative is selected.

Several synchronous function calls are acceptable: they return results directly and are not polling. No request codes, memory-carrier scans, sleeps, retries, polling workers, or elapsed-time deadlines participate in initialization reads. Actual file/driver work can take time; slower execution must not convert a valid result into Unavailable.

## Native responsibilities

- Extract a read-only snapshot operation from the existing graphics parsing service. Share its decoding and normalization rules; do not promote the experiment's separate Win32 boolean reader into a second production parser.
- Read the launcher-owned `UserEngine.ini` via the already-resolved engine INI anchor. Reading must not persist config, overwrite the editable dispatcher draft, or apply engine settings.
- Store named scalar fields with explicit validity/error information. A failed field must not silently become zero, Off, a cached value from a previous screen, or a generated default. Preserve independently valid fields; file-level read failure invalidates all INI-derived fields.
- Keep resolution-catalog errors separate from ordinary setting errors. Detail Level is usable only when its required leaves are valid.
- Own snapshot memory in the native service. Borrow the movie pointer only as call identity; do not own or retain dereferenceable engine objects after their valid lifetime.
- Initialize the service before publishing the hook. Keep dispatch interception executable-pinned, use loaded-module base plus RVA, verify target bytes, and forward non-owned callback names unchanged.
- Contain exceptions from owned operations at the native boundary, log their cause, and return explicit failure. Preserve stock-call behavior rather than swallowing its failures.

## Direct interface

Use a versioned, allowlisted family of `Helen_Graphics_*` callbacks:

- Begin a read snapshot and receive an opaque numeric generation identifier.
- Get a named scalar's normalized integer state by generation and field identifier.
- Get a catalog's availability/count and each entry's width and height by generation, catalog kind, and index.
- Get persisted and desktop dimensions from that same captured read state.
- End the read transfer explicitly after the frontend has copied the values.

Names and field identifiers are shared constants emitted into the frontend and checked by native tests. Identifiers are API arguments, not numeric messages written into game memory. A getter with an invalid generation, field, catalog kind, argument type, argument count, or index fails immediately. No coercion from strings, booleans, fractional numbers, NaN, or infinity is permitted for integer arguments. Missing values return undefined with an explicit diagnostic path; valid zero remains distinguishable from failure.

Before installing this expanded interface, trace and test the numeric return tag and converted argument representation in the supported executable. The boolean proof does not establish those layouts. Use verified primitive value operations only; never manufacture a managed Scaleform string/object or assume the argument layout equals the return layout.

Keep only bounded transfer state. A new generation invalidates prior transfer handles; End releases the transfer snapshot. Screen initialization performs Begin/getters/End without yielding or enabling editing partway through transfer. Ensure End also runs on frontend validation failure. A missing End must not create an unbounded per-movie map or allow a reused movie address to revive an old generation.

## Resolution and Apply compatibility

The existing Apply protocol sends a mode index, not width/height. Therefore the snapshot's mode order and the native service consulted by Apply must be identical. Ending transfer releases snapshot transport state, **not** the native catalog needed by Apply.

Use the same `BatmanDisplayModeService` instance and mode-specific catalogs for read publication and subsequent Apply revalidation. Do not refresh, resort, or replace a catalog from individual getter calls. Getters expose the captured ordering; the current pair and desktop pair belong to that capture.

Preserve the established single-active-graphics-screen workflow. Catalog publication must not race an outstanding Apply/rollback operation. Check the existing coordinator and service synchronization while implementing; if they cannot enforce this without altering the write transaction contract, stop and report that dependency rather than adding an unsynchronized cache. This design does not introduce support for simultaneous independent graphics editors.

Keep the existing apply-time exact-pair revalidation. A monitor/driver change must fail an unsupported selection explicitly, never reinterpret an old index as a different resolution. Add an integration test proving that each displayed index resolves to its displayed width/height through the unchanged write path.

## Frontend and packaging

Replace BeginInitialization's carrier sequence with the direct snapshot transfer. Populate the same Initial/Draft fields and resolution arrays consumed by the existing rows. Preserve enum ordering and derived Detail Level calculation. Complete initialization once transfer has completed and field validity is known; no timer can determine read success.

Remove old scalar-read and catalog-read emissions, read polling branches, and read deadlines. Retain only observers needed for existing writes, Apply, and rollback; shared read/write observers require a deliberate split or removal of their read mappings, not deletion of write behavior. Tests must prove that no old read request can rescue a broken direct callback.

Update the source builder and validators together; do not rely on repeatedly transforming generated ActionScript as the shipping solution. Build the candidate from checked-in sources and the verified retail base. Installed packs, `batma/`, old archives, and historical generated artifacts are not build inputs.

Use a separate staged candidate and rollback-safe deployment while Batman is closed. Verify exact source/live hashes, delta reconstruction, the intended enabled pack set, unchanged subtitle assets, and cleanup of temporary deployment directories. Preserve the known-working direct Fullscreen installation until candidate checks pass.

## Verification and acceptance

- Test first: valid scalar ranges, per-field failure, file failure, enum normalization, unsupported values, and mixed-validity detail leaves.
- Demonstrate one INI capture per initialization and no extra I/O in getters. Delay the injected capture operation and confirm its eventual valid response is accepted without a timeout.
- Test numeric ABI, strict argument validation, stock forwarding, primitive result ownership, stale generations, explicit release, repeated opens, and distinct movie identities.
- Test windowed custom-size preservation, supported fullscreen modes, exact ordering, desktop values, catalog failure isolation, and display changes at Apply revalidation.
- Exercise emitted and decompiled ActionScript. Verify direct-only reads, unchanged drafts/arrows/Back, and existing write/acknowledgement/rollback sequences.
- Parse the candidate with the native pack loader and reconstruct its frontend delta byte-for-byte. Run relevant native, shell, retail-base, layout, and deployment checks.
- Live: verify every value against the launcher INI; reopen and relaunch; edit and discard; apply a controlled change and confirm acknowledgement/persistence still work. Automated results alone do not establish in-game correctness.

This is a read migration. Moving Apply to direct callbacks and removing all remaining write polling are separate follow-up work.
