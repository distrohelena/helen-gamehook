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

The regression assertions were written before the production edits. The pre-fix native test rebuild attempt was interrupted by the workspace MSBuild wrapper after partial compilation; the direct full-suite executable therefore was not available for a behavioral pre-fix run. This deviation is disclosed rather than represented as a passing RED run.

Direct v143 Win32 compilation checks after the edits succeeded with no diagnostics:

```text
FileApiHookSet.cpp
FileWriteRoutingService.cpp
FileWriteRoutingHookFixture.cpp
FileWriteRoutingServiceTests.cpp
BatmanDirectGraphicsPackageTests.cpp
```

The normal MSBuild invocation is currently blocked in this environment by the inherited duplicate `Path`/`PATH` environment entries (`MSB6001`, `Item has already been added`); the `rtk proxy` child returned before the later build output. A fresh full native build and behavioral GREEN run remain required from the controller using its isolated build wrapper.

## Files changed

- `HelenRuntime/FileWriteRoutingService.cpp`
- `HelenRuntime/FileApiHookSet.cpp`
- `tests/HelenRuntime.Tests/FileWriteRoutingServiceTests.cpp`
- `tests/HelenRuntime.Tests/FileWriteRoutingHookFixture.cpp`
- `tests/HelenRuntime.Tests/BatmanDirectGraphicsPackageTests.cpp`
- `games/HelenBatmanAA/experiments/file-routing/README.md`

No candidate artifacts were generated or installed in this wave. The controller must rebuild and regenerate candidates after committing source changes.
