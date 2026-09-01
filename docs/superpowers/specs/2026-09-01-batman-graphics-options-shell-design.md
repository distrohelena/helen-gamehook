# Batman Graphics Options Shell Design

## Goal

Restore the smallest known-working Batman: Arkham Asylum frontend experiment: a `Graphics Options` entry that opens a dedicated screen and returns safely through Back. This checkpoint proves package generation, frontend injection, menu navigation, and pack deployment independently from graphics-setting behavior.

## Background

The repository tag `working-batman-graphics-options-2026-04-23` at commit `5b0bae9` identifies a revision where the custom graphics screen appeared in the retail Steam GOTY frontend. Later work combined that visible shell with live INI loading, editable draft state, custom `Helen_*` callbacks, Apply behavior, an exit prompt, and extensive layout changes.

The existing design records show that opening the screen did not reliably execute the custom callback route. As a result, the full experiment could display stale values, reject row edits, fail to apply changes, or leave navigation and focus in an unstable state. Those behaviors are outside this checkpoint.

## Scope

The shell package will provide only these user-visible behaviors:

1. A `Graphics Options` entry appears in Batman's frontend Options menu.
2. Activating the entry opens the dedicated `Graphics Options` screen.
3. The screen displays a fixed, non-editable representation of the intended option rows.
4. Back returns to the parent Options menu without an Apply/Discard prompt.
5. Input remains usable after returning to the parent menu.

## Non-Goals

- Reading graphics values from `BmEngine.ini`
- Editing graphics values
- Applying or saving graphics values
- Resolution enumeration or display-mode changes
- Draft/baseline state tracking
- Apply/Discard prompts
- Runtime `Helen_GetInt`, `Helen_SetInt`, `Helen_RunCommand`, or graphics-specific callback hooks
- Final visual polish of the graphics screen
- Combining this checkpoint with the Skip Videos pack

## Considered Approaches

### Reuse the old generated delta unchanged

This is the fastest route to the previously visible screen, but it does not prove that the repository can reproduce the asset. It also makes the package's provenance and relationship to the current builder difficult to verify.

### Repair the complete current graphics experiment

This preserves all intended functionality, but it couples basic visibility and navigation to the known-broken callback, Apply, prompt, and focus paths. A failure would not identify which layer is responsible.

### Reproduce the known-working shell from source

Use the tagged revision as behavioral evidence while generating a new frontend asset from the verified retail base through the repository's builder. Add an explicit shell-only build path that omits all runtime graphics behavior.

This is the chosen approach because it gives a trustworthy package and a narrow live test. It preserves the successful part of the old experiment without restoring its unstable runtime dependencies.

## Package Architecture

The existing `batman-aa-graphics-options` pack remains the owner of the experiment. Its Steam GOTY build will be regenerated in shell mode for this checkpoint instead of introducing a second graphics pack that targets the same frontend file.

The shell build contains one virtual file:

- `BmGame/CookedPC/Maps/Frontend/Frontend.umap`

The virtual file is produced as a delta against the documented stock retail `Frontend.umap`. The build manifest records the base size and SHA-256, delta parameters, and generated target size and SHA-256.

The shell build does not declare graphics startup commands, graphics bindings, configuration keys, hooks, or texture replacement behavior. If the generic pack schema requires an empty manifest file, that file must be explicitly empty rather than retaining dormant graphics behavior.

## Builder Design

The graphics asset builder gains an explicit shell build mode. Shell mode reuses the existing package parsing, GFx export, XML patching, GFx import, and Unreal package writing pipeline, but selects a reduced ActionScript contract.

The reduced contract performs only these operations:

- register the graphics menu entry and graphics screen class
- route activation from the parent Options screen to the graphics screen
- populate fixed labels needed to recognize the screen
- accept Back and call the stock screen-return path
- report the active screen using the stock `FE_SetActiveScreenName` route if required by the retail frontend

It must not contain any `Helen_*` external calls. It must not expose an enabled Apply row or an exit prompt. Option rows may retain presentation clips for layout recognition, but their left/right and activation handlers must be inert.

The full experimental templates may remain in source for later staged work, but the shell output must be selected deliberately and covered by tests so a future rebuild cannot silently restore full behavior.

## Navigation Contract

The parent Options entry follows the stock frontend's focus and activation conventions. Activating it opens `ScreenOptionsGraphics` through the same proven screen transition used by the tagged working revision.

Within `ScreenOptionsGraphics`:

- Back is always available.
- Back immediately invokes the stock return path.
- No unsaved-state check is performed.
- No Apply/Discard prompt can be opened.
- Returning restores control to the parent Options screen.

Failure to locate a required source symbol, screen class, placement, or script insertion point is a build error. The builder must not emit a partially patched frontend asset.

## Compatibility With the Working Subtitle Pack

The currently working subtitle pack virtualizes `BmGame/CookedPC/BmGame.u`. The graphics shell virtualizes `BmGame/CookedPC/Maps/Frontend/Frontend.umap`. Because the paths are distinct, both packs can be enabled for the live test without composing two deltas onto the same base file.

The test install will enable only:

- `batman-aa-subtitles`
- `batman-aa-graphics-options`

The Skip Videos pack remains disabled so startup-video behavior cannot obscure frontend diagnosis.

## Clean-Build and Provenance Requirements

The deploy path must rebuild the shell from a clean temporary builder workspace. It must not use the installed game's previously modified frontend file, an old loose output, or an old generated delta as its source.

Before packaging, verification must prove:

1. The input frontend package matches the expected retail base size and SHA-256.
2. The newly generated target package can be reopened by the package tooling.
3. The generated target contains the graphics menu and screen insertions.
4. The generated target contains the shell script contract.
5. The generated target does not contain prohibited `Helen_*`, Apply, or exit-prompt behavior.
6. Applying the emitted delta to the declared base recreates the target byte-for-byte.
7. The pack repository accepts the manifest and resolves the Steam GOTY build.

Deployment must copy only the newly generated, verified pack output. Existing unrelated files in the game directory are not package inputs.

## Automated Verification

Regression tests will cover:

- the graphics builder's shell-mode selection
- presence of the menu entry, graphics screen, fixed labels, and Back route
- absence of `Helen_GetInt`, `Helen_SetInt`, `Helen_RunCommand`, graphics Apply, and exit-prompt script paths
- absence of graphics commands and bindings in the packaged build
- base and target hash validation
- delta round-trip reconstruction
- coexistence of the subtitle and graphics packs without a duplicate virtual-file path

Tests should inspect generated behavior rather than merely checking that files exist.

## Live Verification

After automated verification passes, deploy the rebuilt pack and configure the runtime with the working subtitle pack plus the graphics shell only. The user will test:

1. Launch Batman.
2. Open the frontend Options menu.
3. Confirm `Graphics Options` appears.
4. Open it and confirm the dedicated screen is visible.
5. Press Back.
6. Confirm the parent Options menu responds normally.
7. Reopen the graphics screen and repeat Back once to catch stale focus or transition state.

The runtime log will then be checked for pack selection, frontend virtualization, delta reconstruction, and file-serving errors.

## Success Criteria

This checkpoint succeeds only when the newly generated pack—not an old installed artifact—shows the Graphics Options entry, opens the screen, and returns safely in the live retail game while the subtitle pack remains enabled.

No graphics setting is expected to change. Editable rows and Apply behavior will be designed as later checkpoints after the shell is proven stable.
