param(
    [string]$BatmanRoot
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}

$GeneratedRoot = Join-Path $BatmanRoot 'builder\generated\main-menu-audio'
$FrontendXmlPath = Join-Path $GeneratedRoot '_build\MainV2-subtitle-size.xml'
$FrontendFrame1ScriptPath = Join-Path $GeneratedRoot '_build\frontend-scripts\DefineSprite_359_ScreenOptionsAudio\frame_1\DoAction.as'
$FrontendClipActionPath = Join-Path $GeneratedRoot '_build\frontend-scripts\DefineSprite_359_ScreenOptionsAudio\frame_1\PlaceObject2_290_List_Template_61\CLIPACTIONRECORD onClipEvent(load).as'
$FrontendListItemPath = Join-Path $GeneratedRoot '_build\frontend-scripts\__Packages\rs\ui\ListItem.as'

foreach ($RequiredPath in @($FrontendXmlPath, $FrontendFrame1ScriptPath, $FrontendClipActionPath, $FrontendListItemPath)) {
    if (-not (Test-Path -LiteralPath $RequiredPath)) {
        throw "Generated main-menu audio artifact not found: $RequiredPath"
    }
}

[xml]$Document = Get-Content -LiteralPath $FrontendXmlPath -Raw
$AudioScreen = $Document.SelectSingleNode("//item[@type='DefineSpriteTag' and @spriteId='359']")
if ($null -eq $AudioScreen) {
    throw 'Frontend audio screen sprite 359 was not found.'
}

$SubTags = $AudioScreen.SelectSingleNode('subTags')
if ($null -eq $SubTags) {
    throw 'Frontend audio screen subTags node was not found.'
}

$ExpectedRows = @(
    @{ Name = 'Subtitles';      Depth = '53'; TranslateY = 1885 },
    @{ Name = 'VolumeSFX';      Depth = '45'; TranslateY = 2785 },
    @{ Name = 'VolumeMusic';    Depth = '37'; TranslateY = 3622 },
    @{ Name = 'VolumeDialogue'; Depth = '29'; TranslateY = 4466 },
    @{ Name = 'SubtitleSize';   Depth = '61'; TranslateY = 5310 }
)

$ExpectedVisualOrder = @(
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

$VerifiedRows = @()

foreach ($ExpectedRow in $ExpectedRows) {
    $Row = $SubTags.SelectSingleNode("item[@type='PlaceObject2Tag' and @name='$($ExpectedRow.Name)' and @depth='$($ExpectedRow.Depth)']")
    if ($null -eq $Row) {
        throw "Expected frontend audio row was not found: $($ExpectedRow.Name)"
    }

    $Matrix = $Row.SelectSingleNode('matrix')
    if ($null -eq $Matrix) {
        throw "Expected matrix was not found for frontend row: $($ExpectedRow.Name)"
    }

    $ActualTranslateY = [int]$Matrix.Attributes['translateY'].Value
    if ($ActualTranslateY -ne [int]$ExpectedRow.TranslateY) {
        throw "Frontend audio row $($ExpectedRow.Name) had translateY=$ActualTranslateY, expected $($ExpectedRow.TranslateY)."
    }

    $VerifiedRows += $Row
}

$ActualVisualOrder = @(
    $VerifiedRows |
        Sort-Object { [int]$_.SelectSingleNode('matrix').Attributes['translateY'].Value } |
        ForEach-Object { $_.Attributes['name'].Value }
)

if (($ActualVisualOrder -join "`n") -cne ($ExpectedVisualOrder -join "`n")) {
    throw "Frontend audio row visual order mismatch. Expected $($ExpectedVisualOrder -join ', ') but found $($ActualVisualOrder -join ', ')."
}

$Depth61Nodes = @(
    $SubTags.SelectNodes("item[@depth='61']")
)

foreach ($Depth61Node in $Depth61Nodes) {
    $Matrix = $Depth61Node.SelectSingleNode('matrix')
    if ($null -eq $Matrix) {
        continue
    }

    $ActualTranslateY = [int]$Matrix.Attributes['translateY'].Value
    if ($ActualTranslateY -ne 5310) {
        throw "Frontend audio depth 61 tag had translateY=$ActualTranslateY, expected every depth 61 placement to stay at 5310."
    }
}

$ActualAddItemLines = @(
    Get-Content -LiteralPath $FrontendFrame1ScriptPath |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -like 'this.AddItem(*' }
)

if (($ActualAddItemLines -join "`n") -cne ($ExpectedAddItemLines -join "`n")) {
    throw "Frontend audio AddItem script order mismatch. Expected $($ExpectedAddItemLines -join ' | ') but found $($ActualAddItemLines -join ' | ')."
}

$ClipActionContents = Get-Content -LiteralPath $FrontendClipActionPath -Raw
$ExpectedClipAction = @(
    'onClipEvent(load){',
    '   this.Init("SubtitleSize","Small","Normal","Large");',
    '}'
) -join [Environment]::NewLine

if ($ClipActionContents.Trim() -cne $ExpectedClipAction.Trim()) {
    throw 'Frontend audio subtitle-size clip action did not match the expected dedicated row initializer.'
}

$ListItemContents = Get-Content -LiteralPath $FrontendListItemPath -Raw
$ExpectedListItemTokens = @(
    'this.GameVariable == "SubtitleSize"',
    'function AreSubtitlesEnabled()',
    'function IsDisabled()',
    'FE_GetSubtitles',
    'this.ItemText._alpha = _loc2_ ? 40 : 100;',
    'this.Label._alpha = _loc2_ ? 40 : 100;',
    'if(this.IsDisabled())'
)

foreach ($ExpectedToken in $ExpectedListItemTokens) {
    if ($ListItemContents.IndexOf($ExpectedToken, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Frontend ListItem script is missing required token: $ExpectedToken"
    }
}

$UnexpectedClipActionPath = Join-Path $GeneratedRoot '_build\frontend-scripts\DefineSprite_359_ScreenOptionsAudio\frame_1\PlaceObject2_290_List_Template_53\CLIPACTIONRECORD onClipEvent(load).as'
if (Test-Path -LiteralPath $UnexpectedClipActionPath) {
    $UnexpectedClipActionContents = Get-Content -LiteralPath $UnexpectedClipActionPath -Raw
    if ($UnexpectedClipActionContents.IndexOf('SubtitleSize', [System.StringComparison]::Ordinal) -ge 0) {
        throw 'Frontend audio still writes subtitle-size clip action into the old depth 53 template path.'
    }
}

Write-Output 'PASS'
