param(
    [string]$BatmanRoot,
    [string]$BuilderRoot
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}

if ([string]::IsNullOrWhiteSpace($BuilderRoot)) {
    $BuilderRoot = Join-Path $BatmanRoot 'builder'
} elseif (-not [System.IO.Path]::IsPathRooted($BuilderRoot)) {
    $BuilderRoot = Join-Path $BatmanRoot $BuilderRoot
}
$BuilderRoot = [System.IO.Path]::GetFullPath($BuilderRoot)

$GeneratedRoot = Join-Path $BuilderRoot 'generated'
$ListItemPath = Join-Path $GeneratedRoot 'pause-runtime-scale\_build\pause-scripts\__Packages\rs\ui\ListItem.as'
$ClipActionPath = Join-Path $GeneratedRoot 'pause-runtime-scale\_build\pause-scripts\DefineSprite_394_ScreenOptionsAudio\frame_1\PlaceObject2_322_List_Template_61\CLIPACTIONRECORD onClipEvent(load).as'
 
if (-not (Test-Path $ListItemPath)) {
    throw "Generated pause runtime scale script not found: $ListItemPath"
}

if (-not (Test-Path $ClipActionPath)) {
    throw "Generated pause subtitle clip action not found: $ClipActionPath"
}

$ListItemContents = Get-Content $ListItemPath -Raw
$ClipActionContents = Get-Content $ClipActionPath -Raw

if ($ListItemContents.IndexOf('Helen_GetInt', [System.StringComparison]::Ordinal) -ge 0) {
    throw "Pause runtime scale builder still emits Helen_GetInt."
}

if ($ListItemContents.IndexOf('Helen_SetInt', [System.StringComparison]::Ordinal) -ge 0) {
    throw "Pause runtime scale builder still emits Helen_SetInt."
}

if ($ListItemContents.IndexOf('Helen_RunCommand', [System.StringComparison]::Ordinal) -ge 0) {
    throw "Pause runtime scale builder still emits Helen_RunCommand."
}

if ($ListItemContents.IndexOf('FE_GetControlType', [System.StringComparison]::Ordinal) -lt 0) {
    throw "Pause runtime scale builder does not emit FE_GetControlType."
}

if ($ListItemContents.IndexOf('FE_SetControlType', [System.StringComparison]::Ordinal) -lt 0) {
    throw "Pause runtime scale builder does not emit FE_SetControlType."
}

if ($ListItemContents.IndexOf('4101', [System.StringComparison]::Ordinal) -lt 0) {
    throw "Pause runtime scale builder does not emit the Small raw code 4101."
}

if ($ListItemContents.IndexOf('4102', [System.StringComparison]::Ordinal) -lt 0) {
    throw "Pause runtime scale builder does not emit the Normal raw code 4102."
}

if ($ListItemContents.IndexOf('4103', [System.StringComparison]::Ordinal) -lt 0) {
    throw "Pause runtime scale builder does not emit the Large raw code 4103."
}

if ($ListItemContents.IndexOf('function ApplySubtitleSizeRuntime()', [System.StringComparison]::Ordinal) -lt 0) {
    throw "Pause runtime scale builder does not emit ApplySubtitleSizeRuntime for live subtitle refresh."
}

if ($ListItemContents.IndexOf('function GetSubtitleSizeBurstCount()', [System.StringComparison]::Ordinal) -lt 0) {
    throw "Pause runtime scale builder does not emit GetSubtitleSizeBurstCount for live subtitle refresh."
}

if ($ListItemContents.IndexOf('flash.external.ExternalInterface.call("FE_SetSubtitles"', [System.StringComparison]::Ordinal) -lt 0) {
    throw "Pause runtime scale builder does not emit FE_SetSubtitles live refresh pulses."
}

$ExpectedClipAction = 'this.Init("SubtitleSize","Small","Normal","Large");'
if ($ClipActionContents.IndexOf($ExpectedClipAction, [System.StringComparison]::Ordinal) -lt 0) {
    throw "Pause runtime scale builder does not emit the three-choice SubtitleSize initializer."
}

Write-Output 'PASS'
