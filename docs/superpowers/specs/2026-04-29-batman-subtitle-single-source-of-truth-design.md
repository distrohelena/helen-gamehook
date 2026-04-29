# Batman Subtitle Single Source of Truth Design

## Goal

Eliminate subtitle preset drift by removing the frontend/main-menu subtitle path from the active system and defining gameplay subtitle presets in exactly one place.

## Problem

The subtitle feature drifted because multiple layers each defined their own preset contract:

- gameplay clip-action labels
- frontend/main-menu clip-action labels
- pack metadata labels
- runtime scale mapping
- signal code expectations

Those layers were edited independently. Rebuilds therefore produced inconsistent artifacts such as:

- three visible options with six configured scales
- six visible options with no runtime effect
- gameplay and frontend paths disagreeing on labels or state counts

The current system needs one active subtitle path and one preset definition.

## Decision

The frontend/main-menu subtitle path is removed from the active build and deploy flow.

The gameplay subtitle path remains the only supported subtitle-size implementation.

All gameplay subtitle outputs are generated from one shared preset catalog.

## Scope

### In scope

- define one shared subtitle preset catalog in builder code
- remove frontend/main-menu subtitle generation from active subtitle build flow
- generate gameplay subtitle labels from the shared catalog
- generate signal-code mapping from the shared catalog
- generate pack metadata labels from the shared catalog
- generate runtime scale mapping from the shared catalog
- add artifact-level verification that checks the rebuilt gameplay artifact, not only source templates

### Out of scope

- reintroducing frontend/main-menu subtitle size UI
- adding new subtitle persistence behavior beyond the existing gameplay flow
- changing subtitle transport from the current gameplay signal-hook route
- changing non-subtitle gameplay settings

## Architecture

### Single source of truth

Introduce a single builder-side subtitle preset catalog. Each preset entry defines:

- stable ordinal index
- user-facing label
- signal code
- runtime scale value
- default flag

This catalog is the only place where subtitle preset count, labels, codes, and scale values may be edited.

### Active consumers

Only the gameplay subtitle pipeline consumes the catalog:

- gameplay ActionScript generator
- gameplay pack metadata generation
- gameplay runtime command generation
- gameplay verification scripts

### Removed active path

Frontend/main-menu subtitle generation is no longer part of the active subtitle pack contract. Any stale frontend-specific subtitle templates must either be deleted or isolated so they cannot affect gameplay rebuilds.

## Preset contract

The gameplay subtitle system uses exactly six presets:

| Index | Label       | Signal | Scale |
|------:|-------------|-------:|------:|
| 0     | Small       | 4101   | 1.0   |
| 1     | Medium      | 4102   | 1.5   |
| 2     | Large       | 4103   | 2.0   |
| 3     | Very Large  | 4104   | 4.0   |
| 4     | Huge        | 4105   | 6.0   |
| 5     | Massive     | 4106   | 8.0   |

Default preset is `Medium`.

## Build behavior

The gameplay subtitle rebuild flow must produce these outputs from the shared catalog:

1. gameplay subtitle row initializer with six labels
2. gameplay signal writes using codes `4101..4106`
3. pack metadata enum labels matching the six labels exactly
4. runtime subtitle scale commands matching the six scales exactly
5. default config value corresponding to `Medium`

No generated subtitle artifact may hardcode its own independent preset list.

## Verification

Verification must fail if any gameplay subtitle artifact drifts from the catalog.

Required checks:

1. rebuilt gameplay clip action contains the exact six-label initializer
2. rebuilt gameplay row script contains `4101..4106`
3. pack metadata exposes exactly six labels in the expected order
4. runtime scale command mapping exposes exactly six scale values in the expected order
5. deploy-time compatibility still validates the retail Batman base files before patching

Verification is artifact-level, not only source-level. The test target is the rebuilt output that deploy actually installs.

## Failure behavior

The build must fail fast if:

- any consumer attempts to use a preset count different from the catalog count
- a default preset is missing or duplicated
- any generated artifact does not match the catalog during verification

No fallback to three-state behavior is allowed.

## Migration

Existing gameplay subtitle functionality remains the baseline.

The migration is structural:

- remove frontend subtitle path from active build/deploy/test flow
- replace duplicate preset literals with the shared catalog
- rebuild gameplay-only subtitle artifacts
- verify rebuilt artifacts before deploy

## Risks

### Risk: stale frontend code remains in repo and confuses future work

Mitigation:

- remove it from active build entry points and tests now
- if practical, delete the dead frontend-specific subtitle templates in the same change
- otherwise isolate them clearly so they cannot be consumed by gameplay rebuilds

### Risk: generated artifacts still hide a second preset definition

Mitigation:

- add artifact-level tests against rebuilt gameplay outputs
- fail on any mismatch instead of tolerating drift

### Risk: deploy script reinstalls stale assets

Mitigation:

- deploy only gameplay subtitle artifacts for this pack
- keep retail frontend restoration in deploy logic

## Success criteria

This work is complete when:

- there is one builder-side subtitle preset definition
- frontend/main-menu subtitle generation is no longer active
- rebuilt gameplay subtitle artifacts all reflect the same six-preset contract
- deploy installs only gameplay subtitle behavior
- future changes to subtitle presets require editing one catalog and passing one artifact-level verifier
