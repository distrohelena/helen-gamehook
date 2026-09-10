# Batman session-only live graphics options

## Scope and approved direction

Extend the verified session-INI reload experiment to the existing graphics menu.
The user explicitly wants session-only changes: Helen may edit the active
redirected copy, but neither Helen nor Batman may persist these graphics changes
to the protected original INI. Restarting the game discards this session's edits.
Keep the existing startup-bound menu communication and proven D3D9 display path.
No polling, timer-driven discovery, automated game launches or process termination.

The user approved this design for implementation. Baseline: main `a820697`.

Implementation evidence correction: the original Bloom probe used AmbientOcclusion's
live offset and mirror, so its old PASS does not establish live Bloom success.
The DynamicShadows live/visual test remains valid. A corrected Bloom binding and
its live acceptance are prerequisites to expanding that part of the native path.
That prerequisite subsequently passed: corrected live Bloom and cache changed
0 -> 1, unrelated payload remained unchanged, and all 31 original INIs matched.

## Approach

Use a session apply coordinator with separate responsibilities:

1. Validate the complete requested delta and each setting's live capability.
2. Stage supported INI changes in the authoritative session overlay.
3. Explicitly reload Batman's cached INI and apply supported engine settings.
4. Perform a display refresh only if required by the requested delta.
5. Read back actual engine/display state and publish a verified session result.

This is preferred to independent per-setting memory patches because the reload
path has reached live and renderer values for DynamicShadows. Bloom's cache
reload succeeded, but its earlier live observation was mislabeled (see correction).
It is preferred to blindly reloading all keys because PhysX, stereo and device
settings have different initialization and ownership requirements. Existing
working bindings remain the starting point, not a replacement discovery scheme.

## Setting coverage

| Menu field | Existing config representation | Apply responsibility |
| --- | --- | --- |
| Resolution / Fullscreen | SystemSettings.ResX, ResY, Fullscreen | Existing validated viewport resize and mode validation |
| VSync | SystemSettings.UseVsync | Existing D3D9 presentation override and same-size refresh |
| Anti-aliasing | SystemSettings.MaxMultisamples | Validate normalized sample mapping and device support; trace resource/reset requirements |
| Bloom / Dynamic Shadows | SystemSettings.Bloom, DynamicShadows | Verified reload and stock Apply(save=false), then renderer readback |
| Motion Blur / Distortion / Fog Volumes | Corresponding SystemSettings Boolean keys | Trace exact live consumers and refresh requirements before enabling |
| Spherical Harmonic Lighting | SystemSettings.DisableSphericalHarmonicLights | Preserve inverse UI encoding; trace lighting refresh requirements |
| Ambient Occlusion | SystemSettings.AmbientOcclusion | Trace live consumer and render-resource refresh requirements |
| PhysX | Engine.Engine.PhysXLevel, integer 0–2 | Separate initialization/capability investigation; no assumed live support |
| Stereo | SystemSettings.Stereo | Separate renderer/device capability investigation; no assumed live support |

Desktop dimensions and CanApply are observations, not editable INI settings.
All existing editable menu fields are in scope; additional options are not.

## Session state and results

Keep distinct requested draft, verified applied session state, and persistent
original configuration. Opening/reopening the menu must show the applied session
values, not stale original INI values. Successful session application must update
the menu baseline and clear its dirty state without claiming a disk save.

Introduce an explicit session-applied outcome rather than reusing Committed,
whose documented meaning is verified persistence. Update the direct interface
and menu consumers consistently. Preserve the ordinary trusted persistence path
for callers outside this session-only mode; it must not be called by this mode.

Replace the process-wide one-attempt restriction with an owned operation guard
and explicit health state. A verified successful Apply allows another Apply.
Reentrant/overlapping calls fail before mutation. No-op Apply does not reset the
device or rewrite the overlay. Reset boundaries retain existing lifetime checks.

## Validation, ordering and failures

Validate the whole delta before mutation, including executable bindings,
session route identity, game-thread/idle conditions, value encodings and device
capabilities. Unsupported PhysX/stereo transitions reject the entire delta
before staging; unrelated settings can be applied after removing that change.
Report the specific unsupported field and reason, rather than general success.

Build complete target overlay bytes before publishing them. Reuse the existing
INI parser/encoding contracts without invoking its original-file writer. Verify
overlay contents and cached values independently. Preserve the exact logical
cache-key to physical routed-INI validation proven by the Bloom experiment.

Do not assume multi-key reload plus GPU reset is atomic. If failure occurs after
mutation, record the completed stages and observed state. Do not publish the
requested draft as applied, silently restore engine fields, or report NotApplied
as though nothing happened. Lock further Apply when consistent live state cannot
be established; reopening the menu must not clear that lock. Original-file
preservation remains mandatory even on failure.

Stock settings Apply can affect unrelated fields; preserve complete known-live
input data and verify expected changes, including the existing directional-
lightmap guard. Determine the exact ordering and whether stock Apply already
refreshes resources before adding a device reset. Avoid redundant resets.

## PhysX and stereo investigation boundaries

Trace where config values are consumed and where their runtime owners are
created. Determine whether a supported reinitialization path exists on the game
thread, whether scene objects require recreation, and whether enable/disable and
quality transitions differ. A cache value is not proof of runtime application.

If safe in-session reinitialization cannot be established, expose an explicit
unsupported-live/restart-required capability, not a fake working control. Do not
promise a restart will apply a session-only draft: this mode discards its overlay
on exit. Persisting restart-required choices or carrying an overlay across a game
restart requires a separate user-approved persistence policy. Do not unload
physics libraries, destroy an active world, or auto-reload a level as a workaround.
Report evidence and request direction if such expansion is necessary.

## Verification and delivery

- Test value mapping, inverse lighting encoding, sample-count mapping, mixed
  deltas, unsupported-capability rejection and no-op behavior.
- Exercise real ANSI/UTF-16 session files; verify only requested keys change and
  original bytes remain unchanged on success and injected failures.
- Test repeat Apply, reverse toggles, reopen, stale transactions, reentrancy,
  baseline publication and partial-failure locking through the real session API.
- Verify display-only and mixed display/effect requests preserve existing VSync,
  supported resolution and fullscreen behavior; do not infer success from config.
- Record per-setting engine/render or device evidence. Hardware validation is
  still required for new live transitions; CPU fixtures cannot establish it.
- Build into a fresh output root, test the matching runtime/proxy artifacts, then
  install only with Batman/launcher closed, backups and hash/INI checks.
- Keep current known-good binaries available. No commits or package promotion
  are implied by implementation approval; commit when the user requests it.

Success means repeatable session Apply with truthful outcomes and validated
capabilities, not merely every menu value becoming editable. Any remaining
unsupported field must be explicitly reported as incomplete live coverage.
