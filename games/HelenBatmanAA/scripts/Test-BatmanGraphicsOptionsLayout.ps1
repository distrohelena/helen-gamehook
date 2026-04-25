param(
    [string]$BatmanRoot
)

$ErrorActionPreference = 'Stop'

function Invoke-ExternalProcess {
    param(
        [string]$FilePath,
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

$DefaultBuilderSentinel = Join-Path $BatmanRoot 'builder'
if (-not (Test-Path -LiteralPath $DefaultBuilderSentinel)) {
    $NestedBatmanRoot = Join-Path $BatmanRoot 'games\HelenBatmanAA'
    if (Test-Path -LiteralPath (Join-Path $NestedBatmanRoot 'builder')) {
        $BatmanRoot = (Resolve-Path -LiteralPath $NestedBatmanRoot).Path
    }
}

$BuilderRoot = Join-Path $BatmanRoot 'builder'
$ProjectPath = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj'
$FfdecPath = Join-Path $BuilderRoot 'extracted\ffdec\ffdec-cli.exe'
$OutputRoot = Join-Path $BuilderRoot 'generated\main-menu-graphics'
$OutputGfxPath = Join-Path $OutputRoot 'MainV2-graphics-options.gfx'
$OutputXmlPath = Join-Path $OutputRoot 'MainV2-graphics-options.xml'
$ExportRoot = Join-Path $OutputRoot 'MainV2-graphics-options-export'
$ExportScriptsRoot = Join-Path $ExportRoot 'scripts'

$ExpectedGraphicsRows = @(
    'Fullscreen',
    'Resolution',
    'VSync',
    'MSAA',
    'DetailLevel',
    'Bloom',
    'DynamicShadows',
    'MotionBlur',
    'Distortion',
    'FogVolumes',
    'SphericalHarmonicLighting',
    'AmbientOcclusion',
    'PhysX',
    'Stereo3D',
    'ApplyChanges'
)

$ExpectedGraphicsRowClipPaths = @(
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

$ExpectedGraphicsRowTranslateYByDepth = @{
    '141' = '-1380'
    '133' = '-1020'
    '125' = '-660'
    '117' = '-300'
    '109' = '60'
    '101' = '420'
    '93' = '780'
    '85' = '1140'
    '77' = '1500'
    '69' = '1860'
    '61' = '2220'
    '53' = '2580'
    '45' = '2940'
    '37' = '3300'
    '29' = '3660'
}

$ExpectedGraphicsRowInstanceNameByDepth = @{
    '141' = 'GraphicsRow1'
    '133' = 'GraphicsRow2'
    '125' = 'GraphicsRow3'
    '117' = 'GraphicsRow4'
    '109' = 'GraphicsRow5'
    '101' = 'GraphicsRow6'
    '93' = 'GraphicsRow7'
    '85' = 'GraphicsRow8'
    '77' = 'GraphicsRow9'
    '69' = 'GraphicsRow10'
    '61' = 'GraphicsRow11'
    '53' = 'GraphicsRow12'
    '45' = 'GraphicsRow13'
    '37' = 'GraphicsRow14'
    '29' = 'GraphicsRow15'
}

$ExpectedGraphicsRowDepths = @(
    '141',
    '133',
    '125',
    '117',
    '109',
    '101',
    '93',
    '85',
    '77',
    '69',
    '61',
    '53',
    '45',
    '37',
    '29'
)

$ForbiddenLegacyGameRowNames = @(
    'CameraAssist',
    'Vibration',
    'InvertGlide',
    'InvertLook'
)

$RemovedGraphicsShellDepths = @(
    '21',
    '22',
    '23',
    '25'
)

$ExpectedGraphicsShellPlacements = @(
    @{
        Depth = '1'
        CharacterId = '141'
        TranslateX = '-7126'
        TranslateY = '899'
        HasScale = 'false'
        ScaleX = $null
        ScaleY = $null
    },
    @{
        Depth = '3'
        CharacterId = '307'
        TranslateX = '-7879'
        TranslateY = '-1400'
        HasScale = 'true'
        ScaleX = '2.0688171'
        ScaleY = '2.890625'
    },
    @{
        Depth = '26'
        CharacterId = '332'
        TranslateX = '-1641'
        TranslateY = '4609'
        HasScale = 'true'
        ScaleX = '-0.5350189'
        ScaleY = '-0.5350189'
    },
    @{
        Depth = '146'
        CharacterId = '118'
        TranslateX = '-6582'
        TranslateY = '-4780'
        HasScale = 'true'
        ScaleX = '0.7967224'
        ScaleY = '0.47787476'
    },
    @{
        Depth = '147'
        CharacterId = '340'
        TranslateX = '-8292'
        TranslateY = '-5436'
        HasScale = 'false'
        ScaleX = $null
        ScaleY = $null
    }
)

$RequiredFixedRowControllerTokens = @(
    'this.BindFixedRow(this.Screen.GraphicsRow1,"Fullscreen");',
    'this.BindFixedRow(this.Screen.GraphicsRow2,"Resolution");',
    'this.BindFixedRow(this.Screen.GraphicsRow3,"VSync");',
    'this.BindFixedRow(this.Screen.GraphicsRow4,"MSAA");',
    'this.BindFixedRow(this.Screen.GraphicsRow5,"DetailLevel");',
    'this.BindFixedRow(this.Screen.GraphicsRow6,"Bloom");',
    'this.BindFixedRow(this.Screen.GraphicsRow7,"DynamicShadows");',
    'this.BindFixedRow(this.Screen.GraphicsRow8,"MotionBlur");',
    'this.BindFixedRow(this.Screen.GraphicsRow9,"Distortion");',
    'this.BindFixedRow(this.Screen.GraphicsRow10,"FogVolumes");',
    'this.BindFixedRow(this.Screen.GraphicsRow11,"SphericalHarmonicLighting");',
    'this.BindFixedRow(this.Screen.GraphicsRow12,"AmbientOcclusion");',
    'this.BindFixedRow(this.Screen.GraphicsRow13,"PhysX");',
    'this.BindFixedRow(this.Screen.GraphicsRow14,"Stereo3D");',
    'this.BindFixedRow(this.Screen.GraphicsRow15,"ApplyChanges");',
    'flash.external.ExternalInterface.call("Helen_GetInt",key)',
    'flash.external.ExternalInterface.call("Helen_SetInt"',
    'flash.external.ExternalInterface.call("Helen_RunCommand","applyBatmanGraphicsDraft")',
    'HasUnsavedChanges',
    'return new Array("Windowed","Fullscreen");',
    '_root.TriggerEvent("Options");',
    'this.AddItem(GraphicsRow1,14,1,-1,-1);',
    'this.AddItem(GraphicsRow15,13,0,-1,-1);'
)

$ForbiddenInteractiveControllerTokens = @(
    'return new Array(this.GetRowDisplayValue(rowName));',
    'return "Preview Only";',
    'return new Array(this.DraftState.fullscreen == 0 ? "Windowed" : "Fullscreen");'
)

$ForbiddenScrollControllerTokens = @(
    'this.WindowStartIndex = 0;',
    'this.VisibleRowCount = 5;',
    'this.VisibleRows = new Array();',
    'ScrollWindowUp',
    'ScrollWindowDown',
    'BindVisibleRows',
    'BindVisibleRowClip',
    'GetLogicalRowNameByOffset',
    'BaseMoveUPDown = this.MoveUPDown'
)


foreach ($RequiredPath in @($ProjectPath, $FfdecPath)) {
    if (-not (Test-Path -LiteralPath $RequiredPath)) {
        throw "Required path was not found: $RequiredPath"
    }
}

foreach ($GeneratedPath in @($OutputRoot, $OutputXmlPath, $ExportRoot)) {
    if (Test-Path -LiteralPath $GeneratedPath) {
        Remove-Item -LiteralPath $GeneratedPath -Recurse -Force
    }
}

$BuildResult = Invoke-ExternalProcess `
    -FilePath 'dotnet' `
    -Arguments @(
        'run'
        '--project'
        $ProjectPath
        '-c'
        'Debug'
        '--'
        'build-main-menu-graphics'
        '--root'
        $BuilderRoot
        '--output-dir'
        $OutputRoot
        '--ffdec'
        $FfdecPath
    )

if ($BuildResult.ExitCode -ne 0) {
    throw 'build-main-menu-graphics failed.'
}

$BuildOutputText = $BuildResult.Output -join [Environment]::NewLine
if ($BuildOutputText.IndexOf('SEVERE: SWF already contains characterId=600', [System.StringComparison]::Ordinal) -ge 0) {
    throw 'build-main-menu-graphics emitted a duplicate-character-id error for id 600.'
}

if ($BuildOutputText.IndexOf('SEVERE: SWF already contains characterId=601', [System.StringComparison]::Ordinal) -ge 0) {
    throw 'build-main-menu-graphics emitted a duplicate-character-id error for id 601.'
}

if (-not (Test-Path -LiteralPath $OutputGfxPath)) {
    throw "Expected generated graphics prototype was not found: $OutputGfxPath"
}

$XmlResult = Invoke-ExternalProcess `
    -FilePath $FfdecPath `
    -Arguments @('-swf2xml', $OutputGfxPath, $OutputXmlPath)

if ($XmlResult.ExitCode -ne 0) {
    throw 'FFDec swf2xml failed for MainV2-graphics-options.gfx.'
}

$ExportResult = Invoke-ExternalProcess `
    -FilePath $FfdecPath `
    -Arguments @('-export', 'script', $ExportRoot, $OutputGfxPath)

if ($ExportResult.ExitCode -ne 0) {
    throw 'FFDec script export failed for MainV2-graphics-options.gfx.'
}

if (-not (Test-Path -LiteralPath $ExportScriptsRoot)) {
    throw "Expected script export root was not found: $ExportScriptsRoot"
}

[xml]$Document = Get-Content -LiteralPath $OutputXmlPath -Raw
$ScreenOptionsMenu = $Document.SelectSingleNode("//item[@type='DefineSpriteTag' and @spriteId='333']")
if ($null -eq $ScreenOptionsMenu) {
    throw 'ScreenOptionsMenu sprite 333 was not found in generated output.'
}

$ScreenOptionsGraphics = $Document.SelectSingleNode("//item[@type='DefineSpriteTag' and @spriteId='600']")
if ($null -eq $ScreenOptionsGraphics) {
    throw 'ScreenOptionsGraphics sprite 600 was not found in generated output.'
}

$GraphicsHeaderBackingDefinition = $Document.SelectSingleNode("/swf/tags/item[@type='DefineShapeTag' and @shapeId='118']")
if ($null -eq $GraphicsHeaderBackingDefinition) {
    throw 'Expected character 118 to remain a direct DefineShapeTag backing asset.'
}

$GraphicsHeaderBackingSpriteDefinitions = @(
    $Document.SelectNodes("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='118']")
)
if ($GraphicsHeaderBackingSpriteDefinitions.Count -ne 0) {
    throw 'Character 118 unexpectedly became a nested sprite parent; update the graphics-header plan before patching.'
}

$GraphicsHeaderTitleDefinition = $Document.SelectSingleNode("/swf/tags/item[@type='DefineEditTextTag' and @characterID='340']")
if ($null -eq $GraphicsHeaderTitleDefinition) {
    throw 'Expected character 340 to remain the direct DefineEditTextTag title asset.'
}

$GraphicsHeaderTitleSpriteDefinitions = @(
    $Document.SelectNodes("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='340']")
)
if ($GraphicsHeaderTitleSpriteDefinitions.Count -ne 0) {
    throw 'Character 340 unexpectedly became a nested sprite parent; update the graphics-header plan before patching.'
}

$GraphicsExitPrompt = $Document.SelectSingleNode("//item[@type='DefineSpriteTag' and @spriteId='601']")
if ($null -eq $GraphicsExitPrompt) {
    throw 'Graphics exit prompt sprite 601 was not found in generated output.'
}

$Id600Definitions = @(
    $Document.SelectNodes("/swf/tags/item[starts-with(@type,'Define') and (@shapeId='600' or @spriteId='600' or @characterID='600' or @characterId='600' or @buttonId='600' or @fontId='600')]")
)

if ($Id600Definitions.Count -ne 1) {
    throw "Expected exactly one definition with id 600, found $($Id600Definitions.Count)."
}

if ($Id600Definitions[0].Attributes['type'].Value -ne 'DefineSpriteTag') {
    throw "Expected id 600 to belong to DefineSpriteTag, but found $($Id600Definitions[0].Attributes['type'].Value)."
}

$Id601Definitions = @(
    $Document.SelectNodes("/swf/tags/item[starts-with(@type,'Define') and (@shapeId='601' or @spriteId='601' or @characterID='601' or @characterId='601' or @buttonId='601' or @fontId='601')]")
)

if ($Id601Definitions.Count -ne 1) {
    throw "Expected exactly one definition with id 601, found $($Id601Definitions.Count)."
}

if ($Id601Definitions[0].Attributes['type'].Value -ne 'DefineSpriteTag') {
    throw "Expected id 601 to belong to DefineSpriteTag, but found $($Id601Definitions[0].Attributes['type'].Value)."
}

$ExpectedOptionsButtons = @(
    @{ Name = 'Game'; Depth = '43' },
    @{ Name = 'Graphics'; Depth = '37' },
    @{ Name = 'Audio'; Depth = '35' },
    @{ Name = 'Controls'; Depth = '33' },
    @{ Name = 'Credits'; Depth = '31' }
)

$OptionsMenuSubTags = $ScreenOptionsMenu.SelectSingleNode('subTags')
if ($null -eq $OptionsMenuSubTags) {
    throw 'ScreenOptionsMenu sprite 333 is missing subTags.'
}

foreach ($ExpectedButton in $ExpectedOptionsButtons) {
    $ButtonNode = $OptionsMenuSubTags.SelectSingleNode("item[@type='PlaceObject2Tag' and @characterId='117' and @name='$($ExpectedButton.Name)' and @depth='$($ExpectedButton.Depth)']")
    if ($null -eq $ButtonNode) {
        throw "Expected options-menu button placement was not found: $($ExpectedButton.Name) at depth $($ExpectedButton.Depth)."
    }
}

$OptionsButtonPlacements = @(
    $OptionsMenuSubTags.SelectNodes("item[@type='PlaceObject2Tag' and @characterId='117' and (@depth='43' or @depth='37' or @depth='35' or @depth='33' or @depth='31')]")
)
if ($OptionsButtonPlacements.Count -ne 5) {
    throw "ScreenOptionsMenu sprite 333 should contain exactly five button placements for the options stack, but found $($OptionsButtonPlacements.Count)."
}

$OptionsHeaderBacking = $OptionsMenuSubTags.SelectSingleNode("item[@type='PlaceObject2Tag' and @depth='39' and @characterId='118' and @characterId!='0']")
if ($null -eq $OptionsHeaderBacking) {
    throw 'ScreenOptionsMenu sprite 333 should keep the vanilla Options header backing shell on depth 39.'
}

$OptionsTitlePlacement = $OptionsMenuSubTags.SelectSingleNode("item[@type='PlaceObject2Tag' and @depth='40' and @characterId='326' and @characterId!='0']")
if ($null -eq $OptionsTitlePlacement) {
    throw 'ScreenOptionsMenu sprite 333 should keep the vanilla $UI.Options title on depth 40.'
}

$RelocatedOptionsHeaderTimeline = @(
    $OptionsMenuSubTags.SelectNodes("item[((@depth='41' or @depth='42')) and (@type='PlaceObject2Tag' or @type='RemoveObject2Tag')]")
)
if ($RelocatedOptionsHeaderTimeline.Count -ne 0) {
    throw 'ScreenOptionsMenu sprite 333 should not relocate the vanilla Options header timeline onto depths 41 or 42.'
}

$InjectedGameOptionsHeader = @(
    $OptionsMenuSubTags.SelectNodes("item[@type='PlaceObject2Tag' and ((@depth='146' and @characterId='118') or (@depth='147' and @characterId='340'))]")
)
if ($InjectedGameOptionsHeader.Count -ne 0) {
    throw 'ScreenOptionsMenu sprite 333 should not inject a Game Options submenu header into the main Options chooser.'
}

$ScreenRegistrationPath = Join-Path $ExportScriptsRoot 'ScreenOptionsGraphics.as'
if (-not (Test-Path -LiteralPath $ScreenRegistrationPath)) {
    throw "Expected screen registration script was not found: $ScreenRegistrationPath"
}

$GraphicsExitPromptRegistrationPath = Join-Path $ExportScriptsRoot 'GraphicsExitPrompt.as'
if (-not (Test-Path -LiteralPath $GraphicsExitPromptRegistrationPath)) {
    throw "Expected graphics exit prompt registration script was not found: $GraphicsExitPromptRegistrationPath"
}

$OptionsMenuGraphicsButtonScriptPath = Join-Path $ExportScriptsRoot 'DefineSprite_333_ScreenOptionsMenu\frame_1\PlaceObject2_117_GenericButton_37\CLIPACTIONRECORD onClipEvent(load).as'
if (-not (Test-Path -LiteralPath $OptionsMenuGraphicsButtonScriptPath)) {
    throw "Expected options-menu Graphics button script was not found: $OptionsMenuGraphicsButtonScriptPath"
}

$OptionsMenuGraphicsButtonScript = Get-Content -LiteralPath $OptionsMenuGraphicsButtonScriptPath -Raw
if ($OptionsMenuGraphicsButtonScript.IndexOf('_parent.GotoScreen("OptionsGraphics");', [System.StringComparison]::Ordinal) -lt 0) {
    throw 'Options-menu Graphics button script is missing expected OptionsGraphics routing.'
}

$GraphicsScreenScriptPath = Join-Path $ExportScriptsRoot 'DefineSprite_600_ScreenOptionsGraphics\frame_1\DoAction.as'
if (-not (Test-Path -LiteralPath $GraphicsScreenScriptPath)) {
    throw "Expected graphics screen frame script was not found: $GraphicsScreenScriptPath"
}

$GraphicsScreenSettledScriptPath = Join-Path $ExportScriptsRoot 'DefineSprite_600_ScreenOptionsGraphics\frame_15\DoAction.as'
if (-not (Test-Path -LiteralPath $GraphicsScreenSettledScriptPath)) {
    throw "Expected graphics screen settled-frame script was not found: $GraphicsScreenSettledScriptPath"
}

$GraphicsRowScriptPaths = @(
    Get-ChildItem -LiteralPath (Join-Path $ExportScriptsRoot 'DefineSprite_600_ScreenOptionsGraphics\frame_1') -Recurse -File -Filter '*.as'
)

if ($GraphicsRowScriptPaths.Count -eq 0) {
    throw 'Expected ActionScript files for sprite 600 frame_1.'
}

foreach ($ExpectedGraphicsRowClipPath in $ExpectedGraphicsRowClipPaths) {
    $ResolvedGraphicsRowClipPath = Join-Path $ExportScriptsRoot $ExpectedGraphicsRowClipPath
    if (-not (Test-Path -LiteralPath $ResolvedGraphicsRowClipPath)) {
        throw "Expected graphics row clip script was not found: $ResolvedGraphicsRowClipPath"
    }
}

$LegacyAudioRowScriptPaths = @(
    (Join-Path $ExportScriptsRoot 'DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_303_Slider_Template_29\CLIPACTIONRECORD onClipEvent(load).as'),
    (Join-Path $ExportScriptsRoot 'DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_303_Slider_Template_37\CLIPACTIONRECORD onClipEvent(load).as'),
    (Join-Path $ExportScriptsRoot 'DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_303_Slider_Template_45\CLIPACTIONRECORD onClipEvent(load).as')
)

foreach ($LegacyAudioRowScriptPath in $LegacyAudioRowScriptPaths) {
    if (Test-Path -LiteralPath $LegacyAudioRowScriptPath) {
        throw "Legacy audio slider clip action still exists in sprite 600: $LegacyAudioRowScriptPath"
    }
}

$GraphicsExitPromptScriptPath = Join-Path $ExportScriptsRoot 'DefineSprite_601_GraphicsExitPrompt\frame_1\DoAction.as'
if (-not (Test-Path -LiteralPath $GraphicsExitPromptScriptPath)) {
    throw "Expected graphics exit prompt frame script was not found: $GraphicsExitPromptScriptPath"
}

$PromptButtonScriptPaths = @(
    (Join-Path $ExportScriptsRoot 'DefineSprite_601_GraphicsExitPrompt\frame_1\PlaceObject2_117_GenericButton_11\CLIPACTIONRECORD onClipEvent(load).as'),
    (Join-Path $ExportScriptsRoot 'DefineSprite_601_GraphicsExitPrompt\frame_1\PlaceObject2_117_GenericButton_13\CLIPACTIONRECORD onClipEvent(load).as'),
    (Join-Path $ExportScriptsRoot 'DefineSprite_601_GraphicsExitPrompt\frame_1\PlaceObject2_117_GenericButton_15\CLIPACTIONRECORD onClipEvent(load).as')
)

foreach ($PromptButtonScriptPath in $PromptButtonScriptPaths) {
    if (-not (Test-Path -LiteralPath $PromptButtonScriptPath)) {
        throw "Expected prompt button script was not found: $PromptButtonScriptPath"
    }
}

$GraphicsExitPromptScript = Get-Content -LiteralPath $GraphicsExitPromptScriptPath -Raw
$ExpectedPromptAddItemLines = @(
    'this.AddItem(Yes,2,1,-1,-1);',
    'this.AddItem(Apply,0,2,-1,-1);',
    'this.AddItem(No,1,0,-1,-1);'
)

foreach ($ExpectedPromptLine in $ExpectedPromptAddItemLines) {
    if ($GraphicsExitPromptScript.IndexOf($ExpectedPromptLine, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Graphics exit prompt script is missing required AddItem line: $ExpectedPromptLine"
    }
}

$PromptButtonContents = ($PromptButtonScriptPaths | ForEach-Object {
    Get-Content -LiteralPath $_ -Raw
}) -join [Environment]::NewLine

foreach ($ExpectedPromptToken in @('Response("apply")', 'Response("discard")', 'Response("cancel")')) {
    if ($PromptButtonContents.IndexOf($ExpectedPromptToken, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Graphics exit prompt button wiring is missing token: $ExpectedPromptToken"
    }
}

$ScriptFiles = @(
    Get-ChildItem -LiteralPath $ExportScriptsRoot -Recurse -File -Filter '*.as'
)
if ($ScriptFiles.Count -eq 0) {
    throw 'FFDec script export produced no ActionScript files.'
}

$CombinedScriptContents = ($ScriptFiles | Sort-Object -Property FullName | ForEach-Object {
    Get-Content -LiteralPath $_.FullName -Raw
}) -join [Environment]::NewLine

$GraphicsRowScriptContents = ($GraphicsRowScriptPaths | Sort-Object -Property FullName | ForEach-Object {
    Get-Content -LiteralPath $_.FullName -Raw
}) -join [Environment]::NewLine

$GraphicsScreenScriptFiles = @(
    Get-ChildItem -LiteralPath (Join-Path $ExportScriptsRoot 'DefineSprite_600_ScreenOptionsGraphics') -Recurse -File -Filter '*.as'
)
if ($GraphicsScreenScriptFiles.Count -eq 0) {
    throw 'Expected ActionScript files for sprite 600.'
}

$GraphicsScreenScriptContents = ($GraphicsScreenScriptFiles | Sort-Object -Property FullName | ForEach-Object {
    Get-Content -LiteralPath $_.FullName -Raw
}) -join [Environment]::NewLine

for ($RowIndex = 0; $RowIndex -lt $ExpectedGraphicsRowClipPaths.Count; $RowIndex++) {
    $ExpectedGraphicsRowClipPath = $ExpectedGraphicsRowClipPaths[$RowIndex]
    $ResolvedGraphicsRowClipPath = Join-Path $ExportScriptsRoot $ExpectedGraphicsRowClipPath
    $GraphicsRowClipScript = Get-Content -LiteralPath $ResolvedGraphicsRowClipPath -Raw

    $ExpectedFixedRowNameToken = 'this.RowName = "' + $ExpectedGraphicsRows[$RowIndex] + '";'
    if ($GraphicsRowClipScript.IndexOf($ExpectedFixedRowNameToken, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Graphics row clip script is missing expected fixed row-name token '$ExpectedFixedRowNameToken': $ResolvedGraphicsRowClipPath"
    }

    foreach ($ExpectedControllerToken in @(
        '_parent.GraphicsController.BindFixedRow(this,this.RowName);',
        '_parent.GraphicsController.GetRowLabel(rowName);',
        '_parent.GraphicsController.GetRowValues(rowName);',
        'this.Names = _parent.GraphicsController.GetRowValues(this.RowName);',
        'this.State = _parent.GraphicsController.GetRowState(this.RowName);',
        '_parent.GraphicsController.HandleRowAction(this.RowName);',
        '_parent.GraphicsController.IncrementRow(this.RowName);',
        '_parent.GraphicsController.DecrementRow(this.RowName);',
        '"$UI.Cycle"',
        'this.LeftClicker._visible = false;',
        'this.RightClicker._visible = false;'
    )) {
        if ($GraphicsRowClipScript.IndexOf($ExpectedControllerToken, [System.StringComparison]::Ordinal) -lt 0) {
            throw "Graphics row clip script is missing expected controller token '$ExpectedControllerToken': $ResolvedGraphicsRowClipPath"
        }
    }

    foreach ($ForbiddenControllerToken in @(
        'this.RowOffset =',
        'OnVisibleRowClipLoaded',
        'HandleVisibleRowAction',
        'IncrementVisibleRow',
        'DecrementVisibleRow',
        'ScrollWindowUp',
        'ScrollWindowDown',
        'GetLogicalRowNameByOffset',
        '_parent.GraphicsController.GetRowDisplayLabel(rowName);',
        '_parent.GraphicsController.GetValueForRow(this.RowName);',
        'this.Init(rowName,"","");'
    )) {
        if ($GraphicsRowClipScript.IndexOf($ForbiddenControllerToken, [System.StringComparison]::Ordinal) -ge 0) {
            throw "Graphics row clip script still contains unsupported controller token '$ForbiddenControllerToken': $ResolvedGraphicsRowClipPath"
        }
    }
}

foreach ($ForbiddenGraphicsAudioToken in @(
    'this.Init("Subtitles"',
    'this.Init("VolumeSFX"',
    'this.Init("VolumeMusic"',
    'this.Init("VolumeDialogue"'
)) {
    if ($GraphicsRowScriptContents.IndexOf($ForbiddenGraphicsAudioToken, [System.StringComparison]::Ordinal) -ge 0) {
        throw "Sprite 600 still contains legacy audio row init token: $ForbiddenGraphicsAudioToken"
    }
}

foreach ($ExpectedRow in $ExpectedGraphicsRows) {
    $ExpectedRowToken = '"' + $ExpectedRow + '"'
    if ($CombinedScriptContents.IndexOf($ExpectedRowToken, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Missing expected graphics row token: $ExpectedRow"
    }
}

$GraphicsScreenScript = Get-Content -LiteralPath $GraphicsScreenScriptPath -Raw
$GraphicsScreenSettledScript = Get-Content -LiteralPath $GraphicsScreenSettledScriptPath -Raw
$ExpectedCurrentControllerTokens = @($RequiredFixedRowControllerTokens)
foreach ($ExpectedCurrentControllerToken in $ExpectedCurrentControllerTokens) {
    if ($GraphicsScreenScript.IndexOf($ExpectedCurrentControllerToken, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Graphics screen script is missing expected fixed-row token: $ExpectedCurrentControllerToken"
    }
}

foreach ($ForbiddenInteractiveControllerToken in $ForbiddenInteractiveControllerTokens) {
    if ($GraphicsScreenScript.IndexOf($ForbiddenInteractiveControllerToken, [System.StringComparison]::Ordinal) -ge 0) {
        throw "Graphics screen script still contains forbidden interactive token: $ForbiddenInteractiveControllerToken"
    }
}

if ($GraphicsScreenSettledScript.IndexOf('stop();', [System.StringComparison]::Ordinal) -lt 0) {
    throw 'Graphics screen settled-frame script is missing stop();'
}

foreach ($ExpectedFailFastToken in @(
    'throw "Missing required graphics row clip: " + rowName;',
    'throw "Graphics row clip missing BindGraphicsRow: " + rowName;',
    'throw "Graphics row clip missing Update: " + rowName;'
)) {
    if ($GraphicsScreenScript.IndexOf($ExpectedFailFastToken, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Graphics screen script is missing required fail-fast row clip token: $ExpectedFailFastToken"
    }
}

foreach ($ForbiddenFailSoftToken in @(
    'if(rowClip == undefined || rowClip.BindGraphicsRow == undefined)',
    'if(rowClip == undefined || rowClip.Update == undefined)'
)) {
    if ($GraphicsScreenScript.IndexOf($ForbiddenFailSoftToken, [System.StringComparison]::Ordinal) -ge 0) {
        throw "Graphics screen script still contains forbidden fail-soft guard token: $ForbiddenFailSoftToken"
    }
}

if ($GraphicsScreenScript.IndexOf('return new Array("Windowed","Fullscreen");', [System.StringComparison]::Ordinal) -lt 0) {
    throw 'Graphics screen script is missing the expected two-value Fullscreen display contract.'
}

foreach ($ForbiddenFullscreenDisplayToken in @(
    'return new Array(this.DraftState.fullscreen == 0 ? "Windowed" : "Fullscreen");'
)) {
    if ($GraphicsScreenScript.IndexOf($ForbiddenFullscreenDisplayToken, [System.StringComparison]::Ordinal) -ge 0) {
        throw "Graphics screen script still contains legacy fullscreen display token: $ForbiddenFullscreenDisplayToken"
    }
}

foreach ($ForbiddenScrollControllerToken in $ForbiddenScrollControllerTokens) {
    if ($GraphicsScreenScript.IndexOf($ForbiddenScrollControllerToken, [System.StringComparison]::Ordinal) -ge 0) {
        throw "Graphics screen script still contains forbidden scroll-window token: $ForbiddenScrollControllerToken"
    }
}

$GraphicsScreenLines = $GraphicsScreenScript -split "\r?\n"
$RowOrderLine = $GraphicsScreenLines | Where-Object {
    $_.IndexOf('this.RowOrder = new Array("', [System.StringComparison]::Ordinal) -ge 0
} | Select-Object -First 1

if ([string]::IsNullOrWhiteSpace($RowOrderLine)) {
    throw 'Could not locate populated RowOrder array assignment in graphics screen script.'
}

$RowOrderMatch = [System.Text.RegularExpressions.Regex]::Match(
    $RowOrderLine,
    'new Array\((?<rows>.*)\);',
    [System.Text.RegularExpressions.RegexOptions]::Singleline
)

if (-not $RowOrderMatch.Success) {
    throw 'Could not locate RowOrder array assignment in graphics screen script.'
}

$CapturedRows = @(
    $RowOrderMatch.Groups['rows'].Value.Split(',') | ForEach-Object {
        $_.Trim().Trim('"')
    } | Where-Object {
        -not [string]::IsNullOrWhiteSpace($_)
    }
)

if ($CapturedRows.Count -ne $ExpectedGraphicsRows.Count) {
    throw "Expected RowOrder length $($ExpectedGraphicsRows.Count) but found $($CapturedRows.Count)."
}

for ($Index = 0; $Index -lt $ExpectedGraphicsRows.Count; $Index++) {
    if ($CapturedRows[$Index] -ne $ExpectedGraphicsRows[$Index]) {
        throw "RowOrder mismatch at index $Index. Expected '$($ExpectedGraphicsRows[$Index])' but found '$($CapturedRows[$Index])'."
    }
}

foreach ($RowDepth in $ExpectedGraphicsRowTranslateYByDepth.Keys) {
    $ExpectedTranslateY = $ExpectedGraphicsRowTranslateYByDepth[$RowDepth]
    $RowPlacements = @(
        $ScreenOptionsGraphics.SelectNodes("subTags/item[@type='PlaceObject2Tag' and @depth='$RowDepth']")
    )

    if ($RowPlacements.Count -eq 0) {
        throw "Expected graphics row placements at depth $RowDepth."
    }

    foreach ($RowPlacement in $RowPlacements) {
        $MatrixNode = $RowPlacement.SelectSingleNode('matrix')
        if ($null -eq $MatrixNode) {
            continue
        }

        $ActualTranslateY = $MatrixNode.Attributes['translateY'].Value
        if ($ActualTranslateY -ne $ExpectedTranslateY) {
            throw "Graphics row depth $RowDepth contains translateY $ActualTranslateY, expected $ExpectedTranslateY."
        }
    }
}

foreach ($RowDepth in $ExpectedGraphicsRowInstanceNameByDepth.Keys) {
    $ExpectedInstanceName = $ExpectedGraphicsRowInstanceNameByDepth[$RowDepth]
    $RowPlacement = $ScreenOptionsGraphics.SelectSingleNode("subTags/item[@type='PlaceObject2Tag' and @depth='$RowDepth' and @characterId!='0']")
    if ($null -eq $RowPlacement) {
        throw "Expected graphics row placement with non-zero character id at depth $RowDepth."
    }

    $ActualInstanceName = $RowPlacement.Attributes['name'].Value
    if ($ActualInstanceName -ne $ExpectedInstanceName) {
        throw "Graphics row depth $RowDepth has instance name '$ActualInstanceName', expected '$ExpectedInstanceName'."
    }
}

$GraphicsInitialListRows = @(
    $ScreenOptionsGraphics.SelectNodes("subTags/item[@type='PlaceObject2Tag' and @characterId='290' and @placeFlagMove='false']")
)

if ($GraphicsInitialListRows.Count -ne $ExpectedGraphicsRowDepths.Count) {
    throw "Expected exactly $($ExpectedGraphicsRowDepths.Count) initial non-zero list rows in sprite 600, found $($GraphicsInitialListRows.Count)."
}

$ActualGraphicsRowDepths = @(
    $GraphicsInitialListRows | ForEach-Object {
        $_.Attributes['depth'].Value
    } | Sort-Object {
        [int]$_
    }
)

$ExpectedSortedGraphicsRowDepths = @(
    $ExpectedGraphicsRowDepths | Sort-Object {
        [int]$_
    }
)

for ($Index = 0; $Index -lt $ExpectedSortedGraphicsRowDepths.Count; $Index++) {
    if ($ActualGraphicsRowDepths[$Index] -ne $ExpectedSortedGraphicsRowDepths[$Index]) {
        throw "Graphics screen contains unexpected initial list-row depth '$($ActualGraphicsRowDepths[$Index])'; expected '$($ExpectedSortedGraphicsRowDepths[$Index])' at sorted index $Index."
    }
}

foreach ($ForbiddenLegacyGameRowName in $ForbiddenLegacyGameRowNames) {
    $LegacyGameRowPlacement = $ScreenOptionsGraphics.SelectSingleNode("subTags/item[@type='PlaceObject2Tag' and @name='$ForbiddenLegacyGameRowName' and @characterId!='0']")
    if ($null -ne $LegacyGameRowPlacement) {
        throw "Graphics screen still contains forbidden legacy Game-row placement '$ForbiddenLegacyGameRowName' at depth $($LegacyGameRowPlacement.Attributes['depth'].Value)."
    }
}

$GraphicsSpritePlaceObjectNodes = @(
    $ScreenOptionsGraphics.SelectNodes("subTags/item[@type='PlaceObject2Tag']")
)

foreach ($RemovedDepth in $RemovedGraphicsShellDepths) {
    $RemovedPlacement = $GraphicsSpritePlaceObjectNodes | Where-Object {
        $_.Attributes['depth'] -and $_.Attributes['depth'].Value -eq $RemovedDepth
    } | Select-Object -First 1

    if ($null -ne $RemovedPlacement) {
        throw "Graphics screen still contains removed inner-panel depth $RemovedDepth."
    }
}

$GraphicsPanelOwnerPlacement = $ScreenOptionsGraphics.SelectSingleNode(
    "subTags/item[@type='PlaceObject2Tag' and @depth='3' and @characterId='307']"
)

if ($null -eq $GraphicsPanelOwnerPlacement) {
    throw 'Expected retained graphics panel owner at depth 3 with character 307.'
}

$GraphicsPanelOwnerMatrix = $GraphicsPanelOwnerPlacement.SelectSingleNode('matrix')
if ($null -eq $GraphicsPanelOwnerMatrix) {
    throw 'Expected retained graphics panel owner to contain a matrix node.'
}

if ($GraphicsPanelOwnerMatrix.Attributes['hasScale'].Value -ne 'true') {
    throw "Expected retained graphics panel owner at depth 3 to keep hasScale='true'."
}

$GraphicsHeaderShellPlacement = $ScreenOptionsGraphics.SelectSingleNode(
    "subTags/item[@type='PlaceObject2Tag' and @depth='1' and @characterId='141']"
)

if ($null -eq $GraphicsHeaderShellPlacement) {
    throw 'Expected retained graphics shell placement at depth 1 with character 141.'
}

$GraphicsHeaderShellMatrix = $GraphicsHeaderShellPlacement.SelectSingleNode('matrix')
if ($GraphicsHeaderShellMatrix.Attributes['hasScale'].Value -ne 'false') {
    throw "Expected retained shell depth 1 character 141 to keep hasScale='false'."
}

foreach ($ExpectedGraphicsShellPlacement in $ExpectedGraphicsShellPlacements) {
    $ExpectedDepth = $ExpectedGraphicsShellPlacement.Depth
    $ExpectedCharacterId = $ExpectedGraphicsShellPlacement.CharacterId
    $ExpectedTranslateX = $ExpectedGraphicsShellPlacement.TranslateX
    $ExpectedTranslateY = $ExpectedGraphicsShellPlacement.TranslateY

    $ShellPlacement = $ScreenOptionsGraphics.SelectSingleNode("subTags/item[@type='PlaceObject2Tag' and @depth='$ExpectedDepth' and @characterId='$ExpectedCharacterId']")
    if ($null -eq $ShellPlacement) {
        throw "Expected graphics shell placement was not found at depth $ExpectedDepth for character $ExpectedCharacterId."
    }

    $MatrixNode = $ShellPlacement.SelectSingleNode('matrix')
    if ($null -eq $MatrixNode) {
        throw "Expected graphics shell placement at depth $ExpectedDepth for character $ExpectedCharacterId did not contain a matrix."
    }

    $ActualTranslateX = $MatrixNode.Attributes['translateX'].Value
    if ($ActualTranslateX -ne $ExpectedTranslateX) {
        throw "Graphics shell depth $ExpectedDepth character $ExpectedCharacterId has translateX $ActualTranslateX, expected $ExpectedTranslateX."
    }

    $ActualTranslateY = $MatrixNode.Attributes['translateY'].Value
    if ($ActualTranslateY -ne $ExpectedTranslateY) {
        throw "Graphics shell depth $ExpectedDepth character $ExpectedCharacterId has translateY $ActualTranslateY, expected $ExpectedTranslateY."
    }

    $ExpectedHasScale = $ExpectedGraphicsShellPlacement.HasScale
    $ActualHasScale = $MatrixNode.Attributes['hasScale'].Value
    if ($ActualHasScale -ne $ExpectedHasScale) {
        throw "Graphics shell depth $ExpectedDepth character $ExpectedCharacterId has hasScale $ActualHasScale, expected $ExpectedHasScale."
    }

    if ($null -ne $ExpectedGraphicsShellPlacement.ScaleX) {
        $ActualScaleX = $MatrixNode.Attributes['scaleX'].Value
        if ($ActualScaleX -ne $ExpectedGraphicsShellPlacement.ScaleX) {
            throw "Graphics shell depth $ExpectedDepth character $ExpectedCharacterId has scaleX $ActualScaleX, expected $($ExpectedGraphicsShellPlacement.ScaleX)."
        }
    }

    if ($null -ne $ExpectedGraphicsShellPlacement.ScaleY) {
        $ActualScaleY = $MatrixNode.Attributes['scaleY'].Value
        if ($ActualScaleY -ne $ExpectedGraphicsShellPlacement.ScaleY) {
            throw "Graphics shell depth $ExpectedDepth character $ExpectedCharacterId has scaleY $ActualScaleY, expected $($ExpectedGraphicsShellPlacement.ScaleY)."
        }
    }
}

$GraphicsTitlePlacement = $ScreenOptionsGraphics.SelectSingleNode("subTags/item[@type='PlaceObject2Tag' and @depth='147' and @characterId='340' and @characterId!='0']")
if ($null -eq $GraphicsTitlePlacement) {
    throw 'Graphics screen title placement at depth 147 for character 340 was not found.'
}

$GraphicsTitlePlacementName = $GraphicsTitlePlacement.Attributes['name']
if ($null -eq $GraphicsTitlePlacementName -or $GraphicsTitlePlacementName.Value -ne 'Title') {
    throw "Graphics screen title placement should be named 'Title', but found '$($GraphicsTitlePlacementName.Value)'."
}

$GraphicsTitleHasNameFlag = $GraphicsTitlePlacement.Attributes['placeFlagHasName']
if ($null -eq $GraphicsTitleHasNameFlag -or $GraphicsTitleHasNameFlag.Value -ne 'true') {
    throw "Graphics screen title placement should set placeFlagHasName='true', but found '$($GraphicsTitleHasNameFlag.Value)'."
}

foreach ($ForbiddenHelperToken in @(
    'PadOnly',
    'BrightnessHelp',
    'CameraAssistHelp'
)) {
    if ($GraphicsScreenScriptContents.IndexOf($ForbiddenHelperToken, [System.StringComparison]::Ordinal) -ge 0) {
        throw "Graphics screen exported scripts still contain forbidden helper token: $ForbiddenHelperToken"
    }
}

Write-Output 'PASS'
