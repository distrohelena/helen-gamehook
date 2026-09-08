# D3D9 reset failure diagnostics

The diagnostic-only installed DLL was subsequently superseded by the tested lifecycle fix; see `2026-09-07-d3d9-replacement-reset-fix.md` for the current installed hash and rollback.

## Scope

Diagnostic-only follow-up to Batman's captured resolution hang. The x86 snapshot of PID 35528 showed the engine retrying a failed device Reset with a one-second sleep. The exact HRESULT was not retained at that sleeping stack frame. This change does not fix the resolution failure, add retries, or change file routing or persistence.

Each device now owns diagnostic suppression state. ResetDetour copies the requested presentation parameters before the original call, records the returned HRESULT and calling thread, and logs the first failure or a changed failure/request/thread. Identical consecutive failures are silent. Successful recovery emits one message and rearms suppression. The diagnostic adds no device queries and holds no mutex across the original call or logging.

Failure messages include every D3DPRESENT_PARAMETERS field, including the window handle. A null argument is reported explicitly. Driver HRESULTs and the original Reset call remain unchanged.

## Verification

- Focused Win32 console test first failed with `Failed Reset must emit its HRESULT and request.` against a no-op implementation, then passed after implementation (`RESET_DIAGNOSTICS_PASS`).
- Focused tests cover emitted error/request fields, 100 identical retries, changed HRESULT/request/thread, null input, recovery, rearming, and independent device state.
- Full Release Win32 native suite: PASS, including the three Batman persistence fault children, generated protocol check, and file-routing hook child.
- First full-suite invocation failed because the isolated output layout did not match the export test's parent-directory DLL lookup. The fresh test executable was copied into `native/tests` and rerun successfully against the fresh parent DLL. No production fallback was added.
- Fresh no-save build: PASS. Existing Hook.cpp C4244 warnings remain; focused diagnostic compilation passed with /W4 /WX.
- No-save session test: PASS; real Commit bypasses the Helen writer, foreign executable rejected, fixture INIs unchanged.
- Existing routing graphics pack plus new DLL: `BATMAN_DIRECT_PACKAGE_PASS` and `BATMAN_DIRECT_DELTA_PASS`. Menu assets intentionally unchanged, not rebuilt.
- Hook integration inspected: pre-call parameter snapshot, original call once, result recorded after return, logging outside the mutex, failed HRESULT returned untouched. This does not substitute for live driver verification.

## Verified artifact — installed

Fresh root: `output/reset-diagnostics-nosave-20260907`

| Artifact | SHA-256 |
| --- | --- |
| native/HelenGameHook.dll | 73DCBED2F3F4975D7C0459ECE55F4903DA0797341B10F763EC5AF8FAC4360DDE |
| native/HelenRuntime.lib | D7B34CD4DB43AFF8F5CCA8C9715E809159FC28EE01122A55C7E9254533712875 |
| BatmanGraphicsSessionService.cpp (fresh no-save substitution) | CD533AFDB711CCF06A8FA76C90C7EC8F38B564B869030457372A247C9E0134E1 |

Built from the working changes on main based on d7189e5, not a new committed release. Source remains uncommitted. Matching PDB is beside the DLL.

Package validation used unchanged `output/batman-direct-graphics/candidate-4699beb4b778468ca5030d6ae673fecc/packs`, its target map, the verified retail base map, and the fresh DLL/library above. The old candidate's provenance is not rewritten to represent this new DLL.

Installed after the user closed Batman and authorized proceeding. Only HelenGameHook.dll was replaced; its installed SHA-256 matches the fresh artifact above. All 52 preserved files (packs, hook config, user INIs, and proxy) and their file set were unchanged.

Verified previous DLL backups: `output/reset-diagnostics-rollback-20260907/HelenGameHook.dll` and `replace-original.dll`, both SHA-256 `1920EF2081956D236CBB8C195FEF6F8892A7B60E81FF03D140D8756D09A53FE0`. The first replacement call rejected PowerShell's null backup-path argument before changing the target; after verifying the unchanged target and staged/backup hashes, replacement succeeded with an explicit backup path. Staging was consumed by the atomic replacement.

Live diagnostic reproduction remains pending. No live freeze fix is claimed.

## Live reproduction — PID 22532

The user reproduced the freeze with the diagnostic DLL. Log line 494604 reports:

```text
[d3d9] reset device=0x20E4C660 failed hr=0x8876086c thread=46152 width=2560 height=1440 format=21 buffers=1 multisample=0 quality=0 swapEffect=3 hwnd=002502D0 windowed=1 autoDepth=0 depthFormat=0 flags=1 refresh=0 interval=1
```

The probe recorded 1280x720 before requesting 2560x1440. The x86 stack again shows Sleep(1000), returning to the engine Reset retry loop at 0x00ea66e5. HRESULT 0x8876086c is D3DERR_INVALIDCALL.

### Confirmed resource-lifetime defect

- Current-session log lines 458436–458448 show a subtitle replacement created for device 0x20E4C660: original tracked texture 0x473F21C0, replacement 0x473F2FA0, asset subtitle_scaled.dds.
- TryCreateA8R8G8B8ReplacementTexture creates that replacement with D3DUSAGE_DYNAMIC and D3DPOOL_DEFAULT.
- Full snapshot inspection with the matching PDB finds exactly one non-null ReplacementTexture in g_tracked_texture_records. It is the same replacement 0x473F2FA0, still owned by device 0x20E4C660. Original texture description is 256x256 DXT5, D3DPOOL_MANAGED; ReplacementMatched and HasFingerprint are true.
- ResetDetour invokes the original Reset without releasing owned replacement textures. TextureReleaseDetour releases the replacement only when the original texture reaches reference count zero. The managed original remains alive through this reset, retaining our default-pool replacement.
- This violates the documented Reset prerequisite to release D3DPOOL_DEFAULT resources: https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-reset . It supplies a concrete blocker consistent with the observed invalid-call failure, not proof that every historical crash has this cause.

Next implementation should correct ownership across device reset, preserving subtitle replacement after successful reset and keeping failure handling explicit. No fix or live memory modification was performed during this diagnosis; a corrected-build reproduction remains required.

### Evidence retained

- `output/batman-hang-22532-reset-invalidcall-x86.dmp`: thread snapshot.
- `output/batman-hang-22532-reset-resources-x86.dmp`: full local memory snapshot, 1,438,178,092 bytes; not uploaded.
- Debugger query: `dx -r2 (HelenGameHook!g_tracked_texture_records).Where(p => p.second.ReplacementTexture != 0).First().second`.
- Original BmEngine.ini SHA-256 remains B27DB7109E2FD4810D9A212873DBA93F6D4B160914B6190E127292176E80C045; UserEngine.ini remains EAD871C3699C28B9500FC4DA624126A6DFFFD94011E914AA0CAE3F69E20D1536.

Batman was left running. User may close it now that snapshots are captured.
