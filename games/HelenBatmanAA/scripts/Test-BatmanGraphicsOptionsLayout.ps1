param(
    [string]$BatmanRoot,
    [string]$BuilderRoot,
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

function Invoke-ExternalProcess {
    param([string]$FilePath, [string[]]$Arguments)
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { $output = @(& $FilePath @Arguments 2>&1) } finally { $ErrorActionPreference = $previousErrorActionPreference }
    [pscustomobject]@{ ExitCode = $LASTEXITCODE; Output = $output }
}

function Assert-ContainsOrdinal {
    param([string]$Text, [string]$Token, [string]$Context)
    if ($Text.IndexOf($Token, [StringComparison]::Ordinal) -lt 0) { throw "$Context is missing '$Token'." }
}

function Assert-NotContainsOrdinal {
    param([string]$Text, [string]$Token, [string]$Context)
    if ($Text.IndexOf($Token, [StringComparison]::Ordinal) -ge 0) { throw "$Context contains forbidden '$Token'." }
}

function Get-ActionFunctionBody {
    param([string]$Text, [string]$Assignment)
    $match = [regex]::Match($Text, [regex]::Escape($Assignment) + '\s*=\s*function\s*\(\s*\)\s*\{(?<body>.*?)\}', [Text.RegularExpressions.RegexOptions]::Singleline)
    if (-not $match.Success) { throw "Missing expected row action $Assignment." }
    return $match.Groups['body'].Value.Trim()
}

function Assert-RowShellContract {
    param([string]$ScreenDirectory)
    $depths = @('141', '133', '125', '117', '109', '101', '93', '85', '77', '69', '61', '53', '45', '37', '29')
    $labels = @('Fullscreen', 'Resolution', 'VSync', 'MSAA', 'Detail Level', 'Bloom', 'Dynamic Shadows', 'Motion Blur', 'Distortion', 'Fog Volumes', 'Spherical Harmonic Lighting', 'Ambient Occlusion', 'PhysX', 'Stereo 3D', '')
    for ($index = 0; $index -lt $depths.Count; $index++) {
        $rowPath = Join-Path $ScreenDirectory "frame_1\PlaceObject2_290_List_Template_$($depths[$index])\CLIPACTIONRECORD onClipEvent(load).as"
        if (-not (Test-Path -LiteralPath $rowPath)) { throw "Missing known row script $rowPath." }
        $rowText = Get-Content -LiteralPath $rowPath -Raw
        if ($index -ge 2 -and $index -lt 14 -and $index -ne 4) {
            $activeNames = if ($index -eq 3) { 'this.Names = new Array("Off","2x","4x","8x","16x");' } elseif ($index -eq 12) { 'this.Names = new Array("Off","Normal","High");' } else { 'this.Names = new Array("Off","On");' }
            Assert-ContainsOrdinal $rowText $activeNames "row $($index + 1) names"
            Assert-ContainsOrdinal $rowText "this.RowIndex = $($index + 1);" "row $($index + 1) row index"
            Assert-ContainsOrdinal $rowText 'this.State = _parent.GraphicsOptionsController.GetDraftIndex(this.RowIndex);' "row $($index + 1) initial state"
            Assert-ContainsOrdinal $rowText '_parent.GraphicsOptionsController.ToggleSetting(this.RowIndex);' "row $($index + 1) RunAction"
            Assert-ContainsOrdinal $rowText '_parent.GraphicsOptionsController.IncrementSetting(this.RowIndex);' "row $($index + 1) Increment"
            Assert-ContainsOrdinal $rowText '_parent.GraphicsOptionsController.DecrementSetting(this.RowIndex);' "row $($index + 1) Decrement"
            Assert-ContainsOrdinal $rowText $labels[$index] "row $($index + 1) label"
            Assert-ContainsOrdinal $rowText 'this._visible = true;' "row $($index + 1) visibility"
        } elseif ($index -eq 4) {
            Assert-ContainsOrdinal $rowText 'this.Names = new Array("Low","Medium","High","Very High","Custom");' 'row 5 names'
            Assert-ContainsOrdinal $rowText 'this.State = -1;' 'row 5 state'
            Assert-ContainsOrdinal $rowText 'this.Initial = -1;' 'row 5 initial state'
            Assert-ContainsOrdinal $rowText 'this.Default = -1;' 'row 5 default state'
            Assert-ContainsOrdinal $rowText 'GetDetailLevelDraftIndex();' 'row 5 draft state'
            Assert-ContainsOrdinal $rowText 'GetDetailLevelInitialIndex();' 'row 5 initial state'
            Assert-ContainsOrdinal $rowText 'ToggleDetailPreset();' 'row 5 RunAction'
            Assert-ContainsOrdinal $rowText 'IncrementDetailPreset();' 'row 5 Increment'
            Assert-ContainsOrdinal $rowText 'DecrementDetailPreset();' 'row 5 Decrement'
            Assert-ContainsOrdinal $rowText 'this._visible = true;' 'row 5 visibility'
            if ($rowText.IndexOf('ToggleSetting', [StringComparison]::Ordinal) -ge 0 -or $rowText.IndexOf('IncrementSetting', [StringComparison]::Ordinal) -ge 0 -or $rowText.IndexOf('DecrementSetting', [StringComparison]::Ordinal) -ge 0) { throw 'row 5 must delegate only to detail preset methods.' }
        } elseif ($index -lt 2) {
            Assert-ContainsOrdinal $rowText 'this.Names = new Array("Not active");' "row $($index + 1)"
            if ($rowText -notmatch 'this\.(?:Label\.)?Label\.Text\.text\s*=\s*"' -and $rowText -notmatch 'this\.Label\.Text\.text\s*=\s*"') { throw "row $($index + 1) is missing its fixed label assignment." }
            Assert-ContainsOrdinal $rowText $labels[$index] "row $($index + 1) label"
            Assert-ContainsOrdinal $rowText 'this._visible = true;' "row $($index + 1) visibility"
            foreach ($action in @('RunAction', 'Increment', 'Decrement', 'ShowPrompt')) {
                if ((Get-ActionFunctionBody $rowText "this.$action") -ne '') { throw "row $($index + 1) action $action must be a no-op." }
            }
        } else {
            Assert-ContainsOrdinal $rowText 'this.Names = new Array("");' 'row 15 names'
            Assert-ContainsOrdinal $rowText 'Apply Changes' 'row 15 label'
            Assert-ContainsOrdinal $rowText 'this.ItemText.text = "";' 'row 15 value'
            Assert-ContainsOrdinal $rowText 'this._visible = true;' 'row 15 visibility'
            Assert-ContainsOrdinal $rowText '_parent.GraphicsOptionsController.ApplyChanges();' 'row 15 RunAction'
            foreach ($action in @('Increment', 'Decrement', 'ShowPrompt')) {
                if ((Get-ActionFunctionBody $rowText "this.$action") -ne '') { throw "row 15 action $action must be a no-op." }
            }
        }
    }
}

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) { $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path } else { $BatmanRoot = (Resolve-Path $BatmanRoot).Path }
if ([string]::IsNullOrWhiteSpace($BuilderRoot)) { $BuilderRoot = Join-Path $BatmanRoot 'builder' } elseif ([IO.Path]::IsPathRooted($BuilderRoot)) { $BuilderRoot = [IO.Path]::GetFullPath($BuilderRoot) } elseif (Test-Path -LiteralPath $BuilderRoot) { $BuilderRoot = (Resolve-Path $BuilderRoot).Path } else { $BuilderRoot = [IO.Path]::GetFullPath((Join-Path $BatmanRoot $BuilderRoot)) }

$targetPath = Join-Path $BuilderRoot 'generated\graphics-options-experiment\Frontend-graphics-options.umap'
$patcherProject = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\BmGameGfxPatcher\BmGameGfxPatcher.csproj'
$ffdec = Join-Path $BuilderRoot 'extracted\ffdec\ffdec-cli.exe'
foreach ($path in @($targetPath, $patcherProject, $ffdec)) { if (-not (Test-Path -LiteralPath $path)) { throw "Layout test input not found: $path" } }

$verificationRoot = Join-Path ([IO.Path]::GetTempPath()) ('HelenBatmanGraphicsLayout-' + [Guid]::NewGuid().ToString('N'))
$extractedGfx = Join-Path $verificationRoot 'MainV2.gfx'
$xmlPath = Join-Path $verificationRoot 'MainV2.xml'
$exportRoot = Join-Path $verificationRoot 'export'
New-Item -ItemType Directory -Force -Path $verificationRoot | Out-Null
try {
    $extract = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('run', '--project', $patcherProject, '-c', $Configuration, '--', 'extract-gfx', '--package', $targetPath, '--owner', 'MainMenu', '--name', 'MainV2', '--output', $extractedGfx)
    if ($extract.ExitCode -ne 0) { throw "Failed to extract graphics shell: $($extract.Output -join [Environment]::NewLine)" }
    $xml = Invoke-ExternalProcess -FilePath $ffdec -Arguments @('-swf2xml', $extractedGfx, $xmlPath)
    if ($xml.ExitCode -ne 0) { throw 'FFDec could not reopen the graphics shell.' }
    $export = Invoke-ExternalProcess -FilePath $ffdec -Arguments @('-export', 'script', $exportRoot, $extractedGfx)
    if ($export.ExitCode -ne 0) { throw 'FFDec could not export the graphics shell scripts.' }

    [xml]$document = Get-Content -LiteralPath $xmlPath -Raw
    $screen = $document.SelectSingleNode("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='600']")
    if ($null -eq $screen) { throw 'ScreenOptionsGraphics sprite 600 was not found.' }
    if (@($document.SelectNodes("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='600']")).Count -ne 1) { throw 'ScreenOptionsGraphics sprite 600 must occur exactly once.' }
    if (@($document.SelectNodes("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='601']")).Count -ne 0) { throw 'Graphics shell must not contain prompt sprite 601.' }
    $exports = @(@($document.SelectNodes("/swf/tags/item[@type='ExportAssetsTag']")) | Where-Object { @($_.names.item | Where-Object { $_ -eq 'ScreenOptionsGraphics' }).Count -gt 0 })
    if ($exports.Count -ne 1 -or @($exports[0].tags.item | Where-Object { $_ -eq '600' }).Count -ne 1) { throw 'Graphics shell export must map ScreenOptionsGraphics to sprite 600 exactly.' }

    $rowNames = @('GraphicsRow1', 'GraphicsRow2', 'GraphicsRow3', 'GraphicsRow4', 'GraphicsRow5', 'GraphicsRow6', 'GraphicsRow7', 'GraphicsRow8', 'GraphicsRow9', 'GraphicsRow10', 'GraphicsRow11', 'GraphicsRow12', 'GraphicsRow13', 'GraphicsRow14', 'GraphicsRow15')
    $rowDepths = @('141', '133', '125', '117', '109', '101', '93', '85', '77', '69', '61', '53', '45', '37', '29')
    $rowY = @('-4452', '-3912', '-3372', '-2832', '-2292', '-1752', '-1212', '-672', '-132', '408', '948', '1488', '2028', '2568', '3108')
    $placements = @($screen.SelectNodes("subTags/item[@type='PlaceObject2Tag' and @characterId='290' and @placeFlagMove='false']"))
    if ($placements.Count -ne 15) { throw "Expected exactly 15 fixed row placements, found $($placements.Count)." }
    for ($index = 0; $index -lt 15; $index++) {
        $depth = $rowDepths[$index]
        $placement = $screen.SelectSingleNode("subTags/item[@type='PlaceObject2Tag' and @depth='$depth' and @characterId='290' and @placeFlagMove='false']")
        if ($null -eq $placement) { throw "Missing fixed row placement at depth $depth." }
        if ($placement.name -ne $rowNames[$index]) { throw "Depth $depth has row '$($placement.name)', expected '$($rowNames[$index])'." }
        $matrix = $placement.SelectSingleNode('matrix')
        if ($null -eq $matrix -or $matrix.translateX -ne '-1805' -or $matrix.translateY -ne $rowY[$index]) { throw "Row $($rowNames[$index]) has unexpected geometry." }
    }

    $expectedShell = @(
        @{ Depth = '1'; CharacterId = '141'; X = '-7126'; Y = '899' },
        @{ Depth = '3'; CharacterId = '307'; X = '-10951'; Y = '-5500' },
        @{ Depth = '26'; CharacterId = '332'; X = '-1641'; Y = '4609' },
        @{ Depth = '146'; CharacterId = '118'; X = '-7482'; Y = '-5020' },
        @{ Depth = '147'; CharacterId = '340'; X = '-9192'; Y = '-5676' }
    )
    foreach ($expected in $expectedShell) {
        $placement = $screen.SelectSingleNode("subTags/item[@type='PlaceObject2Tag' and @depth='$($expected.Depth)' and @characterId='$($expected.CharacterId)' and @characterId!='0']")
        if ($null -eq $placement) { throw "Missing retained shell placement depth $($expected.Depth), character $($expected.CharacterId)." }
        $matrix = $placement.SelectSingleNode('matrix')
        if ($null -eq $matrix -or $matrix.translateX -ne $expected.X -or $matrix.translateY -ne $expected.Y) { throw "Shell placement depth $($expected.Depth) has unexpected geometry." }
    }

    $scriptsRoot = Join-Path $exportRoot 'scripts'
    $scripts = @(Get-ChildItem -LiteralPath $scriptsRoot -Recurse -Filter *.as -File)
    $screenDirectories = @(Get-ChildItem -LiteralPath $scriptsRoot -Recurse -Directory | Where-Object { $_.Name -eq 'DefineSprite_600_ScreenOptionsGraphics' })
    if ($screenDirectories.Count -ne 1) { throw "Expected exactly one DefineSprite_600_ScreenOptionsGraphics directory, found $($screenDirectories.Count)." }
    $screenDirectory = $screenDirectories[0]
    $screenScriptPath = Join-Path $screenDirectory.FullName 'frame_1\DoAction.as'
    if (-not (Test-Path -LiteralPath $screenScriptPath)) { throw 'Known Options Graphics screen script was not exported.' }
    $screenText = Get-Content -LiteralPath $screenScriptPath -Raw
    $menuScriptPath = Join-Path $scriptsRoot 'DefineSprite_333_ScreenOptionsMenu\frame_1\PlaceObject2_117_GenericButton_37\CLIPACTIONRECORD onClipEvent(load).as'
    if (-not (Test-Path -LiteralPath $menuScriptPath)) { throw 'Known Options menu script was not exported.' }
    $menuText = Get-Content -LiteralPath $menuScriptPath -Raw
    Assert-ContainsOrdinal $menuText 'GotoScreen("OptionsGraphics")' 'Options menu script'
    Assert-ContainsOrdinal $menuText 'Graphics Options' 'Options menu script'
    foreach ($required in @('Graphics Options', 'CancelScreen', 'ReturnFromScreen', 'FE_SetActiveScreenName","Graphics Options', 'GraphicsRow15._visible = true;', 'this.AddItem(GraphicsRow14,12,14,-1,-1);')) { Assert-ContainsOrdinal $screenText $required 'Options Graphics screen script' }
    Assert-RowShellContract -ScreenDirectory $screenDirectory.FullName
    foreach ($forbidden in @('GraphicsController', 'ExitPrompt', 'Helen_', 'Unsaved graphics changes', 'Some changes require a restart')) { Assert-NotContainsOrdinal -Text $screenText -Token $forbidden -Context 'graphics shell screen script' }
    foreach ($script in $scripts) { foreach ($forbidden in @('Helen_', 'GraphicsExitPrompt', 'loadBatmanGraphicsDraftIntoConfig', 'applyBatmanGraphicsDraft')) { Assert-NotContainsOrdinal -Text (Get-Content -LiteralPath $script.FullName -Raw) -Token $forbidden -Context $script.Name } }
}
finally {
    if (Test-Path -LiteralPath $verificationRoot) { Remove-Item -LiteralPath $verificationRoot -Recurse -Force }
}

Write-Output 'PASS'
