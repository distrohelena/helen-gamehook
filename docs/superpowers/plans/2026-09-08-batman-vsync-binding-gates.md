# Batman VSync Binding Gates Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish or reject the engine-call and machine-code contracts required by the approved VSync refresh design before building a live candidate.

**Architecture:** Read the pinned executable, trace the complete same-size refresh and settings-apply call chains, then exercise the proposed decision bridge in an isolated x86 process. No Batman mutation or deployment is part of this gate phase. A failed gate revises the design rather than authorizing a workaround.

**Tech Stack:** Windows x86, MSVC v143, PowerShell console-tool wrapper, Python pefile/capstone, existing Helen inline-hook infrastructure.

**Spec:** `docs/superpowers/specs/2026-09-08-batman-vsync-refresh-design.md`

## Global constraints

### Execution checkpoint (2026-09-08, inline on main)

- [x] Verify the pinned executable hash and decode the decision bytes directly.
- [x] Resolve the relevant viewport/renderer virtual slots and establish the
  conditional synchronous same-size route; record thread synchronization evidence.
- [x] Evaluate the settings-preservation gate: **rejected**. Configuration reload
  followed by conditional DirectionalLightmaps inversion can change an unrelated
  live field even with save=false. See the report's binding-gates checkpoint.
- [x] Stop before Task 3 as required by the failed Task 2 gate.
- [x] Verify the installed DLL hash remains the expected DB757D38 candidate.

The detailed unchecked items below remain outstanding where their complete
contracts were not established. In particular, full settings ownership/transitive
writer certification, request ordering, patch-span validation and executable
bridge tests are not complete. No production follow-on plan is authorized by
these results; revise the settings-mutation design first. Only documentation was
changed, and no commit or deployment was performed.

- Work remains on main, based on 9dc4358.
- No other graphics settings, repeated-Apply feature or saved-success UI is included.
- No exception may escape the machine-code bridge.
- Do not call an interior renderer address as a function.
- Do not add automatic rollback resets in this slice.
- No automated desktop fullscreen switch or live Batman mutation is required for the development checkpoint.
- Installation requires a later user request; preserve the installed DB757D38 candidate.
- Do not start a fresh independent review session without user approval.
- One class per file, substantive Doxygen comments, no local helper functions, RAII ownership.

## Why this plan stops at the binding gates

The approved spec explicitly requires revision if the refresh is asynchronous or
settings ownership/save behavior conflicts with the design. These facts are not
established. Writing production offsets, thread handoffs or snapshot-copy code now
would invent the contract this phase must determine. This phase delivers executable
bridge evidence and a complete binding dossier; production request-service and
menu integration receive a follow-on implementation plan only after these gates
pass. It does not claim to implement the entire live feature.

## Files and responsibilities

- Existing `games/HelenBatmanAA/experiments/windowed-resolution/Inspect-Viewport.py`:
  pinned executable verification and exact-boundary static decoding.
- Existing `tests/Invoke-D3d9ReplacementResetTests.ps1`: console-only MSVC invocation
  pattern; do not change its behavior for this gate.
- Create `games/HelenBatmanAA/experiments/vsync-refresh/DecisionBridgeTests.cpp`:
  isolated x86 instruction/ABI fixture, never injected into Batman.
- Create `games/HelenBatmanAA/experiments/vsync-refresh/Test-DecisionBridge.ps1`:
  compile/run fixture with process error dialogs suppressed and unique output.
- Update `docs/superpowers/reports/2026-09-08-vsync-static-investigation.md`:
  provenance, call chains, ownership, side-effect and ABI findings.
- Update this plan with completed checks, actual commands and results.

## Task 1: Establish synchronous same-size refresh reachability

**Consumes:** Pinned retail executable and the existing read-only decoder.
**Produces:** An address-by-address call-chain record with argument mapping,
thread checks, early-return conditions and request-arm/disarm boundaries.

- [ ] Verify the executable before interpreting any address:

```powershell
Get-FileHash 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe'
```

Expected SHA-256: `4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028`.

- [ ] Trace the complete 0x00EB91D0 viewport call, its 0x00EB7250 helper and
  virtual destinations through renderer 0x00EA6310. Resolve virtual slots from
  the pinned image's actual tables; retain each caller's argument values.

```powershell
rg -n -A 100 '00EB7250:' output/ShippingPC-BmGame.disasm.txt
rg -n -A 75 '00EB91D0:' output/ShippingPC-BmGame.disasm.txt
rg -n -B 8 -A 10 'call        00EA6310' output/ShippingPC-BmGame.disasm.txt
```

- [ ] Decode the verified decision bytes directly, not only the retained text dump:

```powershell
python games/HelenBatmanAA/experiments/windowed-resolution/Inspect-Viewport.py --executable 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe' --address 0xEA659C --length 0x12
```

Expected final instructions: `test eax,eax` at 0xEA65A6 and conditional jump to
0xEA7388 at 0xEA65A8; fallthrough at 0xEA65AE. This is not an approved patch span
until inbound branches and overwritten instructions are fully checked.

- [ ] Resolve each thread predicate and synchronization call. Record whether the
  caller blocks until the decision and Reset complete. If asynchronous, stop this
  plan before runtime design work and report the required ownership redesign.
- [ ] Prove that identical dimensions/mode reach the decision without an earlier
  no-op return. Record all conditions under which they do not. Do not compensate
  with temporary dimensions or a fabricated device-error flag.
- [ ] Record a pass/fail result for this gate, distinguishing static proof from
  live behavior that still needs eventual acceptance.

## Task 2: Establish settings snapshot and save contracts

**Consumes:** The pinned settings owner and functions identified in the report.
**Produces:** Exact calling convention, parameter layout, snapshot field ownership,
and the permitted no-save call sequence, or a rejected-binding finding.

- [ ] Decode 0x00C40090 and inner 0x00C3FDE0 fully, including every branch and
  transitive writer. Follow 0x00C3F6B0 and 0x00C24710 to distinguish configuration
  cache mutation, physical writes and unrelated side effects.

```powershell
rg -n -A 75 '00C40090:' output/ShippingPC-BmGame.disasm.txt
rg -n -A 215 '00C3FDE0:' output/ShippingPC-BmGame.disasm.txt
rg -n -A 125 '00C24B50:' output/ShippingPC-BmGame.disasm.txt
```

- [ ] Trace construction/copy/destruction of the 0xAB-dword settings data. Map
  field 0x21C to UseVsync and identify any pointers or owner-dependent fields.
  Reject blind memcpy if the data owns resources or requires deep-copy behavior.
- [ ] Trace the configuration reload in the apply routine and prove that a draft
  copied from current live state does not normalize or replace unrelated fields.
  Record command-line overrides, callbacks and writes that remain with save=false.
- [ ] Verify the order: shared attempt consumption, engine settings mutation,
  refresh-request arming, synchronized viewport call, disarming, readback. Locate
  any reentrant refresh window before arming; revise if it can apply stale state.
- [ ] Document whether both Task 1 and Task 2 support the approved design. Stop
  before the bridge task if the required callable boundary is not established.

## Task 3: Executable x86 decision-bridge contract fixture

**Consumes:** Approved patch span and branch destinations established by Tasks 1–2.
**Produces:** `Test-DecisionBridge.ps1` with a nonzero exit on an ABI/control-flow
regression and `VSYNC_DECISION_BRIDGE_PASS` on success. This is a throwaway fixture,
not the production hook installer or request service.

- [ ] Read the TDD skill and its writing-good-tests reference. Inspect
  `HelenRuntime/Hook.cpp` relocation support before selecting the bridge form.
- [ ] Define test-only state in a dedicated file if a class is needed. Use literal
  register/flag sentinels and capture state at two synthetic branch destinations.
  Execute actual machine code with a pinned x86 fixture ABI; do not test source
  strings or use a disassembler result as execution evidence.
- [ ] Write these behavior tests first; use a real unchanged decision for the
  failing forced-request case:

```text
inactive + eax=0         => original skip destination
inactive + eax=1         => original rebuild destination
matching request/eax=0  => rebuild destination, consumed once
second visit/eax=0      => original skip destination
wrong renderer/device/thread => original decision, not consumed
each destination        => unchanged stack and non-result registers
each inactive destination => flags equal original test-eax behavior
callback refused request => no lingering forced branch
```

- [ ] Build using the existing console wrapper, `/std:c++20 /EHsc /MD /W4 /WX`,
  Win32 compiler and unique `output/vsync-bridge-<guid>` directory. Set error mode
  0x8003 in the fixture. No MessageBoxes, debugger launch or display-mode switch.
- [ ] Run the fixture before implementing request-aware branching; require the
  forced-request assertion to fail for the missing behavior, not a compiler error.
- [ ] Implement only the isolated bridge and callback needed by the fixture,
  preserving the original decision and all established machine state. No engine
  writes or COM calls are allowed inside the decision callback.
- [ ] Run the full matrix; require exit zero and `VSYNC_DECISION_BRIDGE_PASS`.
  Deliberately break inactive branching and preservation separately to demonstrate
  the corresponding assertions fail, then restore and rerun.

## Task 4: Gate review and production-plan handoff

**Consumes:** Findings and executable fixture results from Tasks 1–3.
**Produces:** A defensible go/no-go decision and, only if go, a separate production
implementation plan derived from the now-established interfaces.

- [ ] Self-review the gates against the approved spec. Record failures rather than
  disguising them as defaults, polling or a best-effort engine call.
- [ ] The follow-on plan must cover the reusable request service and RAII scope,
  initialization-time hook ownership, editor-baseline change detection, mixed-edit
  rejection, one shared attempt allowance, typed Batman settings adapter, COM
  observation lifetime, interval/mode/size verification and uncertainty lockout.
- [ ] Include no-save session, decision-bridge, engine-boundary failure and real
  D3D9 subtitle/reset regressions in that production plan. Keep the existing full
  suite failures explicit and deployment separate.
- [ ] Verify the installed DLL remains SHA-256
  `DB757D38C9E02BB9DBD87CD29F6DDABDE77866868C4636956FE9AFB6C7A90752`, inspect
  `git diff --check`, and report exactly what was established and what remains.
- [ ] Leave implementation changes uncommitted until requested; do not stage
  `batma/` or generated output. No automatic independent-review session.

## Plan self-review

This is deliberately the prerequisite-gates phase, not a claim of full spec
coverage. Tasks 1–2 resolve the explicit contract questions; Task 3 establishes
the bridge behavior in isolation; Task 4 requires the follow-on plan to cover all
remaining runtime and acceptance requirements. No production API or unverified
settings layout is invented to make the plan appear executable prematurely.
