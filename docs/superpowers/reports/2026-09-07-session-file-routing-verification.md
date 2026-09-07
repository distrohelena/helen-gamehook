# Session file-routing verification

Implementation source: `455e88f03897c850b7e40abd2e2bb7cdb68645c8` on `main`.
Pre-feature baseline: `05a7e2c`. Specification and plan are in the adjacent
`specs/2026-09-06-session-file-routing.md` and
`plans/2026-09-06-session-file-routing.md`.

The final scoped review found no remaining Critical or Important production issue.
No candidate has been installed or launched; live Batman acceptance remains pending.
Do not substitute the no-save experiment
for the normal trusted-save configuration flow.

## Candidate provenance

All three candidates were freshly composed from source `455e88f`, explicitly
provided native inputs, and the verified retail Frontend. Their provenance records
`liveTestPending: true`. The controller separately verified source DLL, copied DLL,
runtime-library, and generated no-save source hashes against the provenance files.

| Mode | Candidate under `output/batman-direct-graphics/` | Native DLL SHA-256 |
| --- | --- | --- |
| TrustedSaveRouting | `candidate-c818dbb877da42ca8a705acca365a5d0` | `B0FA2BDFBF1E8DC48DFE55966086FBA71A7E503B2A647BD5A66ACFBD6DC7924E` |
| RoutingNoSaveProbe | `candidate-4699beb4b778468ca5030d6ae673fecc` | `1920EF2081956D236CBB8C195FEF6F8892A7B60E81FF03D140D8756D09A53FE0` |
| NoRoute control | `candidate-aac36145d0ba4c03ad0f7f25d5ab28b1` | `B0FA2BDFBF1E8DC48DFE55966086FBA71A7E503B2A647BD5A66ACFBD6DC7924E` |

Normal native output is `output/routing-final-fix-native-20260907-d/native`;
its runtime library hashes to
`C59533A7E662061E339A20555AA58BF5667A7D25EBD5377FE051A0FB4516F7FC`.
No-save output is `output/routing-final-fix-nosave-20260907/native`;
its runtime library hashes to
`D34699978DC6D9058C61876BC3FB0336F74E17BF01A00D6BCDB676D27EAD251B`.
The generated no-save source hashes to
`CD533AFDB711CCF06A8FA76C90C7EC8F38B564B869030457372A247C9E0134E1`.

The candidate guide at
`games/HelenBatmanAA/experiments/file-routing/README.md` contains rebuild commands,
the exact opt-in route, import coverage, and separate live acceptance steps.
Historical candidates remain inert output; they are not final inputs or deliverables.
The README retains older Task5 tables and commands before its final section. Use
the current candidate table in this verification record, not those older examples.

## Controller verification on the final artifacts

- The fresh Release Win32 full native runner exited 0. It printed all three Batman
  synchronization-fault child PASS markers, `GENERATED_BATMAN_PROTOCOL_PASS`,
  `FILE_ROUTING_HOOK_CHILD_PASS`, `FILE_ROUTING_HOOK_CHILD_EXIT=0`, and `PASS`.
  Its default branch has 42 top-level suite/entrypoint calls, including four child
  launchers; this is not an individual test-case count.
- Both routing candidates independently passed the native pack parser/export/hash
  verifier and delta reconstruction (`BATMAN_DIRECT_PACKAGE_PASS` and
  `BATMAN_DIRECT_DELTA_PASS`). Each verifier received the separately captured fresh
  source DLL SHA-256 explicitly.
- All nine rejection cases printed `REJECTED_AS_EXPECTED`, followed by
  `BATMAN_DIRECT_PACKAGE_REJECTIONS_PASS`: duplicate hook, missing export, wrong
  signature, stale target, legacy commands, malformed route, duplicate route,
  wrong target route, and stale DLL.
- The fresh no-save library passed `NO_SAVE_SESSION_PASS`: the real Commit bypassed
  the writer, the foreign executable was rejected, and both fixture INIs were unchanged.
- All four actual frontend checks passed: emitted and FFDec-decompiled controllers
  for each of the two routing candidates. Inputs were their own fresh
  `builder/generated/direct-graphics-<candidate-id>/` directories, not old controllers.
- Source/candidate/runtime/generated-source provenance checks passed for all three
  candidates. Installed DLL and real INI hashes still matched the pre-feature baseline.

The controller ran the native runner through `Invoke-BatmanConsoleTool.ps1`, both
`Test-BatmanDirectGraphicsPackage.ps1` invocations, the final
`Test-BatmanDirectGraphicsPackageRejections.ps1` invocation, and
`Test-NoSaveSession.ps1`. The four frontend checks used
`Test-BatmanDirectGraphicsFrontend.js` with explicit emitted/decompiled file paths.

## Preserved user state

| Original | Unchanged SHA-256 |
| --- | --- |
| Installed `Binaries/HelenGameHook.dll` | `8E918BE2B2A86F85271B0FFFE9976100A3E3D0B55FBFE2B8102939FF611E21FF` |
| Documents Batman `BmEngine.ini` | `B27DB7109E2FD4810D9A212873DBA93F6D4B160914B6190E127292176E80C045` |
| Documents Batman `UserEngine.ini` | `EAD871C3699C28B9500FC4DA624126A6DFFFD94011E914AA0CAE3F69E20D1536` |

The user-owned untracked `batma/` assets were not changed or used as build inputs.

## Limits and evidence qualifications

- This is cooperative main-executable IAT routing, not an OS security sandbox.
  Unhooked modules, dynamic exports, native syscalls, other processes, and pre-existing
  mappings remain outside the stated boundary. Unsupported alias/mutation forms fail
  explicitly; cross-volume overlay publication is not emulated with copy/delete.
- Sessions never reuse scratch. Explicit service disposal cleans its own directory;
  process-lifetime game exit can retain inert files because filesystem teardown is not
  performed under loader lock. No cleanup daemon was added.
- Fresh builds retain the existing `Hook.cpp` C4244 conversion warning. Package test
  linking reports an informational `/GL` to `/LTCG` restart. FFDec reported its profile
  log-lock permission warning while successfully exporting scripts; these are not
  silently treated as clean diagnostics.
- Initial test ordering was not consistently behavioral test-first: core, integration,
  and packaging stages contain documented compile-only or other insufficient RED
  evidence. Task 2's branch-disabled check is mutation evidence, not historical TDD.
  No router-level pre-fix RED executable ran in the final fix wave. Native delete-on-close
  characterization and code inspection are not substituted for that missing run.
- Security/MAXIMUM_ALLOWED open coverage accepts legitimate access/privilege errors and
  checks unchanged originals, but does not identify the successful handle's final path
  or report each privileged branch outcome. The original-read GENERIC_ALL regression
  does explicitly verify overlay writes and unchanged original bytes.

## Review disposition

The single scoped re-review of `8cc8120..0005870` marked all five original findings
addressed and found no new Critical or Important production issue. The narrow
security/MAX test-evidence gap and historical README ambiguity are deferred and
disclosed above; no second implementation wave was started.

The review also questioned accepting high bytes in the legacy rename union. The
controller retained that behavior: Windows SDK 10.0.26100 `winbase.h:9112-9124`
defines the legacy member as `BOOLEAN ReplaceIfExists`, and the extended member
as `DWORD Flags`. Only the active legacy member is meaningful; rejecting arbitrary
unused bytes would reject otherwise valid callers. Microsoft's
[FILE_RENAME_INFO member documentation](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info)
confirms that distinction. The reviewer found no protected-path bypass here.

## Review evidence archive

The completed plan's 33 scratch files were archived to
`output/session-file-routing-review-20260907-455e88f.zip`. Every archived file was
SHA-256 checked against its source before removing only
`.superpowers/sdd/2026-09-06-session-file-routing/`. The archive preserves the
ledger, all recorded rulings, briefs, reports, and review packages. The two reports
previously tracked there also remain recoverable from Git history. Other plans,
candidate outputs, installed game files, and user assets were not removed.
