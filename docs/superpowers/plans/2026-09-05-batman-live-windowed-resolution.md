# Batman Live Windowed Resolution Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Execute inline on main; do not launch agents or create a worktree.

**Goal:** Establish a verified, engine-owned windowed resize boundary before implementing live Apply.

**Architecture:** First produce reproducible read-only executable evidence for viewport ownership, resize ordering, and thread safety. Only after that checkpoint can a concrete controlled-probe implementation be planned; production transaction integration follows a successful live proof. Do not fabricate an ABI to make the downstream plan appear executable.

**Tech Stack:** Windows x86 PE, PowerShell, Python 3.11 with pefile and Capstone for binary analysis; existing C++/MSVC runtime and generated ActionScript remain unchanged in this phase.

**Spec:** `docs/superpowers/specs/2026-09-05-batman-live-windowed-resolution-design.md`

## Global Constraints

- Arrows edit the draft; Apply Changes performs the resize.
- No confirmation countdown, polling, retry timers, or timeout-based success decisions.
- Runtime bindings use the loaded module base plus verified RVAs, never persisted process addresses.
- Outer window dimensions do not count as resolution.
- Do not assume the ExternalInterface callback is a safe resize boundary.
- Do not use installed packs, archives, `batma/`, or historical generated assets as build inputs.
- Execute inline on main, preserve unrelated dirty files, and do not launch an independent review without Helena's approval.
- Follow AGENTS.md: focused files, substantive Doxygen comments, explicit types, no tuples, no hidden fallbacks.
- Use apply_patch for edits. Do not install, attach a mutating debugger, or invoke engine functions during this phase.

## Why this plan has a discovery checkpoint

The approved spec requires a verified engine path before a controlled resize. Static leads do not yet establish a callable high-level function or its owner. This plan executes that prerequisite, not the entire feature. The probe and production implementation require a follow-up plan using the resulting exact ABI and lifetime evidence. A negative result is a valid discovery outcome, not permission to bypass the engine or claim live resizing works.

## File map

- Create `games/HelenBatmanAA/experiments/windowed-resolution/Inspect-Viewport.py`: read-only PE identity verification and structured disassembly of selected instruction ranges. No process access or runtime binding code.
- Create `games/HelenBatmanAA/experiments/windowed-resolution/Test-InspectViewport.py`: unittest coverage of identity rejection, address conversion, and bounded disassembly.
- Create `docs/superpowers/plans/2026-09-05-batman-windowed-resolution-evidence.md`: verified addresses, call graph, ownership/thread findings, unresolved claims, and gate outcome.
- Update this plan's checkboxes and evidence link as work completes.

Read, but do not modify, `HelenGameHook/BatmanGraphicsRuntime.cpp`, `HelenGameHook/BatmanGraphicsRuntimeContext.cpp`, `HelenGameHook/BatmanGraphicsExternalInterface.cpp`, `HelenRuntime/D3d9TextureReplacementHookSet.cpp`, `HelenRuntime/WindowBehaviorHookSet.cpp`, and `games/HelenBatmanAA/experiments/external-interface-probe/Inspect-Dispatch.ps1`.

## Task 1: Reproducible read-only executable inspector

**Interfaces:** `VerifyExecutable(Data: bytes) -> None` raises ValueError for wrong length or digest. `ReadVirtualBytes(Image, Address: int, Length: int) -> bytes` converts preferred VA through PE sections and rejects unmapped/truncated ranges. Command line: `--executable PATH --address INTEGER --length INTEGER`; integers accept a 0x prefix. Output contains identity, preferred VA, RVA, file offset, raw instruction bytes, and decoded instructions. Addresses are static evidence only.

- [x] Capture current `git status --short` and installed executable SHA256 without changing either. Record current commit and preserve all existing dirty paths.

```powershell
git status --short
git log -1 --oneline
Get-FileHash -LiteralPath 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe' -Algorithm SHA256
```

- [x] Write unittest cases before implementing the inspector. Load the hyphenated module using importlib.util.spec_from_file_location. Use a fake PE object with ImageBase 0x400000 and one bounded raw section. Required assertions:

```python
with self.assertRaises(ValueError):
    inspector.VerifyExecutable(b'not a PE')
with self.assertRaises(ValueError):
    inspector.VerifyExecutable(bytes(38758728))
self.assertEqual(inspector.ReadVirtualBytes(self.Image, 0x401000, 3), b'\x90\x90\xc3')
with self.assertRaises(ValueError):
    inspector.ReadVirtualBytes(self.Image, 0x3fffff, 1)
with self.assertRaises(ValueError):
    inspector.ReadVirtualBytes(self.Image, 0x401000, 0)
with self.assertRaises(ValueError):
    inspector.ReadVirtualBytes(self.Image, 0x401000, 4096)
```

- [x] Run `python -B games/HelenBatmanAA/experiments/windowed-resolution/Test-InspectViewport.py -v`; confirm the missing inspector is the initial failure, not an unrelated environment error. Correction: unittest discovery skips this hyphenated filename and reported zero tests; invoke the file directly and require a nonzero test count.
- [x] Implement identity verification with hashlib.sha256, exact length 38758728, digest `4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028`. Parse pefile.PE with fast_load=True. Validate requested bounds against raw section data, not just total file length. Decode with capstone.Cs(CS_ARCH_X86, CS_MODE_32). Read the input as binary; never write to it. Fail visibly on wrong identity, invalid addresses, incomplete decoding, or missing dependencies. Add substantive module/function docstrings describing read-only constraints.
- [x] Re-run unittest and require all cases passing. Exercise the real executable at the known function boundary:

```powershell
python -B games/HelenBatmanAA/experiments/windowed-resolution/Inspect-Viewport.py --executable 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe' --address 0xEAA0A0 --length 0x53
```

- [x] Verify output ends with the complete ret 0x10 instruction at preferred VA 0xEAA0F0. The return occupies three bytes, hence corrected length 0x53. A successful inspection is not proof of safe runtime invocation.
- [ ] Stage only the new inspector, tests, and evidence document after `git diff --check`; commit `test: add read-only Batman viewport inspection` after recording actual outputs.

## Task 2: Establish the engine path and ownership contract

**Consumes:** Inspector from Task 1; known preferred VAs 0xEAA0A0 (candidate renderer update), 0xEA6310 (callee), 0x21282E8 (candidate vtable slot), 0x715880 (thread-check lead), 0x11F67F7 (wxWidgets setres formatter).

**Produces:** Evidence document with a table for each verified function: preferred VA, RVA, file offset, pinned instruction bytes, calling convention, parameter evidence, ownership source, thread requirements, and callers. Inferences must be labeled separately from observations.

- [x] Reproduce the candidate update's game-thread assertion, dimension writes, and callee using instruction-aligned disassembly. Record raw bytes alongside interpretations.
- [x] Inspect the apparent vtable containing 0x21282E8, find its constructor/installation references, and trace virtual callers of that slot. Do not treat a global pointer occurrence as ownership proof.
- [ ] Trace higher-level callers until the path accounts for render-resource synchronization, window/client sizing, viewport dimensions, and input/UI dimensions. Record each edge with its actual call instruction and owner derivation.
- [ ] Trace creation/destruction and replacement of the active viewport owner. Identify a stable lifecycle acquisition boundary and the invalidation mechanism; explicitly reject retaining a borrowed pointer after destruction.
- [ ] Trace the thread-check lead and compare the resize callers with the existing ExternalInterface dispatch path. Establish whether a synchronous call from that path is safe, including reset-driven message reentrancy and graphics transaction guards.
- [ ] Trace configuration-related calls reachable from the proposed resize entry. Identify whether it can mutate INIs internally. Static evidence alone cannot prove all file effects: list the exact file hashes the controlled proof must compare.
- [ ] Document authoritative sources for actual fullscreen state, client width/height, and backbuffer/rendered width/height. Do not substitute saved INI values or outer window bounds.
- [x] Record a gate decision: insufficient evidence to invoke. See `2026-09-05-batman-windowed-resolution-evidence.md` for the settings-serialization branch and remaining ownership/thread checks. Discovery is partial; no controlled resize is authorized by these findings alone.
- [ ] Self-review every proposed callable against identity, ABI, owner lifetime, thread, synchronization, and persistence evidence. Run inspector tests again and commit only the evidence/plan updates as `docs: record Batman viewport binding evidence`.

## Task 3: Review checkpoint and downstream plan

- [ ] Report the actual engine-path finding concisely, with links to evidence. Explicitly state that the installed pack is unchanged and live resizing is not yet verified.
- [ ] If the synchronous safe boundary cannot be established, stop production integration. If deferred completion is required, propose the spec's engine-event extension without polling.
- [ ] If established, use writing-plans to create the concrete controlled-probe plan using the now-verified signatures and ownership. That plan must include executable byte rejection tests, safe owner invalidation, thread checks, no INI mutation, before/after client and backbuffer capture, two sizes and restoration, and a uniquely built rollback-safe test candidate.
- [ ] After a successful controlled live proof, create the production integration plan. Its mandatory coverage is listed below; do not execute it from this discovery document.

## Production follow-up coverage ledger

This is scope carried forward, not executable implementation tasks with guessed interfaces:

- Engine binding and live service: lifecycle validation, authoritative actual state, safe synchronous resize, both dimension checks, missing binding isolated from unrelated menu reads.
- Session coordination: live-first ordering under the existing operation guard, unchanged-size handling, exact pair revalidation, actual-versus-saved mode, no disk publication after failed resize.
- Recovery: original state verification, one safe compensation after recoverable publication failure, no reset on unknown/device-lost state, disk uncertainty precedence, process-wide live latch surviving session recreation.
- Protocol: CommitV1 compatibility; CommitV2 outcomes 0 through 4; shared consumed transaction identity; no V1 fallback; V1 cannot bypass integrity latches.
- Frontend: local arrows, clean success, editable recoverable error, locked uncertain outcomes, generated and decompiled behavioral tests.
- Packaging: fresh source and verified retail base, native and ABI tests, exact delta reconstruction, durable rollback, Batman closed, unchanged subtitles/proxy/config, no skip-video installation.
- User acceptance: two sizes both directions, measured rendered/client dimensions, aligned input/UI, reopen/restart persistence, unrelated settings preserved. Separate automated and user-confirmed evidence.

## Plan self-review

The discovery and controlled-proof gate is covered by Tasks 1-3. Production requirements are intentionally deferred to an evidence-driven follow-up plan, not claimed complete. No callable engine ABI is invented. This plan contains no runtime modifications or deployment actions.
