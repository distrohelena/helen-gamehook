# Owned memory patch failure contract

Approved scope: the user's 2026-09-08 “yes” authorizes extending the shared
patch writer and installer for partial failures and failure-injection tests.
This is an architectural amendment to the VSync binding gates, not permission
to install a gameplay experiment. Work stays inline on main, uncommitted.

## Contract

An owned MemoryPatch captures original bytes and each committed memory region's
protection before changing anything. All storage allocation happens before the
first protection change. Invalid, guarded, inaccessible, overflowing or empty
ranges are rejected. Callers must provide valid readable source storage and
exclusive access to stable target mappings; this is not thread suspension,
instruction relocation, SEH recovery, or an atomic multi-byte write.

Apply reports write-access, instruction-cache flush and protection-restoration
errors separately. A failed operation may still own changed bytes or protection.
Restore retains ownership until original bytes, instruction-cache visibility,
and every original protection are restored. Explicit retry is supported; there
are no timers or background retries. Regions with different protections retain
their distinct protections. Destruction with unresolved restoration terminates
the process with a diagnostic, without raising a modal error dialog.

InlineHook and IatHook gain TryInstall/TryRemove paths for callers that retain
their hook and detour dependencies after failure. Existing Install/Remove callers
cannot express that obligation: failed Install performs explicit rollback before
returning false, and Remove either restores safely or terminates. IsInstalled is
conservative ownership, not a claim that a partial installation is usable.
Trampoline storage is released only after restoration. Destructors enforce the
same invariant. Trampolines still require relocation-safe original instructions.

Legacy WriteMemory/FillMemoryBytes use the owned transaction and commit on full
success; failure returns false only after rollback succeeds. Irrecoverable
rollback terminates rather than allowing callers to free live detour state.
No silent success and no silently leaked ownership are acceptable alternatives.

## Verification

Compile the actual Memory/Hook sources in an isolated x86 console fixture. A
test-only forced include intercepts only Win32 protection/cache calls, forwarding
all non-injected calls to Windows. Use real VirtualAlloc pages, real byte writes,
real hooks and PE imports. Assert bytes, protection, ownership, callable restored
code, explicit retry, and child-process termination. Include distinct page
protections, pre-write refusal, flush failure, restore failure, combined failure,
and healthy install/remove. Run the full native suite against a fresh library.
Installed Batman DLL hashes must remain unchanged.
