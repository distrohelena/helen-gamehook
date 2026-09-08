# D3D9 replacement texture reset lifecycle

## Scope and implementation

User-approved follow-up to the captured Batman Reset failure documented in `2026-09-07-d3d9-reset-diagnostics.md`. Changes remain on main and are not committed yet. No menu, subtitle image, file-routing policy, INI writer, or resolution probe behavior changed.

- Detach cached replacement ownership under the registry mutex, then release through original COM methods outside the mutex before calling Reset.
- Preserve matching information and pending restoration across failed reset attempts. Recreate replacements for surviving sources only after successful device reset. Restoration failure is logged and returned, not silently treated as success.
- Reset can replace driver dispatch methods. Re-establish device hooks on return from Reset, adopting changed driver originals while retaining original addresses for detours still in place. No extra D3D calls are made on failed-reset devices.
- Shared texture vtables also intercept untracked replacement objects. Release now forwards those objects to the real COM implementation instead of discarding their reference releases.
- Close references acquired by the cache's GetTexture stage inspection, including nonmatching stages, using scoped ownership and original Release methods.
- Clear stale binding/lock snapshots after successful reset while retaining the fingerprint required to restore a matched replacement.

## Real-device regression

`tests/Invoke-D3d9ReplacementResetTests.ps1` compiles the standalone `tests/HelenRuntime.Tests/D3d9ReplacementResetTests.cpp` fixture against an explicitly supplied runtime library. It installs the actual production import/vtable hooks and uses a real HAL Direct3D9 device with a hidden window, a generated 8x8 replacement DDS, and surviving 4x4 managed source textures. It does not launch or modify Batman. It is separate from the portable native suite because a hardware/desktop session is required. All failures are console-only.

Observed sequence:

1. Initial fixture build needed the SDK winrt include directory for WRL. The sandbox then returned D3DERR_NOTAVAILABLE on device creation. Neither was counted as regression RED.
2. Running outside the sandbox against the pre-fix diagnostic library produced the expected behavioral RED: `Reset must release the hook-owned default-pool replacement hr=0x8876086c`.
3. Releasing cached ownership alone still failed. Observer Release calls both returned zero: the shared-vtable hook was swallowing releases for untracked objects. Correcting dispatch allowed reset to succeed.
4. The post-reset substitution assertion then failed. Direct inspection showed SetTexture's address changing from the fixture hook to the driver during Reset. Restoring device interception made replacement substitution and repeated reset/recovery pass.
5. Added a second source upload while the first replacement was bound. This produced another behavioral RED with D3DERR_INVALIDCALL. Scoped release of the cache's GetTexture references made that case pass.

Passing fixture covers multiple replacements, successful resize, replacement substitution/dimensions after reset, an application-retained backbuffer forcing the original D3DERR_INVALIDCALL, recovery after releasing that resource, and subsequent repeated reset. No test bypasses the production ResetDetour or substitutes a fake driver's success result.

Full Release Win32 native suite also passed, including the three Batman persistence fault children, generated protocol check, and file-routing hook child. `git diff --check` passed (line-ending normalization notices only).

## Live acceptance

A fresh no-save build is prepared separately so the current test still bypasses Helen's INI save. Batman live reproduction remains required: repeat the failing windowed resolution change and verify subtitles afterward. Passing the fixture is not a claim that every historical crash or GPU configuration is fixed.

## Installed artifact and final verification

Fresh build root: `output/replacement-reset-fix-nosave-20260907`. Matching PDB is in its native directory. Built from the current uncommitted working changes on main, based on d7189e5.

| Artifact | SHA-256 |
| --- | --- |
| native/HelenGameHook.dll | E79D669A5F0A96B0C75B934CD24A4BAE7801F4FA13DF1C2751D7DA9C69B065D7 |
| native/HelenRuntime.lib | 2D25D702EEA7F9AEC673CE4EFE964632CF36356A7757F0CCBC6FC030C9942175 |

Final verification against this fresh no-save library/DLL: D3D9_REPLACEMENT_RESET_PASS, NO_SAVE_SESSION_PASS, BATMAN_DIRECT_PACKAGE_PASS, BATMAN_DIRECT_DELTA_PASS. Existing menu assets from candidate-4699beb4b778468ca5030d6ae673fecc were intentionally retained and verified, not rebuilt. No stale native library was used for those final checks. Existing Hook.cpp C4244 warnings remain; the standalone fixture compiled with /W4 /WX.

Installed after confirming Batman was closed. Only HelenGameHook.dll was replaced, and its installed SHA-256 matched the fresh source. All 52 preserved files and their file set (packs, hook config, original user INIs, proxy) remained unchanged.

Previous diagnostic DLL backups: `output/replacement-reset-fix-rollback-20260907/HelenGameHook.dll` and `replace-original.dll`, SHA-256 73DCBED2F3F4975D7C0459ECE55F4903DA0797341B10F763EC5AF8FAC4360DDE. Earlier baseline backups remain intact. No commit was made this turn; live Batman acceptance remains pending.

## Failed live acceptance and surface-dispatch reproduction

The user reported another freeze with the installed E79D669A build. Acceptance is **failed**, superseding the pending status above. PID 15308 remained running; no replacement installation or process termination was performed during this investigation.

The current-session log records a 1280x720 to 2560x1440 windowed request. Native Reset now succeeds, but replacement creation subsequently fails: CreateTexture returns S_OK, then the original texture LockRect returns D3DERR_INVALIDCALL. ResetDetour returns that restoration failure to Batman, whose engine retries. This is a later failure boundary than the original retained-default-pool-resource failure.

Captured x86 dumps are `output/batman-hang-15308-restore-failure-x86.dmp` and `output/batman-hang-15308-restore-state-x86.dmp`. The saved original texture LockRect resolves to d3d9!CMipMap::LockRect. Its disassembly includes an internal dispatch through surface vtable slot 13, which our shared surface hook patches.

Extended the real-device fixture against the exact installed library, one difference at a time:

1. Batman's actual 1024x1024 subtitle DDS and COPY/lockable-backbuffer presentation: passed.
2. Expose and release a managed source's level-zero surface before Reset: passed.
3. Also create a default-pool render-target texture, expose its level-zero surface, and release both before Reset: failed at replacement LockRect after successful native Reset, matching the live failure boundary.

The reproducing log is `output/d3d9-reset-fixture-1ae536969ee6434294c52cc685e578ba/fixtures/hook.log`. The current test extension is intentionally retained as a failing regression; it is not a passing final fixture.

Source inspection identifies a strong causal candidate: SurfaceLockRectDetour and SurfaceUnlockRectDetour reject unregistered surfaces, although shared-vtable patching intercepts driver-internal surfaces as well as registered game objects. SurfaceReleaseDetour similarly discards unregistered releases. The original driver's internal surface call can therefore be intercepted despite invoking the saved original texture method. The exact live rejection branch has not been breakpoint-confirmed.

Architectural constraint: replacement restoration/upload currently calls into Direct3D while holding the nonrecursive registry mutex. Adding mutex-protected original-method lookup to the internal surface callback would risk reentrant deadlock; unsynchronized registry reads would not be a sound repair either. Pause for agreement on a bounded dispatch/locking redesign: separate original COM dispatch from optional texture tracking and avoid external COM calls under registry locks. Do not add retries, silently ignore restoration errors, or install an unverified forwarding patch. No additional production behavior was changed in this investigation.

## Approved dispatch/locking implementation

The user approved the architecture and then the written design committed as
`d5aa436`. The implementation remains uncommitted on main. This section supersedes
the preceding architectural pause; it does not yet establish live acceptance.

- `ComOriginalDispatch` owns synchronized original table snapshots separately from
  optional object tracking. Lookup returns copied addresses; original dispatch is
  preserved for unregistered objects sharing patched tables.
- All intercepted texture/surface methods forward through that dispatch.
  Tracking/bookkeeping is optional; internal surfaces no longer receive invented
  INVALIDCALL or zero-reference results merely because they are unregistered.
- Cached replacement ownership is shared between the record and active calls.
  Final COM releases happen outside tracking locks. Texture final release also
  removes stale texture-owned surface registrations.
- Initial uploads and reset restoration use the same prepare/execute/publish
  cache operation. Weak operation tickets reject duplicates and stale work;
  registration/write/reset identities prevent publication after reuse or rewrite.
  Reset work uses source pointers only as identity keys, not as borrowed COM
  objects to dereference after unlocking.
- Texture and surface unlock share an owned-pixel snapshot path. Pixels are
  copied before native unlock; hashing and replacement work use that copy.
  Native unlock is invoked exactly once and outside registry locks.
- `D3d9TextureBindingTransaction` owns prior stage references until publication.
  Failed operations conditionally roll back only their surviving bindings, and
  explicitly log rollback failures. Application binding revisions detect a stage
  observation invalidated by reentry; no timers or polling were added.
- CreateTexture obtains a valid description outside the tracking mutex instead
  of issuing a COM query under the lock or manufacturing description defaults.

### New red/green evidence

The normal native suite first failed with `Dispatch returned our detour instead
of the saved original.` The implemented registry made that test pass.

The original real-driver surface/reset reproduction then passed against the new
library. The fixture now also checks untracked texture/surface LockRect,
UnlockRect, GetSurfaceLevel and GetDevice forwarding, COM device identity, and
every uploaded pixel against the raw payload of Batman's actual subtitle DDS.
An initial-surface-hooks mode installs render-target surface interception before
the first replacement is created, not only before reset.

The separate failure fixture compiles the production hook implementation into
its own translation unit and instruments saved driver entries only inside that
test process. This enables real-device callbacks without adding a production
fault-injection API. Other runtime components still link from the explicitly
selected library. It covers eleven cases: CreateTexture failure, LockRect failure,
UnlockRect failure, invalidation during creation, duplicate nested attempt,
registration reuse, failure after the first stage bind, invalidation during bind,
new application binding during query, newer binding before rollback, and source
rewrite during creation. It asserts original failure HRESULTs, surviving bindings,
expired pending operations and exactly one actual final release per created
replacement.

Observed RED cases drove further changes:

- `932f25d208874cd2bf84c939db856951`: failed cache left an unpublished replacement
  bound; conditional transaction rollback made this pass.
- `a3c7701a4c294f8dae15a22fd409f463`: cache overwrote a newer binding; application
  binding revisions made the reentrant case pass.
- `47586dadf3e44d0fafcb567bcfd62a38`: source rewrite did not invalidate the outer
  cache result; write identity invalidation made the case pass.

Each identifier names a directory beneath `output/d3d9-reset-fixture-`.
The fresh normal Release Win32 native suite passed after these changes, including
the persistence children, generated Batman protocol and file-routing child.
`git diff --check` passed with line-ending notices only.

Fresh no-save candidate verification and installed hashes are recorded below
when completed. The no-save generated session source remains SHA-256
CD533AFDB711CCF06A8FA76C90C7EC8F38B564B869030457372A247C9E0134E1.

### Verified and installed dispatch/locking candidate

Fresh output root: `output/dispatch-locking-nosave-20260907`; matching PDB is in
its native directory. Built from the uncommitted implementation on main, with
design commit d5aa436 as HEAD. No old native object/library was selected for this
fresh build. Existing unrelated Hook.cpp C4244 warnings remain.

| Artifact | SHA-256 |
| --- | --- |
| native/HelenGameHook.dll | AAA4203263263D4686C0FE57F45070EE22A0A25ADE2F5FF9EFA0934A70BB739F |
| native/HelenRuntime.lib | C1F851A937F86EB6D3F734BF8D8EFCAB1DA7456BCAA35CBDAABDA1FEC7EB20B4 |

Final checks against this library/DLL:

- Reset and exact-pixel fixture: PASS, output suffix `d88b3c8240384a0c94e71710e8bb01e5`.
- Initial surface hooks plus reset/exact pixels: PASS, suffix `8f946d265b654db89a8ac28099108e9c`.
- Eleven failure/reentrancy cases: PASS, suffix `8256c89c63054d6ca0dfcd51f3e8d9ab`.
- NO_SAVE_SESSION_PASS: real Commit bypasses writer, foreign executable rejected,
  both fixture INIs unchanged.
- BATMAN_DIRECT_PACKAGE_PASS and BATMAN_DIRECT_DELTA_PASS with the pinned new DLL
  hash and unchanged menu assets from candidate-4699beb4b778468ca5030d6ae673fecc.
- Normal native suite PASS after the final native changes, using isolated
  `output/dispatch-locking-tests-20260907`; `git diff --check` also passed.

Batman was closed before installation. `output/Install-DispatchLocking.ps1`
validated paths, pinned both DLL hashes, backed up the installed E79D669A build,
atomically replaced only HelenGameHook.dll and verified the resulting AAA42032
hash. All 52 preserved files and their file set remained unchanged: packs, hook
configuration, original user INIs and proxy DLL.

Rollback copies are `output/dispatch-locking-rollback-20260907/HelenGameHook.dll`
and `replace-original.dll`, both E79D669A5F0A96B0C75B934CD24A4BAE7801F4FA13DF1C2751D7DA9C69B065D7.
Earlier backups remain intact. Runtime/test changes remain uncommitted.

**Live acceptance is still pending.** User should launch Batman, repeat the
windowed resize and inspect subtitles. The unchanged no-save probe allows one
engine resize attempt per process; further attempts require restarting Batman.
Its intentional NotApplied/Apply Failed result remains separate from whether the
engine resize returned successfully. No claim is made that all historical freezes
or GPU configurations are fixed.

### Continue Game regression: surface-owned parent lifetime

The user confirmed the resize returned, then Continue Game crashed with
D3DERR_INVALIDCALL creating a 128x128 DXT1 texture with eight mips. This invalidates
live acceptance of the AAA42032 candidate. Launch.log and the game's own dump
were preserved in `output/continue-crash-evidence-20260907`. The separate external
capture failed as Batman exited; its zero-byte file is not usable evidence.

The hook log showed address 0x480437C0 previously registered as a 512x512 DEFAULT
depth texture, then reused at the failing creation. Native creation succeeded;
the duplicate-registration guard rejected the stale record. A real D3D9 test
reproduced why: releasing the texture reference before its final surface lets
native surface destruction destroy the parent without our texture-release entry.

The fix obtains an owned parent reference through GetContainer before native
surface Release, then releases that reference through TextureReleaseDetour after
surface bookkeeping and outside the registry mutex. Every tracked mip surface
now participates in lifetime handling; only mip zero supplies replacement pixels.
The duplicate guard remains intact. No runtime polling or retry was introduced.

Red/green evidence:

- White-box stale-registration failure before the fix: fixture suffix
  `13911fcc20eb42529230c7092629cea3`; fixed pass `146cb404b9044594816f01c6d892e187`.
- Public-API regression against installed AAA42032 library: suffix
  `a4fdf411680c4eeba68f13b97a72e523`, CreateTexture returned 0x8876086c.
- Added 256 allocation cycles of 128x128 DXT1/eight-mip textures, releasing parent
  before surface, alternating levels zero/three and writing mip three.

Fresh no-save build: `output/surface-parent-nosave-20260907/native`.

| Artifact | SHA-256 |
| --- | --- |
| HelenGameHook.dll | 91BDC45B142B444D1580468050B2829F2755CB68C871C89D0404CA95E2770679 |
| HelenRuntime.lib | 81A535203364C30979CE069D2E2103571F893C5C9693F3091482E61F1D02F20A |

Final fresh-library tests passed: ordinary reset/subtitle/gameplay fixture
`48cab75a576440a5a07c4c76ba6bfb31`, initial-surface fixture
`7a4dad3eab0c4ed4bcaddba0bc7cd4ae`, and failure/reentrancy fixture
`fd7c7e2a0ac0494482c91e5d00d989f9`. NO_SAVE_SESSION_PASS,
BATMAN_DIRECT_PACKAGE_PASS, BATMAN_DIRECT_DELTA_PASS, the rebuilt normal native
suite and git diff --check also passed. Existing Hook.cpp conversion warnings
remain. The generated no-save session source is unchanged.

Installed using `output/Install-SurfaceParent.ps1` after confirming Batman closed.
The installed DLL hash matches the table. The prior AAA42032 DLL is preserved in
`output/surface-parent-rollback-20260907/HelenGameHook.dll` and
`replace-original.dll`. All 52 packs/configuration/INI/proxy files and their file
set were verified unchanged. Only the hook DLL was replaced.

**Live acceptance remains pending:** restart Batman, resize once, then Continue
Game and check rendering/subtitles. Apply Failed is still intentional in this
unchanged no-save probe. Implementation remains uncommitted.

### User acceptance and commit authorization

After installing the surface-parent candidate, the user reported "all normal"
in response to the requested resize followed by Continue Game test, then explicitly
authorized committing the work. This records acceptance of that tested live path,
not universal GPU coverage. The no-save probe's intentional Apply Failed result
and one-resize-attempt-per-process restriction remain unchanged.

Commit-time verification: the fresh Direct3D failure/gameplay fixture passed again
(`a3c7ae9347df4b0b8dbad0dd5fa0f58f`), and the installed DLL still matched 91BDC45B.
The full native suite did not pass on this rerun: first it reported
"Subtitle upsert did not create Engine.HUD section." A rerun with a newly created
TEMP/TMP directory passed that point but reported "Concurrent Batman graphics apply
did not complete both calls successfully." These failures are in unchanged
CommandExecutor tests; their cause is not established. The earlier recorded full
suite pass remains historical evidence, not a claim that the suite currently passes.
No unrelated fixture/configuration changes were made to conceal these failures.
