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

function Get-DefineSpriteNodeOuterXml {
    param(
        [Parameter(Mandatory = $true)]
        [xml]$Document,
        [Parameter(Mandatory = $true)]
        [string]$SpriteId
    )

    $Nodes = @(
        $Document.SelectNodes("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='$SpriteId']")
    )

    if ($Nodes.Count -ne 1) {
        throw "Expected exactly one DefineSpriteTag sprite with id $SpriteId, found $($Nodes.Count)."
    }

    return $Nodes[0].OuterXml
}

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}

if ([string]::IsNullOrWhiteSpace($RetailFrontendPackagePath)) {
    $RetailFrontendPackagePath = Join-Path $BatmanRoot 'builder\extracted\frontend-retail\Frontend.umap'
} elseif (-not [System.IO.Path]::IsPathRooted($RetailFrontendPackagePath)) {
    $RetailFrontendPackagePath = Join-Path $BatmanRoot $RetailFrontendPackagePath
}

$RetailFrontendPackagePath = [System.IO.Path]::GetFullPath($RetailFrontendPackagePath)

$BuilderRoot = Join-Path $BatmanRoot 'builder'
$SubtitleSizeModBuilderProjectPath = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj'
$ProjectPath = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\BmGameGfxPatcher\BmGameGfxPatcher.csproj'
$FfdecPath = Join-Path $BuilderRoot 'extracted\ffdec\ffdec-cli.exe'
$CleanMainV2XmlPath = Join-Path $BuilderRoot 'extracted\frontend\mainv2\frontend-mainv2.xml'
$OutputRoot = Join-Path $BuilderRoot 'generated\test-retail-main-menu-graphics-options'
$PrototypeBuildRoot = Join-Path $OutputRoot 'prototype'
$PrototypeGfxPath = Join-Path $PrototypeBuildRoot 'MainV2-graphics-options.gfx'
$DecompressedRetailFrontendPackagePath = Join-Path $OutputRoot 'Frontend-retail.decompressed.umap'
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
    'Helen_GetInt',
    'Helen_SetInt',
    'Helen_RunCommand',
    'applyBatmanGraphicsDraft',
    'this.BindFixedRow(this.Screen.GraphicsRow1,"Fullscreen");',
    'this.BindFixedRow(this.Screen.GraphicsRow15,"ApplyChanges");',
    'this.RefreshRowClip(this.Screen.GraphicsRow15,"ApplyChanges");',
    'return new Array("Windowed","Fullscreen");',
    'this.AddItem(GraphicsRow1,14,1,-1,-1);',
    'this.AddItem(GraphicsRow15,13,0,-1,-1);'
)

$ForbiddenTokens = @(
    'var PCVersionString = "',
    'Subtitle Size',
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
    'return new Array(this.DraftState.fullscreen == 0 ? "Windowed" : "Fullscreen");'
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

$RequiredCleanRetailSpriteIds = @(
    '153',
    '154',
    '157',
    '232',
    '356',
    '393',
    '395'
)

if (-not (Test-Path -LiteralPath $FfdecPath)) {
    throw "FFDec CLI was not found at '$FfdecPath'."
}

if (-not (Test-Path -LiteralPath $RetailFrontendPackagePath)) {
    throw "Retail frontend package was not found at '$RetailFrontendPackagePath'."
}

if (-not (Test-Path -LiteralPath $CleanMainV2XmlPath)) {
    throw "Clean MainV2 XML baseline was not found at '$CleanMainV2XmlPath'."
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
    decompress `
    --package $RetailFrontendPackagePath `
    --output $DecompressedRetailFrontendPackagePath
if ($LASTEXITCODE -ne 0) {
    throw 'decompress failed for the retail graphics patch test.'
}

& dotnet run --project $ProjectPath -c $Configuration -- `
    patch-mainv2-graphics-options `
    --package $DecompressedRetailFrontendPackagePath `
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
[xml]$CleanMainV2Document = Get-Content -LiteralPath $CleanMainV2XmlPath

foreach ($RequiredCleanRetailSpriteId in $RequiredCleanRetailSpriteIds) {
    $PatchedOuterXml = Get-DefineSpriteNodeOuterXml -Document $Document -SpriteId $RequiredCleanRetailSpriteId
    $CleanOuterXml = Get-DefineSpriteNodeOuterXml -Document $CleanMainV2Document -SpriteId $RequiredCleanRetailSpriteId

    if ($PatchedOuterXml -cne $CleanOuterXml) {
        throw "Patched MainV2 changed clean retail sprite id $RequiredCleanRetailSpriteId. Graphics-only patch must preserve baseline title/profile/main sprite tags."
    }
}

$RetailOptionsHeaderBacking = $Document.SelectSingleNode("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='333']/subTags/item[@type='PlaceObject2Tag' and @depth='39' and @characterId='118' and @characterId!='0']")
if ($null -eq $RetailOptionsHeaderBacking) {
    throw 'Retail-patched ScreenOptionsMenu should keep the vanilla Options header backing shell on depth 39.'
}

$RetailOptionsTitlePlacement = $Document.SelectSingleNode("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='333']/subTags/item[@type='PlaceObject2Tag' and @depth='40' and @characterId='326' and @characterId!='0']")
if ($null -eq $RetailOptionsTitlePlacement) {
    throw 'Retail-patched ScreenOptionsMenu should keep the vanilla $UI.Options title on depth 40.'
}

$RetailRelocatedOptionsHeaderTimeline = @(
    $Document.SelectNodes("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='333']/subTags/item[((@depth='41' or @depth='42')) and (@type='PlaceObject2Tag' or @type='RemoveObject2Tag')]")
)
if ($RetailRelocatedOptionsHeaderTimeline.Count -ne 0) {
    throw 'Retail-patched ScreenOptionsMenu should not relocate the vanilla Options header timeline onto depths 41 or 42.'
}

$RetailInjectedGameOptionsHeader = @(
    $Document.SelectNodes("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='333']/subTags/item[@type='PlaceObject2Tag' and ((@depth='146' and @characterId='118') or (@depth='147' and @characterId='340'))]")
)
if ($RetailInjectedGameOptionsHeader.Count -ne 0) {
    throw 'Retail-patched ScreenOptionsMenu should not inject a Game Options submenu header into the main Options chooser.'
}

$RetailGraphicsScreenDefinitions = @(
    $Document.SelectNodes("/swf/tags/item[starts-with(@type,'Define') and (@shapeId='$RetailScreenOptionsGraphicsSpriteId' or @spriteId='$RetailScreenOptionsGraphicsSpriteId' or @characterID='$RetailScreenOptionsGraphicsSpriteId' or @characterId='$RetailScreenOptionsGraphicsSpriteId' or @buttonId='$RetailScreenOptionsGraphicsSpriteId' or @fontId='$RetailScreenOptionsGraphicsSpriteId')]")
)
if ($RetailGraphicsScreenDefinitions.Count -ne 1) {
    throw "Expected exactly one definition with id $RetailScreenOptionsGraphicsSpriteId, found $($RetailGraphicsScreenDefinitions.Count)."
}

if ($RetailGraphicsScreenDefinitions[0].Attributes['type'].Value -ne 'DefineSpriteTag') {
    throw "Expected id $RetailScreenOptionsGraphicsSpriteId to belong to DefineSpriteTag, but found $($RetailGraphicsScreenDefinitions[0].Attributes['type'].Value)."
}

$RetailGraphicsTitlePlacement = $Document.SelectSingleNode("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='$RetailScreenOptionsGraphicsSpriteId']/subTags/item[@type='PlaceObject2Tag' and @depth='147' and @characterId='340' and @characterId!='0']")
if ($null -eq $RetailGraphicsTitlePlacement) {
    throw "Expected retail-patched ScreenOptionsGraphics title placement at depth 147 for character 340."
}

$RetailGraphicsTitlePlacementName = $RetailGraphicsTitlePlacement.Attributes['name']
if ($null -eq $RetailGraphicsTitlePlacementName -or $RetailGraphicsTitlePlacementName.Value -ne 'Title') {
    throw "Retail-patched ScreenOptionsGraphics title placement should be named 'Title', but found '$($RetailGraphicsTitlePlacementName.Value)'."
}

$RetailGraphicsTitleHasNameFlag = $RetailGraphicsTitlePlacement.Attributes['placeFlagHasName']
if ($null -eq $RetailGraphicsTitleHasNameFlag -or $RetailGraphicsTitleHasNameFlag.Value -ne 'true') {
    throw "Retail-patched ScreenOptionsGraphics title placement should set placeFlagHasName='true', but found '$($RetailGraphicsTitleHasNameFlag.Value)'."
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
    foreach ($RequiredRowClipToken in $RequiredRowClipTokens) {
        if ($FixedRowClipScript.IndexOf($RequiredRowClipToken, [System.StringComparison]::Ordinal) -lt 0) {
            throw "Required interactive token was not found in exported row clip '$ResolvedFixedRowClipPath': $RequiredRowClipToken"
        }
    }

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
