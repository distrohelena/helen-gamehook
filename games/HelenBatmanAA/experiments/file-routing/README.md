# Batman session file-routing candidates

Task 5 produced two fresh, uninstalled routing candidates from committed source `e028c5d`.
Both use the verified retail `Frontend.umap` (`271916B888F83374122AF0FCCC5C685804F4C8286A92A772CD71E4F48A00F2CC`) and the read-only user input `C:\Users\Helena\Documents\Square Enix\Batman Arkham Asylum GOTY\BmGame\Config\BmEngine.ini`.

The exact route in each candidate is:

```json
{"id":"engine-config","root":"documents","path":"Square Enix/Batman Arkham Asylum GOTY/BmGame/Config/BmEngine.ini","writePolicy":"redirect","readPolicy":"redirected","lifetime":"session"}
```

## Candidate artifacts

| Candidate | Path | Native DLL SHA-256 | Runtime library SHA-256 | Mode |
| --- | --- | --- | --- | --- |
| Trusted-save routing | `C:\dev\helenhook\output\batman-direct-graphics\candidate-3cb422e04ba54825bd303579176f7fdb` | `D6C238135AF11D2D0499B1BDB3DF2A79AC6DE03C39B2C0752FF01F87AB004C96` | `6F4D9A2511B18EC431E61C7BBEFC8125F69A50C7856A9EDAC7777789BAC22461` | `TrustedSaveRouting` |
| Routing plus no-save probe | `C:\dev\helenhook\output\batman-direct-graphics\candidate-d3902b5299124415851eeff2ee9f52f8` | `EB69DABC8B85CA9CA8D7C157E502D4A6EA3372A4ABBDE7A7541E493FCF24802E` | `2F267303633DCC9850B5B696731CC72909B926B16058B97AE43DB69340882A3D` | `RoutingNoSaveProbe` |

The throwaway no-save generated source is `C:\dev\helenhook\output\batman-file-routing-native-20260906-nosave-a\BatmanGraphicsSessionService.cpp`, SHA-256 `CD533AFDB711CCF06A8FA76C90C7EC8F38B564B869030457372A247C9E0134E1`. Its source substitution is only `Config.ApplyDraft(attempted.Draft)` → `NoSaveResolutionProbe::Apply(attempted.Draft)` and is not part of normal source. The no-route control candidate is retained at `C:\dev\helenhook\output\batman-direct-graphics\candidate-d6fd65b36d564cc79f01fde5cc39a7b5`.

## Rebuild commands

Fresh native output uses `FreshBuildIsolation.targets`, which gives each project `native\` output and `obj\<project>\` intermediates. The normal build used:

```powershell
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' 'HelenGameHook\HelenGameHook.vcxproj' /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:FreshOutput=C:\dev\helenhook\output\batman-file-routing-native-20260906-normal-e /p:ForceImportBeforeCppTargets=C:\dev\helenhook\games\HelenBatmanAA\experiments\file-routing\FreshBuildIsolation.targets /m:1 /nodeReuse:false /v:minimal; exit `$LASTEXITCODE"
```

The no-save experiment used the existing generator and target:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File games/HelenBatmanAA/experiments/windowed-resolution/Prepare-NoSaveProbe.ps1 -OutputRoot C:/dev/helenhook/output/batman-file-routing-native-20260906-nosave-a
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' 'HelenGameHook\HelenGameHook.vcxproj' /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:ForceImportBeforeCppTargets=C:\dev\helenhook\games\HelenBatmanAA\experiments\windowed-resolution\NoSaveProbe.targets /p:NoSaveOutput=C:\dev\helenhook\output\batman-file-routing-native-20260906-nosave-a /m:1 /nodeReuse:false /v:minimal; exit `$LASTEXITCODE"
```

The candidate builder requires the native DLL, runtime library, and user INI explicitly. For example:

```powershell
rtk proxy powershell.exe -NoProfile -ExecutionPolicy Bypass -File games/HelenBatmanAA/scripts/Build-BatmanDirectGraphicsCandidate.ps1 -NativeDllPath output/batman-file-routing-native-20260906-normal-e/native/HelenGameHook.dll -RuntimeLibraryPath output/batman-file-routing-native-20260906-normal-e/native/HelenRuntime.lib -BatmanUserIniPath 'C:\Users\Helena\Documents\Square Enix\Batman Arkham Asylum GOTY\BmGame\Config\BmEngine.ini' -EnableFileWriteRouting -CandidateMode TrustedSaveRouting -NativeOutputRoot output/batman-file-routing-native-20260906-normal-e -SourceCommit e028c5d
rtk proxy powershell.exe -NoProfile -ExecutionPolicy Bypass -File games/HelenBatmanAA/scripts/Build-BatmanDirectGraphicsCandidate.ps1 -NativeDllPath output/batman-file-routing-native-20260906-nosave-a/native/HelenGameHook.dll -RuntimeLibraryPath output/batman-file-routing-native-20260906-nosave-a/native/HelenRuntime.lib -BatmanUserIniPath 'C:\Users\Helena\Documents\Square Enix\Batman Arkham Asylum GOTY\BmGame\Config\BmEngine.ini' -EnableFileWriteRouting -CandidateMode RoutingNoSaveProbe -NativeOutputRoot output/batman-file-routing-native-20260906-nosave-a -GeneratedSourcePath output/batman-file-routing-native-20260906-nosave-a/BatmanGraphicsSessionService.cpp -SourceCommit e028c5d
```

Omitting `-EnableFileWriteRouting` preserves the normal no-route behavior. Do not pass the installed package, `batma/`, an old native output, or a generated frontend as an input.

## Verification coverage

- The native verifier loads the real pack parser and delta reader, checks the exact eight pack files, exact route fields (or zero routes for no-route mode), direct hook/export contract, current-run delta bytes, and the expected fresh DLL SHA-256.
- `Test-BatmanDirectGraphicsPackageRejections.ps1` passes the five existing no-route rejection cases (`duplicate-hook`, `missing-export`, `wrong-signature`, `stale-target`, `legacy-command`) plus routing `malformed-route`, `duplicate-route`, `wrong-target-route`, and `stale-dll` cases. All are real parser/verifier failures, not text-presence checks.
- Both candidates passed emitted and FFDec-decompiled frontend behavior, package parsing, and delta reconstruction. FFDec repeatedly emitted a sandbox profile-lock warning (`AccessDeniedException` for `JPEXS\\FFDec\\logs\\log.txt.lck`); the external-SWF warning may also occur. These warnings are recorded rather than hidden; export completed and the behavior checks passed.
- The fresh Release Win32 native suite at `C:\dev\helenhook\output\batman-file-routing-tests-20260906` passed with `PASS`, `GENERATED_BATMAN_PROTOCOL_PASS`, and `FILE_ROUTING_HOOK_CHILD_PASS` markers. The suite reports markers, not individual case counts. The existing export check expects the DLL one directory above the test executable, so the already-fresh DLL was copied within this isolated output root before execution.
- The default runner completed 42 top-level `Run...` suite/entrypoint calls, including 4 child-launcher entrypoints. This is a suite/entrypoint count, not an individual assertion or test-case count.
- The existing no-save session test passed: `NO_SAVE_SESSION_PASS: real Commit bypasses writer; foreign executable rejected; both INIs unchanged`.

### Batman import coverage

The pinned retail `ShippingPC-BmGame.exe` imports these relevant kernel32 APIs: `CreateFileA/W`, `WriteFile`, `ReadFile`, `SetFilePointer`, `GetFileSizeEx`, `GetFileSize`, `GetFileAttributesW`, `CreateFileMappingA`, `CloseHandle`, `MoveFileExW`, `DeleteFileW`, `FlushFileBuffers`, `CopyFileW`, `SetFileAttributesW`, `CreateDirectoryW`, `RemoveDirectoryA/W`, `FindFirstFileA/W`, `FindNextFileA/W`, `FindClose`, `GetCurrentDirectoryW`, and `SetCurrentDirectoryW`. The real child fixture exercises imported `CreateFileA/W`, `WriteFile`, `ReadFile`, `SetFilePointer`, `GetFileSizeEx`, `GetFileAttributesW`, `CreateFileMappingA`, `CloseHandle`, `MoveFileExW`, `DeleteFileW`, `FlushFileBuffers`, `CopyFileW`, `SetFileAttributesW`, `CreateDirectoryW`, `RemoveDirectoryA/W`, `FindFirst/FindNext/FindClose`, plus the CWD normalization path through `GetCurrentDirectoryW`/`SetCurrentDirectoryW`. The fixture also covers protected alias mutation and fail-closed pre-activation handle cases through production routing.

`SetFilePointerEx`, `GetFileAttributesA`, `CreateFileMappingW`, `ReplaceFileW`, `GetPrivateProfileStringW`, and `WritePrivateProfileStringW` are absent from the pinned retail import table; they are not claimed as Batman coverage. Dynamically resolved APIs, native syscalls, other processes, and unhooked modules remain outside the cooperative IAT boundary.

## Non-goals and scratch/rollback policy

Neither candidate was installed or launched. The installed working package and the real user INI were left untouched. The candidates, native outputs, generated frontend assets, and no-save source are physical scratch artifacts; logical session discard must not be confused with deleting arbitrary cache content. Keep them for provenance until acceptance is complete. If a candidate is rejected, remove it from any test selection and restore the previously installed package; no candidate deployment is required for rollback.

The no-save candidate intentionally exercises automatic engine resize writes only and returns `NotApplied`/`ApplyFailed` for explicit saves. It cannot establish trusted-save persistence. The cooperative IAT boundary still excludes dynamically resolved APIs, native syscalls, other processes, and unhooked imports.

## Live acceptance steps (pending)

When a controlled, user-approved Batman session is available, test the trusted-save candidate first: automatic engine resize writes must affect only the redirected overlay; an explicit Helen save must persist both the original and active overlay; and restart must ignore abandoned scratch. Repeat the automatic-resize-only observation with the separately labeled no-save candidate. Compare the original user INI and session overlay before/after each action, retain logs, and mark acceptance only after the controlled session. `liveTestPending` remains `true` in both provenance files.

## Review-fix regeneration

The latest regenerated candidates are `candidate-3cb422e04ba54825bd303579176f7fdb` (trusted routing), `candidate-d3902b5299124415851eeff2ee9f52f8` (routing plus no-save), and `candidate-2153059d589f49e4be82dab9511abde4` (no-route control). Their provenance records source commit `e028c5d`, the source hash before copy, and a matching `candidateDllSha256`; routing verification now requires the source hash explicitly.

Invalid modes fail before creating a candidate tree. For example, omitting `-GeneratedSourcePath` from `RoutingNoSaveProbe` returns `RoutingNoSaveProbe requires -GeneratedSourcePath`, and supplying `-GeneratedSourcePath` to `TrustedSaveRouting` returns `-GeneratedSourcePath is valid only for RoutingNoSaveProbe`. Calling package verification with `-ExpectedRouteMode engine-config` but no `-ExpectedNativeDllSha256` returns `Routing verification requires the fresh source DLL SHA-256 explicitly` before compiling the verifier.

The final corrected no-route control and routing rejection run were:

```powershell
rtk proxy powershell.exe -NoProfile -ExecutionPolicy Bypass -File games/HelenBatmanAA/scripts/Test-BatmanDirectGraphicsPackageRejections.ps1 -CandidateRoot output/batman-direct-graphics/candidate-2153059d589f49e4be82dab9511abde4 -RuntimeLibraryPath output/batman-file-routing-native-20260906-normal-e/native/HelenRuntime.lib -RoutingCandidateRoot output/batman-direct-graphics/candidate-3cb422e04ba54825bd303579176f7fdb
```

It returned nine `REJECTED_AS_EXPECTED` markers (five existing plus four routing) and `BATMAN_DIRECT_PACKAGE_REJECTIONS_PASS`. The earlier `candidate-d6fd...` path above is a retained historical no-route control; `candidate-215...` is the final control regenerated from `e028c5d`.

The verifier utility still performs manual BCrypt handle cleanup on its error paths; this is a test-only verifier concern, not production routing code. Live acceptance remains pending and no candidate was installed or launched.
