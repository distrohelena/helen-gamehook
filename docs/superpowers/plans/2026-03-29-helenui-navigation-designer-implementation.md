# HelenUI Navigation Designer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `C:\dev\helenui` as a standalone Vue 3 + TypeScript editor for visual navigation graphs with standalone HelenUI JSON import/export, variables, conditional transitions, direction hints, validation, and Batman seed data.

**Architecture:** Use Vite to host a Vue 3 single-page app. Keep the navigation document model, validation, and serialization as pure TypeScript modules; keep the UI as a canvas shell around a Pinia store and a Vue Flow adapter. Import/export and Batman seed data stay in separate services so later OCR or automation adapters can be added without coupling runtime formats into editor components.

**Tech Stack:** Vue 3, TypeScript, Vite, Pinia, `@vue-flow/core`, `@vue-flow/background`, `@vue-flow/controls`, Zod, Vitest, Vue Test Utils, jsdom.

---

## File Structure

### Tooling and App Entry

- `C:\dev\helenui\package.json`
  Purpose: scripts and project dependencies.
- `C:\dev\helenui\vite.config.ts`
  Purpose: Vue plugin and Vitest configuration.
- `C:\dev\helenui\src\main.ts`
  Purpose: boot Vue and Pinia.
- `C:\dev\helenui\src\App.vue`
  Purpose: root component that renders the editor shell.
- `C:\dev\helenui\src\style.css`
  Purpose: global layout variables and base styles.

### Domain and Serialization

- `C:\dev\helenui\src\domain\navigationProject.ts`
  Purpose: HelenUI document types, constants, and empty project factory.
- `C:\dev\helenui\src\domain\validation.ts`
  Purpose: project validation rules and issue reporting.
- `C:\dev\helenui\src\services\projectSerializer.ts`
  Purpose: Zod-backed JSON parsing and stringifying.

### State

- `C:\dev\helenui\src\store\navigationProjectStore.ts`
  Purpose: authoritative editor state, selection state, and document mutations.

### Graph and UI Components

- `C:\dev\helenui\src\graph\flowElements.ts`
  Purpose: map domain screens and transitions to Vue Flow nodes and edges.
- `C:\dev\helenui\src\components\graph\NavigationCanvas.vue`
  Purpose: graph canvas with pan, zoom, selection, connect, and node movement events.
- `C:\dev\helenui\src\components\variables\VariablePanel.vue`
  Purpose: create, edit, and delete project variables.
- `C:\dev\helenui\src\components\conditions\ConditionEditor.vue`
  Purpose: recursive editor for `all` / `any` groups and leaf rules.
- `C:\dev\helenui\src\components\inspector\ScreenInspector.vue`
  Purpose: edit screen metadata and entry flag.
- `C:\dev\helenui\src\components\inspector\TransitionInspector.vue`
  Purpose: edit transition label, target, notes, conditions, and direction hints.
- `C:\dev\helenui\src\components\toolbar\ProjectToolbar.vue`
  Purpose: load Batman seed, import JSON, and export JSON.
- `C:\dev\helenui\src\components\validation\ValidationPanel.vue`
  Purpose: display current validation issues.
- `C:\dev\helenui\src\components\layout\AppShell.vue`
  Purpose: compose the toolbar, panels, canvas, and store wiring.

### Seed Data

- `C:\dev\helenui\src\seeds\createBatmanSampleProject.ts`
  Purpose: first-run sample project showing Batman title, main menu, options, audio, and subtitle flows.

### Tests

- `C:\dev\helenui\src\App.test.ts`
- `C:\dev\helenui\src\services\projectSerializer.test.ts`
- `C:\dev\helenui\src\domain\validation.test.ts`
- `C:\dev\helenui\src\store\navigationProjectStore.test.ts`
- `C:\dev\helenui\src\components\variables\VariablePanel.test.ts`
- `C:\dev\helenui\src\components\conditions\ConditionEditor.test.ts`
- `C:\dev\helenui\src\components\inspector\ScreenInspector.test.ts`
- `C:\dev\helenui\src\components\inspector\TransitionInspector.test.ts`
- `C:\dev\helenui\src\graph\flowElements.test.ts`
- `C:\dev\helenui\src\components\graph\NavigationCanvas.test.ts`
- `C:\dev\helenui\src\components\toolbar\ProjectToolbar.test.ts`
- `C:\dev\helenui\src\components\validation\ValidationPanel.test.ts`
- `C:\dev\helenui\src\components\layout\AppShell.test.ts`

### Task 1: Scaffold the Vue Workspace

**Files:**
- Create: `C:\dev\helenui\package.json`
- Create: `C:\dev\helenui\vite.config.ts`
- Create: `C:\dev\helenui\src\main.ts`
- Create: `C:\dev\helenui\src\App.vue`
- Create: `C:\dev\helenui\src\App.test.ts`
- Create: `C:\dev\helenui\src\style.css`

- [ ] **Step 1: Scaffold the project**

```powershell
Set-Location C:\dev
npm create vite@latest helenui -- --template vue-ts
```

- [ ] **Step 2: Install runtime and test dependencies**

```powershell
Set-Location C:\dev\helenui
npm install pinia zod @vue-flow/core @vue-flow/background @vue-flow/controls
npm install -D vitest @vue/test-utils jsdom
```

- [ ] **Step 3: Configure scripts and Vitest**

```json
{
  "scripts": {
    "dev": "vite",
    "build": "vue-tsc -b && vite build",
    "test": "vitest --run",
    "test:watch": "vitest"
  }
}
```

```ts
import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';

export default defineConfig({
  plugins: [vue()],
  test: {
    environment: 'jsdom',
  },
});
```

- [ ] **Step 4: Write the failing smoke test**

```ts
import { describe, expect, it } from 'vitest';
import { mount } from '@vue/test-utils';
import App from './App.vue';

describe('App', () => {
  it('renders the HelenUI shell heading', () => {
    const wrapper = mount(App);

    expect(wrapper.get('[data-testid="app-title"]').text()).toBe('HelenUI Navigation Designer');
    expect(wrapper.text()).toContain('Navigation graph editor');
  });
});
```

- [ ] **Step 5: Run the smoke test to verify it fails**

```powershell
Set-Location C:\dev\helenui
npm test -- src/App.test.ts
```

Expected: FAIL because the generated `App.vue` does not render `HelenUI Navigation Designer`.

- [ ] **Step 6: Write the minimal app entry and shell**

```ts
import { createApp } from 'vue';
import { createPinia } from 'pinia';
import App from './App.vue';
import './style.css';

createApp(App).use(createPinia()).mount('#app');
```

```vue
<template>
  <main class="app-shell">
    <h1 data-testid="app-title">HelenUI Navigation Designer</h1>
    <p>Navigation graph editor</p>
  </main>
</template>
```

```css
:root {
  color: #f3f1ea;
  background: #11161d;
  font-family: "Segoe UI", sans-serif;
}

body {
  margin: 0;
  min-height: 100vh;
  background:
    radial-gradient(circle at top left, rgba(200, 172, 96, 0.18), transparent 28rem),
    linear-gradient(180deg, #151c25 0%, #0c1117 100%);
}

#app {
  min-height: 100vh;
}

.app-shell {
  display: grid;
  place-content: center;
  min-height: 100vh;
  gap: 0.75rem;
  text-align: center;
}
```

- [ ] **Step 7: Run the smoke test to verify it passes**

```powershell
Set-Location C:\dev\helenui
npm test -- src/App.test.ts
```

Expected: PASS with `1 passed`.

- [ ] **Step 8: Commit**

```powershell
Set-Location C:\dev\helenui
git add .
git commit -m "chore: scaffold helenui workspace"
```

### Task 2: Define the HelenUI Document Schema and Serializer

**Files:**
- Create: `C:\dev\helenui\src\domain\navigationProject.ts`
- Create: `C:\dev\helenui\src\services\projectSerializer.ts`
- Test: `C:\dev\helenui\src\services\projectSerializer.test.ts`

- [ ] **Step 1: Write the failing serializer tests**

```ts
import { describe, expect, it } from 'vitest';
import { parseNavigationProjectJson, stringifyNavigationProjectJson } from './projectSerializer';

describe('projectSerializer', () => {
  it('parses and stringifies a valid navigation project', () => {
    const jsonText = JSON.stringify({
      project: {
        schemaVersion: 1,
        id: 'batman-sample',
        name: 'Batman Navigation Sample',
        description: 'Menu navigation model for Batman-style flows.',
      },
      screens: [
        {
          id: 'title-screen',
          name: 'Title Screen',
          notes: 'Initial landing screen.',
          position: { x: 120, y: 80 },
          isEntry: true,
        },
      ],
      transitions: [],
      variables: [],
    });

    const parsed = parseNavigationProjectJson(jsonText);
    const roundTrip = stringifyNavigationProjectJson(parsed);

    expect(parsed.project.name).toBe('Batman Navigation Sample');
    expect(roundTrip).toContain('"schemaVersion": 1');
    expect(roundTrip).toContain('"title-screen"');
  });

  it('throws for an invalid navigation project', () => {
    const jsonText = JSON.stringify({
      project: {
        schemaVersion: 1,
        id: 'broken-project',
      },
      screens: [],
      transitions: [],
      variables: [],
    });

    expect(() => parseNavigationProjectJson(jsonText)).toThrow('NavigationProject parse failed');
  });
});
```

- [ ] **Step 2: Run the serializer tests to verify they fail**

```powershell
Set-Location C:\dev\helenui
npm test -- src/services/projectSerializer.test.ts
```

Expected: FAIL because `projectSerializer.ts` does not exist yet.

- [ ] **Step 3: Implement the domain types and empty project factory**

```ts
export type VariableType = 'boolean' | 'number' | 'string' | 'enum';
export type ConditionOperator =
  | 'equals'
  | 'notEquals'
  | 'greaterThan'
  | 'greaterThanOrEqual'
  | 'lessThan'
  | 'lessThanOrEqual'
  | 'contains';
export type DirectionHintKind = 'up' | 'down' | 'left' | 'right';

export interface NavigationProjectMetadata {
  schemaVersion: 1;
  id: string;
  name: string;
  description: string;
}

export interface ScreenPosition {
  x: number;
  y: number;
}

export interface NavigationScreen {
  id: string;
  name: string;
  notes: string;
  position: ScreenPosition;
  isEntry: boolean;
}

export interface NavigationConditionRule {
  kind: 'rule';
  variableId: string;
  operator: ConditionOperator;
  value: boolean | number | string;
}

export interface NavigationConditionGroup {
  kind: 'all' | 'any';
  rules: NavigationCondition[];
}

export type NavigationCondition = NavigationConditionRule | NavigationConditionGroup;

export interface DirectionHint {
  direction: DirectionHintKind;
  relativeToTransitionId: string;
}

export interface NavigationTransition {
  id: string;
  sourceScreenId: string;
  targetScreenId: string;
  label: string;
  notes: string;
  condition: NavigationCondition | null;
  directionHint: DirectionHint | null;
}

export interface BooleanVariable {
  id: string;
  name: string;
  type: 'boolean';
  description: string;
}

export interface NumberVariable {
  id: string;
  name: string;
  type: 'number';
  description: string;
}

export interface StringVariable {
  id: string;
  name: string;
  type: 'string';
  description: string;
}

export interface EnumVariable {
  id: string;
  name: string;
  type: 'enum';
  description: string;
  enumValues: string[];
}

export type ProjectVariable =
  | BooleanVariable
  | NumberVariable
  | StringVariable
  | EnumVariable;

export interface NavigationProject {
  project: NavigationProjectMetadata;
  screens: NavigationScreen[];
  transitions: NavigationTransition[];
  variables: ProjectVariable[];
}

export function createEmptyProject(name = 'Untitled Project'): NavigationProject {
  return {
    project: {
      schemaVersion: 1,
      id: 'untitled-project',
      name,
      description: '',
    },
    screens: [],
    transitions: [],
    variables: [],
  };
}
```

- [ ] **Step 4: Implement the Zod-backed JSON serializer**

```ts
import { z } from 'zod';
import type { NavigationCondition, NavigationProject } from '../domain/navigationProject';

const conditionRuleSchema = z.object({
  kind: z.literal('rule'),
  variableId: z.string().min(1),
  operator: z.enum([
    'equals',
    'notEquals',
    'greaterThan',
    'greaterThanOrEqual',
    'lessThan',
    'lessThanOrEqual',
    'contains',
  ]),
  value: z.union([z.boolean(), z.number(), z.string()]),
});

const conditionSchema: z.ZodType<NavigationCondition> = z.lazy(() =>
  z.union([
    conditionRuleSchema,
    z.object({
      kind: z.enum(['all', 'any']),
      rules: z.array(conditionSchema),
    }),
  ]),
);

const navigationProjectSchema = z.object({
  project: z.object({
    schemaVersion: z.literal(1),
    id: z.string().min(1),
    name: z.string().min(1),
    description: z.string(),
  }),
  screens: z.array(
    z.object({
      id: z.string().min(1),
      name: z.string().min(1),
      notes: z.string(),
      position: z.object({
        x: z.number(),
        y: z.number(),
      }),
      isEntry: z.boolean(),
    }),
  ),
  transitions: z.array(
    z.object({
      id: z.string().min(1),
      sourceScreenId: z.string().min(1),
      targetScreenId: z.string().min(1),
      label: z.string().min(1),
      notes: z.string(),
      condition: conditionSchema.nullable(),
      directionHint: z
        .object({
          direction: z.enum(['up', 'down', 'left', 'right']),
          relativeToTransitionId: z.string().min(1),
        })
        .nullable(),
    }),
  ),
  variables: z.array(
    z.discriminatedUnion('type', [
      z.object({
        id: z.string().min(1),
        name: z.string().min(1),
        type: z.literal('boolean'),
        description: z.string(),
      }),
      z.object({
        id: z.string().min(1),
        name: z.string().min(1),
        type: z.literal('number'),
        description: z.string(),
      }),
      z.object({
        id: z.string().min(1),
        name: z.string().min(1),
        type: z.literal('string'),
        description: z.string(),
      }),
      z.object({
        id: z.string().min(1),
        name: z.string().min(1),
        type: z.literal('enum'),
        description: z.string(),
        enumValues: z.array(z.string().min(1)).min(1),
      }),
    ]),
  ),
});

export function parseNavigationProjectJson(jsonText: string): NavigationProject {
  const parsedJson = JSON.parse(jsonText);
  const parsedProject = navigationProjectSchema.safeParse(parsedJson);

  if (!parsedProject.success) {
    throw new Error(`NavigationProject parse failed: ${parsedProject.error.issues[0].message}`);
  }

  return parsedProject.data;
}

export function stringifyNavigationProjectJson(project: NavigationProject): string {
  return JSON.stringify(project, null, 2);
}
```

- [ ] **Step 5: Run the serializer tests to verify they pass**

```powershell
Set-Location C:\dev\helenui
npm test -- src/services/projectSerializer.test.ts
```

Expected: PASS with `2 passed`.

- [ ] **Step 6: Commit**

```powershell
Set-Location C:\dev\helenui
git add src/domain/navigationProject.ts src/services/projectSerializer.ts src/services/projectSerializer.test.ts
git commit -m "feat: add helenui project schema"
```

### Task 3: Implement Validation Rules

**Files:**
- Create: `C:\dev\helenui\src\domain\validation.ts`
- Test: `C:\dev\helenui\src\domain\validation.test.ts`

- [ ] **Step 1: Write the failing validation tests**

```ts
import { describe, expect, it } from 'vitest';
import type { NavigationProject } from './navigationProject';
import { validateNavigationProject } from './validation';

const validProject: NavigationProject = {
  project: {
    schemaVersion: 1,
    id: 'sample-project',
    name: 'Sample Project',
    description: '',
  },
  screens: [
    {
      id: 'entry-screen',
      name: 'Entry Screen',
      notes: '',
      position: { x: 100, y: 100 },
      isEntry: true,
    },
    {
      id: 'options-screen',
      name: 'Options Screen',
      notes: '',
      position: { x: 300, y: 100 },
      isEntry: false,
    },
  ],
  transitions: [
    {
      id: 'open-options',
      sourceScreenId: 'entry-screen',
      targetScreenId: 'options-screen',
      label: 'Options',
      notes: '',
      condition: null,
      directionHint: null,
    },
  ],
  variables: [
    {
      id: 'has-save',
      name: 'Has Save',
      type: 'boolean',
      description: 'Whether a save file exists.',
    },
  ],
};

describe('validateNavigationProject', () => {
  it('returns no issues for a valid project', () => {
    expect(validateNavigationProject(validProject)).toEqual([]);
  });

  it('reports entry, duplicate id, unreachable screen, and bad direction errors', () => {
    const invalidProject: NavigationProject = {
      ...validProject,
      screens: [
        { ...validProject.screens[0], isEntry: false },
        validProject.screens[1],
        {
          id: 'options-screen',
          name: 'Duplicate Screen',
          notes: '',
          position: { x: 500, y: 100 },
          isEntry: false,
        },
        {
          id: 'orphan-screen',
          name: 'Orphan Screen',
          notes: '',
          position: { x: 700, y: 100 },
          isEntry: false,
        },
      ],
      transitions: [
        {
          ...validProject.transitions[0],
          directionHint: {
            direction: 'down',
            relativeToTransitionId: 'missing-transition',
          },
        },
      ],
    };

    const codes = validateNavigationProject(invalidProject).map((issue) => issue.code);

    expect(codes).toContain('missing-entry-screen');
    expect(codes).toContain('duplicate-screen-id');
    expect(codes).toContain('unreachable-screen');
    expect(codes).toContain('invalid-direction-reference');
  });
});
```

- [ ] **Step 2: Run the validation tests to verify they fail**

```powershell
Set-Location C:\dev\helenui
npm test -- src/domain/validation.test.ts
```

Expected: FAIL because `validation.ts` does not exist yet.

- [ ] **Step 3: Implement the validation engine**

```ts
import type {
  NavigationCondition,
  NavigationProject,
  NavigationTransition,
  ProjectVariable,
} from './navigationProject';

export interface ValidationIssue {
  code:
    | 'missing-entry-screen'
    | 'multiple-entry-screens'
    | 'duplicate-screen-id'
    | 'duplicate-transition-id'
    | 'duplicate-variable-id'
    | 'missing-transition-screen'
    | 'missing-condition-variable'
    | 'invalid-operator-for-variable'
    | 'invalid-direction-reference'
    | 'unreachable-screen';
  severity: 'error' | 'warning';
  subjectType: 'project' | 'screen' | 'transition' | 'variable';
  subjectId?: string;
  message: string;
}

export function validateNavigationProject(project: NavigationProject): ValidationIssue[] {
  const issues: ValidationIssue[] = [];

  appendEntryIssues(project, issues);
  appendDuplicateIssues(project, issues);
  appendTransitionIssues(project, issues);
  appendUnreachableScreenIssues(project, issues);

  return issues;
}

function appendEntryIssues(project: NavigationProject, issues: ValidationIssue[]): void {
  const entryScreens = project.screens.filter((screen) => screen.isEntry);

  if (entryScreens.length === 0) {
    issues.push({
      code: 'missing-entry-screen',
      severity: 'error',
      subjectType: 'project',
      message: 'A navigation project must have exactly one entry screen.',
    });
  }

  if (entryScreens.length > 1) {
    issues.push({
      code: 'multiple-entry-screens',
      severity: 'error',
      subjectType: 'project',
      message: 'A navigation project cannot have more than one entry screen.',
    });
  }
}

function appendDuplicateIssues(project: NavigationProject, issues: ValidationIssue[]): void {
  appendDuplicateIdIssues(project.screens.map((screen) => screen.id), 'duplicate-screen-id', 'screen', issues);
  appendDuplicateIdIssues(project.transitions.map((transition) => transition.id), 'duplicate-transition-id', 'transition', issues);
  appendDuplicateIdIssues(project.variables.map((variable) => variable.id), 'duplicate-variable-id', 'variable', issues);
}

function appendDuplicateIdIssues(
  ids: string[],
  code: ValidationIssue['code'],
  subjectType: ValidationIssue['subjectType'],
  issues: ValidationIssue[],
): void {
  const counts = new Map<string, number>();

  for (const id of ids) {
    counts.set(id, (counts.get(id) ?? 0) + 1);
  }

  for (const [id, count] of counts.entries()) {
    if (count > 1) {
      issues.push({
        code,
        severity: 'error',
        subjectType,
        subjectId: id,
        message: `Duplicate id detected: ${id}`,
      });
    }
  }
}

function appendTransitionIssues(project: NavigationProject, issues: ValidationIssue[]): void {
  const screenIds = new Set(project.screens.map((screen) => screen.id));
  const variableMap = new Map(project.variables.map((variable) => [variable.id, variable]));
  const transitionsById = new Map(project.transitions.map((transition) => [transition.id, transition]));

  for (const transition of project.transitions) {
    if (!screenIds.has(transition.sourceScreenId) || !screenIds.has(transition.targetScreenId)) {
      issues.push({
        code: 'missing-transition-screen',
        severity: 'error',
        subjectType: 'transition',
        subjectId: transition.id,
        message: `Transition ${transition.id} references a missing screen.`,
      });
    }

    if (transition.directionHint !== null) {
      const siblingTransition = transitionsById.get(transition.directionHint.relativeToTransitionId);
      const isSameSourceScreen = siblingTransition?.sourceScreenId === transition.sourceScreenId;

      if (!isSameSourceScreen) {
        issues.push({
          code: 'invalid-direction-reference',
          severity: 'error',
          subjectType: 'transition',
          subjectId: transition.id,
          message: `Transition ${transition.id} has a direction hint that does not reference a sibling transition.`,
        });
      }
    }

    if (transition.condition !== null) {
      appendConditionIssues(transition, transition.condition, variableMap, issues);
    }
  }
}

function appendConditionIssues(
  transition: NavigationTransition,
  condition: NavigationCondition,
  variableMap: Map<string, ProjectVariable>,
  issues: ValidationIssue[],
): void {
  if (condition.kind === 'rule') {
    const variable = variableMap.get(condition.variableId);

    if (!variable) {
      issues.push({
        code: 'missing-condition-variable',
        severity: 'error',
        subjectType: 'transition',
        subjectId: transition.id,
        message: `Transition ${transition.id} references a missing variable.`,
      });

      return;
    }

    if (!isOperatorAllowedForVariable(variable, condition.operator)) {
      issues.push({
        code: 'invalid-operator-for-variable',
        severity: 'error',
        subjectType: 'transition',
        subjectId: transition.id,
        message: `Operator ${condition.operator} is not valid for variable ${variable.id}.`,
      });
    }

    return;
  }

  for (const childRule of condition.rules) {
    appendConditionIssues(transition, childRule, variableMap, issues);
  }
}

function isOperatorAllowedForVariable(variable: ProjectVariable, operator: string): boolean {
  if (variable.type === 'boolean') {
    return operator === 'equals' || operator === 'notEquals';
  }

  if (variable.type === 'number') {
    return operator !== 'contains';
  }

  if (variable.type === 'string') {
    return operator === 'equals' || operator === 'notEquals' || operator === 'contains';
  }

  return operator === 'equals' || operator === 'notEquals';
}

function appendUnreachableScreenIssues(project: NavigationProject, issues: ValidationIssue[]): void {
  const entryScreen = project.screens.find((screen) => screen.isEntry);

  if (!entryScreen) {
    return;
  }

  const visited = new Set<string>([entryScreen.id]);
  const queue = [entryScreen.id];

  while (queue.length > 0) {
    const currentScreenId = queue.shift()!;
    const outgoingTransitions = project.transitions.filter((transition) => transition.sourceScreenId === currentScreenId);

    for (const transition of outgoingTransitions) {
      if (!visited.has(transition.targetScreenId)) {
        visited.add(transition.targetScreenId);
        queue.push(transition.targetScreenId);
      }
    }
  }

  for (const screen of project.screens) {
    if (!visited.has(screen.id)) {
      issues.push({
        code: 'unreachable-screen',
        severity: 'warning',
        subjectType: 'screen',
        subjectId: screen.id,
        message: `Screen ${screen.id} is unreachable from the entry screen.`,
      });
    }
  }
}
```

- [ ] **Step 4: Run the validation tests to verify they pass**

```powershell
Set-Location C:\dev\helenui
npm test -- src/domain/validation.test.ts
```

Expected: PASS with `2 passed`.

- [ ] **Step 5: Commit**

```powershell
Set-Location C:\dev\helenui
git add src/domain/validation.ts src/domain/validation.test.ts
git commit -m "feat: add project validation"
```

### Task 4: Add the Navigation Project Store

**Files:**
- Create: `C:\dev\helenui\src\store\navigationProjectStore.ts`
- Test: `C:\dev\helenui\src\store\navigationProjectStore.test.ts`

- [ ] **Step 1: Write the failing store tests**

```ts
import { beforeEach, describe, expect, it } from 'vitest';
import { createPinia, setActivePinia } from 'pinia';
import { useNavigationProjectStore } from './navigationProjectStore';

describe('navigationProjectStore', () => {
  beforeEach(() => {
    setActivePinia(createPinia());
  });

  it('creates the first screen as the entry screen', () => {
    const store = useNavigationProjectStore();

    const screenId = store.createScreen({ x: 100, y: 120 });

    expect(store.project.screens).toHaveLength(1);
    expect(store.project.screens[0].id).toBe(screenId);
    expect(store.project.screens[0].isEntry).toBe(true);
  });

  it('creates transitions, variables, and derived validation issues', () => {
    const store = useNavigationProjectStore();
    const entryId = store.createScreen({ x: 100, y: 120 });
    const targetId = store.createScreen({ x: 400, y: 120 });
    const variableId = store.createVariable('boolean');

    const transitionId = store.createTransition({
      sourceScreenId: entryId,
      targetScreenId: targetId,
      label: 'Continue',
    });

    expect(store.project.variables[0].id).toBe(variableId);
    expect(store.project.transitions[0].id).toBe(transitionId);
    expect(store.validationIssues).toEqual([]);
  });
});
```

- [ ] **Step 2: Run the store tests to verify they fail**

```powershell
Set-Location C:\dev\helenui
npm test -- src/store/navigationProjectStore.test.ts
```

Expected: FAIL because `navigationProjectStore.ts` does not exist yet.

- [ ] **Step 3: Implement the Pinia store**

```ts
import { computed, ref } from 'vue';
import { defineStore } from 'pinia';
import {
  createEmptyProject,
  type NavigationCondition,
  type NavigationProject,
  type NavigationScreen,
  type NavigationTransition,
  type ProjectVariable,
  type ScreenPosition,
  type VariableType,
} from '../domain/navigationProject';
import { validateNavigationProject } from '../domain/validation';

export type SelectionState =
  | { kind: 'none' }
  | { kind: 'screen'; id: string }
  | { kind: 'transition'; id: string }
  | { kind: 'variable'; id: string };

export const useNavigationProjectStore = defineStore('navigationProject', () => {
  const project = ref<NavigationProject>(createEmptyProject('HelenUI Project'));
  const selection = ref<SelectionState>({ kind: 'none' });

  const validationIssues = computed(() => validateNavigationProject(project.value));
  const selectedScreen = computed(() =>
    selection.value.kind === 'screen'
      ? project.value.screens.find((screen) => screen.id === selection.value.id) ?? null
      : null,
  );
  const selectedTransition = computed(() =>
    selection.value.kind === 'transition'
      ? project.value.transitions.find((transition) => transition.id === selection.value.id) ?? null
      : null,
  );

  function replaceProject(nextProject: NavigationProject): void {
    project.value = structuredClone(nextProject);
    selection.value = { kind: 'none' };
  }

  function createScreen(position: ScreenPosition): string {
    const screenId = crypto.randomUUID();
    const screen: NavigationScreen = {
      id: screenId,
      name: `Screen ${project.value.screens.length + 1}`,
      notes: '',
      position,
      isEntry: project.value.screens.every((existingScreen) => !existingScreen.isEntry),
    };

    project.value.screens.push(screen);
    selection.value = { kind: 'screen', id: screenId };

    return screenId;
  }

  function updateScreen(nextScreen: NavigationScreen): void {
    project.value.screens = project.value.screens.map((screen) => (screen.id === nextScreen.id ? nextScreen : screen));
  }

  function setEntryScreen(screenId: string): void {
    project.value.screens = project.value.screens.map((screen) => ({
      ...screen,
      isEntry: screen.id === screenId,
    }));
  }

  function deleteScreen(screenId: string): void {
    project.value.screens = project.value.screens.filter((screen) => screen.id !== screenId);
    project.value.transitions = project.value.transitions.filter(
      (transition) => transition.sourceScreenId !== screenId && transition.targetScreenId !== screenId,
    );
    selection.value = { kind: 'none' };
  }

  function createTransition(input: {
    sourceScreenId: string;
    targetScreenId: string;
    label: string;
    condition?: NavigationCondition | null;
  }): string {
    const transitionId = crypto.randomUUID();
    const transition: NavigationTransition = {
      id: transitionId,
      sourceScreenId: input.sourceScreenId,
      targetScreenId: input.targetScreenId,
      label: input.label,
      notes: '',
      condition: input.condition ?? null,
      directionHint: null,
    };

    project.value.transitions.push(transition);
    selection.value = { kind: 'transition', id: transitionId };

    return transitionId;
  }

  function updateTransition(nextTransition: NavigationTransition): void {
    project.value.transitions = project.value.transitions.map((transition) =>
      transition.id === nextTransition.id ? nextTransition : transition,
    );
  }

  function deleteTransition(transitionId: string): void {
    project.value.transitions = project.value.transitions.filter((transition) => transition.id !== transitionId);
    selection.value = { kind: 'none' };
  }

  function createVariable(type: VariableType): string {
    const variableId = crypto.randomUUID();
    const variable: ProjectVariable =
      type === 'enum'
        ? {
            id: variableId,
            name: `Variable ${project.value.variables.length + 1}`,
            type: 'enum',
            description: '',
            enumValues: ['Value 1'],
          }
        : {
            id: variableId,
            name: `Variable ${project.value.variables.length + 1}`,
            type,
            description: '',
          };

    project.value.variables.push(variable);
    selection.value = { kind: 'variable', id: variableId };

    return variableId;
  }

  function updateVariable(nextVariable: ProjectVariable): void {
    project.value.variables = project.value.variables.map((variable) => (variable.id === nextVariable.id ? nextVariable : variable));
  }

  function deleteVariable(variableId: string): void {
    project.value.variables = project.value.variables.filter((variable) => variable.id !== variableId);
    selection.value = { kind: 'none' };
  }

  return {
    project,
    selection,
    validationIssues,
    selectedScreen,
    selectedTransition,
    replaceProject,
    createScreen,
    updateScreen,
    setEntryScreen,
    deleteScreen,
    createTransition,
    updateTransition,
    deleteTransition,
    createVariable,
    updateVariable,
    deleteVariable,
  };
});
```

- [ ] **Step 4: Run the store tests to verify they pass**

```powershell
Set-Location C:\dev\helenui
npm test -- src/store/navigationProjectStore.test.ts
```

Expected: PASS with `2 passed`.

- [ ] **Step 5: Commit**

```powershell
Set-Location C:\dev\helenui
git add src/store/navigationProjectStore.ts src/store/navigationProjectStore.test.ts
git commit -m "feat: add navigation project store"
```

### Task 5: Build the Inspector and Variable Editing Components

**Files:**
- Create: `C:\dev\helenui\src\components\variables\VariablePanel.vue`
- Create: `C:\dev\helenui\src\components\variables\VariablePanel.test.ts`
- Create: `C:\dev\helenui\src\components\conditions\ConditionEditor.vue`
- Create: `C:\dev\helenui\src\components\conditions\ConditionEditor.test.ts`
- Create: `C:\dev\helenui\src\components\inspector\ScreenInspector.vue`
- Create: `C:\dev\helenui\src\components\inspector\ScreenInspector.test.ts`
- Create: `C:\dev\helenui\src\components\inspector\TransitionInspector.vue`
- Create: `C:\dev\helenui\src\components\inspector\TransitionInspector.test.ts`

- [ ] **Step 1: Write the failing editor component tests**

```ts
import { describe, expect, it } from 'vitest';
import { mount } from '@vue/test-utils';
import VariablePanel from './VariablePanel.vue';

describe('VariablePanel', () => {
  it('emits create and update events', async () => {
    const wrapper = mount(VariablePanel, {
      props: {
        variables: [
          {
            id: 'has-save',
            name: 'Has Save',
            type: 'boolean',
            description: 'Whether a save file exists.',
          },
        ],
      },
    });

    await wrapper.get('[data-testid="create-boolean-variable"]').trigger('click');
    await wrapper.get('[data-testid="variable-name-has-save"]').setValue('Has Save Data');

    expect(wrapper.emitted('create-variable')?.[0]).toEqual(['boolean']);
    expect(wrapper.emitted('update-variable')?.at(-1)?.[0]).toMatchObject({
      id: 'has-save',
      name: 'Has Save Data',
    });
  });
});
```

```ts
import { describe, expect, it } from 'vitest';
import { mount } from '@vue/test-utils';
import ConditionEditor from './ConditionEditor.vue';

describe('ConditionEditor', () => {
  it('emits a default rule when adding the first condition', async () => {
    const wrapper = mount(ConditionEditor, {
      props: {
        modelValue: null,
        variables: [
          {
            id: 'has-save',
            name: 'Has Save',
            type: 'boolean',
            description: 'Whether a save file exists.',
          },
        ],
      },
    });

    await wrapper.get('[data-testid="add-condition-rule"]').trigger('click');

    expect(wrapper.emitted('update:modelValue')?.[0][0]).toEqual({
      kind: 'all',
      rules: [
        {
          kind: 'rule',
          variableId: 'has-save',
          operator: 'equals',
          value: true,
        },
      ],
    });
  });
});
```

```ts
import { describe, expect, it } from 'vitest';
import { mount } from '@vue/test-utils';
import ScreenInspector from './ScreenInspector.vue';

describe('ScreenInspector', () => {
  it('emits updates when the screen name changes and when entry is toggled', async () => {
    const wrapper = mount(ScreenInspector, {
      props: {
        screen: {
          id: 'main-menu',
          name: 'Main Menu',
          notes: '',
          position: { x: 300, y: 120 },
          isEntry: false,
        },
      },
    });

    await wrapper.get('[data-testid="screen-name"]').setValue('Main Title Menu');
    await wrapper.get('[data-testid="screen-entry"]').setValue(true);

    expect(wrapper.emitted('update-screen')?.at(-1)?.[0]).toMatchObject({
      id: 'main-menu',
      name: 'Main Title Menu',
    });
    expect(wrapper.emitted('set-entry-screen')?.[0]).toEqual(['main-menu']);
  });
});
```

```ts
import { describe, expect, it } from 'vitest';
import { mount } from '@vue/test-utils';
import TransitionInspector from './TransitionInspector.vue';

describe('TransitionInspector', () => {
  it('emits updates for label, target, and direction hint changes', async () => {
    const wrapper = mount(TransitionInspector, {
      props: {
        transition: {
          id: 'main-menu-continue',
          sourceScreenId: 'main-menu',
          targetScreenId: 'continue-game',
          label: 'Continue',
          notes: '',
          condition: null,
          directionHint: null,
        },
        screens: [
          {
            id: 'main-menu',
            name: 'Main Menu',
            notes: '',
            position: { x: 100, y: 100 },
            isEntry: true,
          },
          {
            id: 'options-menu',
            name: 'Options',
            notes: '',
            position: { x: 300, y: 100 },
            isEntry: false,
          },
        ],
        siblingTransitions: [
          {
            id: 'main-menu-start',
            sourceScreenId: 'main-menu',
            targetScreenId: 'start-game',
            label: 'Start',
            notes: '',
            condition: null,
            directionHint: null,
          },
        ],
        variables: [
          {
            id: 'has-save',
            name: 'Has Save',
            type: 'boolean',
            description: 'Whether a save file exists.',
          },
        ],
      },
    });

    await wrapper.get('[data-testid="transition-label"]').setValue('Continue Story');
    await wrapper.get('[data-testid="transition-target"]').setValue('options-menu');
    await wrapper.get('[data-testid="direction-kind"]').setValue('down');
    await wrapper.get('[data-testid="direction-relative"]').setValue('main-menu-start');

    expect(wrapper.emitted('update-transition')?.at(-1)?.[0]).toMatchObject({
      id: 'main-menu-continue',
      label: 'Continue Story',
      targetScreenId: 'options-menu',
      directionHint: {
        direction: 'down',
        relativeToTransitionId: 'main-menu-start',
      },
    });
  });
});
```

- [ ] **Step 2: Run the editor component tests to verify they fail**

```powershell
Set-Location C:\dev\helenui
npm test -- src/components/variables/VariablePanel.test.ts src/components/conditions/ConditionEditor.test.ts src/components/inspector/ScreenInspector.test.ts src/components/inspector/TransitionInspector.test.ts
```

Expected: FAIL because the component files do not exist yet.

- [ ] **Step 3: Implement the variable panel**

```vue
<script setup lang="ts">
import type { ProjectVariable, VariableType } from '../../domain/navigationProject';

defineProps<{
  variables: ProjectVariable[];
}>();

const emit = defineEmits<{
  'create-variable': [type: VariableType];
  'update-variable': [variable: ProjectVariable];
  'delete-variable': [variableId: string];
}>();
</script>

<template>
  <section class="variable-panel">
    <header class="panel-header">
      <h2>Variables</h2>
      <div class="variable-actions">
        <button data-testid="create-boolean-variable" type="button" @click="emit('create-variable', 'boolean')">+ Boolean</button>
        <button data-testid="create-number-variable" type="button" @click="emit('create-variable', 'number')">+ Number</button>
        <button data-testid="create-string-variable" type="button" @click="emit('create-variable', 'string')">+ String</button>
        <button data-testid="create-enum-variable" type="button" @click="emit('create-variable', 'enum')">+ Enum</button>
      </div>
    </header>

    <article v-for="variable in variables" :key="variable.id" class="variable-card">
      <label>
        Name
        <input
          :data-testid="`variable-name-${variable.id}`"
          :value="variable.name"
          @input="emit('update-variable', { ...variable, name: ($event.target as HTMLInputElement).value })"
        />
      </label>
      <label>
        Description
        <textarea
          :data-testid="`variable-description-${variable.id}`"
          :value="variable.description"
          @input="emit('update-variable', { ...variable, description: ($event.target as HTMLTextAreaElement).value })"
        />
      </label>
      <label v-if="variable.type === 'enum'">
        Enum Values
        <input
          :data-testid="`variable-enum-values-${variable.id}`"
          :value="variable.enumValues.join(', ')"
          @input="emit('update-variable', {
            ...variable,
            enumValues: ($event.target as HTMLInputElement).value
              .split(',')
              .map((value) => value.trim())
              .filter((value) => value.length > 0),
          })"
        />
      </label>
      <button type="button" @click="emit('delete-variable', variable.id)">Delete</button>
    </article>
  </section>
</template>
```

- [ ] **Step 4: Implement the recursive condition editor**

```vue
<script setup lang="ts">
import { computed } from 'vue';
import type { NavigationCondition, NavigationConditionRule, ProjectVariable } from '../../domain/navigationProject';

defineOptions({
  name: 'ConditionEditor',
});

const props = withDefaults(
  defineProps<{
    modelValue: NavigationCondition | null;
    variables: ProjectVariable[];
  }>(),
  {
    modelValue: null,
  },
);

const emit = defineEmits<{
  'update:modelValue': [value: NavigationCondition | null];
}>();

const canAddRule = computed(() => props.variables.length > 0);

function buildDefaultRuleForVariable(variable: ProjectVariable): NavigationConditionRule {
  if (variable.type === 'boolean') {
    return {
      kind: 'rule',
      variableId: variable.id,
      operator: 'equals',
      value: true,
    };
  }

  if (variable.type === 'number') {
    return {
      kind: 'rule',
      variableId: variable.id,
      operator: 'equals',
      value: 0,
    };
  }

  if (variable.type === 'enum') {
    return {
      kind: 'rule',
      variableId: variable.id,
      operator: 'equals',
      value: variable.enumValues[0],
    };
  }

  return {
    kind: 'rule',
    variableId: variable.id,
    operator: 'contains',
    value: '',
  };
}

function buildDefaultRule(): NavigationConditionRule {
  return buildDefaultRuleForVariable(props.variables[0]);
}

function findVariable(variableId: string): ProjectVariable | undefined {
  return props.variables.find((variable) => variable.id === variableId);
}

function parseRuleValue(rule: NavigationConditionRule, rawValue: string): boolean | number | string {
  const variable = findVariable(rule.variableId);

  if (!variable) {
    return rawValue;
  }

  if (variable.type === 'boolean') {
    return rawValue === 'true';
  }

  if (variable.type === 'number') {
    return Number(rawValue);
  }

  return rawValue;
}

function allowedOperators(variableId: string): NavigationConditionRule['operator'][] {
  const variable = findVariable(variableId);

  if (!variable || variable.type === 'number') {
    return ['equals', 'notEquals', 'greaterThan', 'greaterThanOrEqual', 'lessThan', 'lessThanOrEqual'];
  }

  if (variable.type === 'boolean' || variable.type === 'enum') {
    return ['equals', 'notEquals'];
  }

  return ['equals', 'notEquals', 'contains'];
}

function wrapInGroup(nextRule: NavigationConditionRule): NavigationCondition {
  if (props.modelValue && props.modelValue.kind !== 'rule') {
    return {
      ...props.modelValue,
      rules: [...props.modelValue.rules, nextRule],
    };
  }

  return {
    kind: 'all',
    rules: [nextRule],
  };
}
</script>

<template>
  <section class="condition-editor">
    <template v-if="modelValue === null">
      <button data-testid="add-condition-rule" type="button" :disabled="!canAddRule" @click="emit('update:modelValue', wrapInGroup(buildDefaultRule()))">
        Add Condition
      </button>
    </template>

    <template v-else-if="modelValue.kind === 'rule'">
      <div class="condition-row">
        <select
          :value="modelValue.variableId"
          @change="
            emit(
              'update:modelValue',
              buildDefaultRuleForVariable(findVariable(($event.target as HTMLSelectElement).value) ?? props.variables[0]),
            )
          "
        >
          <option v-for="variable in variables" :key="variable.id" :value="variable.id">{{ variable.name }}</option>
        </select>
        <select
          :value="modelValue.operator"
          @change="emit('update:modelValue', { ...modelValue, operator: ($event.target as HTMLSelectElement).value as NavigationConditionRule['operator'] })"
        >
          <option v-for="operator in allowedOperators(modelValue.variableId)" :key="operator" :value="operator">{{ operator }}</option>
        </select>
        <input
          :value="String(modelValue.value)"
          @input="emit('update:modelValue', { ...modelValue, value: parseRuleValue(modelValue, ($event.target as HTMLInputElement).value) })"
        />
        <button type="button" @click="emit('update:modelValue', null)">Remove</button>
      </div>
    </template>

    <template v-else>
      <div class="condition-group">
        <header class="condition-group-header">
          <select
            :value="modelValue.kind"
            @change="emit('update:modelValue', { ...modelValue, kind: ($event.target as HTMLSelectElement).value as 'all' | 'any' })"
          >
            <option value="all">all</option>
            <option value="any">any</option>
          </select>
          <button data-testid="add-condition-rule" type="button" :disabled="!canAddRule" @click="emit('update:modelValue', { ...modelValue, rules: [...modelValue.rules, buildDefaultRule()] })">
            + Rule
          </button>
        </header>

        <ConditionEditor
          v-for="(rule, index) in modelValue.rules"
          :key="index"
          :model-value="rule"
          :variables="variables"
          @update:model-value="
            emit(
              'update:modelValue',
              $event === null
                ? { ...modelValue, rules: modelValue.rules.filter((_, ruleIndex) => ruleIndex !== index) }
                : {
                    ...modelValue,
                    rules: modelValue.rules.map((currentRule, ruleIndex) => (ruleIndex === index ? $event : currentRule)),
                  },
            )
          "
        />
      </div>
    </template>
  </section>
</template>
```

- [ ] **Step 5: Implement the screen and transition inspectors**

```vue
<script setup lang="ts">
import type { NavigationScreen } from '../../domain/navigationProject';

defineProps<{
  screen: NavigationScreen | null;
}>();

const emit = defineEmits<{
  'update-screen': [screen: NavigationScreen];
  'delete-screen': [screenId: string];
  'set-entry-screen': [screenId: string];
}>();
</script>

<template>
  <section v-if="screen" class="inspector-panel">
    <h2>Screen</h2>
    <label>
      Name
      <input
        data-testid="screen-name"
        :value="screen.name"
        @input="emit('update-screen', { ...screen, name: ($event.target as HTMLInputElement).value })"
      />
    </label>
    <label>
      Notes
      <textarea
        :value="screen.notes"
        @input="emit('update-screen', { ...screen, notes: ($event.target as HTMLTextAreaElement).value })"
      />
    </label>
    <label>
      <input
        data-testid="screen-entry"
        type="checkbox"
        :checked="screen.isEntry"
        @change="emit('set-entry-screen', screen.id)"
      />
      Entry Screen
    </label>
    <button type="button" @click="emit('delete-screen', screen.id)">Delete Screen</button>
  </section>
  <section v-else class="inspector-panel">
    <h2>Screen</h2>
    <p>Select a screen to edit it.</p>
  </section>
</template>
```

```vue
<script setup lang="ts">
import ConditionEditor from '../conditions/ConditionEditor.vue';
import type {
  NavigationScreen,
  NavigationTransition,
  ProjectVariable,
} from '../../domain/navigationProject';

defineProps<{
  transition: NavigationTransition | null;
  screens: NavigationScreen[];
  siblingTransitions: NavigationTransition[];
  variables: ProjectVariable[];
}>();

const emit = defineEmits<{
  'update-transition': [transition: NavigationTransition];
  'delete-transition': [transitionId: string];
}>();
</script>

<template>
  <section v-if="transition" class="inspector-panel">
    <h2>Transition</h2>
    <label>
      Label
      <input
        data-testid="transition-label"
        :value="transition.label"
        @input="emit('update-transition', { ...transition, label: ($event.target as HTMLInputElement).value })"
      />
    </label>
    <label>
      Target Screen
      <select
        data-testid="transition-target"
        :value="transition.targetScreenId"
        @change="emit('update-transition', { ...transition, targetScreenId: ($event.target as HTMLSelectElement).value })"
      >
        <option v-for="screen in screens" :key="screen.id" :value="screen.id">{{ screen.name }}</option>
      </select>
    </label>
    <label>
      Notes
      <textarea
        :value="transition.notes"
        @input="emit('update-transition', { ...transition, notes: ($event.target as HTMLTextAreaElement).value })"
      />
    </label>

    <fieldset class="direction-hint-group">
      <legend>Direction Hint</legend>
      <select
        data-testid="direction-kind"
        :value="transition.directionHint?.direction ?? ''"
        @change="emit('update-transition', {
          ...transition,
          directionHint: ($event.target as HTMLSelectElement).value === ''
            ? null
            : {
                direction: ($event.target as HTMLSelectElement).value as 'up' | 'down' | 'left' | 'right',
                relativeToTransitionId: transition.directionHint?.relativeToTransitionId ?? '',
              },
        })"
      >
        <option value="">None</option>
        <option value="up">up</option>
        <option value="down">down</option>
        <option value="left">left</option>
        <option value="right">right</option>
      </select>
      <select
        data-testid="direction-relative"
        :value="transition.directionHint?.relativeToTransitionId ?? ''"
        @change="emit('update-transition', {
          ...transition,
          directionHint: {
            direction: transition.directionHint?.direction ?? 'down',
            relativeToTransitionId: ($event.target as HTMLSelectElement).value,
          },
        })"
      >
        <option value="">Choose sibling</option>
        <option v-for="siblingTransition in siblingTransitions" :key="siblingTransition.id" :value="siblingTransition.id">
          {{ siblingTransition.label }}
        </option>
      </select>
    </fieldset>

    <fieldset class="condition-group">
      <legend>Condition</legend>
      <ConditionEditor
        :model-value="transition.condition"
        :variables="variables"
        @update:model-value="emit('update-transition', { ...transition, condition: $event })"
      />
    </fieldset>

    <button type="button" @click="emit('delete-transition', transition.id)">Delete Transition</button>
  </section>
  <section v-else class="inspector-panel">
    <h2>Transition</h2>
    <p>Select a transition to edit it.</p>
  </section>
</template>
```

- [ ] **Step 6: Run the editor component tests to verify they pass**

```powershell
Set-Location C:\dev\helenui
npm test -- src/components/variables/VariablePanel.test.ts src/components/conditions/ConditionEditor.test.ts src/components/inspector/ScreenInspector.test.ts src/components/inspector/TransitionInspector.test.ts
```

Expected: PASS with `4 passed`.

- [ ] **Step 7: Commit**

```powershell
Set-Location C:\dev\helenui
git add src/components/variables/VariablePanel.vue src/components/variables/VariablePanel.test.ts src/components/conditions/ConditionEditor.vue src/components/conditions/ConditionEditor.test.ts src/components/inspector/ScreenInspector.vue src/components/inspector/ScreenInspector.test.ts src/components/inspector/TransitionInspector.vue src/components/inspector/TransitionInspector.test.ts
git commit -m "feat: add navigation editing panels"
```

### Task 6: Add the Graph Canvas and Flow Mapping

**Files:**
- Create: `C:\dev\helenui\src\graph\flowElements.ts`
- Create: `C:\dev\helenui\src\graph\flowElements.test.ts`
- Create: `C:\dev\helenui\src\components\graph\NavigationCanvas.vue`
- Create: `C:\dev\helenui\src\components\graph\NavigationCanvas.test.ts`

- [ ] **Step 1: Write the failing graph tests**

```ts
import { describe, expect, it } from 'vitest';
import { toFlowEdges, toFlowNodes } from './flowElements';

describe('flowElements', () => {
  it('maps screens and transitions to Vue Flow elements', () => {
    const nodes = toFlowNodes([
      {
        id: 'title-screen',
        name: 'Title Screen',
        notes: '',
        position: { x: 120, y: 80 },
        isEntry: true,
      },
    ]);
    const edges = toFlowEdges([
      {
        id: 'title-to-main-menu',
        sourceScreenId: 'title-screen',
        targetScreenId: 'main-menu',
        label: 'Press Start',
        notes: '',
        condition: null,
        directionHint: null,
      },
    ]);

    expect(nodes[0]).toMatchObject({
      id: 'title-screen',
      position: { x: 120, y: 80 },
      data: { label: 'Title Screen', isEntry: true },
    });
    expect(edges[0]).toMatchObject({
      id: 'title-to-main-menu',
      source: 'title-screen',
      target: 'main-menu',
      label: 'Press Start',
    });
  });
});
```

```ts
import { describe, expect, it } from 'vitest';
import { mount } from '@vue/test-utils';
import NavigationCanvas from './NavigationCanvas.vue';

describe('NavigationCanvas', () => {
  it('emits a screen creation request on canvas double click', async () => {
    const wrapper = mount(NavigationCanvas, {
      props: {
        screens: [],
        transitions: [],
      },
      global: {
        stubs: {
          VueFlow: {
            template: '<div data-testid="vue-flow"><slot /></div>',
          },
          Background: true,
          Controls: true,
        },
      },
    });

    await wrapper.get('[data-testid="canvas-root"]').trigger('dblclick', {
      clientX: 240,
      clientY: 180,
    });

    expect(wrapper.emitted('create-screen')?.[0][0]).toEqual({
      x: 240,
      y: 180,
    });
  });
});
```

- [ ] **Step 2: Run the graph tests to verify they fail**

```powershell
Set-Location C:\dev\helenui
npm test -- src/graph/flowElements.test.ts src/components/graph/NavigationCanvas.test.ts
```

Expected: FAIL because the graph mapping module and canvas component do not exist yet.

- [ ] **Step 3: Implement flow element mapping**

```ts
import type { Edge, Node } from '@vue-flow/core';
import type { NavigationScreen, NavigationTransition } from '../domain/navigationProject';

export function toFlowNodes(screens: NavigationScreen[]): Node[] {
  return screens.map((screen) => ({
    id: screen.id,
    position: screen.position,
    data: {
      label: screen.name,
      isEntry: screen.isEntry,
    },
    type: 'default',
  }));
}

export function toFlowEdges(transitions: NavigationTransition[]): Edge[] {
  return transitions.map((transition) => ({
    id: transition.id,
    source: transition.sourceScreenId,
    target: transition.targetScreenId,
    label: transition.label,
  }));
}
```

- [ ] **Step 4: Implement the navigation canvas**

```vue
<script setup lang="ts">
import { computed } from 'vue';
import { Background } from '@vue-flow/background';
import { Controls } from '@vue-flow/controls';
import { VueFlow } from '@vue-flow/core';
import type { Connection, EdgeMouseEvent, NodeDragEvent, NodeMouseEvent } from '@vue-flow/core';
import type { NavigationScreen, NavigationTransition, ScreenPosition } from '../../domain/navigationProject';
import { toFlowEdges, toFlowNodes } from '../../graph/flowElements';

const props = defineProps<{
  screens: NavigationScreen[];
  transitions: NavigationTransition[];
}>();

const emit = defineEmits<{
  'create-screen': [position: ScreenPosition];
  'create-transition': [input: { sourceScreenId: string; targetScreenId: string }];
  'select-screen': [screenId: string];
  'select-transition': [transitionId: string];
  'move-screen': [input: { screenId: string; position: ScreenPosition }];
}>();

const nodes = computed(() => toFlowNodes(props.screens));
const edges = computed(() => toFlowEdges(props.transitions));

function handleCanvasDoubleClick(event: MouseEvent): void {
  emit('create-screen', {
    x: event.clientX,
    y: event.clientY,
  });
}

function handleConnect(connection: Connection): void {
  if (!connection.source || !connection.target) {
    return;
  }

  emit('create-transition', {
    sourceScreenId: connection.source,
    targetScreenId: connection.target,
  });
}

function handleNodeClick(event: NodeMouseEvent): void {
  emit('select-screen', event.node.id);
}

function handleEdgeClick(event: EdgeMouseEvent): void {
  emit('select-transition', event.edge.id);
}

function handleNodeDragStop(event: NodeDragEvent): void {
  emit('move-screen', {
    screenId: event.node.id,
    position: {
      x: event.node.position.x,
      y: event.node.position.y,
    },
  });
}
</script>

<template>
  <div data-testid="canvas-root" class="canvas-root" @dblclick="handleCanvasDoubleClick">
    <VueFlow
      :nodes="nodes"
      :edges="edges"
      fit-view-on-init
      class="navigation-flow"
      @connect="handleConnect"
      @node-click="handleNodeClick"
      @edge-click="handleEdgeClick"
      @node-drag-stop="handleNodeDragStop"
    >
      <Background />
      <Controls />
    </VueFlow>
  </div>
</template>
```

- [ ] **Step 5: Run the graph tests to verify they pass**

```powershell
Set-Location C:\dev\helenui
npm test -- src/graph/flowElements.test.ts src/components/graph/NavigationCanvas.test.ts
```

Expected: PASS with `2 passed`.

- [ ] **Step 6: Commit**

```powershell
Set-Location C:\dev\helenui
git add src/graph/flowElements.ts src/graph/flowElements.test.ts src/components/graph/NavigationCanvas.vue src/components/graph/NavigationCanvas.test.ts
git commit -m "feat: add graph canvas"
```

### Task 7: Compose the App Shell, Batman Seed, Toolbar, and Validation Panel

**Files:**
- Create: `C:\dev\helenui\src\components\toolbar\ProjectToolbar.vue`
- Create: `C:\dev\helenui\src\components\toolbar\ProjectToolbar.test.ts`
- Create: `C:\dev\helenui\src\components\validation\ValidationPanel.vue`
- Create: `C:\dev\helenui\src\components\validation\ValidationPanel.test.ts`
- Create: `C:\dev\helenui\src\seeds\createBatmanSampleProject.ts`
- Create: `C:\dev\helenui\src\components\layout\AppShell.vue`
- Create: `C:\dev\helenui\src\components\layout\AppShell.test.ts`
- Modify: `C:\dev\helenui\src\App.vue`
- Modify: `C:\dev\helenui\src\App.test.ts`
- Modify: `C:\dev\helenui\src\style.css`

- [ ] **Step 1: Write the failing integration tests**

```ts
import { describe, expect, it } from 'vitest';
import { mount } from '@vue/test-utils';
import ProjectToolbar from './ProjectToolbar.vue';

describe('ProjectToolbar', () => {
  it('emits load and export commands', async () => {
    const wrapper = mount(ProjectToolbar);

    await wrapper.get('[data-testid="load-batman-seed"]').trigger('click');
    await wrapper.get('[data-testid="export-project"]').trigger('click');

    expect(wrapper.emitted('load-batman-seed')?.length).toBe(1);
    expect(wrapper.emitted('export-project')?.length).toBe(1);
  });
});
```

```ts
import { describe, expect, it } from 'vitest';
import { mount } from '@vue/test-utils';
import ValidationPanel from './ValidationPanel.vue';

describe('ValidationPanel', () => {
  it('renders validation issues', () => {
    const wrapper = mount(ValidationPanel, {
      props: {
        issues: [
          {
            code: 'missing-entry-screen',
            severity: 'error',
            subjectType: 'project',
            message: 'A navigation project must have exactly one entry screen.',
          },
        ],
      },
    });

    expect(wrapper.text()).toContain('missing-entry-screen');
    expect(wrapper.text()).toContain('A navigation project must have exactly one entry screen.');
  });
});
```

```ts
import { beforeEach, describe, expect, it } from 'vitest';
import { createPinia, setActivePinia } from 'pinia';
import { mount } from '@vue/test-utils';
import AppShell from './AppShell.vue';
import { useNavigationProjectStore } from '../../store/navigationProjectStore';

describe('AppShell', () => {
  beforeEach(() => {
    setActivePinia(createPinia());
  });

  it('wires canvas events into the store', async () => {
    const wrapper = mount(AppShell, {
      global: {
        stubs: {
          NavigationCanvas: {
            template: `
              <div>
                <button data-testid="emit-create-screen" @click="$emit('create-screen', { x: 100, y: 100 })">Add Screen</button>
                <button data-testid="emit-create-transition" @click="$emit('create-transition', { sourceScreenId: 'title-screen', targetScreenId: 'main-menu' })">Add Transition</button>
              </div>
            `,
          },
        },
      },
    });

    const store = useNavigationProjectStore();
    store.replaceProject({
      project: {
        schemaVersion: 1,
        id: 'batman-sample',
        name: 'Batman Navigation Sample',
        description: '',
      },
      screens: [
        {
          id: 'title-screen',
          name: 'Title Screen',
          notes: '',
          position: { x: 100, y: 100 },
          isEntry: true,
        },
        {
          id: 'main-menu',
          name: 'Main Menu',
          notes: '',
          position: { x: 300, y: 100 },
          isEntry: false,
        },
      ],
      transitions: [],
      variables: [],
    });

    await wrapper.get('[data-testid="emit-create-screen"]').trigger('click');
    await wrapper.get('[data-testid="emit-create-transition"]').trigger('click');

    expect(store.project.screens).toHaveLength(3);
    expect(store.project.transitions).toHaveLength(1);
  });
});
```

```ts
import { describe, expect, it } from 'vitest';
import { mount } from '@vue/test-utils';
import App from './App.vue';

describe('App', () => {
  it('renders the full editor with the Batman seed loaded', async () => {
    const wrapper = mount(App);
    await wrapper.vm.$nextTick();

    expect(wrapper.text()).toContain('Batman Navigation Sample');
    expect(wrapper.get('[data-testid="graph-region"]').exists()).toBe(true);
    expect(wrapper.get('[data-testid="variable-region"]').exists()).toBe(true);
    expect(wrapper.get('[data-testid="validation-region"]').exists()).toBe(true);
  });
});
```

- [ ] **Step 2: Run the integration tests to verify they fail**

```powershell
Set-Location C:\dev\helenui
npm test -- src/components/toolbar/ProjectToolbar.test.ts src/components/validation/ValidationPanel.test.ts src/components/layout/AppShell.test.ts src/App.test.ts
```

Expected: FAIL because the shell, toolbar, validation panel, and Batman seed do not exist yet.

- [ ] **Step 3: Implement the toolbar, validation panel, and Batman seed**

```vue
<script setup lang="ts">
const emit = defineEmits<{
  'load-batman-seed': [];
  'export-project': [];
  'import-project-json': [jsonText: string];
}>();

async function handleImport(event: Event): Promise<void> {
  const input = event.target as HTMLInputElement;
  const file = input.files?.[0];

  if (!file) {
    return;
  }

  emit('import-project-json', await file.text());
  input.value = '';
}
</script>

<template>
  <header class="project-toolbar">
    <button data-testid="load-batman-seed" type="button" @click="emit('load-batman-seed')">Load Batman Seed</button>
    <label class="toolbar-file-input">
      Import JSON
      <input type="file" accept="application/json" @change="handleImport" />
    </label>
    <button data-testid="export-project" type="button" @click="emit('export-project')">Export JSON</button>
  </header>
</template>
```

```vue
<script setup lang="ts">
import type { ValidationIssue } from '../../domain/validation';

defineProps<{
  issues: ValidationIssue[];
}>();
</script>

<template>
  <section data-testid="validation-region" class="validation-panel">
    <h2>Validation</h2>
    <p v-if="issues.length === 0">No validation issues.</p>
    <ul v-else>
      <li v-for="issue in issues" :key="`${issue.code}-${issue.subjectId ?? 'project'}`">
        <strong>{{ issue.severity.toUpperCase() }}</strong>
        <span>{{ issue.code }}</span>
        <span>{{ issue.message }}</span>
      </li>
    </ul>
  </section>
</template>
```

```ts
import type { NavigationProject } from '../domain/navigationProject';

export function createBatmanSampleProject(): NavigationProject {
  return {
    project: {
      schemaVersion: 1,
      id: 'batman-sample',
      name: 'Batman Navigation Sample',
      description: 'Batman-style title, menu, and options navigation flow.',
    },
    screens: [
      {
        id: 'title-screen',
        name: 'Title Screen',
        notes: 'Press Start moves into the main menu.',
        position: { x: 80, y: 120 },
        isEntry: true,
      },
      {
        id: 'main-menu',
        name: 'Main Menu',
        notes: 'Primary list of selectable menu options.',
        position: { x: 380, y: 120 },
        isEntry: false,
      },
      {
        id: 'options-menu',
        name: 'Options',
        notes: 'Sub-menu for settings.',
        position: { x: 700, y: 80 },
        isEntry: false,
      },
      {
        id: 'audio-menu',
        name: 'Audio',
        notes: 'Audio-related settings.',
        position: { x: 1000, y: 40 },
        isEntry: false,
      },
      {
        id: 'subtitles-menu',
        name: 'Subtitles',
        notes: 'Subtitle toggle settings.',
        position: { x: 1000, y: 220 },
        isEntry: false,
      },
      {
        id: 'continue-game',
        name: 'Continue Game',
        notes: 'Visible only when a save file exists.',
        position: { x: 700, y: 260 },
        isEntry: false,
      },
    ],
    transitions: [
      {
        id: 'title-to-main-menu',
        sourceScreenId: 'title-screen',
        targetScreenId: 'main-menu',
        label: 'Press Start',
        notes: '',
        condition: null,
        directionHint: null,
      },
      {
        id: 'main-menu-options',
        sourceScreenId: 'main-menu',
        targetScreenId: 'options-menu',
        label: 'Options',
        notes: '',
        condition: null,
        directionHint: null,
      },
      {
        id: 'options-audio',
        sourceScreenId: 'options-menu',
        targetScreenId: 'audio-menu',
        label: 'Audio',
        notes: '',
        condition: null,
        directionHint: null,
      },
      {
        id: 'options-subtitles',
        sourceScreenId: 'options-menu',
        targetScreenId: 'subtitles-menu',
        label: 'Subtitles',
        notes: '',
        condition: null,
        directionHint: {
          direction: 'down',
          relativeToTransitionId: 'options-audio',
        },
      },
      {
        id: 'main-menu-continue',
        sourceScreenId: 'main-menu',
        targetScreenId: 'continue-game',
        label: 'Continue',
        notes: '',
        condition: {
          kind: 'all',
          rules: [
            {
              kind: 'rule',
              variableId: 'has-save',
              operator: 'equals',
              value: true,
            },
          ],
        },
        directionHint: {
          direction: 'down',
          relativeToTransitionId: 'main-menu-options',
        },
      },
    ],
    variables: [
      {
        id: 'has-save',
        name: 'Has Save',
        type: 'boolean',
        description: 'Whether a save file exists.',
      },
    ],
  };
}
```

- [ ] **Step 4: Implement the app shell and wire it to the store**

```vue
<script setup lang="ts">
import { computed } from 'vue';
import NavigationCanvas from '../graph/NavigationCanvas.vue';
import VariablePanel from '../variables/VariablePanel.vue';
import ScreenInspector from '../inspector/ScreenInspector.vue';
import TransitionInspector from '../inspector/TransitionInspector.vue';
import ProjectToolbar from '../toolbar/ProjectToolbar.vue';
import ValidationPanel from '../validation/ValidationPanel.vue';
import { useNavigationProjectStore } from '../../store/navigationProjectStore';
import { parseNavigationProjectJson, stringifyNavigationProjectJson } from '../../services/projectSerializer';
import { createBatmanSampleProject } from '../../seeds/createBatmanSampleProject';

const store = useNavigationProjectStore();

const siblingTransitions = computed(() => {
  if (!store.selectedTransition) {
    return [];
  }

  return store.project.transitions.filter(
    (transition) =>
      transition.sourceScreenId === store.selectedTransition.sourceScreenId &&
      transition.id !== store.selectedTransition.id,
  );
});

function exportProject(): void {
  const projectJson = stringifyNavigationProjectJson(store.project);
  const blob = new Blob([projectJson], { type: 'application/json;charset=utf-8' });
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');

  link.href = url;
  link.download = `${store.project.project.id}.json`;
  link.click();

  URL.revokeObjectURL(url);
}
</script>

<template>
  <div class="editor-shell">
    <ProjectToolbar
      @load-batman-seed="store.replaceProject(createBatmanSampleProject())"
      @export-project="exportProject"
      @import-project-json="store.replaceProject(parseNavigationProjectJson($event))"
    />

    <header class="project-summary">
      <h1>{{ store.project.project.name }}</h1>
      <p>{{ store.project.project.description }}</p>
    </header>

    <aside data-testid="variable-region" class="panel panel-left">
      <VariablePanel
        :variables="store.project.variables"
        @create-variable="store.createVariable($event)"
        @update-variable="store.updateVariable($event)"
        @delete-variable="store.deleteVariable($event)"
      />
    </aside>

    <section data-testid="graph-region" class="panel panel-center">
      <NavigationCanvas
        :screens="store.project.screens"
        :transitions="store.project.transitions"
        @create-screen="store.createScreen($event)"
        @create-transition="store.createTransition({ ...$event, label: 'New Option' })"
        @select-screen="store.selection = { kind: 'screen', id: $event }"
        @select-transition="store.selection = { kind: 'transition', id: $event }"
        @move-screen="
          store.updateScreen({
            ...(store.project.screens.find((screen) => screen.id === $event.screenId)!),
            position: $event.position,
          })
        "
      />
    </section>

    <aside class="panel panel-right">
      <ScreenInspector
        :screen="store.selectedScreen"
        @update-screen="store.updateScreen($event)"
        @set-entry-screen="store.setEntryScreen($event)"
        @delete-screen="store.deleteScreen($event)"
      />
      <TransitionInspector
        :transition="store.selectedTransition"
        :screens="store.project.screens"
        :sibling-transitions="siblingTransitions"
        :variables="store.project.variables"
        @update-transition="store.updateTransition($event)"
        @delete-transition="store.deleteTransition($event)"
      />
    </aside>

    <footer class="panel panel-bottom">
      <ValidationPanel :issues="store.validationIssues" />
    </footer>
  </div>
</template>
```

```vue
<template>
  <AppShell />
</template>

<script setup lang="ts">
import { onMounted } from 'vue';
import AppShell from './components/layout/AppShell.vue';
import { useNavigationProjectStore } from './store/navigationProjectStore';
import { createBatmanSampleProject } from './seeds/createBatmanSampleProject';

const store = useNavigationProjectStore();

onMounted(() => {
  if (store.project.screens.length === 0) {
    store.replaceProject(createBatmanSampleProject());
  }
});
</script>
```

```css
:root {
  color: #f3f1ea;
  background: #11161d;
  font-family: "Segoe UI", sans-serif;
  --panel-background: rgba(12, 18, 24, 0.9);
  --panel-border: rgba(212, 184, 108, 0.28);
  --panel-shadow: 0 18px 48px rgba(0, 0, 0, 0.28);
}

body {
  margin: 0;
  min-height: 100vh;
  background:
    radial-gradient(circle at top left, rgba(200, 172, 96, 0.18), transparent 28rem),
    radial-gradient(circle at bottom right, rgba(75, 118, 166, 0.18), transparent 30rem),
    linear-gradient(180deg, #151c25 0%, #0c1117 100%);
}

#app {
  min-height: 100vh;
}

.editor-shell {
  display: grid;
  grid-template-columns: 18rem 1fr 22rem;
  grid-template-rows: auto auto 1fr auto;
  min-height: 100vh;
  gap: 1rem;
  padding: 1rem;
}

.project-toolbar,
.project-summary,
.panel {
  border: 1px solid var(--panel-border);
  border-radius: 1rem;
  background: var(--panel-background);
  box-shadow: var(--panel-shadow);
}

.project-toolbar {
  grid-column: 1 / -1;
  display: flex;
  gap: 0.75rem;
  align-items: center;
  padding: 0.75rem 1rem;
}

.project-summary {
  grid-column: 1 / -1;
  padding: 1rem;
}

.panel {
  min-height: 0;
  overflow: auto;
  padding: 1rem;
}

.panel-bottom {
  grid-column: 1 / -1;
}
```

- [ ] **Step 5: Run the integration tests to verify they pass**

```powershell
Set-Location C:\dev\helenui
npm test -- src/components/toolbar/ProjectToolbar.test.ts src/components/validation/ValidationPanel.test.ts src/components/layout/AppShell.test.ts src/App.test.ts
```

Expected: PASS with `4 passed`.

- [ ] **Step 6: Commit**

```powershell
Set-Location C:\dev\helenui
git add src/components/toolbar/ProjectToolbar.vue src/components/toolbar/ProjectToolbar.test.ts src/components/validation/ValidationPanel.vue src/components/validation/ValidationPanel.test.ts src/seeds/createBatmanSampleProject.ts src/components/layout/AppShell.vue src/components/layout/AppShell.test.ts src/App.vue src/App.test.ts src/style.css
git commit -m "feat: compose helenui editor shell"
```

### Task 8: Run Full Verification

**Files:**
- Verify: `C:\dev\helenui\src\**\*.ts`
- Verify: `C:\dev\helenui\src\**\*.vue`

- [ ] **Step 1: Run the full test suite**

```powershell
Set-Location C:\dev\helenui
npm test
```

Expected: PASS with every planned test file green.

- [ ] **Step 2: Run the production build**

```powershell
Set-Location C:\dev\helenui
npm run build
```

Expected: PASS with Vite build output in `dist`.

- [ ] **Step 3: Launch the local development server for manual review**

```powershell
Set-Location C:\dev\helenui
npm run dev -- --host 127.0.0.1 --port 4173
```

Expected: a local URL such as `http://127.0.0.1:4173/`.

- [ ] **Step 4: Manually verify the core flows**

```text
1. Double-click the canvas and confirm a new screen appears.
2. Drag a connection between two screens and confirm a new transition appears.
3. Select a screen and confirm the name and entry toggle update the JSON model.
4. Select a transition and confirm the label, target, condition, and direction hint update the JSON model.
5. Add a boolean variable and confirm it is available in the condition editor.
6. Export the project and confirm the JSON contains screens, transitions, and variables.
7. Import the exported JSON and confirm the same graph reloads.
8. Load the Batman seed and confirm the validation panel is empty.
```

- [ ] **Step 5: Commit the verified editor**

```powershell
Set-Location C:\dev\helenui
git add .
git commit -m "feat: deliver helenui navigation designer"
```
