param(
    [string]$BatmanRoot
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}

$GeneratedRoot = Join-Path $BatmanRoot 'builder\generated'
$PauseXmlPath = Join-Path $GeneratedRoot 'pause-runtime-scale\_build\Pause-runtime-scale.xml'
$PauseAudioFrame1ScriptPath = Join-Path $GeneratedRoot 'pause-runtime-scale\_build\pause-scripts\DefineSprite_394_ScreenOptionsAudio\frame_1\DoAction.as'

if (-not (Test-Path -LiteralPath $PauseXmlPath)) {
    throw "Generated pause XML not found: $PauseXmlPath"
}

if (-not (Test-Path -LiteralPath $PauseAudioFrame1ScriptPath)) {
    throw "Generated pause audio frame script not found: $PauseAudioFrame1ScriptPath"
}

[xml]$Document = Get-Content -LiteralPath $PauseXmlPath -Raw
$AudioScreen = $Document.SelectSingleNode("//item[@type='DefineSpriteTag' and @spriteId='394']")
if ($null -eq $AudioScreen) {
    throw 'Pause audio screen sprite 394 was not found.'
}

$SubTags = $AudioScreen.SelectSingleNode('subTags')
if ($null -eq $SubTags) {
    throw 'Pause audio screen subTags node was not found.'
}

$ExpectedRows = @(
    @{ Name = 'Subtitles';      Depth = '57'; TranslateY = 3055 },
    @{ Name = 'VolumeSFX';      Depth = '49'; TranslateY = 3813 },
    @{ Name = 'VolumeMusic';    Depth = '41'; TranslateY = 4490 },
    @{ Name = 'VolumeDialogue'; Depth = '33'; TranslateY = 5170 },
    @{ Name = 'SubtitleSize';   Depth = '61'; TranslateY = 5850 }
)

$ExpectedRowNamesInVisualOrder = @(
    'Subtitles',
    'VolumeSFX',
    'VolumeMusic',
    'VolumeDialogue',
    'SubtitleSize'
)

$ExpectedAddItemLines = @(
    'this.AddItem(Subtitles,4,1,-1,-1);',
    'this.AddItem(VolumeSFX,0,2,-1,-1);',
    'this.AddItem(VolumeMusic,1,3,-1,-1);',
    'this.AddItem(VolumeDialogue,2,4,-1,-1);',
    'this.AddItem(SubtitleSize,3,0,-1,-1);'
)

$ExpectedRowNamesInScriptOrder = @(
    'Subtitles',
    'VolumeSFX',
    'VolumeMusic',
    'VolumeDialogue',
    'SubtitleSize'
)

$ExpectedSubtitleSizeAnimatedY = 5850

foreach ($ExpectedRow in $ExpectedRows) {
    $Row = $SubTags.SelectSingleNode("item[@type='PlaceObject2Tag' and @name='$($ExpectedRow.Name)' and @depth='$($ExpectedRow.Depth)']")
    if ($null -eq $Row) {
        throw "Expected pause audio row was not found: $($ExpectedRow.Name)"
    }

    $Matrix = $Row.SelectSingleNode('matrix')
    if ($null -eq $Matrix) {
        throw "Expected matrix was not found for row: $($ExpectedRow.Name)"
    }

    if ([int]$Matrix.Attributes['translateY'].Value -ne [int]$ExpectedRow.TranslateY) {
        throw "Pause audio row $($ExpectedRow.Name) had translateY=$($Matrix.Attributes['translateY'].Value), expected $($ExpectedRow.TranslateY)."
    }
}

$PlacedRows = @(
    $SubTags.SelectNodes("item[@type='PlaceObject2Tag' and (@name='Subtitles' or @name='VolumeSFX' or @name='VolumeMusic' or @name='VolumeDialogue' or @name='SubtitleSize')]")
)

if ($PlacedRows.Count -ne $ExpectedRowNamesInVisualOrder.Length) {
    throw "Pause audio screen expected $($ExpectedRowNamesInVisualOrder.Length) visible rows but found $($PlacedRows.Count)."
}

$ActualRowNamesInVisualOrder = @(
    $PlacedRows |
        Sort-Object { [int]$_.SelectSingleNode('matrix').Attributes['translateY'].Value } |
        ForEach-Object { $_.Attributes['name'].Value }
)

if (($ActualRowNamesInVisualOrder -join "`n") -cne ($ExpectedRowNamesInVisualOrder -join "`n")) {
    throw "Pause audio row visual order mismatch. Expected $($ExpectedRowNamesInVisualOrder -join ', ') but found $($ActualRowNamesInVisualOrder -join ', ')."
}

$SubtitleSizeDepthNodes = @(
    $SubTags.SelectNodes("item[@type='PlaceObject2Tag' and @depth='61']")
)

foreach ($DepthNode in $SubtitleSizeDepthNodes) {
    $Matrix = $DepthNode.SelectSingleNode('matrix')
    if ($null -eq $Matrix) {
        continue
    }

    if ([int]$Matrix.Attributes['translateY'].Value -ne $ExpectedSubtitleSizeAnimatedY) {
        throw "Pause audio SubtitleSize depth 61 tag had translateY=$($Matrix.Attributes['translateY'].Value), expected every depth 61 placement to stay at $ExpectedSubtitleSizeAnimatedY."
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

$ActualAddItemLines = @(
    Get-Content -LiteralPath $PauseAudioFrame1ScriptPath |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -like 'this.AddItem(*' }
)

if (($ActualAddItemLines -join "`n") -cne ($ExpectedAddItemLines -join "`n")) {
    throw "Pause audio AddItem script order mismatch. Expected $($ExpectedAddItemLines -join ' | ') but found $($ActualAddItemLines -join ' | ')."
}

$Frame12ScriptPath = Join-Path $GeneratedRoot 'pause-runtime-scale\_build\pause-scripts\DefineSprite_394_ScreenOptionsAudio\frame_12\DoAction.as'
if (-not (Test-Path -LiteralPath $Frame12ScriptPath)) {
    throw "Generated pause audio frame 12 script not found: $Frame12ScriptPath"
}

$Frame12ScriptContents = Get-Content -LiteralPath $Frame12ScriptPath -Raw
foreach ($ForbiddenToken in @('SubtitleSizeHelp', 'SubtitlePreviewLabel', 'SubtitlePreviewText')) {
    if ($Frame12ScriptContents.IndexOf($ForbiddenToken, [System.StringComparison]::Ordinal) -ge 0) {
        throw "Pause audio frame 12 script still references removed layout token: $ForbiddenToken"
    }
}

$ActualRowNamesInScriptOrder = @(
    $ActualAddItemLines |
        ForEach-Object {
            if ($_ -match '^this\.AddItem\(([^,]+),') {
                $Matches[1]
                return
            }

            throw "Unable to parse AddItem line: $_"
        }
)

if (($ActualRowNamesInScriptOrder -join "`n") -cne ($ExpectedRowNamesInScriptOrder -join "`n")) {
    throw "Pause audio script row order mismatch. Expected $($ExpectedRowNamesInScriptOrder -join ', ') but found $($ActualRowNamesInScriptOrder -join ', ')."
}

Write-Output 'PASS'
