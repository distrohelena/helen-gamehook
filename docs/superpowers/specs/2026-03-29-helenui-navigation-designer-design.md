# HelenUI Navigation Designer Design

## Overview

HelenUI will be a new Vue 3 + TypeScript application rooted at `C:\dev\helenui`.
Its first milestone is a generic navigation graph editor for applications or games.
The editor will let a developer model how a user moves through screens and sub-screens, while keeping the document structure reusable for later OCR, screen detection, automation, and richer UI composition features.

The first release is intentionally narrow.
It focuses on navigation only:

- Screens are modeled as nodes.
- Menu options are modeled as directed transitions between screens.
- Variables and conditional rules control whether transitions are available.
- Directional hints describe relative menu navigation such as `up`, `down`, `left`, and `right`.

Screen layout authoring, OCR anchors, automation steps, and runtime adapters are explicitly deferred.

## Goals

- Provide a canvas-first editor for authoring navigation graphs visually.
- Export a standalone HelenUI JSON schema that is not coupled to `helenhook`.
- Support conditional navigation using typed variables and structured rules.
- Support menu-relative direction hints without introducing full automation behavior.
- Seed the project with a Batman example that demonstrates title, main menu, options, and nested option screens.

## Non-Goals

- Designing the contents of individual screens.
- Placing UI components on screens.
- OCR anchor authoring or image matching.
- Input automation scripting.
- Direct integration with `helenhook` runtime data formats in v1.

## Recommended Approach

The editor should be graph-first in the user experience and strict in the document model.

This is the preferred approach because it matches the intended workflow while preserving a stable domain schema:

- The user edits screens and transitions on a visual canvas.
- The application stores data in explicit typed objects rather than loosely shaped blobs.
- Validation rules are applied against the domain model rather than inferred from the UI state.
- Future adapters can consume the exported JSON without needing to understand editor-specific details.

Alternative designs were considered and rejected:

- A loose document model would be faster to sketch but would create migration and validation problems once conditions and adapters are added.
- A tree editor with a secondary graph view would weaken the core user experience because the main product value is visual path design.

## Architecture

The application should be split into three layers with clear responsibilities.

### Graph Editor UI Layer

This layer owns:

- Canvas rendering, pan, and zoom.
- Node placement and edge drawing.
- Selection state for the current screen, transition, or variable.
- Inspector panels and editor commands triggered by the user.

This layer should remain focused on presentation and input wiring.
It should not contain document validation rules or export logic.

### Navigation Document Layer

This layer owns:

- The `NavigationProject` model.
- Screen, transition, variable, and condition types.
- Validation rules.
- Document mutations such as adding screens, creating transitions, renaming ids, and updating conditions.

This layer is the durable core of the product.
If later UI technologies or runtime consumers change, this layer should remain stable.

### Import/Export Layer

This layer owns:

- Serialization to the HelenUI JSON schema.
- Deserialization and schema version handling.
- Seed data loading for the Batman example.
- Future adapters for automation or runtime-specific formats.

The import/export layer must not leak storage concerns into the editor components.

## Domain Model

The top-level document is a `NavigationProject`.
It should contain four top-level sections:

- `project`: metadata and schema version.
- `screens`: all screen nodes.
- `transitions`: all directed edges between screens.
- `variables`: all typed variables referenced by conditions.

### Project Metadata

Project metadata should include:

- `schemaVersion`
- `id`
- `name`
- optional `description`

### Screen Model

A screen represents a reachable app or game state.

Each screen should contain:

- `id`: stable unique identifier.
- `name`: human-readable label.
- `notes`: optional author notes.
- `position`: canvas coordinates for editor layout.
- `isEntry`: whether the screen is the single entry screen.

### Transition Model

A transition represents one menu option that moves from a source screen to a target screen.

Each transition should contain:

- `id`: stable unique identifier.
- `sourceScreenId`: originating screen.
- `targetScreenId`: destination screen.
- `label`: visible menu option text.
- `notes`: optional author notes.
- `condition`: optional structured rule tree controlling visibility.
- `directionHint`: optional directional relationship for menu ordering.

### Variable Model

Variables represent the state used by transition conditions.

Supported v1 variable kinds:

- `boolean`
- `number`
- `string`
- `enum`

Each variable should contain:

- `id`
- `name`
- `type`
- optional `description`
- optional `enumValues` when the type is `enum`

### Condition Model

Conditions should use structured JSON rather than freeform expressions.
This keeps the format portable and validates cleanly.

Supported shapes:

- group nodes using `all` semantics
- group nodes using `any` semantics
- leaf rules referencing one variable and one operator

Supported v1 operators:

- `equals`
- `notEquals`
- `greaterThan`
- `greaterThanOrEqual`
- `lessThan`
- `lessThanOrEqual`
- `contains` for string values only

### Direction Hint Model

Direction hints describe relative navigation among options on the same source screen.
They are not automation instructions.

Each direction hint should contain:

- `direction`: `up`, `down`, `left`, or `right`
- `relativeToTransitionId`: the sibling transition used as the reference point

The reference transition must originate from the same `sourceScreenId`.

## Example JSON Shape

```json
{
  "project": {
    "schemaVersion": 1,
    "id": "batman-sample",
    "name": "Batman Navigation Sample",
    "description": "Menu navigation model for Batman-style flows."
  },
  "screens": [
    {
      "id": "title-screen",
      "name": "Title Screen",
      "notes": "Initial landing screen.",
      "position": { "x": 120, "y": 80 },
      "isEntry": true
    },
    {
      "id": "main-menu",
      "name": "Main Menu",
      "notes": "",
      "position": { "x": 420, "y": 80 },
      "isEntry": false
    },
    {
      "id": "continue-game",
      "name": "Continue Game",
      "notes": "",
      "position": { "x": 720, "y": 40 },
      "isEntry": false
    },
    {
      "id": "start-game",
      "name": "Start Game",
      "notes": "",
      "position": { "x": 720, "y": 140 },
      "isEntry": false
    }
  ],
  "transitions": [
    {
      "id": "title-to-main-menu",
      "sourceScreenId": "title-screen",
      "targetScreenId": "main-menu",
      "label": "Press Start",
      "notes": "",
      "condition": null,
      "directionHint": null
    },
    {
      "id": "main-menu-start",
      "sourceScreenId": "main-menu",
      "targetScreenId": "start-game",
      "label": "Start",
      "notes": "",
      "condition": null,
      "directionHint": null
    },
    {
      "id": "main-menu-continue",
      "sourceScreenId": "main-menu",
      "targetScreenId": "continue-game",
      "label": "Continue",
      "notes": "",
      "condition": {
        "kind": "all",
        "rules": [
          {
            "kind": "rule",
            "variableId": "has-save",
            "operator": "equals",
            "value": true
          }
        ]
      },
      "directionHint": {
        "direction": "down",
        "relativeToTransitionId": "main-menu-start"
      }
    }
  ],
  "variables": [
    {
      "id": "has-save",
      "name": "Has Save",
      "type": "boolean",
      "description": "Whether a save file exists."
    }
  ]
}
```

## Interaction Model

The first milestone should prioritize fast graph authoring:

- Double-click the canvas to create a screen.
- Drag from a screen to another screen to create a transition.
- Select a screen to edit its name, notes, and entry status.
- Select a transition to edit its label, notes, condition, and direction hint.
- Manage variables in a dedicated side panel.
- Keep the inspector focused on one selected item at a time.

The product should behave like an editor, not a forgiving form wizard.
When the document is invalid, the invalid state should remain visible until corrected.

## Validation Rules

Validation should run continuously and surface errors in a dedicated panel.

The initial rule set should include:

- exactly one entry screen is required
- every transition must reference an existing source and target screen
- every condition rule must reference an existing variable
- variable types must match operator and comparison value usage
- screen ids must be unique
- transition ids must be unique
- variable ids must be unique
- unreachable screens from the entry screen should be reported
- direction hints must reference a transition with the same source screen

The editor must not silently repair invalid documents.
If required data is missing, import or edit operations should report the error clearly.

## First Milestone Scope

The first implementation milestone for `C:\dev\helenui` should include:

- Vue 3 + TypeScript application scaffold
- graph canvas with pan and zoom
- create, rename, reposition, and delete screens
- create, relabel, retarget, and delete transitions
- define and edit variables
- define structured conditions on transitions
- define direction hints on transitions
- JSON import and export for the HelenUI schema
- Batman seed project data to demonstrate the model
- validation panel with the v1 rules

## Deferred Work

The following features are intentionally postponed until after the navigation editor is stable:

- screen composition and component placement
- visual component library management
- OCR anchors and detection metadata
- image matching configuration
- input automation steps and macros
- adapters to `helenhook` or other runtimes
- collaboration, multi-user editing, or cloud storage

## Testing Strategy

The implementation plan should cover tests at three levels:

- domain-layer unit tests for mutations, condition rules, and validation
- import/export tests for schema round-tripping and invalid document handling
- UI tests for critical graph editing flows such as screen creation, transition creation, and inspector edits

The first milestone does not need broad end-to-end automation beyond the key authoring flows.

## Decision Summary

The approved design decisions are:

- build a new standalone Vue 3 + TypeScript application in `C:\dev\helenui`
- keep v1 generic rather than Batman-specific, but seed Batman sample data
- focus v1 on navigation only
- use a visual node graph as the primary authoring experience
- model screens as nodes and menu options as directed transitions
- support variables and conditional transitions in v1
- allow relative directional hints without full automation behavior
- define and export a standalone HelenUI JSON schema first
