param(
    [string]$BatmanRoot,
    [string]$BuilderRoot
)

$ErrorActionPreference = 'Stop'

function Invoke-ExternalProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    $StdOutPath = [System.IO.Path]::GetTempFileName()
    $StdErrPath = [System.IO.Path]::GetTempFileName()

    $Process = Start-Process `
        -FilePath $FilePath `
        -ArgumentList $Arguments `
        -RedirectStandardOutput $StdOutPath `
        -RedirectStandardError $StdErrPath `
        -NoNewWindow `
        -PassThru `
        -Wait

    $OutputLines = @()
    if (Test-Path -LiteralPath $StdOutPath) {
        $OutputLines += Get-Content -LiteralPath $StdOutPath
    }

    if (Test-Path -LiteralPath $StdErrPath) {
        $OutputLines += Get-Content -LiteralPath $StdErrPath
    }

    Remove-Item -LiteralPath $StdOutPath, $StdErrPath -Force

    return [pscustomobject]@{
        ExitCode = $Process.ExitCode
        Output = $OutputLines
    }
}

function Assert-GraphicsExitPromptScripts {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Context,
        [Parameter(Mandatory = $true)]
        [string]$ScriptsRoot
    )

    $GraphicsScreenDirectories = @(
        Get-ChildItem -LiteralPath $ScriptsRoot -Directory | Where-Object {
            $_.Name -like 'DefineSprite_*_ScreenOptionsGraphics'
        }
    )

    if ($GraphicsScreenDirectories.Count -ne 1) {
        throw "$Context should export exactly one ScreenOptionsGraphics directory, found $($GraphicsScreenDirectories.Count)."
    }

    $GraphicsScreenScriptPath = Join-Path $GraphicsScreenDirectories[0].FullName 'frame_1\DoAction.as'

    if (-not (Test-Path -LiteralPath $GraphicsScreenScriptPath)) {
        throw "$Context is missing required graphics screen script: $GraphicsScreenScriptPath"
    }

    $GraphicsScreenScript = Get-Content -LiteralPath $GraphicsScreenScriptPath -Raw

    foreach ($RequiredScreenToken in @(
        'attachMovie("YesNoPrompt","GraphicsExitPrompt",601)',
        'flash.external.ExternalInterface.call("Helen_Log"',
        '_root.ExitYNOpen = true;',
        '_root.ExitYNOpen = false;',
        'this.Screen.BlockInput(true);',
        'this.ExitPrompt.Yes.ButtonName = "Apply";',
        'this.ExitPrompt.No.ButtonName = "Discard";',
        'this.ExitPrompt.Response = function(bYes)',
        'this.Screen.ReturnFromScreen();',
        'this.Screen.BlockInput(false);',
        'this.Screen.ReUpdate();'
    )) {
        if ($GraphicsScreenScript.IndexOf($RequiredScreenToken, [System.StringComparison]::Ordinal) -lt 0) {
            throw "$Context graphics screen is missing the stock exit prompt token: $RequiredScreenToken"
        }
    }

    $UnblockToken = 'this.Screen.BlockInput(false);'
    $ReturnToken = 'this.Screen.ReturnFromScreen();'
    $ExitTransitionResetToken = 'this.ExitTransitionPending = false;'
    $UnblockIndex = $GraphicsScreenScript.IndexOf($UnblockToken, [System.StringComparison]::Ordinal)
    $ReturnIndex = $GraphicsScreenScript.IndexOf($ReturnToken, [System.StringComparison]::Ordinal)
    $ExitTransitionResetIndex = $GraphicsScreenScript.IndexOf($ExitTransitionResetToken, [System.StringComparison]::Ordinal)

    if ($ExitTransitionResetIndex -lt 0) {
        throw "$Context graphics screen must clear ExitTransitionPending during exit-prompt handoff."
    }

    if ($UnblockIndex -lt 0 -or $ReturnIndex -lt 0) {
        throw "$Context graphics screen must contain both unblock and return tokens for exit-prompt handoff."
    }

    if ($UnblockIndex -gt $ReturnIndex) {
        throw "$Context graphics screen must unblock input before returning from the graphics screen."
    }

    foreach ($ForbiddenScreenToken in @(
        'this.ExitPrompt.onUnload = function()',
        'OnExitPromptClosed',
        'attachMovie("GraphicsExitPrompt","GraphicsExitPrompt",601)',
        'ConfigureUnsavedChanges',
        'ConfigureRestartRequired',
        'PendingResponse',
        'OnExitPromptResponse',
        'GraphicsController.OnExitPromptResponse'
    )) {
        if ($GraphicsScreenScript.IndexOf($ForbiddenScreenToken, [System.StringComparison]::Ordinal) -ge 0) {
            throw "$Context graphics screen must not keep the custom GraphicsExitPrompt lifecycle token: $ForbiddenScreenToken"
        }
    }

    foreach ($RequiredGuardToken in @(
        'IsExitTransitionPending',
        'IsExitTransitionPending()'
    )) {
        if ($GraphicsScreenScript.IndexOf($RequiredGuardToken, [System.StringComparison]::Ordinal) -lt 0) {
            throw "$Context graphics screen must guard against reopening the stock prompt while its close transition is still pending: $RequiredGuardToken"
        }
    }
}

$PackVerificationHelpersPath = Join-Path $PSScriptRoot 'BatmanPackVerificationHelpers.ps1'
. $PackVerificationHelpersPath
$BuilderWorkspaceHelpersPath = Join-Path $PSScriptRoot 'BatmanBuilderWorkspaceHelpers.ps1'
. $BuilderWorkspaceHelpersPath

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
$HooksJsonPath = Join-Path $PackBuildRoot 'hooks.json'
$FilesJsonPath = Join-Path $PackBuildRoot 'files.json'
$DeltaPath = Join-Path $PackBuildRoot 'assets\deltas\Frontend-graphics-options.hgdelta'
$TrustedFrontendBasePath = Join-Path $BuilderRoot 'extracted\frontend-retail\Frontend.umap'
$GeneratedFrontendPackagePath = Join-Path $BuilderRoot 'generated\graphics-options-experiment\Frontend-graphics-options.umap'
$PrototypeExportScriptsRoot = Join-Path $BuilderRoot 'generated\graphics-options-experiment\prototype\_build\frontend-scripts'
$GraphicsVerificationRoot = Join-Path $BuilderRoot 'generated\graphics-options-experiment\verification'
$DecompressedGeneratedFrontendPackagePath = Join-Path $GraphicsVerificationRoot 'Frontend-graphics-options.decompressed.umap'
$ExtractedGfxPath = Join-Path $GraphicsVerificationRoot 'MainV2-graphics-options.gfx'
$ExportRoot = Join-Path $GraphicsVerificationRoot 'MainV2-export'
$ExportScriptsRoot = Join-Path $ExportRoot 'scripts'
$ToolProjectPath = Join-Path $BatmanRoot 'builder\tools\NativeSubtitleExePatcher\BmGameGfxPatcher\BmGameGfxPatcher.csproj'
$FfdecPath = Join-Path $BuilderRoot 'extracted\ffdec\ffdec-cli.exe'
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

$ExpectedGetIntConfigKeys = @(
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

$ExpectedSetIntConfigKeys = @(
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

$ExpectedSetIntFollowUpCommands = @{
    detailLevel = 'syncBatmanGraphicsPreset'
    bloom = 'syncBatmanGraphicsDetailLevel'
    dynamicShadows = 'syncBatmanGraphicsDetailLevel'
    motionBlur = 'syncBatmanGraphicsDetailLevel'
    distortion = 'syncBatmanGraphicsDetailLevel'
    fogVolumes = 'syncBatmanGraphicsDetailLevel'
    sphericalHarmonicLighting = 'syncBatmanGraphicsDetailLevel'
    ambientOcclusion = 'syncBatmanGraphicsDetailLevel'
}

$ExpectedFixedRowClipSuffixes = @(
    'frame_1\PlaceObject2_290_List_Template_141\CLIPACTIONRECORD onClipEvent(load).as',
    'frame_1\PlaceObject2_290_List_Template_133\CLIPACTIONRECORD onClipEvent(load).as',
    'frame_1\PlaceObject2_290_List_Template_125\CLIPACTIONRECORD onClipEvent(load).as',
    'frame_1\PlaceObject2_290_List_Template_117\CLIPACTIONRECORD onClipEvent(load).as',
    'frame_1\PlaceObject2_290_List_Template_109\CLIPACTIONRECORD onClipEvent(load).as',
    'frame_1\PlaceObject2_290_List_Template_101\CLIPACTIONRECORD onClipEvent(load).as',
    'frame_1\PlaceObject2_290_List_Template_93\CLIPACTIONRECORD onClipEvent(load).as',
    'frame_1\PlaceObject2_290_List_Template_85\CLIPACTIONRECORD onClipEvent(load).as',
    'frame_1\PlaceObject2_290_List_Template_77\CLIPACTIONRECORD onClipEvent(load).as',
    'frame_1\PlaceObject2_290_List_Template_69\CLIPACTIONRECORD onClipEvent(load).as',
    'frame_1\PlaceObject2_290_List_Template_61\CLIPACTIONRECORD onClipEvent(load).as',
    'frame_1\PlaceObject2_290_List_Template_53\CLIPACTIONRECORD onClipEvent(load).as',
    'frame_1\PlaceObject2_290_List_Template_45\CLIPACTIONRECORD onClipEvent(load).as',
    'frame_1\PlaceObject2_290_List_Template_37\CLIPACTIONRECORD onClipEvent(load).as',
    'frame_1\PlaceObject2_290_List_Template_29\CLIPACTIONRECORD onClipEvent(load).as'
)

$RequiredInteractiveScreenTokens = @(
    'this.RowOrder = new Array("Fullscreen","Resolution","VSync","MSAA","DetailLevel","Bloom","DynamicShadows","MotionBlur","Distortion","FogVolumes","SphericalHarmonicLighting","AmbientOcclusion","PhysX","Stereo3D","ApplyChanges");',
    'flash.external.ExternalInterface.call("Helen_GetInt",key)',
    'flash.external.ExternalInterface.call("Helen_SetInt"',
    'flash.external.ExternalInterface.call("Helen_RunCommand","applyBatmanGraphicsDraft")',
    'function ApplyDetailPresetToDraft(detailLevel)',
    'function DeriveDetailLevelFromDraft()',
    'var _loc3_ = flash.external.ExternalInterface.call("Helen_SetInt",_loc2_,nextState);',
    'HasUnsavedChanges',
    'return new Array("Windowed","Fullscreen");',
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
    'return new Array(this.GetRowDisplayValue(rowName));',
    'return "Preview Only";'
)

$RequiredRowClipTokens = @(
    '_parent.GraphicsController.HandleRowAction(this.RowName);',
    '_parent.GraphicsController.IncrementRow(this.RowName);',
    '_parent.GraphicsController.DecrementRow(this.RowName);',
    '"$UI.Cycle"'
)

$ForbiddenRowClipTokens = @(
    'this.RowOffset =',
    'OnVisibleRowClipLoaded',
    'HandleVisibleRowAction',
    'IncrementVisibleRow',
    'DecrementVisibleRow'
)

$ForbiddenPatchedPackageTokens = @(
    'var PCVersionString = "',
    'Subtitle Size'
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

if (Test-Path -LiteralPath $HooksJsonPath) {
    throw "Batman graphics-options build should not declare hooks.json: $HooksJsonPath"
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

$GeneratedFrontendPackageInfo = Get-UnrealPackageStorageInfo -Path $GeneratedFrontendPackagePath
if ($GeneratedFrontendPackageInfo.CompressionChunkCount -le 0) {
    throw "Batman graphics-options generated frontend package must be chunk-compressed for retail deployment, but '$GeneratedFrontendPackagePath' has chunkCount=$($GeneratedFrontendPackageInfo.CompressionChunkCount)."
}

if (-not (Test-Path -LiteralPath $PrototypeExportScriptsRoot)) {
    throw "Batman graphics-options prototype script export root not found: $PrototypeExportScriptsRoot"
}

foreach ($RequiredPath in @($ToolProjectPath, $FfdecPath)) {
    if (-not (Test-Path -LiteralPath $RequiredPath)) {
        throw "Batman graphics-options verification dependency not found: $RequiredPath"
    }
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
if ($StartupCommands.Count -ne 1 -or $StartupCommands[0] -ne 'loadBatmanGraphicsDraftIntoConfig') {
    throw 'Batman graphics-options build startupCommands drifted.'
}

$BindingsManifest = Get-Content -LiteralPath $BindingsJsonPath -Raw | ConvertFrom-Json
$ExternalBindings = @()
if ($null -ne $BindingsManifest.bindings) {
    $ExternalBindings = @($BindingsManifest.bindings)
} elseif ($null -ne $BindingsManifest.externalBindings) {
    $ExternalBindings = @($BindingsManifest.externalBindings)
}

if ($ExternalBindings.Count -ne ($ExpectedGetIntConfigKeys.Count + $ExpectedSetIntConfigKeys.Count + 1)) {
    throw "Batman graphics-options binding count drifted. Expected $(($ExpectedGetIntConfigKeys.Count + $ExpectedSetIntConfigKeys.Count + 1)) but found $($ExternalBindings.Count)."
}

foreach ($ExpectedConfigKey in $ExpectedGetIntConfigKeys) {
    $MatchingGetBindings = @(
        $ExternalBindings | Where-Object {
            $_.id -eq "graphicsGet$($ExpectedConfigKey.Substring(0,1).ToUpperInvariant())$($ExpectedConfigKey.Substring(1))" -and
            $_.externalName -eq 'Helen_GetInt' -and
            $_.mode -eq 'get-int' -and
            $_.configKey -eq $ExpectedConfigKey
        }
    )

    if ($MatchingGetBindings.Count -ne 1) {
        throw "Batman graphics-options bindings are missing expected Helen_GetInt config key '$ExpectedConfigKey'."
    }
}

foreach ($ExpectedConfigKey in $ExpectedSetIntConfigKeys) {
    $MatchingSetBindings = @(
        $ExternalBindings | Where-Object {
            $_.id -eq "graphicsSet$($ExpectedConfigKey.Substring(0,1).ToUpperInvariant())$($ExpectedConfigKey.Substring(1))" -and
            $_.externalName -eq 'Helen_SetInt' -and
            $_.mode -eq 'set-int' -and
            $_.configKey -eq $ExpectedConfigKey
        }
    )

    if ($MatchingSetBindings.Count -ne 1) {
        throw "Batman graphics-options bindings are missing expected Helen_SetInt config key '$ExpectedConfigKey'."
    }

    $ExpectedFollowUpCommand = $ExpectedSetIntFollowUpCommands[$ExpectedConfigKey]
    if ($null -ne $ExpectedFollowUpCommand) {
        if ($MatchingSetBindings[0].command -ne $ExpectedFollowUpCommand) {
            throw "Batman graphics-options set binding '$ExpectedConfigKey' should run '$ExpectedFollowUpCommand'."
        }
    } elseif ($null -ne $MatchingSetBindings[0].command) {
        throw "Batman graphics-options set binding '$ExpectedConfigKey' should not run a follow-up command."
    }
}

$ApplyDraftBindings = @(
    $ExternalBindings | Where-Object {
        $_.id -eq 'graphicsApplyDraft' -and
        $_.externalName -eq 'Helen_RunCommand' -and
        $_.mode -eq 'run-command' -and
        $_.command -eq 'applyBatmanGraphicsDraft'
    }
)
if ($ApplyDraftBindings.Count -ne 1) {
    throw 'Batman graphics-options bindings are missing the applyBatmanGraphicsDraft Helen_RunCommand binding.'
}

$CommandsManifest = Get-Content -LiteralPath $CommandsJsonPath -Raw | ConvertFrom-Json
$Commands = @($CommandsManifest.commands)
if ($Commands.Count -ne 4) {
    throw "Batman graphics-options command count drifted. Expected 4 but found $($Commands.Count)."
}

$LoadDraftCommand = @($Commands | Where-Object { $_.id -eq 'loadBatmanGraphicsDraftIntoConfig' })[0]
if ($null -eq $LoadDraftCommand) {
    throw 'Batman graphics-options commands are missing loadBatmanGraphicsDraftIntoConfig.'
}

if ($LoadDraftCommand.name -ne 'Load Batman Graphics Draft Into Config') {
    throw 'Batman graphics-options load command name drifted.'
}

if (@($LoadDraftCommand.steps).Count -ne 1 -or @($LoadDraftCommand.steps)[0].kind -ne 'load-batman-graphics-draft-into-config') {
    throw 'Batman graphics-options load command steps drifted.'
}

$SyncPresetCommand = @($Commands | Where-Object { $_.id -eq 'syncBatmanGraphicsPreset' })[0]
if ($null -eq $SyncPresetCommand) {
    throw 'Batman graphics-options commands are missing syncBatmanGraphicsPreset.'
}

if ($SyncPresetCommand.name -ne 'Sync Batman Graphics Preset') {
    throw 'Batman graphics-options preset-sync command name drifted.'
}

if (@($SyncPresetCommand.steps).Count -ne 1 -or @($SyncPresetCommand.steps)[0].kind -ne 'sync-batman-graphics-detail-preset') {
    throw 'Batman graphics-options preset-sync command steps drifted.'
}

$SyncDetailLevelCommand = @($Commands | Where-Object { $_.id -eq 'syncBatmanGraphicsDetailLevel' })[0]
if ($null -eq $SyncDetailLevelCommand) {
    throw 'Batman graphics-options commands are missing syncBatmanGraphicsDetailLevel.'
}

if ($SyncDetailLevelCommand.name -ne 'Sync Batman Graphics Detail Level') {
    throw 'Batman graphics-options detail-level sync command name drifted.'
}

if (@($SyncDetailLevelCommand.steps).Count -ne 1 -or @($SyncDetailLevelCommand.steps)[0].kind -ne 'sync-batman-graphics-detail-level') {
    throw 'Batman graphics-options detail-level sync command steps drifted.'
}

$ApplyDraftCommand = @($Commands | Where-Object { $_.id -eq 'applyBatmanGraphicsDraft' })[0]
if ($null -eq $ApplyDraftCommand) {
    throw 'Batman graphics-options commands are missing applyBatmanGraphicsDraft.'
}

if ($ApplyDraftCommand.name -ne 'Apply Batman Graphics Draft') {
    throw 'Batman graphics-options apply command name drifted.'
}

$ApplyDraftSteps = @($ApplyDraftCommand.steps)
if ($ApplyDraftSteps.Count -ne 2 `
    -or $ApplyDraftSteps[0].kind -ne 'apply-batman-graphics-config' `
    -or $ApplyDraftSteps[1].kind -ne 'load-batman-graphics-draft-into-config') {
    throw 'Batman graphics-options apply command steps drifted.'
}

$FilesManifest = Get-Content -LiteralPath $FilesJsonPath -Raw | ConvertFrom-Json
$VirtualFiles = @($FilesManifest.virtualFiles)
if ($VirtualFiles.Count -ne 1) {
    throw "Batman graphics-options package manifest expected exactly 1 virtual file, found $($VirtualFiles.Count)."
}

$PrototypeGraphicsScreenDirectories = @(
    Get-ChildItem -LiteralPath $PrototypeExportScriptsRoot -Directory | Where-Object {
        $_.Name -like 'DefineSprite_*_ScreenOptionsGraphics'
    }
)

if ($PrototypeGraphicsScreenDirectories.Count -ne 1) {
    throw "Batman graphics-options prototype should export exactly one ScreenOptionsGraphics directory, found $($PrototypeGraphicsScreenDirectories.Count)."
}

$PrototypeGraphicsScreenRoot = $PrototypeGraphicsScreenDirectories[0].FullName
$GraphicsScreenScriptPath = Join-Path $PrototypeGraphicsScreenRoot 'frame_1\DoAction.as'
if (-not (Test-Path -LiteralPath $GraphicsScreenScriptPath)) {
    throw "Batman graphics-options prototype screen script was not found: $GraphicsScreenScriptPath"
}

$GraphicsScreenScript = Get-Content -LiteralPath $GraphicsScreenScriptPath -Raw
foreach ($RequiredInteractiveScreenToken in $RequiredInteractiveScreenTokens) {
    if ($GraphicsScreenScript.IndexOf($RequiredInteractiveScreenToken, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Batman graphics-options prototype screen script is missing required interactive token: $RequiredInteractiveScreenToken"
    }
}

foreach ($ForbiddenScrollWindowToken in $ForbiddenScrollWindowTokens) {
    if ($GraphicsScreenScript.IndexOf($ForbiddenScrollWindowToken, [System.StringComparison]::Ordinal) -ge 0) {
        throw "Batman graphics-options prototype screen script still contains forbidden scroll-window token: $ForbiddenScrollWindowToken"
    }
}

Assert-GraphicsExitPromptScripts `
    -Context 'Batman graphics-options prototype' `
    -ScriptsRoot $PrototypeExportScriptsRoot

foreach ($ExpectedFixedRowClipSuffix in $ExpectedFixedRowClipSuffixes) {
    $ResolvedFixedRowClipPath = Join-Path $PrototypeGraphicsScreenRoot $ExpectedFixedRowClipSuffix
    if (-not (Test-Path -LiteralPath $ResolvedFixedRowClipPath)) {
        throw "Batman graphics-options prototype is missing required fixed-row clip script: $ResolvedFixedRowClipPath"
    }

    $FixedRowClipScript = Get-Content -LiteralPath $ResolvedFixedRowClipPath -Raw
    foreach ($RequiredRowClipToken in $RequiredRowClipTokens) {
        if ($FixedRowClipScript.IndexOf($RequiredRowClipToken, [System.StringComparison]::Ordinal) -lt 0) {
            throw "Batman graphics-options prototype row clip is missing required interactive token '$RequiredRowClipToken': $ResolvedFixedRowClipPath"
        }
    }

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

if (Test-Path -LiteralPath $GraphicsVerificationRoot) {
    Remove-Item -LiteralPath $GraphicsVerificationRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $GraphicsVerificationRoot | Out-Null

& dotnet run --project $ToolProjectPath -c Debug -- `
    decompress `
    --package $GeneratedFrontendPackagePath `
    --output $DecompressedGeneratedFrontendPackagePath
if ($LASTEXITCODE -ne 0) {
    throw "Batman graphics-options package verification failed to decompress $GeneratedFrontendPackagePath."
}

& dotnet run --project $ToolProjectPath -c Debug -- `
    extract-gfx `
    --package $DecompressedGeneratedFrontendPackagePath `
    --owner MainMenu `
    --name MainV2 `
    --output $ExtractedGfxPath
if ($LASTEXITCODE -ne 0) {
    throw "Batman graphics-options package verification failed to extract MainV2 from $DecompressedGeneratedFrontendPackagePath."
}

$ExportResult = Invoke-ExternalProcess `
    -FilePath $FfdecPath `
    -Arguments @('-export', 'script', $ExportRoot, $ExtractedGfxPath)
if ($ExportResult.ExitCode -ne 0) {
    throw "Batman graphics-options package verification failed to export MainV2 scripts from $ExtractedGfxPath."
}

$PatchedScriptFiles = @(Get-ChildItem -LiteralPath $ExportScriptsRoot -Recurse -Filter *.as -File)
if ($PatchedScriptFiles.Count -eq 0) {
    throw "Batman graphics-options package verification did not export any ActionScript files into $ExportScriptsRoot."
}

foreach ($ForbiddenPatchedPackageToken in $ForbiddenPatchedPackageTokens) {
    foreach ($PatchedScriptFile in $PatchedScriptFiles) {
        [string]$PatchedContents = Get-Content -LiteralPath $PatchedScriptFile.FullName -Raw
        if ([string]::IsNullOrEmpty($PatchedContents)) {
            continue
        }

        if ($PatchedContents.IndexOf($ForbiddenPatchedPackageToken, [System.StringComparison]::Ordinal) -ge 0) {
            throw "Batman graphics-options package exported scripts still contain forbidden non-graphics token '$ForbiddenPatchedPackageToken': $($PatchedScriptFile.FullName)"
        }
    }
}

Assert-GraphicsExitPromptScripts `
    -Context 'Batman graphics-options packaged MainV2 export' `
    -ScriptsRoot $ExportScriptsRoot

Write-Output 'PASS'
