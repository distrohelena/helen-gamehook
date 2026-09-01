# Batman Subtitle Observer Window Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Widen the Batman gameplay subtitle observer to cover the complete live-state scanner window so subtitle-size changes are detected reliably.

**Architecture:** Keep the existing gameplay-only pack, memory observer, runtime slot, native text-scale hook, and texture replacement unchanged. Change only the observer scan-end contract from `0x11000000` to `0x12000000` in the rebuild source, checked-in manifest, and verification expectations.

**Tech Stack:** PowerShell pack scripts, JSON manifests, C++ HelenRuntime tests, .NET/C++ build tooling.

---

### Task 1: Update the observer-window contract

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1` (subtitle observer manifest)
- Modify: `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/hooks.json` (`stateObservers[0].scanEndAddress`)
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanKnownGoodGameplayPackage.ps1` (expected observer header)

- [ ] **Step 1: Change the verification expectation first**

Replace the expected `scanEndAddress` literal in `Test-BatmanKnownGoodGameplayPackage.ps1` with `0x12000000`, leaving every other observer field unchanged.

- [ ] **Step 2: Run the package verifier and confirm the intentional failure**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanKnownGoodGameplayPackage.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder
```

Expected: FAIL because the checked-in manifest still declares `scanEndAddress` `0x11000000`.

- [ ] **Step 3: Update the rebuild source and checked-in manifest**

Change both subtitle observer declarations from:

```text
scanEndAddress = '0x11000000'
```

and:

```json
"scanEndAddress": "0x11000000"
```

to `0x12000000`. Do not alter the signature checks, mappings, hook, runtime slot, or package files.

- [ ] **Step 4: Run focused verification**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanKnownGoodGameplayPackage.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanUiStateLiveScanWindow.ps1
```

Expected: both commands print `PASS`.

- [ ] **Step 5: Rebuild the subtitle package**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Rebuild-BatmanPack.ps1 -Configuration Release -BuilderRoot .\games\HelenBatmanAA\builder
```

Expected: the script completes successfully and reports the gameplay delta, target package, and native blob paths.

- [ ] **Step 6: Validate generated artifacts and runtime tests**

Run:

```powershell
dotnet test .\tests\HelenRuntime.Tests\HelenRuntime.Tests.csproj --no-restore
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanKnownGoodGameplayPackage.ps1 -BatmanRoot .\games\HelenBatmanAA -BuilderRoot .\games\HelenBatmanAA\builder
```

Expected: the test executable passes, and the package verifier prints `PASS`.

- [ ] **Step 7: Commit the source fix**

```powershell
rtk.exe git add -- games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1 games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/hooks.json games/HelenBatmanAA/scripts/Test-BatmanKnownGoodGameplayPackage.ps1
rtk.exe git commit -m "Widen Batman subtitle observer scan window"
```

### Task 2: Install and verify subtitle-only deployment

**Files:**
- External installation target: `C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\helengamehook`

- [ ] **Step 1: Stop the game before replacing runtime files**

Run:

```powershell
Get-Process -Name ShippingPC-BmGame -ErrorAction SilentlyContinue | Stop-Process -Force
```

- [ ] **Step 2: Copy only the rebuilt subtitle pack and matching Release DLL**

Copy the rebuilt `batman-aa-subtitles` package and `bin\Win32\Release\HelenGameHook.dll` into the game’s `Binaries\helengamehook` layout. Preserve the existing backup directory and do not enable `batman-aa-skip-videos`.

- [ ] **Step 3: Verify the enabled pack set and hashes**

Confirm `config\packs.json` contains exactly `batman-aa-subtitles` for `ShippingPC-BmGame.exe`, and compare installed `hooks.json`, delta, native blob, and DDS hashes with the source package.

- [ ] **Step 4: Launch and inspect the live observer evidence**

After changing Subtitle Size in the in-game pause Audio menu, inspect `Binaries\helengamehook\logs\HelenGameHook.log`.

Expected evidence:

```text
[observer] resolved id=subtitleUiStateObserver ...
[observer] update id=subtitleUiStateObserver raw=4101..4106 mapped=0..5 ... commandResult=1
```

The log must show no skip-video pack activation.
