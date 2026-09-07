# Final fix wave report

## Scope

This wave addresses the five findings in `final-fix-brief.md`: mutation-capable open classification, handle-based rename guards, the live guide wording, BCrypt verifier cleanup, and the touched routing-code safety/style requirements. It does not change `batma/`, installed packages, or real user INIs.

## Finding 1: mutation-capable opens

`FileWriteRoutingService::IsWriteRequest` now receives `flags` and treats `FILE_FLAG_DELETE_ON_CLOSE`, `GENERIC_ALL`, `WRITE_DAC`, `WRITE_OWNER`, `ACCESS_SYSTEM_SECURITY`, `MAXIMUM_ALLOWED`, existing write/delete bits, and mutating dispositions as mutation-capable. Therefore deny routes fail closed and redirect routes choose the overlay even when the requested access is otherwise read-only.

Behavioral regression cases were added to `FileWriteRoutingServiceTests.cpp` and the imported child fixture: deny delete-on-close/full-access, redirect original-read delete-on-close, redirect `GENERIC_ALL` with `OPEN_EXISTING`, exact original bytes, and overlay-only imported writes.

## Finding 2: handle rename guard

`SetFileInformationByHandleDetour` now bounds and guarded-copies `FILE_RENAME_INFO` and `FileRenameInfoEx`, validates the full copied header (including flags), rejects malformed/unsupported stream forms, resolves `RootDirectory` directory handles, captures relative names against the process CWD, and checks protected source files, protected source parent directories, protected destinations, and protected destination parents before native dispatch. Native dispatch receives an owned absolute payload with `RootDirectory == nullptr`, preserving the path classified even if caller memory or CWD changes afterward.

Imported fixture coverage now includes overwrite-capable protected destination rename, protected parent-directory handle rename, malformed buffers, unrelated native rename, valid unrelated `RootDirectory`, and a one-shot CWD-switch relative rename. Existing unrelated native operations remain covered.

The guard deliberately checks the destination itself for a protected parent-directory target, not the destination's parent directory; this preserves native sibling-file renames inside a directory that also contains a protected route. RootDirectory forms with root components or traversal are explicitly rejected.

## Findings 3–5

The live guide now describes the trusted candidate as config-save-only: explicit original/overlay synchronization and restart are production checks; only `RoutingNoSaveProbe` exercises experimental automatic engine resize writes. The package verifier uses RAII wrappers for BCrypt algorithm/hash handles. New helper declarations have substantive Doxygen comments and the added guarded memory boundary is isolated from production mutation.

## TDD / verification evidence

No router-level pre-fix regression executable was run in this wave, so there is no executed router RED result to claim. The native characterization probe did confirm that unhooked `GENERIC_READ | FILE_FLAG_DELETE_ON_CLOSE` deletes a temporary file on close; code review identified the original-read `GENERIC_ALL`/`OPEN_EXISTING`, protected-destination handle rename, and protected-parent handle rename bypasses. These are distinguished from the final behavioral GREEN below, which executes the service and imported fixtures against the corrected code and asserts physical bytes, existence, and no-mutation outcomes.

The authoritative build used the proven isolated wrapper (MSBuild 18.9.1, v143, Release, Win32, `/m:1`, `FreshBuildIsolation.targets`) from source commit `455e88f03897c850b7e40abd2e2bb7cdb68645c8`:

```powershell
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj' /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:FreshOutput=C:\dev\helenhook\output\routing-final-fix-native-20260907-d /p:ForceImportBeforeCppTargets=C:\dev\helenhook\games\HelenBatmanAA\experiments\file-routing\FreshBuildIsolation.targets /m:1 /nodeReuse:false /v:minimal; exit `$LASTEXITCODE"
```

Build completed successfully. The fresh artifacts hash to:

```text
HelenGameHook.dll (normal): B0FA2BDFBF1E8DC48DFE55966086FBA71A7E503B2A647BD5A66ACFBD6DC7924E
HelenRuntime.lib (normal): C59533A7E662061E339A20555AA58BF5667A7D25EBD5377FE051A0FB4516F7FC
HelenGameHook.dll (no-save): 1920EF2081956D236CBB8C195FEF6F8892A7B60E81FF03D140D8756D09A53FE0
HelenRuntime.lib (no-save): D34699978DC6D9058C61876BC3FB0336F74E17BF01A00D6BCDB676D27EAD251B
Generated no-save source: CD533AFDB711CCF06A8FA76C90C7EC8F38B564B869030457372A247C9E0134E1
```

The full console run through `Invoke-BatmanConsoleTool.ps1` exited 0 and printed `BATMAN_PARTIAL_SYNC_CHILD_PASS`, `BATMAN_RECOVERED_SYNC_CHILD_PASS`, `BATMAN_FAILED_SAVE_SYNC_CHILD_PASS`, `GENERATED_BATMAN_PROTOCOL_PASS`, `FILE_ROUTING_HOOK_CHILD_PASS`, `FILE_ROUTING_HOOK_CHILD_EXIT=0`, and `PASS`. The package builder printed `BATMAN_DIRECT_GRAPHICS_FRONTEND_PASS`, `BATMAN_DIRECT_PACKAGE_PASS`, `BATMAN_DIRECT_DELTA_PASS`, and `VERIFIED_DIRECT_GRAPHICS_CANDIDATE` for both fresh routing modes; the expected FFDec profile-lock warning remains environmental. The rejection verifier printed all nine `REJECTED_AS_EXPECTED` cases and `BATMAN_DIRECT_PACKAGE_REJECTIONS_PASS`. The no-save session check printed `NO_SAVE_SESSION_PASS` and confirmed both INIs unchanged.

The exact final verification commands were:

```powershell
rtk proxy powershell.exe -NoProfile -ExecutionPolicy Bypass -File games/HelenBatmanAA/scripts/Invoke-BatmanConsoleTool.ps1 -FilePath output/routing-final-fix-native-20260907-d/native/HelenRuntimeTests.exe -Arguments x
rtk proxy powershell.exe -NoProfile -ExecutionPolicy Bypass -File games/HelenBatmanAA/scripts/Test-BatmanDirectGraphicsPackageRejections.ps1 -CandidateRoot output/batman-direct-graphics/candidate-aac36145d0ba4c03ad0f7f25d5ab28b1 -RuntimeLibraryPath output/routing-final-fix-native-20260907-d/native/HelenRuntime.lib -RoutingCandidateRoot output/batman-direct-graphics/candidate-c818dbb877da42ca8a705acca365a5d0
rtk proxy powershell.exe -NoProfile -ExecutionPolicy Bypass -File games/HelenBatmanAA/experiments/windowed-resolution/Test-NoSaveSession.ps1 -RuntimeLibraryPath output/routing-final-fix-nosave-20260907/native/HelenRuntime.lib
```

Both candidate builds ran the emitted frontend check and the FFDec-decompiled frontend behavior check; FFDec's profile-lock warning is recorded and did not prevent either PASS.

## Files changed

- `HelenRuntime/FileWriteRoutingService.cpp`
- `HelenRuntime/FileApiHookSet.cpp`
- `tests/HelenRuntime.Tests/FileWriteRoutingServiceTests.cpp`
- `tests/HelenRuntime.Tests/FileWriteRoutingHookFixture.cpp`
- `tests/HelenRuntime.Tests/BatmanDirectGraphicsPackageTests.cpp`
- `games/HelenBatmanAA/experiments/file-routing/README.md`

## Final commits and candidates

Finding commits are `8f4d2ac` (behavioral classification, rename guard, guide, RAII, and initial style), `7582063` (preserve unrelated sibling renames), `5c6d18e` (coverage, helper separation, fields-first layout, and final test fixes), and `455e88f` (range-limited final rename-helper/detour formatting). No commit amended or pushed; `batma/` remains untouched.

Fresh uninstalled candidates were generated from `455e88f`:

- Trusted routing: `C:\dev\helenhook\output\batman-direct-graphics\candidate-c818dbb877da42ca8a705acca365a5d0`; source/candidate DLL SHA-256 `B0FA2BDFBF1E8DC48DFE55966086FBA71A7E503B2A647BD5A66ACFBD6DC7924E`.
- Routing plus no-save probe: `C:\dev\helenhook\output\batman-direct-graphics\candidate-4699beb4b778468ca5030d6ae673fecc`; source/candidate DLL SHA-256 `1920EF2081956D236CBB8C195FEF6F8892A7B60E81FF03D140D8756D09A53FE0`; generated source SHA-256 `CD533AFDB711CCF06A8FA76C90C7EC8F38B564B869030457372A247C9E0134E1`.
- No-route control: `C:\dev\helenhook\output\batman-direct-graphics\candidate-aac36145d0ba4c03ad0f7f25d5ab28b1`.

No candidate was installed or launched. Remaining concerns are the pre-existing `Hook.cpp` C4244 warning, FFDec's profile-lock warning, and `liveTestPending: true`; no real user INI was written.
