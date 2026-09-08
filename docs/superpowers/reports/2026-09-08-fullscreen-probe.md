# Fullscreen extension checkpoint

## Scope and status

Approved bounded extension on main, based on e7b6aac. Fullscreen is implemented
in the opt-in no-save candidate only. No installation, commit, game launch or
desktop fullscreen switch was performed. Normal projects still do not import
NoSaveProbe.targets. The installed user-tested candidate remains unchanged.

The existing engine entry now receives the requested zero/one fullscreen flag.
Current fullscreen state no longer disqualifies the viewport. Validation allows
equal dimensions for a mode change, rejects identical size/mode, and retains
positive windowed custom resolutions without requiring fullscreen support.

NoSaveDisplayRequest owns validated engine arguments. NoSaveFullscreenModes queries
the live D3D device's adapter and type, current adapter display format and actual
backbuffer format. It checks exclusive format compatibility and enumerates exact
resolution pairs for that adapter/format. Unsupported pairs and discovery errors
stop before the one-shot attempt is consumed. References acquired for enumeration
are released before the engine call. No fallback resolution/adapter/format is used;
the engine still selects refresh rate.

Fingerprint, preferred-base, thread, viewport ownership, idle-renderer and lifetime
checks remain. No timers, retries, direct Reset calls or Win32 mode changes were
added. There remains one engine attempt per process. Helen's writer remains
bypassed, and NotApplied/Apply Failed remains intentional. Engine-owned persistence
is subject to the separately configured file-routing policy.

## Verification

Fresh build: `output/fullscreen-nosave-20260908/native`, using the existing explicit
Prepare-NoSaveProbe/NoSaveProbe.targets build path. All objects were compiled into
the new isolated output. Existing Hook.cpp C4244 warnings remain.

| Artifact | SHA-256 |
| --- | --- |
| HelenGameHook.dll | DB757D38C9E02BB9DBD87CD29F6DDABDE77866868C4636956FE9AFB6C7A90752 |
| HelenRuntime.lib | 3F535BEA74D0A2419948BDBEAE6758267DC90BC83DFC87096BF7DA5623A45522 |
| Installed DLL, unchanged | 91BDC45B142B444D1580468050B2829F2755CB68C871C89D0404CA95E2770679 |

- Request tests first failed on the missing header, then reached a compiled
  unimplemented validation failure in fixture `9b6f950ba8424d868c5aa6558670a46d`.
  Validation passed after implementation (`958b5367179940e1996469b936a2ffe8`).
- Real read-only adapter enumeration failed on its unimplemented entry in fixture
  `bb82bb88fbeb43028cea5fdb96b8d6ec`, then passed after implementation.
- Final `NO_SAVE_DISPLAY_REQUEST_PASS` against the fresh library:
  `display-request-6e48076818c94a368b47658cc81448c5`. Covers mode-only entry/exit,
  windowed/fullscreen size changes, no-op rejection, unsupported/cross-paired
  dimensions, empty catalog, invalid flags/dimensions and hidden-device discovery.
  The device remains windowed at 64x64 and the test window remains hidden.
- `NO_SAVE_FULLSCREEN_SESSION_PASS` and `NO_SAVE_SESSION_PASS`: a real session
  stages the same-size fullscreen catalog pair and reaches the guarded probe;
  the foreign test executable is rejected, both fixture INIs remain byte-identical,
  and consumed transactions cannot execute twice. The first new session test used
  an inconsistent display fixture (desktop absent from supported modes); fixing the
  fixture to include its declared desktop resolved that test failure. No production
  catalog checks were weakened.
- `D3D9_GAMEPLAY_TEXTURE_LIFETIME_PASS` and failure/reentrancy coverage:
  `d3d9-reset-fixture-a8af472f48d94cf38fcd34a0d128deae`.
- Initial-surface hooks, windowed reset, exact subtitle pixels and gameplay texture
  lifetime: `d3d9-reset-fixture-93a8d44bc587467c9699fee670a242f0` passed.
- `BATMAN_DIRECT_PACKAGE_PASS` and `BATMAN_DIRECT_DELTA_PASS`, using the explicit
  fresh DLL/library, pinned DLL hash, engine-config route and unchanged assets in
  candidate-4699beb4b778468ca5030d6ae673fecc.
- `git diff --check` passed. The prior full native suite's subtitle/concurrent-apply
  failures were not investigated or claimed resolved in this fullscreen change.

## Remaining boundary

Automated checks do not prove Batman's actual exclusive-mode transition, renderer
recovery, fullscreen exit/window placement, gameplay, or fullscreen persistence.
No request for manual testing is part of this handoff. Those checks can wait until
the user chooses to test. The candidate is not a release-ready live Apply feature.

## Subsequent user-requested installation

The user explicitly requested installation after the build-only handoff. Batman
was closed. `output/Install-Fullscreen.ps1` pinned source and installed hashes,
rejected reparse-point ancestry, preserved the previous DLL, and atomically
replaced only HelenGameHook.dll. The installed SHA-256 is now
DB757D38C9E02BB9DBD87CD29F6DDABDE77866868C4636956FE9AFB6C7A90752.
All 52 packs/configuration/user-INI/proxy files and their file set were verified
unchanged. The prior 91BDC45B build is preserved at
`output/fullscreen-rollback-20260908/HelenGameHook.dll` and `replace-original.dll`.
No game launch or live fullscreen validation was performed. The one-attempt and
intentional Apply Failed restrictions remain; changes are still uncommitted.

## Live acceptance

After installation, the user reported "it works" and explicitly authorized the
commit. This accepts the fullscreen behavior they tested; it does not establish
every mode/direction, repeated transitions, GPU configuration or persistence path.
The experimental one-attempt and no-save result contracts remain unchanged.
