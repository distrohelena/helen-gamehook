# Batman Audio Menu Layout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebalance the in-game Batman pause `Options Audio` screen so all five rows fit comfortably inside the existing square, with `Subtitle Size` no longer cramped.

**Architecture:** Keep the change conservative by leaving the row widget behavior untouched and only changing the pause-screen XML patching that composes the screen. Remove the injected preview/help block and explicitly place the five row clips at evenly distributed Y positions inside the existing panel, then add a builder-side verification script so future rebuilds fail if the layout regresses.

**Tech Stack:** C#/.NET 8 builder tools, PowerShell verification scripts, FFDec-generated pause XML, existing Batman rebuild/deploy scripts

---

## File Map

### Modified files

- `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/PauseXmlPatcher.cs`
  - Remove preview/help injection and explicitly place the five audio rows inside the pause audio panel.
- `games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1`
  - Run the new pause-audio layout verification after the pause runtime scale asset build.

### New files

- `games/HelenBatmanAA/scripts/Test-BatmanPauseAudioLayout.ps1`
  - Verifies the generated pause XML has the expected five-row layout and no preview/help artifacts.

### Verification outputs touched by commands

- `games/HelenBatmanAA/builder/build-assets/pause-runtime-scale/_build/Pause-runtime-scale.xml`
  - Generated XML the new verifier should inspect.
- `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/assets/deltas/BmGame-subtitle-signal.hgdelta`
  - Rebuilt gameplay delta that should still pass the known-good package check.

---

### Task 1: Add a failing pause-audio layout verifier and implement the conservative row rebalance

**Files:**
- Create: `games/HelenBatmanAA/scripts/Test-BatmanPauseAudioLayout.ps1`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/PauseXmlPatcher.cs`

- [ ] **Step 1: Write the failing layout verification script**

```powershell
param(
    [string]$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'

$PauseXmlPath = Join-Path $BatmanRoot 'builder\build-assets\pause-runtime-scale\_build\Pause-runtime-scale.xml'
if (-not (Test-Path -LiteralPath $PauseXmlPath)) {
    throw "Generated pause XML not found: $PauseXmlPath"
}

[xml]$Document = Get-Content -LiteralPath $PauseXmlPath -Raw
$AudioScreen = $Document.SelectSingleNode("//item[@type='DefineSpriteTag' and @spriteId='394']")
if ($null -eq $AudioScreen) {
    throw 'Pause audio screen sprite 394 was not found.'
}

$SubTags = $AudioScreen.SelectSingleNode("subTags")
if ($null -eq $SubTags) {
    throw 'Pause audio screen subTags node was not found.'
}

$ExpectedRows = @(
    @{ Name = 'Subtitles';      Depth = '57'; TranslateY = 3055 },
    @{ Name = 'SubtitleSize';   Depth = '61'; TranslateY = 3584 },
    @{ Name = 'VolumeSFX';      Depth = '49'; TranslateY = 4113 },
    @{ Name = 'VolumeMusic';    Depth = '41'; TranslateY = 4641 },
    @{ Name = 'VolumeDialogue'; Depth = '33'; TranslateY = 5170 }
)

foreach ($ExpectedRow in $ExpectedRows) {
    $Row = $SubTags.SelectSingleNode("item[@type='PlaceObject2Tag' and @name='$($ExpectedRow.Name)' and @depth='$($ExpectedRow.Depth)']")
    if ($null -eq $Row) {
        throw "Expected pause audio row was not found: $($ExpectedRow.Name)"
    }

    $Matrix = $Row.SelectSingleNode("matrix")
    if ($null -eq $Matrix) {
        throw "Expected matrix was not found for row: $($ExpectedRow.Name)"
    }

    if ([int]$Matrix.Attributes['translateY'].Value -ne [int]$ExpectedRow.TranslateY) {
        throw "Pause audio row $($ExpectedRow.Name) had translateY=$($Matrix.Attributes['translateY'].Value), expected $($ExpectedRow.TranslateY)."
    }
}

$ForbiddenNames = @(
    'SubtitleSizeHelp',
    'SubtitlePreviewLabel',
    'SubtitlePreviewText'
)

foreach ($ForbiddenName in $ForbiddenNames) {
    $ForbiddenNode = $SubTags.SelectSingleNode("item[@type='PlaceObject2Tag' and @name='$ForbiddenName']")
    if ($null -ne $ForbiddenNode) {
        throw "Pause audio screen still contains removed layout artifact: $ForbiddenName"
    }
}

Write-Output 'PASS'
```

- [ ] **Step 2: Run the verifier to confirm it fails against the current generated layout**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanPauseAudioLayout.ps1
```

Expected:

- failure because the current generated pause XML still contains `SubtitleSizeHelp` / preview nodes, and
- failure because the current row Y positions still use the cramped layout

- [ ] **Step 3: Implement the conservative pause-audio row rebalance**

Replace the relevant parts of `PauseXmlPatcher.cs` with the following code:

```csharp
using System.Text;
using System.Xml;

namespace SubtitleSizeModBuilder;

internal static class PauseXmlPatcher
{
    private const int SubtitleRowDepth = 61;
    private const int SubtitlesRowDepth = 57;
    private const int VolumeSfxRowDepth = 49;
    private const int VolumeMusicRowDepth = 41;
    private const int VolumeDialogueRowDepth = 33;

    private const int SubtitlesRowY = 3055;
    private const int SubtitleSizeRowY = 3584;
    private const int VolumeSfxRowY = 4113;
    private const int VolumeMusicRowY = 4641;
    private const int VolumeDialogueRowY = 5170;

    public static void Patch(string inputXmlPath, string outputXmlPath)
    {
        var document = new XmlDocument
        {
            PreserveWhitespace = false
        };
        document.Load(inputXmlPath);

        XmlElement tags = GetSingleElement(document.DocumentElement, "tags");
        XmlElement screenOptionsAudio = FindSpriteById(tags, "394");

        PatchAudioScreen(screenOptionsAudio);

        Directory.CreateDirectory(Path.GetDirectoryName(outputXmlPath)!);

        var settings = new XmlWriterSettings
        {
            Encoding = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false),
            Indent = true,
            NewLineHandling = NewLineHandling.Entitize
        };

        using var writer = XmlWriter.Create(outputXmlPath, settings);
        document.Save(writer);
    }

    private static void PatchAudioScreen(XmlElement audioScreen)
    {
        XmlElement subTags = GetSingleElement(audioScreen, "subTags");
        List<XmlElement> originalChildren = subTags.ChildNodes.OfType<XmlElement>().ToList();

        XmlElement subtitlesInitial = originalChildren.Single(node =>
            node.Name == "item" &&
            GetAttribute(node, "type") == "PlaceObject2Tag" &&
            GetAttribute(node, "depth") == SubtitlesRowDepth.ToString() &&
            GetAttribute(node, "name") == "Subtitles");

        XmlElement subtitleSizeInitial = (XmlElement)subtitlesInitial.CloneNode(deep: true);
        SetAttribute(subtitleSizeInitial, "depth", SubtitleRowDepth.ToString());
        SetAttribute(subtitleSizeInitial, "name", "SubtitleSize");
        subtitlesInitial.ParentNode!.InsertAfter(subtitleSizeInitial, subtitlesInitial);

        foreach (XmlElement original in originalChildren)
        {
            if (ReferenceEquals(original, subtitlesInitial))
            {
                continue;
            }

            string? type = GetAttribute(original, "type");
            string? depth = GetAttribute(original, "depth");
            if (depth != SubtitlesRowDepth.ToString())
            {
                continue;
            }

            if (type is not ("PlaceObject2Tag" or "RemoveObject2Tag"))
            {
                continue;
            }

            XmlElement clone = (XmlElement)original.CloneNode(deep: true);
            SetAttribute(clone, "depth", SubtitleRowDepth.ToString());
            original.ParentNode!.InsertAfter(clone, original);
        }

        SetAudioRowTranslateY(subTags, "Subtitles", SubtitlesRowDepth, SubtitlesRowY);
        SetAudioRowTranslateY(subTags, "SubtitleSize", SubtitleRowDepth, SubtitleSizeRowY);
        SetAudioRowTranslateY(subTags, "VolumeSFX", VolumeSfxRowDepth, VolumeSfxRowY);
        SetAudioRowTranslateY(subTags, "VolumeMusic", VolumeMusicRowDepth, VolumeMusicRowY);
        SetAudioRowTranslateY(subTags, "VolumeDialogue", VolumeDialogueRowDepth, VolumeDialogueRowY);
    }

    private static void SetAudioRowTranslateY(XmlElement subTags, string rowName, int depth, int translateY)
    {
        XmlElement row = subTags.ChildNodes
            .OfType<XmlElement>()
            .Single(node =>
                node.Name == "item" &&
                GetAttribute(node, "type") == "PlaceObject2Tag" &&
                GetAttribute(node, "name") == rowName &&
                GetAttribute(node, "depth") == depth.ToString());

        SetTranslateY(GetSingleElement(row, "matrix"), translateY);
    }

    private static void SetTranslateY(XmlElement matrix, int translateY)
    {
        matrix.SetAttribute("translateY", translateY.ToString());
    }

    private static XmlElement FindSpriteById(XmlElement tags, string spriteId)
    {
        return tags.ChildNodes
            .OfType<XmlElement>()
            .Single(node => GetAttribute(node, "type") == "DefineSpriteTag" && GetAttribute(node, "spriteId") == spriteId);
    }

    private static XmlElement GetSingleElement(XmlNode? parent, string name)
    {
        XmlElement? element = parent?.ChildNodes.OfType<XmlElement>().SingleOrDefault(node => node.Name == name);
        return element ?? throw new InvalidOperationException($"Expected child element '{name}'.");
    }

    private static string? GetAttribute(XmlElement element, string name)
    {
        return element.HasAttribute(name) ? element.GetAttribute(name) : null;
    }

    private static void SetAttribute(XmlElement element, string name, string value)
    {
        element.SetAttribute(name, value);
    }
}
```

- [ ] **Step 4: Rebuild the pause runtime scale assets and rerun the verifier**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Rebuild-BatmanPack.ps1
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanPauseAudioLayout.ps1
```

Expected:

- the Batman pack rebuild succeeds
- the new verifier prints `PASS`
- the existing pause runtime scale verifier still prints `PASS`

- [ ] **Step 5: Commit the layout rebalance and the new verifier**

```powershell
git add games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/PauseXmlPatcher.cs games/HelenBatmanAA/scripts/Test-BatmanPauseAudioLayout.ps1
git commit -m "Rebalance Batman pause audio menu layout"
```

### Task 2: Wire the new layout verifier into the Batman rebuild workflow

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1`

- [ ] **Step 1: Write the failing rebuild integration change**

Add the new verifier path and invocation to `Rebuild-BatmanPack.ps1`:

```powershell
$PauseAudioLayoutVerifierPath = Join-Path $PSScriptRoot 'Test-BatmanPauseAudioLayout.ps1'
```

and later:

```powershell
& powershell -ExecutionPolicy Bypass -File $PauseAudioLayoutVerifierPath -BatmanRoot $BatmanRoot
if ($LASTEXITCODE -ne 0) {
    throw "Pause audio layout verification failed."
}
```

- [ ] **Step 2: Run the full rebuild flow to verify the new verifier executes**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Rebuild-BatmanPack.ps1
```

Expected:

- rebuild succeeds
- output includes the existing pause runtime scale verifier `PASS`
- output includes the new pause audio layout verifier `PASS`

- [ ] **Step 3: Re-run the known-good Batman package verification**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanKnownGoodGameplayPackage.ps1
```

Expected:

- `PASS`

- [ ] **Step 4: Commit the rebuild-workflow verification hook**

```powershell
git add games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1
git commit -m "Verify Batman pause audio layout during rebuild"
```

### Task 3: Deploy and live-verify the updated pause audio layout

**Files:**
- No new repo files should change in this task

- [ ] **Step 1: Deploy the rebuilt Batman runtime and pack**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Deploy-Batman.ps1 -Configuration Debug
```

Expected:

- `PASS`
- `DEPLOYED`

- [ ] **Step 2: Launch Batman and confirm the hook is serving the gameplay package**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Launch-Check-Batman.ps1
Get-Content 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\helengamehook\logs\HelenGameHook.log'
```

Expected:

- `PROCESS_STARTED`
- log contains `serving virtual file path=` for `BmGame.u`

- [ ] **Step 3: Manually verify the in-game pause audio layout**

Manual verification:

- open pause `Options Audio`
- confirm the visible rows are:
  - `Subtitles`
  - `Subtitle Size`
  - `SFX Volume`
  - `Music Volume`
  - `Dialogue Volume`
- confirm the preview/help area is gone
- confirm all five rows sit comfortably inside the existing square
- confirm `Subtitle Size` no longer looks cramped
- confirm changing `Subtitle Size` still updates live subtitle rendering
- confirm closing and reopening the menu preserves the selected `Subtitle Size`

- [ ] **Step 4: Verify the worktree is clean except for any intentional scratch outputs**

Run:

```powershell
git status --short --branch
```

Expected:

- only intentional untracked scratch artifacts remain
- no unexpected modified tracked files remain

---

## Self-Review

### Spec coverage

- Pause-menu `Options Audio` only: Tasks 1 through 3
- Conservative rebalance inside existing square: Task 1
- Remove preview/help UI: Task 1
- Preserve subtitle-size behavior: Tasks 1 and 3
- Builder-side regression protection: Tasks 1 and 2
- Live Batman verification: Task 3

### Placeholder scan

- No `TODO`, `TBD`, or deferred verification steps remain.
- Every code-edit step includes exact file paths and concrete code blocks.
- Every run step names the exact PowerShell command and expected result.

### Type consistency

- `PauseXmlPatcher` remains the only production type modified for layout composition.
- The new verifier script checks the same row names and row depths that `PauseXmlPatcher` is responsible for emitting.
- `Rebuild-BatmanPack.ps1` is the only rebuild entry point that wires the new verifier into the Batman asset flow.
