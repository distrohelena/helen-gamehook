param(
    [string]$RetailFrontendPackagePath,
    [string]$BatmanRoot,
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$HelperScriptPath = Join-Path $PSScriptRoot 'BatmanBuilderWorkspaceHelpers.ps1'
. $HelperScriptPath

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

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}

if ([string]::IsNullOrWhiteSpace($RetailFrontendPackagePath)) {
    $RetailFrontendPackagePath = Join-Path $BatmanRoot 'builder\extracted\frontend\frontend-umap-unpacked\Frontend.umap'
} elseif (-not [System.IO.Path]::IsPathRooted($RetailFrontendPackagePath)) {
    $RetailFrontendPackagePath = Join-Path $BatmanRoot $RetailFrontendPackagePath
}

$RetailFrontendPackagePath = [System.IO.Path]::GetFullPath($RetailFrontendPackagePath)
Assert-UnrealPackageIsUnpacked -Path $RetailFrontendPackagePath -Context 'Test-BatmanRetailGraphicsOptionsPatch frontend base' | Out-Null

$BuilderRoot = Join-Path $BatmanRoot 'builder'
$SubtitleSizeModBuilderProjectPath = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj'
$ProjectPath = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\BmGameGfxPatcher\BmGameGfxPatcher.csproj'
$FfdecPath = Join-Path $BuilderRoot 'extracted\ffdec\ffdec-cli.exe'
$OutputRoot = Join-Path $BuilderRoot 'generated\test-retail-main-menu-graphics-options'
$PrototypeBuildRoot = Join-Path $OutputRoot 'prototype'
$PrototypeGfxPath = Join-Path $PrototypeBuildRoot 'MainV2-graphics-options.gfx'
$PatchedPackagePath = Join-Path $OutputRoot 'Frontend-graphics-options.umap'
$ExtractedGfxPath = Join-Path $OutputRoot 'MainV2-graphics-options.gfx'
$OutputXmlPath = Join-Path $OutputRoot 'MainV2-graphics-options.xml'
$ExportRoot = Join-Path $OutputRoot 'MainV2-export'
$ExportScriptsRoot = Join-Path $ExportRoot 'scripts'
$RetailScreenOptionsGraphicsSpriteId = '4096'
$RetailGraphicsExitPromptSpriteId = '4097'
$ExpectedTokens = @(
    'Graphics Options',
    'ScreenOptionsGraphics',
    'this.RowOrder = new Array("Fullscreen","Resolution","VSync","MSAA","DetailLevel","Bloom","DynamicShadows","MotionBlur","Distortion","FogVolumes","SphericalHarmonicLighting","AmbientOcclusion","PhysX","Stereo3D","ApplyChanges");',
    'return new Array(this.GetRowDisplayValue(rowName));',
    'this.AddItem(GraphicsRow1,14,1,-1,-1);',
    'this.AddItem(GraphicsRow15,13,0,-1,-1);',
    '_root.TriggerEvent("Options");'
)

$ForbiddenTokens = @(
    'Helen_GetInt',
    'Helen_SetInt',
    'Helen_RunCommand',
    'applyBatmanGraphicsDraft',
    'syncBatmanGraphicsDetailLevel',
    'syncBatmanGraphicsPreset',
    'Helen_GfxLoad',
    'Helen_GfxGet',
    'Helen_GfxSet',
    'Helen_GfxApply',
    'Helen_BatmanGraphicsLoadDraft',
    'Helen_BatmanGraphicsGetInt',
    'Helen_BatmanGraphicsSetInt',
    'Helen_BatmanGraphicsApplyDraft',
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
    'this.ExitPromptMode = "unsaved";',
    'this.ExitPromptMode = "restart";',
    'this.LoadDraftValues();',
    'this.CaptureInitialState();',
    'HasUnsavedChanges'
)

$ExpectedFixedRowClipPaths = @(
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

if (-not (Test-Path -LiteralPath $FfdecPath)) {
    throw "FFDec CLI was not found at '$FfdecPath'."
}

if (-not (Test-Path -LiteralPath $SubtitleSizeModBuilderProjectPath)) {
    throw "SubtitleSizeModBuilder project was not found at '$SubtitleSizeModBuilderProjectPath'."
}

if (Test-Path -LiteralPath $OutputRoot) {
    Remove-Item -LiteralPath $OutputRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

& dotnet run --project $SubtitleSizeModBuilderProjectPath -c $Configuration -- `
    build-main-menu-graphics `
    --root $BuilderRoot `
    --output-dir $PrototypeBuildRoot `
    --ffdec $FfdecPath
if ($LASTEXITCODE -ne 0) {
    throw 'build-main-menu-graphics failed for the retail graphics patch test.'
}

& dotnet run --project $ProjectPath -c $Configuration -- `
    patch-mainv2-graphics-options `
    --package $RetailFrontendPackagePath `
    --output $PatchedPackagePath `
    --prototype-gfx $PrototypeGfxPath
if ($LASTEXITCODE -ne 0) {
    throw 'patch-mainv2-graphics-options failed.'
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

$XmlResult = Invoke-ExternalProcess `
    -FilePath $FfdecPath `
    -Arguments @('-swf2xml', $ExtractedGfxPath, $OutputXmlPath)
if ($XmlResult.ExitCode -ne 0) {
    throw 'FFDec swf2xml failed for patched MainV2.'
}

$ExportResult = Invoke-ExternalProcess `
    -FilePath $FfdecPath `
    -Arguments @('-export', 'script', $ExportRoot, $ExtractedGfxPath)
if ($ExportResult.ExitCode -ne 0) {
    throw 'FFDec script export failed for patched MainV2.'
}

$ExportOutputText = @($ExportResult.Output) -join [Environment]::NewLine
foreach ($ForbiddenExportToken in @(
    'SEVERE: SWF already contains characterId=600',
    'SEVERE: SWF already contains characterId=601',
    'SEVERE: SWF already contains characterId=4096',
    'SEVERE: SWF already contains characterId=4097'
)) {
    if ($ExportOutputText.IndexOf($ForbiddenExportToken, [System.StringComparison]::Ordinal) -ge 0) {
        throw "FFDec export reported a duplicate retail character id: $ForbiddenExportToken"
    }
}

[xml]$Document = Get-Content -LiteralPath $OutputXmlPath
$RetailGraphicsScreenDefinitions = @(
    $Document.SelectNodes("/swf/tags/item[starts-with(@type,'Define') and (@shapeId='$RetailScreenOptionsGraphicsSpriteId' or @spriteId='$RetailScreenOptionsGraphicsSpriteId' or @characterID='$RetailScreenOptionsGraphicsSpriteId' or @characterId='$RetailScreenOptionsGraphicsSpriteId' or @buttonId='$RetailScreenOptionsGraphicsSpriteId' or @fontId='$RetailScreenOptionsGraphicsSpriteId')]")
)
if ($RetailGraphicsScreenDefinitions.Count -ne 1) {
    throw "Expected exactly one definition with id $RetailScreenOptionsGraphicsSpriteId, found $($RetailGraphicsScreenDefinitions.Count)."
}

if ($RetailGraphicsScreenDefinitions[0].Attributes['type'].Value -ne 'DefineSpriteTag') {
    throw "Expected id $RetailScreenOptionsGraphicsSpriteId to belong to DefineSpriteTag, but found $($RetailGraphicsScreenDefinitions[0].Attributes['type'].Value)."
}

$RetailGraphicsExitPromptDefinitions = @(
    $Document.SelectNodes("/swf/tags/item[starts-with(@type,'Define') and (@shapeId='$RetailGraphicsExitPromptSpriteId' or @spriteId='$RetailGraphicsExitPromptSpriteId' or @characterID='$RetailGraphicsExitPromptSpriteId' or @characterId='$RetailGraphicsExitPromptSpriteId' or @buttonId='$RetailGraphicsExitPromptSpriteId' or @fontId='$RetailGraphicsExitPromptSpriteId')]")
)
if ($RetailGraphicsExitPromptDefinitions.Count -ne 1) {
    throw "Expected exactly one definition with id $RetailGraphicsExitPromptSpriteId, found $($RetailGraphicsExitPromptDefinitions.Count)."
}

if ($RetailGraphicsExitPromptDefinitions[0].Attributes['type'].Value -ne 'DefineSpriteTag') {
    throw "Expected id $RetailGraphicsExitPromptSpriteId to belong to DefineSpriteTag, but found $($RetailGraphicsExitPromptDefinitions[0].Attributes['type'].Value)."
}

$ScriptFiles = Get-ChildItem -LiteralPath $ExportRoot -Recurse -Filter *.as -File
if ($ScriptFiles.Count -eq 0) {
    throw "FFDec did not export any ActionScript files into $ExportRoot."
}

$GraphicsScreenDirectories = @(
    Get-ChildItem -LiteralPath $ExportScriptsRoot -Directory | Where-Object {
        $_.Name -like 'DefineSprite_*_ScreenOptionsGraphics'
    }
)
if ($GraphicsScreenDirectories.Count -ne 1) {
    throw "Expected exactly one exported ScreenOptionsGraphics sprite directory, found $($GraphicsScreenDirectories.Count)."
}

$GraphicsScreenExportRoot = $GraphicsScreenDirectories[0].FullName

foreach ($ExpectedFixedRowClipPath in $ExpectedFixedRowClipPaths) {
    $ResolvedFixedRowClipPath = Join-Path $GraphicsScreenExportRoot $ExpectedFixedRowClipPath
    if (-not (Test-Path -LiteralPath $ResolvedFixedRowClipPath)) {
        throw "Expected fixed-row clip script was not found in exported retail scripts: $ResolvedFixedRowClipPath"
    }

    $FixedRowClipScript = Get-Content -LiteralPath $ResolvedFixedRowClipPath -Raw
    foreach ($ForbiddenRowClipToken in $ForbiddenRowClipTokens) {
        if ($FixedRowClipScript.IndexOf($ForbiddenRowClipToken, [System.StringComparison]::Ordinal) -ge 0) {
            throw "Forbidden scroll-window token was found in exported row clip '$ResolvedFixedRowClipPath': $ForbiddenRowClipToken"
        }
    }
}

foreach ($ExpectedToken in $ExpectedTokens) {
    $Found = $false

    foreach ($ScriptFile in $ScriptFiles) {
        [string]$Contents = Get-Content -LiteralPath $ScriptFile.FullName -Raw

        if ([string]::IsNullOrEmpty($Contents)) {
            continue
        }

        if ($Contents.IndexOf($ExpectedToken, [System.StringComparison]::Ordinal) -ge 0) {
            $Found = $true
            break
        }
    }

    if (-not $Found) {
        throw "Expected token was not found in exported scripts: $ExpectedToken"
    }
}

foreach ($ForbiddenToken in $ForbiddenTokens) {
    foreach ($ScriptFile in $ScriptFiles) {
        [string]$Contents = Get-Content -LiteralPath $ScriptFile.FullName -Raw

        if ([string]::IsNullOrEmpty($Contents)) {
            continue
        }

        if ($Contents.IndexOf($ForbiddenToken, [System.StringComparison]::Ordinal) -ge 0) {
            throw "Forbidden token was found in exported scripts: $ForbiddenToken"
        }
    }
}

Write-Output 'PASS'
