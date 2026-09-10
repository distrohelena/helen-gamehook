# Batman Session Live Options Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans inline, honoring the user's main-branch preference. Do not spawn agents or commit without permission.

**Goal:** Repeatable, verified, session-only Apply for the existing Batman graphics menu, with explicit unsupported-live capabilities.

**Architecture:** Separate the editor/session protocol from the persistence backend. Add a session-only backend that owns applied state, overlay staging, native application and failure lockout; retain the existing persistence backend for other callers. Use the verified stock reload/Apply and display paths, with per-setting capability evidence.

**Tech Stack:** C++20, Win32/x86, pinned UE3 executable bindings, Direct3D9, existing ActionScript builder and PowerShell fixtures.

**Spec:** `docs/superpowers/specs/2026-09-09-batman-session-live-options-design.md`

## Global Constraints

- Work inline on main; preserve unrelated `batma/` and `__pycache__/` files.
- No polling, timer-driven discovery, automated game launches or process termination.
- Original-file preservation remains mandatory even on failure.
- Session-only success is distinct from persistent Committed success.
- Use console-only tool wrapper; fresh output directories and matching runtime/proxy binaries.
- No commits or package promotion are implied by implementation approval; commit when the user requests it.
- One class per file, substantive Doxygen, explicit required ownership, no silent fallback/repair writes.

## Task 1: Session backend contract and truthful success

**Files:** create `include/HelenHook/BatmanGraphicsSessionBackend.h`; modify `BatmanGraphicsConfigService.h/.cpp`, `BatmanGraphicsSessionService.h/.cpp`, `BatmanGraphicsApplyOutcome.h`; create standalone fixtures under `games/HelenBatmanAA/experiments/session-live/`.

**Interface:** `BatmanGraphicsSessionBackend` exposes virtual `CaptureReadSnapshot() const`, `IsApplyLocked() const noexcept`, and `ApplySessionDraft(const BatmanGraphicsDraftState& baseline, const BatmanGraphicsDraftState& draft) const`. Existing config implements it by forwarding to its unchanged persistence method. Session service takes a required backend reference.

- [x] Add `SessionApplied = 5`, preserving existing numeric outcomes.
- [x] Write a controlled backend fixture driving the real session API. Essential assertions:
  ```cpp
  Expect(result->Outcome == BatmanGraphicsApplyOutcome::SessionApplied, "Session success lost");
  Expect(nextBaseline.Get(BatmanGraphicsField::Bloom) == 0, "Repeat Apply used stale baseline");
  Expect(reopenedBloom == 0, "Reopen lost applied session state");
  ```
  Also exercise rejection, consumed transaction, stale editor, lockout on reopen, and reentrant BeginApply.
- [x] Run the standalone fixture before accepting SessionApplied in session baseline publication; observe the stale-baseline failure.
- [x] Accept SessionApplied when updating editor baseline/snapshot. Keep persistent outcomes unchanged; backend owns cross-editor applied state.
- [x] Run fixture again and existing persistence session tests. Do not switch the installed runtime backend yet.

## Task 2: Capability map and complete session overlay staging

**Files:** new `SessionGraphicsDelta.h/.cpp`, `SessionGraphicsOverlay.h/.cpp`, and tests under `experiments/session-live`; reuse existing normalized fields and file-route identity validation.

**Interfaces:** `SessionGraphicsDelta::Validate(baseline, draft, capabilities)` rejects unsupported changed fields before mutation. `SessionGraphicsOverlay::Stage(original, overlay, draft, delta)` constructs and verifies a complete session-only update, without calling the original-file writer.

- [ ] Trace C20130 key mappings and each consumer/reset requirement for MotionBlur, Distortion, FogVolumes, lighting, AO and MSAA. Record exact addresses and expected encodings in the experiment README. Trace PhysXLevel and Stereo initialization separately.
- [ ] Derive capabilities from verified bindings/device state; reject unsupported transitions without staging. Do not invent PhysX or stereo application callbacks.
- [ ] Test mixed Boolean deltas, lighting inversion, normalized MSAA, no-op, unsupported PhysX/stereo and invalid route identity.
- [ ] Test real ANSI/UTF-16 overlays and injected staging failure; check original bytes unchanged and unrelated keys preserved.
- [ ] Implement using the existing INI encoding rules; publish only complete target bytes and verify disk contents independently.

## Task 3: Owned session-only native coordinator

**Files:** new `SessionGraphicsBackend.h/.cpp`, `SessionGraphicsEngine.h/.cpp` and tests under `experiments/session-live`; reuse/refactor Bloom reload and no-save display bindings without weakening their guards.

**Interface:** `SessionGraphicsBackend` implements Task 1's backend, owns a verified applied snapshot and health state, and coordinates staging/native application. The engine binding exposes preflight, apply and readback stages; tests supply a controlled engine boundary, never pretend to test retail GPU calls.

- [ ] Test two successive reverse changes, no-op with zero engine/reset calls, reentry rejection and cross-editor snapshot preservation.
- [ ] Test preflight rejection before any write and failures after each mutation stage. Ambiguous state locks future Apply; requested draft is never published as success.
- [ ] Initialize ownership once before callbacks publish. Replace one-shot Attempted only for the new owned backend; leave historical experiment reproducibility intact.
- [ ] Stage effects, reload cache and run stock Apply(save=false), retaining the complete live payload and unrelated-field checks. Coordinate display refresh after determining stock Apply's reset effects; avoid duplicate resets.
- [ ] Require observed cache/engine/render/device values to match every requested supported change. Return SessionApplied only after full verification.

## Task 4: Menu and runtime integration

**Files:** `HelenGameHook/BatmanGraphicsRuntimeContext.h/.cpp`, opt-in candidate targets/build script, `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellScriptTemplates.cs` and its executable builder tests.

- [ ] Extend menu result handling to recognize outcome 5 as session success, clearing dirty state without saved wording:
  ```actionscript
  if(outcome === 0 || outcome === 3 || outcome === 5) { /* publish verified UI baseline */ }
  ```
- [ ] Add explicit per-field unsupported-live reasons; reject mixed deltas containing them before staging. Do not label restart-required session edits as persisted.
- [ ] Test emitted/consumed primitive protocol behavior through existing harnesses, not source-text-only assertions.
- [ ] Bind the session backend only for the candidate's graphics context. Do not alter subtitle persistence or legacy trusted saves.

## Task 5: Verification, installation and live acceptance

- [ ] Run all new fixtures plus existing no-save, startup and persistence fixtures against fresh artifacts.
- [ ] Build with `/MP4`, `/m:1`, `/nodeReuse:false`, the console wrapper, and a new output root; verify matching proxy/runtime hashes.
- [ ] Rebuild the menu package from current source, validate provenance and file references; never reuse an old generated package as proof.
- [ ] With Batman/launcher closed, back up binaries and affected pack assets, capture INIs, install and verify hashes plus unchanged original INIs.
- [ ] Request live tests only after automated verification: mixed effects, reverse Apply, reopen, display-only and mixed display/effects. Record exact per-option acceptance and unsupported capabilities.
- [ ] If PhysX/stereo requires world recreation, library unload, persistent restart carryover or another unapproved expansion, report the evidence and stop that workstream for user direction. Continue independent supported-setting work.

## Progress

Task 1 completed; the full normal runtime suite passed from fresh build
`output/session-normal-tests-20260909-a`. Its first run exposed a test-output
layout mismatch; placing the executable under native/tests beside the expected
parent DLL resolved it, and the build harness now preserves that layout.

Task 2 investigation extracted all Boolean field mappings and exposed an old
Bloom binding error (AO's offset/mirror had been used). The corrected binding has
a red/green regression test. Corrected live Bloom acceptance subsequently passed:
live/cache 0 -> 1, unrelated payload preserved, all 31 original INIs unchanged.
PhysXLevel controls loader selection and physics initialization; safe live
reinitialization remains unverified.

Tasks 2–4 now have implementation and passing CPU/file/protocol fixtures.
SessionGraphicsNative binds all ordinary renderer/display fields, with PhysX and
Stereo explicitly unsupported. SessionGraphicsBackend owns repeatable applied
state and failure locks. The menu consumes outcome 5 and supported-field mask 17.
Overlay encoding reuses the proven profile API's ANSI/UTF-16 rules rather than
extracting the persistent writer's private parser. Publication gained a generic
routing-level compare-and-replace API so stale staging cannot overwrite newer
session bytes. The original writer is never invoked by this backend.

Task 5 automated verification passes for the fresh native build and newly rebuilt
menu package. The candidate was installed with closed processes, verified backups,
matching binary/pack hashes, all 31 original INIs unchanged and unrelated packs
preserved. Native live acceptance remains pending. Exact artifacts, warnings,
the intentionally incompatible historical one-shot fixture, and per-field
evidence are recorded in `games/HelenBatmanAA/experiments/session-live/README.md`.
