param(
    [string]$RetailFrontendPackagePath,
    [string]$BatmanRoot,
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}

if ([string]::IsNullOrWhiteSpace($RetailFrontendPackagePath)) {
    $BuilderRoot = Join-Path $BatmanRoot 'builder'
    $RetailFrontendPackagePath = Join-Path $BuilderRoot 'extracted\frontend-retail\Frontend.umap'
} else {
    $RetailFrontendPackagePath = (Resolve-Path $RetailFrontendPackagePath).Path
}

$BuilderRoot = Join-Path $BatmanRoot 'builder'
$ProjectPath = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\BmGameGfxPatcher\BmGameGfxPatcher.csproj'
$FfdecPath = Join-Path $BuilderRoot 'extracted\ffdec\ffdec-cli.exe'
$OutputRoot = Join-Path $BuilderRoot 'generated\test-retail-main-menu-audio-subtitle-size'
$PatchedPackagePath = Join-Path $OutputRoot 'Frontend-subtitle-size.umap'
$ExtractedGfxPath = Join-Path $OutputRoot 'MainV2-subtitle-size.gfx'
$ExportRoot = Join-Path $OutputRoot 'MainV2-export'
$Frame1ScriptPath = Join-Path $ExportRoot 'scripts\DefineSprite_359_ScreenOptionsAudio\frame_1\DoAction.as'
$ClipActionPath = Join-Path $ExportRoot 'scripts\DefineSprite_359_ScreenOptionsAudio\frame_1\PlaceObject2_290_List_Template_61\CLIPACTIONRECORD onClipEvent(load).as'
$ListItemPath = Join-Path $ExportRoot 'scripts\__Packages\rs\ui\ListItem.as'

if (-not (Test-Path -LiteralPath $FfdecPath)) {
    throw "FFDec CLI was not found at '$FfdecPath'."
}

if (Test-Path -LiteralPath $OutputRoot) {
    Remove-Item -LiteralPath $OutputRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

& dotnet run --project $ProjectPath -c $Configuration -- `
    patch-mainv2-audio-subtitle-size `
    --package $RetailFrontendPackagePath `
    --output $PatchedPackagePath
if ($LASTEXITCODE -ne 0) {
    throw 'patch-mainv2-audio-subtitle-size failed.'
}

& dotnet run --project $ProjectPath -c $Configuration -- `
    extract-gfx `
    --package $PatchedPackagePath `
    --owner MainMenu `
    --name MainV2 `
    --output $ExtractedGfxPath
if ($LASTEXITCODE -ne 0) {
    throw 'extract-gfx failed for patched MainV2.'
}

& $FfdecPath -export script $ExportRoot $ExtractedGfxPath
if ($LASTEXITCODE -ne 0) {
    throw 'FFDec script export failed for patched MainV2.'
}

$Frame1 = Get-Content -LiteralPath $Frame1ScriptPath -Raw
$ClipAction = Get-Content -LiteralPath $ClipActionPath -Raw
$ListItem = Get-Content -LiteralPath $ListItemPath -Raw

foreach ($ExpectedToken in @(
    'this.AddItem(Subtitles,4,1,-1,-1);',
    'this.AddItem(VolumeSFX,0,2,-1,-1);',
    'this.AddItem(VolumeMusic,1,3,-1,-1);',
    'this.AddItem(VolumeDialogue,2,4,-1,-1);',
    'this.AddItem(SubtitleSize,3,0,-1,-1);'
)) {
    if ($Frame1.IndexOf($ExpectedToken, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Expected frame-1 token was not found: $ExpectedToken"
    }
}

if ($ClipAction.IndexOf('this.Init("SubtitleSize","Small","Normal","Large");', [System.StringComparison]::Ordinal) -lt 0) {
    throw 'Expected subtitle-size clip action initializer was not found.'
}

foreach ($ExpectedToken in @(
    'this.GameVariable == "SubtitleSize"',
    'function AreSubtitlesEnabled()',
    'function IsDisabled()',
    'this.ItemText._alpha = _loc2_ ? 40 : 100;',
    'this.Label._alpha = _loc2_ ? 40 : 100;',
    'if(this.IsDisabled())',
    'flash.external.ExternalInterface.call("Helen_Ini_LoadDraft","batmanFrontendUi")',
    'flash.external.ExternalInterface.call("Helen_Ini_GetInt","batmanFrontendUi","subtitleSize")',
    'flash.external.ExternalInterface.call("Helen_Ini_SetInt","batmanFrontendUi","subtitleSize",this.State)',
    'flash.external.ExternalInterface.call("Helen_RunCommand","applySubtitleSize")'
)) {
    if ($ListItem.IndexOf($ExpectedToken, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Expected ListItem token was not found: $ExpectedToken"
    }
}

foreach ($UnexpectedToken in @(
    'flash.external.ExternalInterface.call("Helen_GetInt","ui.subtitleSize")',
    'flash.external.ExternalInterface.call("Helen_SetInt","ui.subtitleSize",this.State)'
)) {
    if ($ListItem.IndexOf($UnexpectedToken, [System.StringComparison]::Ordinal) -ge 0) {
        throw "Legacy subtitle storage token is still present: $UnexpectedToken"
    }
}

Write-Output 'PASS'
