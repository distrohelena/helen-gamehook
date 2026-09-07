# D3D9 shared dispatch and replacement locking

## Status and purpose

The user approved refactoring the shared dispatch/locking boundary after the
installed reset lifecycle change failed live acceptance. This document specifies
that refactor for review before implementation. Work stays on main; existing
uncommitted reset work and the unrelated `batma/` directory remain intact.

The objective is successful replacement upload and windowed reset with surface
hooks already installed, without rejecting valid driver-internal calls or holding
registry locks across reentrant Direct3D calls. Passing a standalone fixture does
not establish Batman live acceptance.

## Evidence

The installed E79D669A build successfully resets the device, creates a replacement
texture, then fails its LockRect with D3DERR_INVALIDCALL. Batman retries the error
returned by ResetDetour. A real-device fixture reproduces this boundary when a
default-pool render-target surface is exposed before reset. The same fixture
passes without that exposure, including with the actual 1024x1024 subtitle DDS.

The saved original texture LockRect resolves to d3d9!CMipMap::LockRect, which calls
surface vtable slot 13 internally. Our shared SurfaceLockRectDetour rejects
unregistered surfaces. The precise live rejection branch remains unconfirmed;
the reproducer will test this causal hypothesis before deployment.

## Alternatives and selected approach

1. **Separate dispatch from tracking and narrow locking (selected).** Preserve
   shared COM vtable interception and public pack interfaces, but make original
   method resolution independent of optional per-object tracking. Move external
   calls outside state locks and explicitly protect operation lifetimes.
2. **Replace interception with full COM proxies.** This changes COM identity,
   QueryInterface behavior and many more interface boundaries. It is outside this
   repair's scope and introduces a much larger compatibility test surface.
3. **Recursive locks or unsynchronized forwarding.** Rejected: recursive locks
   permit calls into partially updated state, while unsynchronized maps introduce
   data races. Neither establishes a sound ownership boundary.

## Dispatch component

Extract original-vtable storage/resolution into a focused internal component,
with one class per file and substantive Doxygen documentation. It owns saved
method addresses, not COM objects or texture matching state.

- Save validated original entries before exposing corresponding detours.
- Resolve methods using the object's actual shared vtable, regardless of whether
  the object has a texture/surface tracking record.
- Copy resolved function pointers before releasing dispatch synchronization;
  never return a mutable vector reference for use after unlocking.
- Do not hold the dispatch lock across COM calls or acquire the tracking mutex
  while holding the dispatch lock. Publishing refreshed device entries must
  preserve this order and never record Helen's detours as originals.
- Preserve originals for as long as patched tables can reach the detours; final
  release of one registered object must not invalidate shared-table dispatch.
- A genuinely unpatched table may use its live method. A patched table lacking a
  valid saved original is an invariant violation, not permission to call back
  into the detour, return a fabricated reference count, or silently report success.

All currently intercepted texture and surface methods must distinguish dispatch
from optional bookkeeping, not only LockRect. Unregistered objects forward their
arguments and the original return value without creating fake tracking records.
Registered objects retain matching, dirty-state and ownership behavior. Missing
optional tracking is normal for driver-internal resources, not an error fallback.

## Replacement operations and ownership

Use prepare / execute / publish phases for both ordinary upload caching and
post-reset restoration:

1. Under the tracking mutex, validate state and copy the operation's description,
   matching identity, device generation and binding information. Mark an explicit
   in-progress operation so reentrant observation cannot start duplicate work.
2. Outside tracking and dispatch locks, load/decode the asset, acquire/release COM
   references, create and upload the replacement, and inspect/rebind device stages.
3. Reacquire the tracking mutex to publish only into the same surviving record,
   generation and operation. Reentrant invalidation must not publish stale state.
   Dispose of uncommitted resources outside locks using RAII.

No record/context pointer borrowed from an unordered map may be retained across
the execute phase without explicit lifetime protection. CPU context ownership is
not a substitute for owning the corresponding COM reference. COM reference
acquisition must start from an already-valid caller/owned reference, not from a
raw pointer that might have been released after unlocking. Reset-owned work must
coordinate with release so it cannot resurrect or use a destroyed source.

Keep reset, resource creation and device operations on the caller's existing
thread. Do not add workers, timers, polling or sleeps. This refactor must not claim
to make a non-multithreaded D3D device safe for arbitrary concurrent application
calls. Shared registry access and legal driver reentrancy still require safety.

The lock audit includes callers of TryCacheReplacementTexture, early-return
UnlockRect paths, stage inspection, replacement cleanup, surface callbacks and
reset/release paths. Rebinding must validate actual current bindings; stale stage
snapshots must not overwrite a different texture bound during reentrancy.

## Reset and failure contract

- Detach Helen-owned default-pool replacements before native Reset and release
  their COM references outside locks.
- Call the original Reset once per incoming request and preserve native failures.
- Preserve pending matching information on failure. After successful native
  reset, refresh driver dispatch and restore replacements using the same unlocked
  operation path as normal uploads.
- Clear stale writable-lock and binding snapshots at the reset boundary.
- Keep restoration failures explicit, including whether native reset succeeded.
  Do not hide them by returning success, disable subtitles, or add retries.
- Error paths must release every acquired temporary reference exactly once and
  leave no permanently in-progress operation.

## Verification and acceptance

Keep the existing failing real-D3D9 fixture as the first red-to-green test, run
against an explicitly selected fresh runtime library. Cover:

- Surface exposure before reset, successful replacement upload and substitution.
- Initial replacement creation after surface hooks are installed, without reset.
- Multiple matching sources and stage inspection without leaked references.
- Registered and unregistered texture/surface forwarding, including release.
- Repeated resize and a deliberately retained backbuffer causing native failure,
  followed by recovery once the application releases it.
- Same-thread reentrancy during external calls, stale operation rejection and
  exact resource cleanup on creation, upload and publication failure. Use narrow
  controllable COM test doubles where hardware cannot trigger these boundaries
  deterministically; assert observable results and ownership, not source text.

The real fixture remains GPU-dependent and console-only. Clean up the temporary
generated-DDS-then-overwrite setup while retaining the actual Batman asset case.
Run the full native suite, no-save session test and package/delta verification
against the fresh candidate. Record library/DLL hashes and test results.

No production installation while Batman is running. Preserve the installed packs,
no-save experiment, session-only INI routing, original user INIs and rollback
backups. After automated verification, request closure if needed and install only
the verified DLL. Live acceptance requires a new Batman process, repeated
windowed resolution tests and subtitle verification by the user.

## Non-goals

No menu changes, new options, subtitle asset redesign, video-skip changes,
persistent INI writes, generic file-routing changes, COM-proxy rewrite or broad
unrelated cleanup. No claim that every historical crash or GPU is fixed. No
independent review session or multi-agent workflow without the required user
approval.
