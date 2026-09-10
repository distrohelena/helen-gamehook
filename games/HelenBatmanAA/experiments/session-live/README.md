# Repeatable session-only graphics candidate

## V1 checkpoint (2026-09-10)

V1 retains the user-tested session-only graphics workflow and label-only Apply
row. The user confirmed visible PhysX changes for Normal -> Off -> High without
restarting. Logs independently show cache/live agreement: selected=0/cache=0/live=0
and selected=2/cache=2/live=2. No SDK reinitialization was performed.

Installed native build: `output/session-physx-livefield-20260909-a`.
HelenGameHook SHA256: `2344C6B93ECDBCEBF41F045E5B95223394EF090C7A2E53B8A3F9EA404691FB88`.
dinput8 SHA256: `979218423F1E0760FDD02860352BC7A6AE80F8366AA4331A965E4E1ECB09AAC5`.
The installed frontend is candidate `07308570864e4d6a980b4d8fa1fa76b8`.
All 31 original INIs were unchanged at installation; save restoration is a
separate local operation, not part of the graphics feature or source commit.

Known V1 boundaries: Stereo 3D stays disabled; PhysX transitions across level
loads and startup-Off sessions remain unverified, as do MSAA hardware acceptance
and broader mixed display/effect transitions. Successful scalar readback is not
a claim that every existing actor or streamed level has been recreated.
This is the requested source checkpoint, not a ZIP release or universal
hardware-compatibility claim. Build with `Build-SessionLive.ps1` into fresh
repository output; the ordinary default native build does not select this mode.

Final checkpoint verification: fresh full runtime build/suite at
`output/v1-final-regression-20260910-a` passed, including persistent-save and
file-routing child-process tests. Delta, Overlay, Backend, Payload, Physx and
SessionProtocol fixtures passed against the installed build's library. Both
emitted and decompiled frontend-controller and label-only Apply tests passed.

This candidate replaces the one-shot writer substitution with an owned native
backend. It does not modify the ordinary persistent graphics writer or subtitle
persistence. Work remains on main; no commit is implied by candidate creation.

## Supported boundary and limitations

- Resolution, fullscreen and VSync use the already tested viewport/reset path.
- Bloom, shadows, motion blur, distortion, fog volumes, spherical-harmonic
  lighting and ambient occlusion use explicit cache reload and stock
  FSystemSettings Apply with `save=false`.
- MSAA uses the mapped settings/render summary, checks the target against the
  engine's three initialization pixel formats, and requests the existing reset
  path. Its new live transition still needs hardware acceptance; a CPU test is
  not proof of effective antialiasing.
- Experimental PhysX engine-field/cache edits are enabled at the user's request (2026-09-09).
  Supported-field mask is 14335; Stereo remains noneditable. PhysX changes only
  the session overlay, reloaded Engine.Engine/PhysXLevel cache, and GEngine+0x3C8.
  The pinned getter at 0xCB0420 and script wrapper at 0xCB2470 read this live field.
  Capture and preflight use the live scalar, not the cache. A checked atomic
  update refuses stale values; verification requires engine identity, cache and
  live scalar agreement. No renderer-payload offset is invented for PhysX.
  Logs mark readback EXPERIMENTAL with effects-unverified=1 and reinitialized=0.
  Successful Apply still does NOT establish that existing level content changed.
  No PhysX loader/initializer is called. Compare visible effects manually; this is
  a throwaway feasibility probe, not a verified live-PhysX feature.

The first cache-only probe reached selected=0/cache=0, but the running engine
remained at level 1 and the redirected INI later returned to 1. Further Apply
was rejected by the stale-baseline guard. Read-only inspection confirmed
GEngine=0x1474BA00 and field=0x1474BDC8 in that process; these heap addresses
are evidence only, never reusable bindings. The global pointer is 0x26C3CDC.
The exact writeback caller remains unproven. Native consumers include 0x150F3B0
(PhysX level-loading exclusion) and 0x15B0F90 (level-specific flag filtering).
- Session success is outcome 5. Reopen reads the backend's last verified state;
  repeat/reverse Apply is allowed. A possible partial mutation locks Apply until
  process restart and never publishes the requested draft as success.

No-op requests perform no staging or native calls. Reentry uses the existing
atomic operation guard, not a recursively acquired mutex, poll or timer.

## Pinned evidence

Executable: ShippingPC-BmGame.exe, size 38758728, preferred base 0x400000,
SHA256 `4dac1f5e2ac6710b7378fdce74601f616f4753e3756cb5fda63c7519cc2eb028`.

`Inspect-SettingsLayout.py` decodes the on-disk C20130 Boolean and integer
tables, tracking the intervening stack push. Offsets are relative to payload
0x26C0B3C (owner 0x26C0B38), not to the owner itself.

| Field | Payload offset | Independent renderer summary |
| --- | --- | --- |
| Fullscreen | 0x244 | Actual viewport/swap-chain mode |
| VSync | 0x21C | Actual presentation interval |
| MSAA | 0x248 | 0x26C0DF8 |
| Bloom | 0x34 | None; direct consumers at 0x6508C0 / 0x835470 |
| DynamicShadows | 0x24C | 0x26C0DF0 |
| MotionBlur | 0x28 | 0x26C0DE8 |
| Distortion | 0x3C | None; direct consumer at 0x69AC0D |
| FogVolumes | 0x48 | 0x26C0DF4 |
| DisableSphericalHarmonicLights | 0x60 | None; direct consumer at 0x6C6FDE |
| AmbientOcclusion | 0x30 | 0x26C0DEC |
| Stereo (read only) | 0x2A8 | Consumer at 0xEA6AD7; no safe lifecycle transition established |
| ResX / ResY | 0x23C / 0x240 | Actual viewport/backbuffer dimensions |

PhysXLevel is read at 0xA40AD0 and controls loader selection: the zero path
loads PhysXLocal/PhysXLoader.dll, the nonzero path tries PhysXLoader.dll. At
0xA40BD6 the initializer result is stored in 0x26B63E4 and used by physics
consumers. Calling that initializer in a running world is not an established
reinitialization path. No library unload, world destruction, level reload or
cross-restart overlay persistence is attempted.

Reload: Find 0x622120, FConfigFile::Read 0x6204A0, GetBool 0x6244D0,
GetInt 0x624200. The logical name and file-manager user root must match the
validated physical route; no scratch-directory scanning is used.

Stock Apply 0xC40090 consumes all 0xAB payload dwords. Its C3FDE0 path first
loads cached settings, then compares incoming display values at C3FEB2..C3FED0.
The candidate requires those tuples to agree, suppressing the nested stock
resize, and performs a single explicit viewport transition afterward. It also
retains the directional-lightmap equality guard to prevent the stock toggle
at C3FF88. C24B50/C20B30 transfer render summaries and refresh affected resources.

MSAA format checks mirror EA718F..EA71E1 using D3DFORMAT globals 0x25DA7C0,
0x25DA8E0 and 0x25DA994. The renderer summary drives surface construction at
EA7D4E onward. Normalized values 0,1,2,3,5 encode to 1,2,4,8,16 samples; zero
and one both decode as Off. Vendor-specific 9..12 encodings are not invented.

## Overlay publication

Stage all changed keys into a unique private sibling, using the proven profile
API's ANSI/UTF-16 behavior. No persistent writer is called. The routing service's
`PublishSessionReplacement` compares captured overlay bytes and replaces under
the same mutex as routed opens, refusing outstanding handles, stale bytes,
foreign staging directories, reparse/hardlink sources and wrong route policies.
It never replaces the original. Original and target bytes are checked afterward;
Batman then independently reads the routed file into its config cache.

The profile API's explicit cache-flush operation returns zero by contract;
the candidate does not interpret that as ordinary write failure. See
[Microsoft's API contract](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-writeprivateprofilestringw).

## Verification on 2026-09-09

Corrected one-shot Bloom acceptance: live 0 -> 1, cache 0 -> 1, unrelated
payload preserved, original unchanged. Snapshot
`output/bloom-corrected-live-20260909-a` compared all 31 INIs unchanged.
The older probe's Bloom/AO offset mistake is not evidence for live Bloom.

Fresh candidate build: `output/session-live-20260909-b`.

- Runtime DLL: `7D97647F37721ECCD6FC95739996322B9EFD70A5F611FAECF658FBE8789FDE22`
- Proxy: `5420C46A344CFAE13BDE7726626453BD6DC787C6306E26BC94D1A93DC7D92F41`
- Runtime library: `6D1EB2B3946969FBB7A6BE0BF04B61B05FCB20B75656FCCEF16C9592E9B9C02C`

`source-manifest.json` records all native inputs before compilation and requires
them unchanged afterward. Package creation rechecks those inputs and artifacts.
The build compiles the real session service and does not compile the one-shot
NoSaveResolutionProbe or generated service substitution.

Passed: Delta, Payload, Backend, Overlay, SessionProtocol, real runtime/proxy
startup, anchored refresh activation, full normal runtime/persistence/routing
suite (`output/session-regression-20260909-b`), emitted AND decompiled menu
behavior, package and delta validation, installed retail base compatibility.

Package: `output/batman-direct-graphics/candidate-bed9c9dedc6846f9a6567a9818463053`.
The existing four proxy LNK4104 warnings remain. FFDec reported an inaccessible
user-profile logging lock, but exported successfully and its consumed output
passed the behavior harness and delta reconstruction checks.

The historical Test-NoSaveSession fixture was also tried against the new
library and rejected it because it requires the removed one-shot probe. This
is not counted as a pass or suppressed: that fixture applies only to historical
NoSaveProbe builds. The new backend/protocol/overlay fixtures cover the new mode.

Still required: live mixed effects, reverse Apply, close/reopen, MSAA, display-only
and mixed display/effects tests. CPU tests do not establish retail GPU behavior.

Installed with Batman/launcher closed. Both binaries and all eight graphics pack
files matched the fresh candidate. All 31 original INIs and 32 files belonging
to the subtitle/video-skip packs and runtime configuration remained unchanged.
Prior binaries, the complete prior graphics pack, and before/after INI snapshots
are recoverable in `output/session-live-install-20260909-b`. No commit was made.

Live acceptance subsequently confirmed by the user and logs: Bloom and Shadows
both changed Off -> On in one Apply, then On -> Off in the next Apply without a
restart. Both operations returned verified session success, retained the same
1920x1080 windowed viewport and presentation interval 1, and preserved unrelated
payload values. All 31 original INIs still match the install snapshot in
`output/session-live-confirmed-20260909-b`. This confirms mixed/reverse effects,
not the still-untested MSAA or mixed display transitions.

Approved UI follow-up: hide the Apply row's right-hand status text, leaving only
the Apply Changes label with its existing enabled/dimmed state and activation.
Diagnostics remain in the native logs. The emitted and decompiled row handler
are exercised by `scripts/Test-BatmanApplyRow.js` alongside the controller tests.
The rebuilt UI candidate `07308570864e4d6a980b4d8fa1fa76b8` passed both row
handlers, both controller handlers, and package/delta verification. Only
`files.json` and the Frontend delta were installed; native DLLs, other packs,
configuration and all 31 original INIs stayed unchanged. The replaced menu files
are backed up in `output/apply-label-install-20260909-a`.

Installed with Batman/launcher closed. Both binaries and all eight graphics pack
files matched the fresh candidate. All 31 original INIs and 32 files belonging
to the subtitle/video-skip packs and runtime configuration remained unchanged.
Prior binaries, the complete prior graphics pack, and before/after INI snapshots
are recoverable in `output/session-live-install-20260909-b`. No commit was made.
