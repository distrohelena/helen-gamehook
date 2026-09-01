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

    $scripts = @(Get-ChildItem -LiteralPath (Join-Path $exportRoot 'scripts') -Recurse -Filter *.as -File)
    $screenScripts = @($scripts | Where-Object { $_.FullName -like '*DefineSprite_600_ScreenOptionsGraphics*' })
    $screenText = (($screenScripts | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join [Environment]::NewLine)
    $allScriptText = (($scripts | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join [Environment]::NewLine)
    foreach ($required in @('Graphics Options', 'GotoScreen("OptionsGraphics")', 'CancelScreen', 'ReturnFromScreen', 'Not active', 'GraphicsRow15._visible = false;', 'this.AddItem(GraphicsRow14,12,0,-1,-1);')) { Assert-ContainsOrdinal -Text $allScriptText -Token $required -Context 'graphics shell screen script' }
    foreach ($forbidden in @('GraphicsController', 'ExitPrompt', 'Helen_', 'ApplyChanges', 'Unsaved graphics changes', 'Some changes require a restart')) { Assert-NotContainsOrdinal -Text $screenText -Token $forbidden -Context 'graphics shell screen script' }
    $rowsWithNotActive = 0
    foreach ($script in $screenScripts) {
        $text = Get-Content -LiteralPath $script.FullName -Raw
        if ($text.IndexOf('this.Names = new Array("Not active");', [StringComparison]::Ordinal) -ge 0) { $rowsWithNotActive++ }
        Assert-NotContainsOrdinal -Text $text -Token 'GraphicsController' -Context $script.Name
        Assert-NotContainsOrdinal -Text $text -Token 'ExitPrompt' -Context $script.Name
    }
    if ($rowsWithNotActive -ne 14) { throw "Expected 14 fixed Not active rows, found $rowsWithNotActive." }
    foreach ($script in $scripts) { foreach ($forbidden in @('Helen_', 'ApplyChanges', 'GraphicsExitPrompt', 'loadBatmanGraphicsDraftIntoConfig', 'applyBatmanGraphicsDraft')) { Assert-NotContainsOrdinal -Text (Get-Content -LiteralPath $script.FullName -Raw) -Token $forbidden -Context $script.Name } }
}
finally {
    if (Test-Path -LiteralPath $verificationRoot) { Remove-Item -LiteralPath $verificationRoot -Recurse -Force }
}

Write-Output 'PASS'
