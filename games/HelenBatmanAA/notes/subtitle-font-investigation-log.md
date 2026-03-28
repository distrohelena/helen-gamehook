# Arkham Asylum Subtitle / HUD Font Investigation Log

Date: 2026-03-24

Owner: Codex + Helena

Primary goal: make spoken subtitles, gameplay tips, skip prompts, and related small HUD text readable at `3840x2160`.

Current spoken test line: `Is Commissioner Gordon here yet?`

Current user result after latest test: `literally nothing changed`

## Tooling Setup

- Date: 2026-03-25
- Added local `apitrace 14.0 win32` under `tools/apitrace`
- Reason:
  - the game executable is `32-bit`
  - the game imports `d3d9.dll`
  - current evidence says the intro subtitle path is native fullscreen-movie rendering, not HUD Scaleform
- New helper files:
  - `working/Start-ArkhamBinkTrace.ps1`
  - `working/Open-LatestArkhamTrace.ps1`
  - `working/apitrace-bink-workflow.md`
- Current use:
  - capture the intro Bink subtitle frame
  - inspect the actual draw call, viewport, and scissor state
  - stop guessing at fonts when the likely fault is position/clipping in the native renderer

## Latest Installed Test

- Date: 2026-03-25
- Live package: `BmGame.u`
- Staged build: `working/BmGame-native-hud-largefont-safe.u`
- Manifest: `working/native-hud-largefont-test.manifest.jsonc`
- Install status: copied live and hash-verified

### What This Test Changes

- `RBMCombatManager.DrawHUDMessage`
  - forces every font branch to `Engine.GetLargeFont()`
  - raises the subtitle-path draw scale from `2.0` to `12.0`
  - raises the other draw-scale constants to `6.0` and `8.0`
- `RHUD.DrawHUD`
  - changes its `GetSmallFont()` and `GetMediumFont()` calls to `GetLargeFont()`
- `RHUD.DrawSavingText`
  - changes its `GetMediumFont()` call to `GetLargeFont()`
- `RPlayerController.DrawHUD`
  - changes its `GetMediumFont()` and `GetSmallFont()` calls to `GetLargeFont()`
- Engine config font routes were also reset so `Tiny/Small/Medium/Large` are distinct again, while `SubtitleFontName` and all `AdditionalFontNames` now point to `BmFonts.LargeFont`.

### Why This Attempt Is Different

- This is not another SWF / XML / FFDec text-field patch.
- This is not another `.ini`-only font reroute.
- This is not another `MediumFont` swap.
- It patches the native font-call sites that own:
  - spoken subtitle rendering (`RBMCombatManager.DrawHUDMessage`)
  - skip / help / controller-driven HUD drawing (`RHUD.DrawHUD`, `RPlayerController.DrawHUD`)

### Pending User Test

- Spoken subtitle line: `Is Commissioner Gordon here yet?`
- Gameplay tips / objective text
- Skip prompts
- Interaction / target callouts

## Crash Follow-Up

- Result from Helena:
  - the build crashed when the cutscene started and `Space` was pressed repeatedly
- Strongest root-cause hypothesis:
  - `RBMCombatManager.DrawHUDMessage` had one risky import swap from `Engine.GetAdditionalFont` to `Engine.GetLargeFont`
  - that is not a safe swap because `GetAdditionalFont(...)` takes an index argument and `GetLargeFont()` does not
  - this is the first concrete call-signature mismatch found in the native patch work
- Action:
  - remove that import patch
  - keep only same-signature font swaps (`GetSmallFont/GetMediumFont -> GetLargeFont`)
  - keep the subtitle draw-scale constant changes
- Current installed reroll:
  - `BmGame-native-hud-largefont-safe.u`
- Next isolation step:
  - stop patching skip/help HUD paths for now
  - build a `RBMCombatManager.DrawHUDMessage`-only package
  - target spoken subtitles first, then return to prompts later
- Current installed isolation build:
  - `working/BmGame-spoken-subtitle-only.u`
  - only `RBMCombatManager.DrawHUDMessage` is patched in `BmGame.u`

## Current State

- The game launches.
- Main menu is restored to vanilla behavior.
- The in-game `Subtitle Size` menu experiment is not active anymore.
- Latest live test package was a scaled `Startup_INT.upk` font build.
- User reports no visible change from that build.

## What Was Tried

### 1. Pause menu `Subtitle Size` option

- Added a `Subtitle Size` option to the in-game Audio menu.
- Persistence was initially broken, then fixed.
- Result: the setting saved, but it did not change spoken subtitles, tips, or skip prompts.
- Conclusion: the menu/storage layer was not the real size bottleneck.

### 2. Frontend / main menu `Subtitle Size`

- Patched main-menu frontend movies to add the same option there.
- Versions shown during testing included `v1.1.1 HELENA ARKHAM FIX`, `v1.1.2`, and `v1.1.3`.
- Result: every `MainMenu.MainV2` rebuild done through FFDec `-importScript` broke the press-start / save-select flow.
- Restoration: frontend was restored to the original map.
- Conclusion: do not retry FFDec-based `MainV2` rebuilds for a distributable patch.

### 3. HUD ActionScript subtitle size changes

- Patched `rs.hud.Subtitle` and related HUD ActionScript.
- Tried changing `TextFormat.size`.
- Tried using much larger values for `Small / Normal / Large`.
- Tried applying changes every frame instead of only in `SetText()`.
- Tried clip scaling instead of text size changes.
- Tried direct timeline overrides for `Subtitles.SetText` and `InfoText.SetText`.
- Probe text like `HELENA INFO TEST` and `HELEN` did appear on screen.
- Result: probes proved the HUD movie patch was loading, but real spoken subtitles and tips still stayed tiny.
- Conclusion: the real spoken subtitle path is not controlled by the tested Flash subtitle clip logic.

### 4. Engine commands from the frontend

- Tried pushing engine font-slot changes through `FE_RunCommand`.
- Targeted `TinyFont`, `SmallFont`, `MediumFont`, `LargeFont`, `SubtitleFont`, and `AdditionalFont[0]`.
- Result: no visible change to spoken subtitles, tips, or skip prompts.
- Conclusion: this path did not reach the actual renderer being used in gameplay.

### 5. `.ini` font routing

- Edited live config files to route font names to larger fonts:
  - `BmEngine.ini` in game install
  - `BmEngine.ini` in user profile
  - `DefaultEngine.ini`
  - `UserEngine.ini`
- Also changed HUD config values such as `ConsoleFontSize` and `MessageFontOffset`.
- Result: user reported no visible change at all.
- Conclusion: the effective font selection is not being controlled by these `.ini` overrides for the tested text paths.

### 6. `BmGame.u` HUD package patches

- Patched `GameHUD.HUD` multiple times.
- Confirmed custom HUD probe text could appear, so the package patch itself was loading.
- Result: still no effect on the actual tiny subtitles / tips / skip prompts.
- Conclusion: the game text we care about is either native-rendered or uses a different path than the patched HUD movie.

### 7. `BmGame.u` backend subtitle toggle tracing

- Traced the menu `Subtitles Enabled/Disabled` option.
- Confirmed `FE_SetSubtitles` and `FE_GetSubtitles` go through `RPersistentOptions.Options_Subtitles`.
- Confirmed that path calls engine `SetShowSubtitles(...)`.
- Result: visibility toggle path is understood.
- Conclusion: this is a visibility switch, not a size control.

### 8. Native UnrealScript function tracing

- Decompiled and traced these paths:
  - `RHUD.DrawPromptText`
  - `RHUD.DrawSavingText`
  - `RSeqAct_ShowLevelIntroText.DrawHUD`
  - `RBMCombatManager.DrawHUDMessage`
  - `RTapePlayer.UpdateGFXSubtitles`
  - `RPlayerController.FE_SetSubtitles`
  - `RPlayerController.FE_GetSubtitles`
- Findings:
  - `DrawPromptText` uses `Engine.GetAdditionalFont(0)`.
  - `DrawSavingText` uses `Engine.GetMediumFont()`.
  - `RBMCombatManager.DrawHUDMessage` can call `Engine.GetSubtitleFont()`.
  - `RTapePlayer.UpdateGFXSubtitles` respects `Options_Subtitles`.
- Result: useful mapping, but no tested patch from this layer has yet produced visible size changes in-game.

### 9. `Engine.u` live patch attempt

- Tried replacing or redirecting engine font defaults inside live `Engine.u`.
- Result: this caused `bad name index`.
- Restoration: `Engine.u` was restored from backup.
- Conclusion: do not patch the live compressed `Engine.u` with the current toolchain.

### 10. `Startup_INT.upk` simple font swapping

- Compared live and unpacked `BmFonts` exports.
- Important discovery:
  - In the current live `Startup_INT.upk`, `SmallFont`, `MediumFont`, `TinyFont`, `EngineSmallFont`, and `EngineTinyFont` were already effectively collapsed to identical or equivalent font objects in practice.
- Result: routing `Small -> Medium` could not possibly help because that was already functionally true.
- Conclusion: do not retry simple “rename / redirect to MediumFont” font routing.

### 11. `Startup_INT.upk` actual font metric scaling

- Added a reusable C# command to scale serialized `BmFonts.*` glyph metrics directly.
- Built and installed a live `Startup_INT.upk` where:
  - `EngineSmallFont`
  - `EngineTinyFont`
  - `MediumFont`
  - `SmallFont`
  - `TinyFont`
  had their `Characters` metric fields scaled by `3.0x`.
- Verified the live installed `SmallFont` hash matched the staged package.
- Result: user reported `nothing changed`.
- Conclusion: either the live text is not reading those glyph metric fields the way expected, or another scaling stage clamps / overrides them.

### 12. Native decompile mapping

- Added a reusable C# decompile helper:
  - `tools/UELibFunctionDump`
- Confirmed clean decompiles for:
  - `RBMCombatManager.DrawHUDMessage`
  - `RHUD.DrawPromptText`
  - `RGFxMovieHUD.SetSubtitle`
  - `RGFxMovieHUD.SetInfoText`
  - `RGFxMovieBio.SetSubtitle`
  - `RTapePlayer.UpdateGFXSubtitles`
- Important findings:
  - `RBMCombatManager.DrawHUDMessage` uses:
    - `GetSmallFont()` for `TSize == 0`
    - `GetMediumFont()` for `TSize == 1`
    - `GetLargeFont()` for `TSize == 2`
    - `GetAdditionalFont(0)` plus `FontDrawScale = 3.0` for `TSize == 3`
    - `GetSubtitleFont()` plus `FontDrawScale = 2.0` for `TSize == 4`
  - `RHUD.DrawPromptText` uses `GetAdditionalFont(0)` and repeated `Canvas.DrawTextRA(...)`.
  - `RGFxMovieHUD.SetSubtitle` is just `ActionScript("_root.Subtitles.SetText")`.
  - `RGFxMovieHUD.SetInfoText` is just `ActionScript("_root.HUD.Contents.InfoText.SetText")`.
  - `RGFxMovieBio.SetSubtitle` is just `ActionScript("_root.Master.SetSubtitle")`.
  - `RTapePlayer.UpdateGFXSubtitles` pushes tape subtitles through `RPlayerController.BioMovie.SetSubtitle(CurrentSubtitles)`.
- Conclusion:
  - spoken gameplay subtitles, prompt text, and tape/bio subtitles are not proven to be the same renderer
  - the opening spoken line may not be on the same path as tape subtitles

### 13. Cleanup before next probe

- Restored live `BmGame.u` to the original backup hash:
  - live SHA-256: `4306148E7627EC2C0DE4144FD6AB45521B3B7E090D1028A0B685CADAFAFB89E6`
- Restored live `Startup_INT.upk` from the original backup:
  - backup: `Startup_INT.upk.fonttest-backup-20260324-170102`
  - live SHA-256 after restore: `C8F790A254E953393B314DE4873CA82B96B383E0E42258B2C1982C690577F384`
- Reason:
  - remove leftover no-effect font-package experiments before the next targeted test

### 14. Reusable HUD subtitle probe build

- Added a reusable C# command:
  - `tools/SubtitleSizeModBuilder build-hud-subtitle-probe`
- Added builder source:
  - `tools/SubtitleSizeModBuilder/HudSubtitleProbeAssetBuilder.cs`
- What it does:
  - copies the clean `hud-ffdec-scripts` tree
  - replaces only `__Packages/rs/hud/Subtitle.as`
  - rebuilds `GameHUD.HUD` with FFDec `-importScript`
  - writes a manifest that patches only `GameHUD.HUD`
- Probe behavior:
  - subtitles are prefixed with `HUD:`
  - info text is prefixed with `INFO:`
  - both are given much larger ActionScript text sizes and clip scales
- Built assets:
  - `build-assets/hud-subtitle-probe/HUD-subtitle-probe.gfx`
  - `build-assets/hud-subtitle-probe/hud-subtitle-probe.manifest.jsonc`
- Patched staged package:
  - `working/BmGame-hud-subtitle-probe.u`
  - staged HUD export SHA-256: `01d7ed2ad4df6730362894adbd0dd9d79559bfba3c38b3faa7f15f9006e658bd`
- Installed live package:
  - backup: `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/BmGame/CookedPC/BmGame.u.hud-subtitle-probe-backup-20260325-095303`
  - live SHA-256 after install: `CA4523A169877DE11FC929F460343B3ACE8665F800C91742768A491E4B04C14D`
- Expected manual test result:
  - if the opening line renderer uses `GameHUD.HUD -> _root.Subtitles.SetText`, the line should visibly change to `HUD: <original line>` and appear much larger
  - if the line stays unchanged, the opening subtitle path is not `GameHUD.HUD`

### 15. Probe crash interpretation

- User tested the large HUD-only probe and reported:
  - `General protection`
  - second run crashed when the subtitle should have rendered
  - visible error text referenced `Rendering thread exception` and `TextFont`
- Log confirmation from `Launch.log`:
  - `Assertion failed: TextFont`
  - file path mentions `FullScreenMovieBink.inl`
- Immediate action taken:
  - restored live `BmGame.u` back to the clean original hash again
- Working interpretation:
  - the aggressive HUD probe is not safe enough to use as a route detector
  - the opening test line may be on a different movie/text path than the normal HUD subtitle clip
  - do not reuse the oversized/color/scale HUD probe

### 16. Reusable two-path route probe

- Added a reusable C# command:
  - `tools/SubtitleSizeModBuilder build-subtitle-route-probe`
- Added builder source:
  - `tools/SubtitleSizeModBuilder/SubtitleRouteProbeAssetBuilder.cs`
- What it patches:
  - `GameHUD.HUD`
  - `CharacterBio.CharacterBio`
- Probe behavior:
  - HUD subtitle path adds `HUD: ` prefix only
  - HUD info text path adds `INFO: ` prefix only
  - CharacterBio subtitle path adds `BIO: ` prefix only
  - no forced size changes
  - no forced scale changes
  - no color changes
- Built assets:
  - `build-assets/subtitle-route-probe/HUD-subtitle-route-probe.gfx`
  - `build-assets/subtitle-route-probe/CharacterBio-subtitle-route-probe.gfx`
  - `build-assets/subtitle-route-probe/subtitle-route-probe.manifest.jsonc`
- Built staged package:
  - `working/BmGame-subtitle-route-probe.u`
- Installed live package:
  - backup: `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/BmGame/CookedPC/BmGame.u.subtitle-route-probe-backup-20260325-100213`
  - live SHA-256 after install: `DA98794061EB1A35272DF351BEC2463C285C5DB53664C2A945EAF20F3E5B1B96`
- Expected manual test result:
  - if the tested line shows `HUD:`, the path is `GameHUD.HUD`
  - if it shows `BIO:`, the path is `CharacterBio.CharacterBio`
  - if it shows neither and does not crash, the tested line is on neither of those GFx paths

### 17. Narrowed focus to Bink/fullscreen-movie subtitles

- User reported the route-probe build still crashed and no subtitle prefix ever appeared.
- User provided two useful crash behaviors:
  - pressing `Space` immediately at intro causes general protection
  - pressing `Space` after the subtitle moment causes a rendering-thread crash with `TextFont` assertion
- Log confirmation:
  - `Assertion failed: TextFont`
  - source path mentions `FullScreenMovieBink.inl`
- Interpretation:
  - the current target line is on the fullscreen Bink subtitle path
  - `GameHUD.HUD` and `CharacterBio.CharacterBio` are not the right immediate target for that test case

### 18. Restored known-good `BmGame.u`

- Restored live `BmGame.u` from the original backup again:
  - live SHA-256: `4306148E7627EC2C0DE4144FD6AB45521B3B7E090D1028A0B685CADAFAFB89E6`
- This removes the route-probe GFx edits from the active test state.

### 19. Restored crash-free Bink font routing

- Used old ini-dump snapshots as the source of truth for the last known crash-free fullscreen subtitle setup:
  - `TinyFontName=BmFonts.EngineTinyFont`
  - `SmallFontName=BmFonts.EngineSmallFont`
  - `MediumFontName=BmFonts.MediumFont`
  - `LargeFontName=BmFonts.MediumFont`
  - `SubtitleFontName=BmFonts.MediumFont`
  - `AdditionalFontNames=BmFonts.MediumFont` x4
- Applied that exact routing to:
  - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/BmGame/Config/BmEngine.ini`
  - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/BmGame/Config/DefaultEngine.ini`
  - `C:/Users/Helena/Documents/Square Enix/Batman Arkham Asylum GOTY/BmGame/Config/BmEngine.ini`
  - `C:/Users/Helena/Documents/Square Enix/Batman Arkham Asylum GOTY/BmGame/Config/UserEngine.ini`
- Restored read-only attributes after editing.
- Reason:
  - the live files had been left on the crashy `SubtitleFontName=BmFonts.LargeFont` route
  - that route lines up with the `TextFont` assertion in the Bink renderer

### 20. Bink-only font-size test

- Focus narrowed to the fullscreen-movie subtitle font only.
- Built a new `Startup_INT` test from the clean decompressed package:
  - source: `working/unpacked/Startup_INT.upk`
  - output: `working/Startup_INT-mediumfont-x2.upk`
- Patch behavior:
  - scaled only `BmFonts.MediumFont`
  - scale factor: `2.0x`
- Tooling used:
  - `tools/BmGameGfxPatcher scale-font`
- Installed live package:
  - live file: `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/BmGame/CookedPC/Startup_INT.upk`
  - backup: `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/BmGame/CookedPC/Startup_INT.upk.mediumfont-x2-backup-20260325-100853`
  - live SHA-256 after install: `E01FF8B00425E98BD2E7F5D4D99A0D39E37240CB03B3B59C3E297B0D2137778F`
- Active test state now:
  - `BmGame.u` original
  - Bink/crash-free font routing restored
  - only `Startup_INT.upk` changed, and only `BmFonts.MediumFont` was scaled

## Things That Were Confirmed

- The real subtitle visibility toggle path is understood.
- The tested HUD Flash patch path is not the source of the tiny spoken subtitle line.
- The game can load patched `BmGame.u` HUD assets without crashing.
- The game can load the latest patched `Startup_INT.upk`.
- The main-menu FFDec rebuild path is unsafe and should stay abandoned.
- The `.ini` font override path did not affect the target text.
- Simple font-object rerouting also did not affect the target text.

## Things That Broke And Were Restored

- Live `BmGame.u` built from an unpacked HUD-only package caused `bad name index`.
- Live `Engine.u` patch attempt caused `bad name index`.
- Multiple frontend `MainV2` rebuilds broke the press-start / save-select flow.
- Each of those broken states was restored afterward.

## Do Not Retry

- Do not retry FFDec `MainMenu.MainV2` rebuilds.
- Do not retry `.ini` font-name routing by itself.
- Do not retry simple `SmallFont -> MediumFont` swaps.
- Do not retry HUD-only Flash subtitle size patches as the primary approach.
- Do not retry patching the live compressed `Engine.u` with the current packer.
- Do not retry broad native import swaps like `GetAdditionalFont(...) -> GetLargeFont()`; that can crash because the signatures differ.

## Most Useful Remaining Leads

- Patch the native draw scale in the actual render functions instead of swapping font objects.
- Focus on these native/script functions first:
  - `RBMCombatManager.DrawHUDMessage`
  - `RHUD.DrawPromptText`
  - any spoken-subtitle draw path that ultimately calls `Engine.GetSubtitleFont()`
- Verify whether spoken dialog subtitles use a different renderer from tips / skip prompts.
- If needed, instrument one native path at a time with an obvious visual change instead of broad font routing.

## Reusable Tools Added During This Investigation

- `tools/BmGameGfxPatcher` now supports:
  - `describe-export`
  - `describe-function`
  - `find-function-refs`
  - `describe-properties`
  - `scale-font`

These commands were added specifically so future work can inspect and patch package internals without repeating the earlier blind experiments.

### 21. Bink/FM0D path mapping and `SmallFont` test

- Confirmed the intro fullscreen movie sequence lives in:
  - `working/intro-map-decompressed/Max_IntroParty.umap`
  - object: `Main_Sequence.RSeqAct_FullScreenPlayer_1`
- Reusable inspection helper extended:
  - `tools/UELibFunctionDump/Program.cs`
  - new command: `inspect-object`
- `RSeqAct_FullScreenPlayer_1` properties confirmed:
  - `TheMovieName="106_Joker_escapes"`
  - `ExternalSoundTrack=SoundCue'MUS_JokerEscapes.JokerEscapes'`
  - `MixForMovie=MIXBIN_BinkMovie.Movie_MixBin`
- `MUS_JokerEscapes.JokerEscapes` confirmed as an FMOD-generated cue:
  - `FMODGeneratedCue=true`
  - `FMODSound=RFMODSound'MUS_JokerEscapes.MUS_JokerEscapes'`
- `MUS_JokerEscapes.MUS_JokerEscapes` points at:
  - `C:\Rock\BmGame\Content\Source\Sound\FMODBanks\Music\Cutscene\106_JokerEscapes\MUS_JokerEscapes.fdp`
- Decompressed `Max_IntroParty_LOC_INT.upk` and confirmed the localized intro speech is stored as `SoundNodeWave` exports:
  - `Speech-Max_C1.Max_C1_CH1_Joker_01` through `_12`
- Crucial find:
  - those `SoundNodeWave` objects contain a `LocalizedSubtitles` array
  - this strongly suggests fullscreen-movie speech subtitles are attached to the localized speech waves, not the HUD movie
- Inspected `Engine.Default__Engine` and found:
  - serialized `SubtitleFontName="BmFonts.SmallFont"`
  - this differs from the then-live `.ini` overrides that were forcing `BmFonts.MediumFont`
- New focused live test:
  - restored `SubtitleFontName=BmFonts.SmallFont` in:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/BmGame/Config/BmEngine.ini`
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/BmGame/Config/DefaultEngine.ini`
    - `C:/Users/Helena/Documents/Square Enix/Batman Arkham Asylum GOTY/BmGame/Config/BmEngine.ini`
    - `C:/Users/Helena/Documents/Square Enix/Batman Arkham Asylum GOTY/BmGame/Config/UserEngine.ini`
  - built `working/Startup_INT-smallfont-x3.upk` from the clean original backup
  - scaled only `BmFonts.SmallFont`
  - scale factor: `3.0x`
  - installed live at:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/BmGame/CookedPC/Startup_INT.upk`
  - backup:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/BmGame/CookedPC/Startup_INT.upk.smallfont-backup-20260325-1037`

### 22. Native `TextFont` config hypothesis

- New native clue from `ShippingPC-BmGame.exe` string extraction:
  - `FullScreenMovieBink.inl` contains a `TextFont` string directly next to other fullscreen-movie strings like:
    - `Cutscene`
    - `Subtitles`
    - `MovieIsPaused`
    - `Frontend`
    - `Paused`
    - `HelpText`
  - a separate nearby string cluster also ties fullscreen-movie UI data to:
    - `BmGame.RHUD`
    - `SkipButtonIconName`
    - `BmFonts.PC_LMB`
- Interpretation:
  - fullscreen-movie subtitles/help text likely use a native `TextFont` path that is separate from `Engine.Engine.SubtitleFontName`
  - previous `SubtitleFontName` and `BmFonts.SmallFont` scaling tests may have been hitting the wrong setting entirely
- Live config-only test applied:
  - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/BmGame/Config/DefaultGame.ini`
    - `[BmGame.RHUD]`
    - `TextFont="BmFonts.MediumFont"`
  - `C:/Users/Helena/Documents/Square Enix/Batman Arkham Asylum GOTY/BmGame/Config/BmGame.ini`
    - `[BmGame.RHUD]`
    - `TextFont=BmFonts.MediumFont`
  - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/BmGame/Config/DefaultEngine.ini`
    - `[FullScreenMovie]`
    - `TextFont=BmFonts.MediumFont`
  - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/BmGame/Config/BmEngine.ini`
    - `[FullScreenMovie]`
    - `TextFont=BmFonts.MediumFont`
  - `C:/Users/Helena/Documents/Square Enix/Batman Arkham Asylum GOTY/BmGame/Config/BmEngine.ini`
    - `[FullScreenMovie]`
    - `TextFont=BmFonts.MediumFont`
  - `C:/Users/Helena/Documents/Square Enix/Batman Arkham Asylum GOTY/BmGame/Config/UserEngine.ini`
    - `[FullScreenMovie]`
    - `TextFont=BmFonts.MediumFont`
- Backups created before this edit:
  - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/BmGame/Config/DefaultGame.ini.textfont-backup-20260325-105458`
  - `C:/Users/Helena/Documents/Square Enix/Batman Arkham Asylum GOTY/BmGame/Config/BmGame.ini.textfont-backup-20260325-105458`
  - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/BmGame/Config/DefaultEngine.ini.textfont-backup-20260325-105458`
  - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/BmGame/Config/BmEngine.ini.textfont-backup-20260325-105458`
  - `C:/Users/Helena/Documents/Square Enix/Batman Arkham Asylum GOTY/BmGame/Config/BmEngine.ini.textfont-backup-20260325-105458`
  - `C:/Users/Helena/Documents/Square Enix/Batman Arkham Asylum GOTY/BmGame/Config/UserEngine.ini.textfont-backup-20260325-105458`
- Repair note:
  - one failed regex pass briefly truncated the shipped `DefaultGame.ini` `SkipButtonIconName` line
  - it was immediately restored from the new backup before the final line-based insertion was applied
- Test status:
  - pending manual validation
  - no package files changed in this step
  - no game launch in this step

### 23. Native fullscreen-movie subtitle scale trampoline

- Stopped chasing font-name/config routes and traced the actual fullscreen-movie subtitle renderer in `ShippingPC-BmGame.exe`.
- Strongest native path identified:
  - fullscreen movie state function around `0x006D0800`
  - only caller of subtitle render callee:
    - `0x006D0BDE -> 0x006B93F0`
  - that caller is gated by the localized `Cutscene`, `Subtitles`, and `MovieIsPaused` strings in the `FullScreenMovieBink.inl` block.
- Inside `0x006B93F0`:
  - found native layout/scale constants:
    - `0x1ECEB34 = 0.075f`
    - `0x1ECEB38 = 0.925f`
    - `0x1ECEB3C = 2.2f`
  - the code path at `0x006B96D8..0x006B970B` loads a subtitle scale value and immediately feeds it into a font-size related virtual call.
- New reusable tool added:
  - `tools/NativeSubtitleExePatcher`
  - command:
    - `patch-bink-subtitles --exe <ShippingPC-BmGame.exe> [--scale-multiplier <float>] [--backup]`
- Patch strategy:
  - hook RVA `0x002B96E0`
  - overwrite the 10-byte block:
    - `8B4F088B118B420C6A01`
  - jump to a `.text` cave at RVA `0x01A285E1`
  - cave multiplies the loaded native subtitle scale in `xmm0` by a configurable float, replays the overwritten instructions, and jumps back to `0x002B96EA`
- Live install applied:
  - target:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe`
  - backup:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.bink-subtitle-scale-backup-20260325-114609`
  - installed multiplier:
    - `3.0`
  - verified:
    - hook jump present at `0x006B96E0`
    - cave present at `0x01E285E1`
    - live SHA-256:
      - `85c059979ffa15a09108304f1a835e51d2f1d39e1f8c07097a3201b5a0405f08`
- Status:
  - ready for manual test
  - game not launched automatically after this patch

### 24. Wrong native hook identified and reverted

- The first executable hook was wrong:
  - it patched the float loaded at `0x006B96D8`
  - user test result:
    - fullscreen Bink video became washed out
    - subtitle size did not change
- Conclusion:
  - that float is tied to brightness / opacity / blend behavior, not subtitle text size
- Recovery:
  - restored the live executable from:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.bink-subtitle-scale-backup-20260325-114609`

### 25. Targeted Bink text-scale hook

- Traced the actual text path more deeply:
  - `0x00AB00A0 -> 0x00AA7ED0`
  - `aa7ed0` takes two explicit scale floats at stack args `[ebp+0x20]` and `[ebp+0x24]`
  - the fullscreen subtitle helper was passing `1.0, 1.0`
- Strong inference:
  - these are the real text X/Y scale values for the subtitle draw path
- New reusable patch added to:
  - `tools/NativeSubtitleExePatcher/Program.cs`
  - command:
    - `patch-bink-text-scale --exe <ShippingPC-BmGame.exe> [--scale-multiplier <float>] [--backup]`
- Hook details:
  - hook RVA:
    - `0x006B00DA`
  - original bytes:
    - `D9E8D9542404D91C24`
  - cave RVA:
    - `0x01A285E1`
  - strategy:
    - intercept the `fld1 / fst / fstp` block that sets the text scale to `1.0, 1.0`
    - inspect the callsite return address
    - only replace the scale with the custom multiplier for the Bink subtitle callsite range:
      - `0x006B9C88 .. 0x006B9D7A`
    - all other callers keep the default `1.0`
- Live install applied:
  - target:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe`
  - backup:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.bink-text-scale-backup-20260325-120242`
  - installed multiplier:
    - `2.0`
  - verified:
    - hook present at `0x00AB00DA`
    - cave present at `0x01E285E1`
    - live SHA-256:
      - `671f565ef05dc005c3ce85dbc729fb6a4c2c34e3b5f6561ec8b7ea1eb25f5874`
- Status:
  - ready for manual test
  - game not launched automatically after this patch

### 26. Reduced Bink text-scale multiplier after off-screen disappearance

- User test on the `2.0x` Bink text-scale hook:
  - subtitles disappeared
  - likely explanation: the hook is correct but `2.0x` is too aggressive for the current draw box / placement math
- Action taken:
  - restored the live executable from:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.bink-text-scale-backup-20260325-120242`
  - reapplied the same targeted text-scale hook with a lower multiplier:
    - `1.4`
  - new backup:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.bink-text-scale-backup-20260325-121857`
  - live SHA-256:
    - `112086283eb58a225ac41061344fa3e01ecbf61f055fab69c541d0bdd5d430ba`
- Status:
  - ready for manual test
  - if subtitles still disappear, the next patch target is the Bink subtitle position / clip math rather than the scale multiplier itself

### 27. Fixed constant-address bug in the Bink text-scale cave

- Found a bug in the text-scale cave itself:
  - the custom multiplier `fld` was reading from the wrong address inside the cave
  - this made the earlier `1.4x` build untrustworthy as a real scale test
- Fixed in:
  - `tools/NativeSubtitleExePatcher/Program.cs`
  - corrected the embedded float address so the cave now loads the real custom multiplier
- Revalidated on a staged executable:
  - cave now resolves the custom float correctly
  - verified embedded float:
    - `1.100000023841858`
- Live install applied from the clean backup:
  - restored from:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.bink-text-scale-backup-20260325-121857`
  - reapplied targeted Bink text-scale hook with:
    - `1.1`
  - new backup:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.bink-text-scale-backup-20260325-122440`
  - live SHA-256:
    - `5b6fc0c350b8de9ab0bb5993c4af074faa6abf310ce6454f252b4d148921677d`
- Status:
  - ready for manual test
  - this is the first Bink text-scale build with both the callsite filter and multiplier address verified

### 28. Found EAX clobber bug in the text-scale cave

- GUI comparison between vanilla and patched traces showed:
  - the subtitle draw still exists
  - patched first vertex X was wildly wrong while Y remained sane
- Root cause:
  - the text-scale cave was using `mov eax, [ebp+4]` to inspect the caller return address
  - the original code needs `eax` immediately after the hook to compute subtitle placement
  - this corrupted the horizontal placement math, which matches the broken X coordinate in `qapitrace`
- Fix:
  - update `tools/NativeSubtitleExePatcher/Program.cs`
  - use `ecx` as the scratch register for the callsite filter instead of `eax`
  - adjust the cave-relative jump and constant offsets accordingly
- Live reinstall:
  - restored original exe from:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.bink-subtitle-scale-backup-20260325-114609`
  - preserved broken build at:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.eax-clobber-backup-20260325-134900`
  - reapplied corrected text-scale hook with:
    - `1.1`
  - new live SHA-256:
    - `51f3099fac918c5878f919773837f9c266529281c4c6e527ceec60926286a723`

### 29. Raised corrected Bink text-scale hook to 1.5x

- User result on corrected `1.1x` build:
  - subtitles show again
  - visible size change is too small to matter
- Action:
  - preserved corrected `1.1x` live build at:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.scale-1_1-backup-20260325-135238`
  - restored clean original exe from:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.bink-subtitle-scale-backup-20260325-114609`
  - reapplied the corrected `ecx`-based text-scale cave with:
    - `1.5`
  - new live SHA-256:
    - `28bf4af421b2525e5b60e41bd3a04297e1a7acfc8c0c6ca8f3bfc593fa5fd7a2`

### 30. Filter likely misses the real Bink subtitle caller

- User result on corrected filtered `1.5x` build:
  - visible subtitle size still appears unchanged
- Current inference:
  - the helper hook is no longer corrupting placement
  - but the return-address filter likely targets the wrong `ab00a0` caller block
- Action in progress:
  - add `--global` mode to `patch-bink-text-scale`
  - this forces the custom scale for every `ab00a0` caller without the return-address filter
  - use that as a proof test before trying to isolate the exact Bink-only caller range

### 31. Installed global 1.5x text-helper test

- Purpose:
  - prove whether `ab00a0 -> aa7ed0` is the real size lever for the Bink subtitle path
  - avoid the too-narrow return-address filter
- Live install:
  - preserved filtered `1.5x` build at:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.filtered-1_5-backup-20260325-135905`
  - restored clean original exe from:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.bink-subtitle-scale-backup-20260325-114609`
  - applied global text-helper scale with:
    - `1.5`
    - `--global`
  - new live SHA-256:
    - `0d51c2706799ee0e2906fa8f32dc34505ff5359650d5f110f06ac2996ff43cfe`
- Expected result:
  - if Bink subtitles finally grow, the helper path is correct and the old filter was wrong
  - if still unchanged, this helper is not the dominant size control for the visible Bink subtitle line

### 32. Added global centering compensation for the 1.5x helper patch

- User result on the first working global `1.5x` build:
  - all subtitles and tips are larger
  - text is shifted off center
- Root cause:
  - the global hook changed the scale arguments, but the caller still subtracted the unscaled half-width when centering text
  - this left size larger but horizontal placement too far off
- Fix:
  - update `tools/NativeSubtitleExePatcher/Program.cs`
  - extend the global cave so it replays the overwritten centering code
  - multiply the `cvtsi2ss xmm0, eax` half-width term by the same scale factor before `subss xmm1, xmm0`
- Verification:
  - rebuilt the patcher
  - applied the new global `1.5x` cave to a clean test exe
  - confirmed the cave bytes now include the added `mulss xmm0, [scale]` centering compensation
- Live install:
  - preserved the uncentered global `1.5x` build at:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.global-1_5-uncentered-backup-20260325-141323`
  - restored clean original exe from:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.bink-subtitle-scale-backup-20260325-114609`
  - reapplied global text-helper scale with centered half-width compensation:
    - `1.5`
    - `--global`
  - new live SHA-256:
    - `73f8a34bda912e070fdfbc8fb1c3712aec03ee910e451159ad50306c80065e0c`

### 33. Removed the external watcher by moving the config poll into the exe

- Goal:
  - keep the working centered global text-scale patch
  - stop depending on the external `watch-live-text-scale` process
- Chosen route:
  - keep the pause-menu bridge that writes `bAllowMatureLanguage` / `bEnableKismetLogging`
  - patch the exe so the hooked text helper polls the user `BmEngine.ini` from inside the game process every `250ms`
  - map the current signal bits to:
    - `Small = 1.3`
    - `Normal = 1.5`
    - `Large = 1.8`
- Why this route:
  - direct UObject/runtime-property discovery was not stable enough yet
  - the game already proved the `BmEngine.ini` bridge works reliably
  - moving the poll into the exe removes the sidecar watcher while keeping the working menu path intact
- Implementation:
  - updated `tools/NativeSubtitleExePatcher/Program.cs`
  - added `--internal-ini-live` to `patch-bink-text-scale`
  - reused the existing global scale float and centered helper math
  - added an internal poller in a secondary code cave that:
    - resolves the Documents path with `SHGetFolderPathW`
    - opens the user `BmEngine.ini`
    - finds the latest `bEnableKismetLogging=` / `bAllowMatureLanguage=` values
    - updates the live scale float in-process
- Verification:
  - patched a clean disposable exe copy from:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.bink-subtitle-scale-backup-20260325-114609`
  - verified the patched disposable exe with:
    - `verify-bink-text-scale`
  - launch-tested the disposable exe from the game `Binaries` folder for `20s`
  - process stayed alive through startup
- Live install:
  - previous live exe backed up at:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.internal-ini-live-backup-20260325-154833`
  - installed live exe SHA-256:
    - `fcf7383556111460234f68c4103ec3d048c6d6a99e7307aabfae41237fd2039e`
- Current status:
  - no external watcher is required for runtime subtitle-size changes anymore
  - next validation step is in-game user testing of `Small / Normal / Large` on the live build

### 34. Fixed the no-watcher startup crash by moving runtime state into writable `.data`

- Symptom:
  - the first `--subtitle-size-signal` / no-watcher builds crashed immediately on startup with:
    - `Rendering thread exception`
    - `General protection fault`
- Root cause:
  - the worker cave was storing mutable runtime state inside the RX code cave:
    - `path_ready`
    - `file_buffer_ptr`
    - `bytes_read`
    - `file_handle`
    - the live scale float used by the global text helper
  - writing any of that state at runtime faulted inside the cave
- Fix:
  - update `tools/NativeSubtitleExePatcher/Program.cs`
  - reserve a writable zero-filled block in `.data`
  - move all mutable signal-worker state into that `.data` block
  - change the global text helper to read the live scale from that writable block instead of the RX cave constant
- Verification:
  - rebuilt the patcher
  - patched a clean disposable exe copy from:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.bink-subtitle-scale-backup-20260325-114609`
  - patched disposable exe reported:
    - `State block RVA: 0x0220FE06`
  - verified the patched disposable exe with:
    - `verify-bink-text-scale`
  - launch-tested the disposable exe from the game `Binaries` folder for `20s`
  - process stayed alive through startup
- Live install:
  - previous stable live exe backed up at:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.subtitle-signal-fixed-backup-20260325-200354`
  - installed live exe SHA-256:
    - `41f02f41616ef508f8a8bb711cdff8ccd4c115097f82bc9b88d109941d3c245b`
- quick live startup test:
  - `15s`
  - process stayed alive through startup

### 35. Fixed the second no-watcher crash by moving `FXSAVE/FXRSTOR` out of the code cave

- Symptom:
  - the first `.data`-state fix still crashed on `Continue Game`
  - crash address landed at the text helper cave itself around:
    - `0x01E285E3`
- Root cause:
  - the global helper cave still used:
    - `fxsave [abs]`
    - `fxrstor [abs]`
  - that save area was still inside the RX worker cave
- Fix:
  - keep the subtitle signal state in writable `.data`
  - reserve a second aligned writable `.data` block for the `FXSAVE` area
  - update `tools/NativeSubtitleExePatcher/Program.cs` so the helper cave uses that writable address
  - fix the writable-block allocator so the `FXSAVE` block cannot overlap the subtitle state block
- Verification:
  - rebuilt the patcher
  - patched a clean disposable exe copy from:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.bink-subtitle-scale-backup-20260325-114609`
  - patched disposable exe reported:
    - `State block RVA: 0x0220FE10`
    - `FXSAVE block RVA: 0x02210220`
  - launch-tested live for `10s`
  - process stayed alive through startup

### 36. Switched the runtime subtitle-size burst from same-value repeats to alternating toggles

- Symptom:
  - the corrected burst build stopped crashing
  - subtitles/tips stayed enlarged
  - but changing `Subtitle Size` in the pause menu caused no visible size change
- Root cause:
  - the pause script was issuing repeated `FE_SetSubtitles(currentValue)` calls
  - those same-value repeats did not produce distinct native subtitle events, so the burst hook kept the last active scale instead of selecting a new size
- Fix:
  - update `tools/SubtitleSizeModBuilder/ScriptTemplates.cs`
  - change the pause runtime burst to alternate subtitle values and return to the original subtitle-enabled state
    - `Small` -> `2` calls
    - `Normal` -> `4` calls
    - `Large` -> `6` calls
  - update `tools/NativeSubtitleExePatcher/Program.cs`
  - change the burst hook mapping to:
    - count `2` -> small scale
    - count `4` -> normal scale
    - count `6` -> large scale
  - cap the burst counter at `6` instead of `4`
- Verification:
  - rebuilt:
    - `tools/SubtitleSizeModBuilder`
    - `tools/NativeSubtitleExePatcher`
  - patched a clean disposable exe copy from:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.bink-subtitle-scale-backup-20260325-114609`
  - disposable exe SHA-256:
    - `f187c4b1b93b1e70b623221f57edba13f54eb575446acaaa412832284cf2b227`
  - verified the disposable exe with:
    - `verify-bink-text-scale`
  - launch-tested the disposable exe for `15s`
  - process stayed alive through startup
  - rebuilt a fresh pause package from:
    - `bmgame-unpacked/BmGame.u`
  - fresh pause package SHA-256:
    - `42a6d6c1a2df83310fce34d32f2ec28986f0be3593dde232020ef624e13c9961`
  - installed both live files and launch-tested the live game for `15s`
  - process stayed alive through startup

### 37. Replaced fake subtitle values with an event-driven `BmEngine.ini` signal read

- Symptom:
  - subtitles/tips stayed enlarged
  - `Subtitle Size` still saved in the pause menu
  - changing `Small / Normal / Large` caused no runtime size change
- Root cause:
  - the direct-value build tried to encode size through:
    - `FE_SetSubtitles(2/3/4)`
  - but `FE_SetSubtitles` calls:
    - `SetShowSubtitles(__NFUN_155__(Value, 0))`
  - by the time the native hook ran, the value had already been normalized to `0/1`
- Fix:
  - restore the pause runtime script to the older signal model:
    - write `bAllowMatureLanguage`
    - write `bEnableKismetLogging`
    - then call one normal `FE_SetSubtitles(currentValue)` refresh
  - update `tools/NativeSubtitleExePatcher/Program.cs`
  - change the `SetShowSubtitles` worker so it:
    - builds the user `BmEngine.ini` path once
    - reads the file only when the subtitle event fires
    - maps:
      - `bAllowMatureLanguage=True` -> small
      - `bEnableKismetLogging=True` -> large
      - neither -> normal
  - no watcher and no periodic poll remain in the live path
- Verification:
  - rebuilt:
    - `tools/NativeSubtitleExePatcher`
    - `tools/SubtitleSizeModBuilder`
  - patched a clean disposable exe copy from:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.bink-subtitle-scale-backup-20260325-114609`
  - disposable exe SHA-256:
    - `12129af85c42e2aeb464ff4285a11a8db2a6ee862fbed74ba444f17d519e7f5e`
  - rebuilt a fresh pause package from:
    - `bmgame-unpacked/BmGame.u`
  - fresh pause package path:
    - `C:/Users/Helena/Downloads/arkham-subtitle-mod-tools/working/BmGame-pause-runtime-signal-event.u`
  - installed both live files:
    - `ShippingPC-BmGame.exe`
    - `BmGame.u`
  - launch-tested the live game for `15s`
  - process stayed alive through startup

### 38. Switched to a real single-int backend via `FE_SetControlType` sentinel

- Symptom:
  - `Subtitle Size` saved correctly across launches
  - live size stayed stuck at the normal `1.5x` path
  - the `FE_RunCommand` / disk-signal routes were not producing a reliable immediate runtime event
- Root cause:
  - the pause row was still emitting ordinary option labels through `FE_SetControlType`
  - the exe hook therefore had no unique way to tell:
    - a real subtitle-size write
    - from unrelated FE traffic
  - `SixAxis`, burst counting, watcher polling, and command-string parsing all turned out to be dead ends
- Fix:
  - keep a single dormant persisted PC-native int:
    - `Options_ControlType`
  - stop using:
    - `Options_SixAxis`
    - watcher processes
    - `FE_RunCommand`
    - burst/toggle hacks
  - update `tools/SubtitleSizeModBuilder/ScriptTemplates.cs`
  - the pause row now writes:
    - `FE_SetControlType(state, "HELENA_SUBSIZE_<state>")`
  - the HUD replay now re-applies the saved state with the same sentinel once gameplay loads
  - update `tools/NativeSubtitleExePatcher/Program.cs`
  - replace the old subtitle signal worker with a real 2-arg FE setter tail hook
  - hook RVAs:
    - `0x0000ED56`
    - `0x0000EDA6`
    - `0x0000F009`
  - the worker now:
    - reads the first arg from `ESI`
    - reads the second arg from the original wrapper stack
    - matches only `HELENA_SUBSIZE_` in UTF-16 or ASCII
    - maps:
      - `0` -> `1.3x`
      - `1` -> `1.5x`
      - `2+` -> `1.8x`
    - updates the shared live text-scale state directly
- Verification:
  - rebuilt:
    - `tools/SubtitleSizeModBuilder`
    - `tools/NativeSubtitleExePatcher`
    - `tools/BmGameGfxPatcher`
  - rebuilt fresh pause/HUD assets:
    - `build-assets/pause-runtime-scale/Pause-runtime-scale.gfx`
    - `build-assets/pause-runtime-scale/HUD-runtime-scale.gfx`
  - repatched a staged clean exe from:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/Binaries/ShippingPC-BmGame.exe.bink-subtitle-scale-backup-20260325-114609`
  - staged exe SHA-256:
    - `806d595977e129a653787afa9e047c39db3bffc2a6242413ed6db11fe30f21a7`
  - verified the staged exe with:
    - `verify-bink-text-scale`
  - repatched a staged clean package from:
    - `D:/steam/steamapps/common/Batman Arkham Asylum GOTY/BmGame/CookedPC/BmGame.u.live-command-hook-backup-20260326-1007`
  - installed the staged files live with fresh backups:
    - `ShippingPC-BmGame.exe.controltype-sentinel-backup-20260326-115520`
    - `BmGame.u.controltype-sentinel-backup-20260326-115520`
  - cold-start tested the live game for `15s`
  - process stayed alive through startup

### 39. Replaced `SharedObject` + single-thunk signal with raw `ControlType` codes and multi-thunk gating

- Symptom:
  - the `Subtitle Size` row forgot its value as soon as the pause menu reopened
  - the live scale stayed stuck at the normal `1.5x` path
- Root cause:
  - the row still depended on `SharedObject` for persistence
  - the exe only hooked one guessed FE setter thunk, so the live apply path frequently never fired
  - the string-sentinel path added ambiguity without actually fixing event delivery
- Fix:
  - `PauseRuntimeScaleListItem` now uses `FE_GetControlType` / `FE_SetControlType` directly as the subtitle-size backend
  - dedicated raw codes are now:
    - `4101` = Small
    - `4102` = Normal
    - `4103` = Large
  - HUD startup replay now re-emits the saved raw code through `FE_SetControlType`
  - `NativeSubtitleExePatcher` now hooks the whole simple 2-arg FE setter cluster and ignores everything except those raw codes
  - hooked RVAs:
    - `0x0000E893`
    - `0x0000E8E3`
    - `0x0000E933`
    - `0x0000E983`
    - `0x0000E9D3`
    - `0x0000EA23`
    - `0x0000EA73`
    - `0x0000EAC3`
    - `0x0000ED0B`
    - `0x0000ED56`
    - `0x0000EDA6`
    - `0x0000F009`
- Verification:
  - rebuilt:
    - `tools/SubtitleSizeModBuilder`
    - `tools/NativeSubtitleExePatcher`
  - rebuilt fresh pause/HUD assets via:
    - `build-pause-runtime-scale`
  - patched staged clean files from:
    - `ShippingPC-BmGame.exe.bink-subtitle-scale-backup-20260325-114609`
    - `BmGame.u.live-command-hook-backup-20260326-1007`
  - staged exe SHA-256:
    - `5f62927d1a21429f79c38e468a7adffb449bcce73943cf5a696afcaf438fd16c`
  - staged package SHA-256:
    - `429D66DA5404E381BC1CB2E7CBAF6A04CA9819A52197A5F87F85E4D975734252`
  - installed live with fresh backups:
    - `ShippingPC-BmGame.exe.raw-controltype-backup-20260326-123854`
    - `BmGame.u.raw-controltype-backup-20260326-123854`
  - cold-start tested the live game for `15s`
  - process stayed alive through startup

### 40. Broadened the live subtitle-size hook to all matching executable setter tails

- Symptom:
  - `Subtitle Size` persisted via raw `ControlType` codes
  - live size still did not change, so the 12-RVA FE setter cluster was incomplete
- Fix:
  - changed `NativeSubtitleExePatcher` to scan executable sections for the `8B C6 5E 59 C3` tail pattern instead of relying on a hardcoded thunk list
  - the worker still ignores everything except:
    - `4101` = Small
    - `4102` = Normal
    - `4103` = Large
  - current broad hook count on the clean original exe:
    - `342`
- Verification:
  - rebuilt `tools/NativeSubtitleExePatcher`
  - repatched staged clean exe from:
    - `ShippingPC-BmGame.exe.bink-subtitle-scale-backup-20260325-114609`
  - staged exe SHA-256:
    - `cfd56abf50f202f3883535c98e0d4932f70e11ddbe330adf2f1f922e6ae655cf`
  - installed live exe with backup:
    - `ShippingPC-BmGame.exe.all-tail-hook-backup-20260326-124350`
  - cold-start tested the live game for `15s`
  - process stayed alive through startup
- Next check:
  - verify whether in-game `Subtitle Size` now changes live
  - if it works, narrow the hook down from the broad scan to the exact live path later

### 41. Switched live apply from generic setter tails to one-arg wrapper entry hooks

- Symptom:
  - persistence was working again
  - live size still did not change
  - the broad `342`-tail hook was clearly too indirect for the current `FE_SetRender3D` runtime path
- Fix:
  - changed the pause/HUD runtime apply path to call `FE_SetRender3D(...)`
  - kept persistence on the rewired `FE_Get/SetControlType -> Options_VolumeHeadset` backend
  - changed `NativeSubtitleExePatcher` so `--subtitle-size-signal` no longer hooks `8B C6 5E 59 C3` tails
  - it now hooks the much narrower one-argument wrapper entry shape:
    - `8B 44 24 04 50 B9`
  - the wrapper-entry trampoline reads the raw stack argument before the native call and only updates scale for:
    - `4101` = Small
    - `4102` = Normal
    - `4103` = Large
  - current wrapper hook count on the clean original exe:
    - `13`
  - hooked RVAs:
    - `0x00009660`
    - `0x000096A0`
    - `0x000096E0`
    - `0x00009720`
    - `0x00009760`
    - `0x00009810`
    - `0x00009910`
    - `0x00E7F520`
    - `0x00E84B80`
    - `0x01660730`
    - `0x01668360`
    - `0x016D9EF0`
    - `0x017B3700`
- Verification:
  - rebuilt:
    - `tools/NativeSubtitleExePatcher`
    - `tools/SubtitleSizeModBuilder`
  - rebuilt fresh pause/HUD assets via:
    - `build-pause-runtime-scale`
  - patched staged clean package from:
    - `BmGame.u.live-command-hook-backup-20260326-1007`
  - staged package:
    - `working/BmGame-pause-runtime-scale-render3d.u`
  - patched clean original exe backup to the new wrapper-hook build
  - live files installed with fresh backups:
    - `ShippingPC-BmGame.exe.render3d-wrapper-backup-20260326-145144`
    - `BmGame.u.render3d-wrapper-backup-20260326-145144`
  - live exe SHA-256:
    - `d81121156b792a0982d1db14f99f19f29b41f527188202a9afebaac5fdfe18b1`
  - live package SHA-256:
    - `fda3f6b955439c5d66de2772ad8238fdd9b3a478c8c7d537b426646f9e8b3be3`
  - cold-start tested the live game for `15s`
  - process stayed alive through startup
- Next check:
  - verify whether `Subtitle Size` now changes live in gameplay
  - verify whether the saved value still reapplies after restart

### 42. Replaced FE hook guessing with direct live UI-state scan

- Symptom:
  - `Subtitle Size` persistence was working again
  - live size still did not change
  - FE setter / wrapper / invoke tracing never produced a reliable runtime signal
- Evidence:
  - external RAM snapshots found the real live menu state block
  - stable signature around the active value:
    - `50, 100, 100, 100`
    - then `4101/4102/4103`
    - then `1, 0, 1`
    - then the same `4101/4102/4103` again
  - matching addresses moved between runs, so hardcoding heap pointers was not viable
- Fix:
  - added a new internal exe mode in `NativeSubtitleExePatcher`:
    - `--ui-state-live`
  - this no longer depends on FE wrapper hooks or `FE_RunCommand`
  - the global text helper now calls an in-process worker that:
    - validates a cached pointer to the UI state block
    - scans readable committed memory with `VirtualQuery` if the pointer is missing/invalid
    - matches the subtitle-size UI signature directly in RAM
    - maps:
      - `4101 -> 1.3`
      - `4102 -> 1.5`
      - `4103 -> 1.8`
    - writes the live scale to the existing shared scale slot used by the text helper
  - debug state now marks this path with hook id:
    - `0xFFFFFFFD`
- Verification:
  - rebuilt `tools/NativeSubtitleExePatcher`
  - patched a disposable clean exe copy from:
    - `ShippingPC-BmGame.exe.bink-text-scale-backup-20260325-121857`
  - disposable patched exe:
    - `working/ShippingPC-BmGame-ui-state-test.exe`
  - startup-tested disposable build for `15s`
    - process stayed alive
  - installed the same tested build live to:
    - `D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe`
  - live backup:
    - `ShippingPC-BmGame.exe.ui-state-live-backup-20260326-1629`
  - live exe SHA-256:
    - `5ffd8c522884607f3a1c49e83a726d5e23fd3e1b24d6c17c0b40d96de0fa9980`
- Next check:
  - verify whether changing `Subtitle Size` in the in-game pause menu now changes size live
  - verify whether the saved value still persists across relaunches

### 43. Fixed UI-state scanner crash at region boundaries

- Symptom:
  - the first `ui-state-live` build threw:
    - `Rendering thread exception`
    - `General protection fault`
  - crash report address:
    - `0x1e28971`
  - that address lands inside the worker cave, not FMOD
- Root cause:
  - the worker validated candidates with:
    - `cmp dword ptr [edi-10h], 32h`
  - but the scan start only clamped `edi` to:
    - `max(regionBase, globalStart+0x10)`
  - so on some committed regions `edi` could still equal `regionBase`
  - reading `[edi-10h]` then crossed backward out of the region and faulted in the render thread
- Fix:
  - cached-pointer validation now calls `VirtualQuery` on `edi`, not `edi-10h`
  - cached-pointer validation now requires:
    - `edi >= regionBase + 0x10`
    - `edi + 0x20 < regionEnd`
  - region scan start now uses:
    - `max(regionBase + 0x10, globalStart + 0x10)`
- Verification:
  - rebuilt `tools/NativeSubtitleExePatcher`
  - rebuilt disposable exe:
    - `working/ShippingPC-BmGame-ui-state-test.exe`
  - new disposable SHA-256:
    - `cce2821b55094332b27c71e108b67320b3f8807b01feb8e3323ea5fe8f724e66`
  - startup-tested disposable build for `15s`
    - process stayed alive
- Next check:
  - reinstall the corrected `ui-state-live` build live
  - verify whether it both stays stable and changes subtitle size live

### 44. Narrowed the live UI-state scan to the real heap window

- Symptom:
  - corrected `ui-state-live` build no longer crashed
  - but `cachedPtr` stayed `0` and `hits` stayed `0`
  - external scan found the real current subtitle-size block at:
    - `0x2B246CDC`
  - current live block value after changing to `Small`:
    - `4101`
- Diagnosis:
  - the scanner was walking the broad range:
    - `0x20000000 .. 0x70000000`
  - by the time the pause menu allocated the real UI block, the rolling cursor had already advanced beyond the `0x2B24xxxx` heap area and had not wrapped yet
- Fix:
  - tightened the in-exe live UI-state scan window to the observed hot range:
    - start `0x2B000000`
    - end `0x2C000000`
  - this keeps rescans focused on the heap region where the live menu block has repeatedly appeared
- Verification:
  - rebuilt `tools/NativeSubtitleExePatcher`
  - rebuilt disposable exe:
    - `working/ShippingPC-BmGame-ui-state-test.exe`
  - new disposable SHA-256:
    - `a34123f71a44f8efc6fa1231deebdaea2523b346b985446e63f72550cf72f5d2`
  - startup-tested disposable build for `15s`
    - process stayed alive
  - installed the same build live
  - live backup:
    - `ShippingPC-BmGame.exe.ui-state-hotrange-backup-20260326-1711`
- Next check:
  - verify whether `Subtitle Size` now changes live
  - verify whether the saved value still survives restart
