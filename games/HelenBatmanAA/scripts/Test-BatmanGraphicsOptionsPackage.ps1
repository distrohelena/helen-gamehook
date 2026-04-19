param(
    [string]$BatmanRoot,
    [string]$BuilderRoot
)

$ErrorActionPreference = 'Stop'

$PackVerificationHelpersPath = Join-Path $PSScriptRoot 'BatmanPackVerificationHelpers.ps1'
. $PackVerificationHelpersPath

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
$PackRoot = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-graphics-options'
$PackBuildRoot = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-graphics-options\builds\steam-goty-1.0'
$PackJsonPath = Join-Path $PackRoot 'pack.json'
$BuildJsonPath = Join-Path $PackBuildRoot 'build.json'
$BindingsJsonPath = Join-Path $PackBuildRoot 'bindings.json'
$CommandsJsonPath = Join-Path $PackBuildRoot 'commands.json'
$FilesJsonPath = Join-Path $PackBuildRoot 'files.json'
$DeltaPath = Join-Path $PackBuildRoot 'assets\deltas\Frontend-graphics-options.hgdelta'
$TrustedFrontendBasePath = Join-Path $BuilderRoot 'extracted\frontend-retail\Frontend.umap'
$GeneratedFrontendPackagePath = Join-Path $BuilderRoot 'generated\graphics-options-experiment\Frontend-graphics-options.umap'
$PrototypeExportScriptsRoot = Join-Path $BuilderRoot 'generated\graphics-options-experiment\prototype\_build\frontend-scripts'
$BuildMatchScriptPath = Join-Path $PSScriptRoot 'Get-BatmanSteamBuildMatch.ps1'

$ExpectedConfigKeys = @(
    'fullscreen',
    'resolutionWidth',
    'resolutionHeight',
    'vsync',
    'msaa',
    'detailLevel',
    'bloom',
    'dynamicShadows',
    'motionBlur',
    'distortion',
    'fogVolumes',
    'sphericalHarmonicLighting',
    'ambientOcclusion',
    'physx',
    'stereo'
)

$ExpectedFixedRowClipPaths = @(
    'DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_141\CLIPACTIONRECORD onClipEvent(load).as',
    'DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_133\CLIPACTIONRECORD onClipEvent(load).as',
    'DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_125\CLIPACTIONRECORD onClipEvent(load).as',
    'DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_117\CLIPACTIONRECORD onClipEvent(load).as',
    'DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_109\CLIPACTIONRECORD onClipEvent(load).as',
    'DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_101\CLIPACTIONRECORD onClipEvent(load).as',
    'DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_93\CLIPACTIONRECORD onClipEvent(load).as',
    'DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_85\CLIPACTIONRECORD onClipEvent(load).as',
    'DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_77\CLIPACTIONRECORD onClipEvent(load).as',
    'DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_69\CLIPACTIONRECORD onClipEvent(load).as',
    'DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_61\CLIPACTIONRECORD onClipEvent(load).as',
    'DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_53\CLIPACTIONRECORD onClipEvent(load).as',
    'DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_45\CLIPACTIONRECORD onClipEvent(load).as',
    'DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_37\CLIPACTIONRECORD onClipEvent(load).as',
    'DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_29\CLIPACTIONRECORD onClipEvent(load).as'
)

$RequiredStaticScreenTokens = @(
    'this.RowOrder = new Array("Fullscreen","Resolution","VSync","MSAA","DetailLevel","Bloom","DynamicShadows","MotionBlur","Distortion","FogVolumes","SphericalHarmonicLighting","AmbientOcclusion","PhysX","Stereo3D","ApplyChanges");',
    'return new Array(this.GetRowDisplayValue(rowName));',
    'this.AddItem(GraphicsRow1,14,1,-1,-1);',
    'this.AddItem(GraphicsRow15,13,0,-1,-1);',
    '_root.TriggerEvent("Options");',
    'this.Title.text = "Graphics Options";'
)

$ForbiddenScrollWindowTokens = @(
    'this.WindowStartIndex = 0;',
    'this.VisibleRowCount = 5;',
    'this.VisibleRows = new Array();',
    'BindVisibleRows',
    'BindVisibleRowClip',
    'OnVisibleRowClipLoaded',
    'HandleVisibleRowAction',
    'IncrementVisibleRow',
    'DecrementVisibleRow',
    'GetLogicalRowNameByOffset',
    'ScrollWindowUp',
    'ScrollWindowDown',
    'BaseMoveUPDown = this.MoveUPDown',
    'return new Array(this.DraftState.fullscreen == 0 ? "Windowed" : "Fullscreen");',
    'return new Array("Windowed","Fullscreen");',
    'flash.external.ExternalInterface.call("Helen_GetInt",key)',
    'flash.external.ExternalInterface.call("Helen_SetInt"',
    'flash.external.ExternalInterface.call("Helen_RunCommand","applyBatmanGraphicsDraft")',
    'this.ExitPromptMode = "unsaved";',
    'this.ExitPromptMode = "restart";',
    'this.LoadDraftValues();',
    'this.CaptureInitialState();',
    'this.ApplyChanges();',
    'HasUnsavedChanges'
)

$ForbiddenRowClipTokens = @(
    'this.RowOffset =',
    'OnVisibleRowClipLoaded',
    'HandleVisibleRowAction',
    'IncrementVisibleRow',
    'DecrementVisibleRow',
    '_parent.GraphicsController.HandleRowAction(this.RowName);',
    '_parent.GraphicsController.IncrementRow(this.RowName);',
    '_parent.GraphicsController.DecrementRow(this.RowName);',
    '_loc2_.SetPrompt(_loc2_.CI_Interact,"$UI.Cycle",this._parent.myListener.onPromptClick,100,100);'
)

if (-not (Test-Path -LiteralPath $PackJsonPath)) {
    throw "Batman graphics-options pack manifest not found: $PackJsonPath"
}

if (-not (Test-Path -LiteralPath $BuildJsonPath)) {
    throw "Batman graphics-options build manifest not found: $BuildJsonPath"
}

if (-not (Test-Path -LiteralPath $BindingsJsonPath)) {
    throw "Batman graphics-options bindings manifest not found: $BindingsJsonPath"
}

if (-not (Test-Path -LiteralPath $CommandsJsonPath)) {
    throw "Batman graphics-options commands manifest not found: $CommandsJsonPath"
}

if (-not (Test-Path -LiteralPath $FilesJsonPath)) {
    throw "Batman graphics-options package manifest not found: $FilesJsonPath"
}

if (-not (Test-Path -LiteralPath $DeltaPath)) {
    throw "Batman graphics-options delta not found: $DeltaPath"
}

if (-not (Test-Path -LiteralPath $GeneratedFrontendPackagePath)) {
    throw "Batman graphics-options generated frontend package not found: $GeneratedFrontendPackagePath"
}

if (-not (Test-Path -LiteralPath $PrototypeExportScriptsRoot)) {
    throw "Batman graphics-options prototype script export root not found: $PrototypeExportScriptsRoot"
}

$PackManifest = Get-Content -LiteralPath $PackJsonPath -Raw | ConvertFrom-Json
if ($PackManifest.id -ne 'batman-aa-graphics-options') {
    throw "Batman graphics-options pack id mismatch: $($PackManifest.id)"
}

if (@($PackManifest.builds).Count -ne 1 -or @($PackManifest.builds)[0] -ne 'steam-goty-1.0') {
    throw 'Batman graphics-options pack builds manifest drifted.'
}

$PackConfigEntries = @($PackManifest.config)
if ($PackConfigEntries.Count -ne $ExpectedConfigKeys.Count) {
    throw "Batman graphics-options pack config count mismatch. Expected $($ExpectedConfigKeys.Count) but found $($PackConfigEntries.Count)."
}

foreach ($ExpectedConfigKey in $ExpectedConfigKeys) {
    $MatchingEntries = @($PackConfigEntries | Where-Object { $_.key -eq $ExpectedConfigKey })
    if ($MatchingEntries.Count -ne 1) {
        throw "Batman graphics-options pack config is missing expected key '$ExpectedConfigKey'."
    }

    if ($MatchingEntries[0].type -ne 'int') {
        throw "Batman graphics-options config key '$ExpectedConfigKey' drifted away from type int."
    }
}

$BuildManifest = Get-Content -LiteralPath $BuildJsonPath -Raw | ConvertFrom-Json
$ExpectedBuildMatch = & $BuildMatchScriptPath
if ($BuildManifest.id -ne $ExpectedBuildMatch.BuildId) {
    throw "Batman graphics-options build id mismatch: $($BuildManifest.id)"
}

if ($BuildManifest.executable -ne $ExpectedBuildMatch.Executable) {
    throw "Batman graphics-options build executable mismatch: $($BuildManifest.executable)"
}

if ($BuildManifest.match.fileSize -ne $ExpectedBuildMatch.FileSize) {
    throw "Batman graphics-options build fileSize mismatch: $($BuildManifest.match.fileSize)"
}

if ($BuildManifest.match.sha256 -ne $ExpectedBuildMatch.Sha256) {
    throw "Batman graphics-options build sha256 mismatch: $($BuildManifest.match.sha256)"
}

$StartupCommands = @($BuildManifest.startupCommands)
if ($StartupCommands.Count -ne 0) {
    throw 'Batman graphics-options build should not declare startupCommands for the render-only screen.'
}

$BindingsManifest = Get-Content -LiteralPath $BindingsJsonPath -Raw | ConvertFrom-Json
$ExternalBindings = @()
if ($null -ne $BindingsManifest.bindings) {
    $ExternalBindings = @($BindingsManifest.bindings)
} elseif ($null -ne $BindingsManifest.externalBindings) {
    $ExternalBindings = @($BindingsManifest.externalBindings)
}

if ($ExternalBindings.Count -ne 0) {
    throw "Batman graphics-options bindings should be empty for the render-only screen, but found $($ExternalBindings.Count)."
}

$CommandsManifest = Get-Content -LiteralPath $CommandsJsonPath -Raw | ConvertFrom-Json
$Commands = @($CommandsManifest.commands)
if ($Commands.Count -ne 0) {
    throw "Batman graphics-options commands should be empty for the render-only screen, but found $($Commands.Count)."
}

$FilesManifest = Get-Content -LiteralPath $FilesJsonPath -Raw | ConvertFrom-Json
$VirtualFiles = @($FilesManifest.virtualFiles)
if ($VirtualFiles.Count -ne 1) {
    throw "Batman graphics-options package manifest expected exactly 1 virtual file, found $($VirtualFiles.Count)."
}

$GraphicsScreenScriptPath = Join-Path $PrototypeExportScriptsRoot 'DefineSprite_600_ScreenOptionsGraphics\frame_1\DoAction.as'
if (-not (Test-Path -LiteralPath $GraphicsScreenScriptPath)) {
    throw "Batman graphics-options prototype screen script was not found: $GraphicsScreenScriptPath"
}

$GraphicsScreenScript = Get-Content -LiteralPath $GraphicsScreenScriptPath -Raw
foreach ($RequiredStaticScreenToken in $RequiredStaticScreenTokens) {
    if ($GraphicsScreenScript.IndexOf($RequiredStaticScreenToken, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Batman graphics-options prototype screen script is missing required render-only token: $RequiredStaticScreenToken"
    }
}

foreach ($ForbiddenScrollWindowToken in $ForbiddenScrollWindowTokens) {
    if ($GraphicsScreenScript.IndexOf($ForbiddenScrollWindowToken, [System.StringComparison]::Ordinal) -ge 0) {
        throw "Batman graphics-options prototype screen script still contains forbidden scroll-window token: $ForbiddenScrollWindowToken"
    }
}

foreach ($ExpectedFixedRowClipPath in $ExpectedFixedRowClipPaths) {
    $ResolvedFixedRowClipPath = Join-Path $PrototypeExportScriptsRoot $ExpectedFixedRowClipPath
    if (-not (Test-Path -LiteralPath $ResolvedFixedRowClipPath)) {
        throw "Batman graphics-options prototype is missing required fixed-row clip script: $ResolvedFixedRowClipPath"
    }

    $FixedRowClipScript = Get-Content -LiteralPath $ResolvedFixedRowClipPath -Raw
    foreach ($ForbiddenRowClipToken in $ForbiddenRowClipTokens) {
        if ($FixedRowClipScript.IndexOf($ForbiddenRowClipToken, [System.StringComparison]::Ordinal) -ge 0) {
            throw "Batman graphics-options prototype row clip still contains forbidden scroll-window token '$ForbiddenRowClipToken': $ResolvedFixedRowClipPath"
        }
    }
}

$ExpectedVirtualFile = @{
    Id = 'frontendGraphicsOptionsPackage'
    Path = 'BmGame/CookedPC/Maps/Frontend/Frontend.umap'
    DeltaPath = 'assets/deltas/Frontend-graphics-options.hgdelta'
}

$VirtualFile = $VirtualFiles[0]
Assert-HgdeltaVirtualFileContract `
    -Context 'Batman graphics-options package frontendGraphicsOptionsPackage' `
    -VirtualFile $VirtualFile `
    -ExpectedId $ExpectedVirtualFile.Id `
    -ExpectedPath $ExpectedVirtualFile.Path `
    -ExpectedMode 'delta-on-read' `
    -ExpectedKind 'delta-file' `
    -ExpectedDeltaRelativePath $ExpectedVirtualFile.DeltaPath `
    -BasePath $TrustedFrontendBasePath `
    -TargetPath $GeneratedFrontendPackagePath `
    -DeltaFilePath $DeltaPath `
    -ChunkSize 65536 `
    -ChunkTableOffset 116

Write-Output 'PASS'
