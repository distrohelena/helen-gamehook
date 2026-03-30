param(
    [string]$BatmanRoot,
    [string]$Configuration = 'Debug',
    [string]$BuildLabel = 'tdd-main-menu-version-label'
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}

$BuilderRoot = Join-Path $BatmanRoot 'builder'
$FfdecPath = Join-Path $BuilderRoot 'extracted\ffdec\ffdec-cli.exe'
$SubtitleSizeModBuilderProjectPath = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj'
$OutputRoot = Join-Path $BuilderRoot 'generated\test-main-menu-version-label'
$FrontendSourceScriptsRoot = Join-Path $BuilderRoot 'extracted\frontend\mainv2\frontend-mainv2-export\scripts'

foreach ($RequiredPath in @($FfdecPath, $SubtitleSizeModBuilderProjectPath)) {
    if (-not (Test-Path -LiteralPath $RequiredPath)) {
        throw "Required builder input was not found: $RequiredPath"
    }
}

if (Test-Path -LiteralPath $OutputRoot) {
    Remove-Item -LiteralPath $OutputRoot -Recurse -Force
}

& dotnet run --project $SubtitleSizeModBuilderProjectPath -c $Configuration -- `
    build-main-menu-version-label `
    --root $BuilderRoot `
    --output-dir $OutputRoot `
    --ffdec $FfdecPath `
    --build-label $BuildLabel
if ($LASTEXITCODE -ne 0) {
    throw "build-main-menu-version-label failed for regression test."
}

$RootScriptPath = Join-Path $OutputRoot '_build\frontend-scripts\frame_1\DoAction.as'
$FrontendManifestPath = Join-Path $OutputRoot 'subtitle-size-frontend.manifest.jsonc'
$FrontendAudioFrame1ScriptPath = Join-Path $OutputRoot '_build\frontend-scripts\DefineSprite_359_ScreenOptionsAudio\frame_1\DoAction.as'
$FrontendAudioClipActionPath = Join-Path $OutputRoot '_build\frontend-scripts\DefineSprite_359_ScreenOptionsAudio\frame_1\PlaceObject2_290_List_Template_53\CLIPACTIONRECORD onClipEvent(load).as'
$FrontendListItemPath = Join-Path $OutputRoot '_build\frontend-scripts\__Packages\rs\ui\ListItem.as'
$FrontendXmlPath = Join-Path $OutputRoot '_build\MainV2-subtitle-size.xml'
$SourceFrontendAudioFrame1ScriptPath = Join-Path $FrontendSourceScriptsRoot 'DefineSprite_359_ScreenOptionsAudio\frame_1\DoAction.as'
$SourceFrontendAudioClipActionPath = Join-Path $FrontendSourceScriptsRoot 'DefineSprite_359_ScreenOptionsAudio\frame_1\PlaceObject2_290_List_Template_53\CLIPACTIONRECORD onClipEvent(load).as'
$SourceFrontendListItemPath = Join-Path $FrontendSourceScriptsRoot '__Packages\rs\ui\ListItem.as'

foreach ($RequiredOutput in @($RootScriptPath, $FrontendManifestPath)) {
    if (-not (Test-Path -LiteralPath $RequiredOutput)) {
        throw "Expected version-label build output was not found: $RequiredOutput"
    }
}

$RootScript = Get-Content -LiteralPath $RootScriptPath -Raw
$ExpectedToken = "var PCVersionString = ""$BuildLabel"";"
if ($RootScript.IndexOf($ExpectedToken, [System.StringComparison]::Ordinal) -lt 0) {
    throw "Expected frontend root script to contain '$ExpectedToken'."
}

foreach ($RequiredCopiedScript in @($FrontendAudioFrame1ScriptPath, $FrontendAudioClipActionPath, $FrontendListItemPath)) {
    if (-not (Test-Path -LiteralPath $RequiredCopiedScript)) {
        throw "Version-label build should preserve the extracted frontend script tree: $RequiredCopiedScript"
    }
}

if (Test-Path -LiteralPath $FrontendXmlPath) {
    throw "Version-label build should not generate a patched frontend XML: $FrontendXmlPath"
}

foreach ($Pair in @(
    @{ Source = $SourceFrontendAudioFrame1ScriptPath; Output = $FrontendAudioFrame1ScriptPath; Name = 'audio frame 1 script' },
    @{ Source = $SourceFrontendAudioClipActionPath; Output = $FrontendAudioClipActionPath; Name = 'audio clip action script' },
    @{ Source = $SourceFrontendListItemPath; Output = $FrontendListItemPath; Name = 'frontend ListItem script' }
)) {
    $SourceContents = Get-Content -LiteralPath $Pair.Source -Raw
    $OutputContents = Get-Content -LiteralPath $Pair.Output -Raw
    if ($SourceContents -cne $OutputContents) {
        throw "Version-label build should not rewrite the $($Pair.Name)."
    }
}

Write-Output 'PASS'
