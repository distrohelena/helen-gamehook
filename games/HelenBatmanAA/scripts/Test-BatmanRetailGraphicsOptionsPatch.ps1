param(
    [string]$RetailFrontendPackagePath,
    [string]$BatmanRoot,
    [string]$Configuration = 'Debug',
    [string]$BuilderRoot,
    [string]$BatmanUserIniPath
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'BatmanBuilderWorkspaceHelpers.ps1')

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

function Get-VsyncSliceRowActionBody {
    param([string]$Text, [string]$Assignment)
    return Get-ActionFunctionBody -Text $Text -Assignment $Assignment
}

function Assert-NoOpVsyncSliceRowActions {
    param([string]$Text, [string]$Context)
    foreach ($action in @('RunAction', 'Increment', 'Decrement', 'ShowPrompt')) {
        if ((Get-VsyncSliceRowActionBody -Text $Text -Assignment "this.$action") -ne '') { throw "$Context action $action must be a no-op." }
    }
}

function Assert-VsyncSliceRowContract {
    param([string]$Text, [int]$Index, [string]$Context)

    $labels = @('Fullscreen', 'Resolution', 'VSync', 'MSAA', 'Detail Level', 'Bloom', 'Dynamic Shadows', 'Motion Blur', 'Distortion', 'Fog Volumes', 'Spherical Harmonic Lighting', 'Ambient Occlusion', 'PhysX', 'Stereo 3D', 'Apply Changes')
    if ($Index -eq 2) {
        Assert-ContainsOrdinal $Text 'this.Names = new Array("Off","On");' "$Context names"
        Assert-ContainsOrdinal $Text 'this.State = _parent.GraphicsVsyncController.DraftVsync;' "$Context initial state"
        Assert-ContainsOrdinal $Text '_parent.GraphicsVsyncController.ToggleVsync();' "$Context RunAction"
        Assert-ContainsOrdinal $Text '_parent.GraphicsVsyncController.IncrementVsync();' "$Context Increment"
        Assert-ContainsOrdinal $Text '_parent.GraphicsVsyncController.DecrementVsync();' "$Context Decrement"
        Assert-ContainsOrdinal $Text $labels[$Index] "$Context label"
        Assert-ContainsOrdinal $Text 'this._visible = true;' "$Context visibility"
        return
    }

    if ($Index -eq 14) {
        Assert-ContainsOrdinal $Text 'this.Names = new Array("");' "$Context names"
        Assert-ContainsOrdinal $Text 'Apply Changes' "$Context label"
        Assert-ContainsOrdinal $Text 'this.ItemText.text = "";' "$Context value"
        Assert-ContainsOrdinal $Text 'this._visible = true;' "$Context visibility"
        Assert-ContainsOrdinal $Text '_parent.GraphicsVsyncController.ApplyChanges();' "$Context RunAction"
        foreach ($action in @('Increment', 'Decrement')) {
            if ((Get-VsyncSliceRowActionBody -Text $Text -Assignment "this.$action") -ne '') { throw "$Context action $action must be a no-op." }
        }
        return
    }

    Assert-ContainsOrdinal $Text 'this.Names = new Array("Not active");' "$Context names"
    Assert-ContainsOrdinal $Text 'if(this.ItemText != undefined)' "$Context ItemText guard"
    Assert-ContainsOrdinal $Text 'this.ItemText.text = "Not active";' "$Context visible value"
    Assert-ContainsOrdinal $Text $labels[$Index] "$Context label"
    Assert-ContainsOrdinal $Text 'this._visible = true;' "$Context visibility"
    Assert-NoOpVsyncSliceRowActions -Text $Text -Context $Context
}

function Assert-RowShellContract {
    param([string]$ScreenDirectory)
    $depths = @('141', '133', '125', '117', '109', '101', '93', '85', '77', '69', '61', '53', '45', '37', '29')
    for ($index = 0; $index -lt $depths.Count; $index++) {
        $rowPath = Join-Path $ScreenDirectory "frame_1\PlaceObject2_290_List_Template_$($depths[$index])\CLIPACTIONRECORD onClipEvent(load).as"
        if (-not (Test-Path -LiteralPath $rowPath)) { throw "Missing known row script $rowPath." }
        $rowText = Get-Content -LiteralPath $rowPath -Raw
        if ($index -lt 15) {
            if ($rowText -notmatch 'this\.(?:Label\.)?Label\.Text\.text\s*=\s*"' -and $rowText -notmatch 'this\.Label\.Text\.text\s*=\s*"') { throw "row $($index + 1) is missing its fixed label assignment." }
            Assert-VsyncSliceRowContract -Text $rowText -Index $index -Context "row $($index + 1)"
        } else {
            throw 'Unexpected graphics row index.'
        }
    }
}

function Invoke-ExternalProcess {
    param([string]$FilePath, [string[]]$Arguments)
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { $output = @(& $FilePath @Arguments 2>&1) } finally { $ErrorActionPreference = $previousErrorActionPreference }
    [pscustomobject]@{ ExitCode = $LASTEXITCODE; Output = $output }
}

function New-VsyncIniFixture {
    param([string]$SourcePath, [string]$DestinationPath, [bool]$Enabled)
    $iniText = Get-Content -LiteralPath $SourcePath -Raw
    $useVsyncPattern = '(?m)^UseVsync=(?:True|False|0|1)\r?$'
    $matches = [regex]::Matches($iniText, $useVsyncPattern)
    if ($matches.Count -ne 1) { throw "Expected exactly one exact UseVsync assignment in valid INI fixture source, found $($matches.Count)." }
    $replacement = if ($Enabled) { 'UseVsync=True' } else { 'UseVsync=False' }
    $fixtureText = [regex]::Replace($iniText, $useVsyncPattern, $replacement, 1)
    [IO.File]::WriteAllText($DestinationPath, $fixtureText, [Text.UTF8Encoding]::new($false))
}

function New-MalformedIniFixture {
    param([string]$SourcePath, [string]$DestinationPath)
    $iniText = Get-Content -LiteralPath $SourcePath -Raw
    $useVsyncPattern = '(?m)^UseVsync=(?:True|False|0|1)\r?$'
    $matches = [regex]::Matches($iniText, $useVsyncPattern)
    if ($matches.Count -ne 1) { throw "Expected exactly one exact UseVsync assignment in valid INI fixture source, found $($matches.Count)." }
    $fixtureText = [regex]::Replace($iniText, $useVsyncPattern, 'UseVsync=Malformed', 1)
    [IO.File]::WriteAllText($DestinationPath, $fixtureText, [Text.UTF8Encoding]::new($false))
}

function Invoke-ShellBuild {
    param([string]$BuilderProject, [string]$Configuration, [string]$BuilderRoot, [string]$OutputDirectory, [string]$FfdecPath, [string]$IniPath)
    $result = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('run', '--project', $BuilderProject, '-c', $Configuration, '--', 'build-main-menu-graphics-shell', '--root', $BuilderRoot, '--output-dir', $OutputDirectory, '--ffdec', $FfdecPath, '--ini', $IniPath)
    if ($result.ExitCode -ne 0) { throw "Shell build failed for INI '$IniPath': $($result.Output -join [Environment]::NewLine)" }
}

function Assert-ShellBootstrapExport {
    param([string]$GfxPath, [string]$ExportRoot, [string]$FfdecPath, [int]$ExpectedVsync, [string]$Context)
    $export = Invoke-ExternalProcess -FilePath $FfdecPath -Arguments @('-export', 'script', $ExportRoot, $GfxPath)
    if ($export.ExitCode -ne 0) { throw "FFDec failed to export $Context shell scripts: $($export.Output -join [Environment]::NewLine)" }
    $screenDirectory = @(Get-ChildItem -LiteralPath (Join-Path $ExportRoot 'scripts') -Recurse -Directory | Where-Object { $_.Name -eq 'DefineSprite_600_ScreenOptionsGraphics' })
    if ($screenDirectory.Count -ne 1) { throw "$Context export did not contain exactly one graphics screen directory." }
    $screenText = Get-Content -LiteralPath (Join-Path $screenDirectory[0].FullName 'frame_1\DoAction.as') -Raw
    Assert-ContainsOrdinal -Text $screenText -Token "this.GraphicsVsyncController = new rs.ui.BatmanGraphicsVsyncController(this,$ExpectedVsync);" -Context "$Context controller bootstrap"
    $rowPath = Join-Path $screenDirectory[0].FullName 'frame_1\PlaceObject2_290_List_Template_125\CLIPACTIONRECORD onClipEvent(load).as'
    $rowText = Get-Content -LiteralPath $rowPath -Raw
    Assert-ContainsOrdinal -Text $rowText -Token "this.State = $ExpectedVsync;" -Context "$Context VSync row state"
    Assert-ContainsOrdinal -Text $rowText -Token "this.Initial = $ExpectedVsync;" -Context "$Context VSync row initial state"
}

function Assert-BuilderRejectsIni {
    param([string]$BuilderProject, [string]$Configuration, [string]$BuilderRoot, [string]$OutputDirectory, [string]$FfdecPath, [string]$IniPath, [string]$ExpectedDiagnostic, [string]$Context)
    $result = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('run', '--project', $BuilderProject, '-c', $Configuration, '--', 'build-main-menu-graphics-shell', '--root', $BuilderRoot, '--output-dir', $OutputDirectory, '--ffdec', $FfdecPath, '--ini', $IniPath)
    if ($result.ExitCode -eq 0) { throw "$Context unexpectedly accepted INI '$IniPath'." }
    Assert-ContainsOrdinal -Text ($result.Output -join [Environment]::NewLine) -Token $ExpectedDiagnostic -Context "$Context diagnostic"
    $outputPath = Join-Path $OutputDirectory 'MainV2-graphics-options.gfx'
    if (Test-Path -LiteralPath $outputPath) { throw "$Context accepted output before rejecting INI '$IniPath': $outputPath" }
}

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) { $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path } else { $BatmanRoot = (Resolve-Path $BatmanRoot).Path }
if ([string]::IsNullOrWhiteSpace($BuilderRoot)) { $BuilderRoot = Join-Path $BatmanRoot 'builder' } elseif ([IO.Path]::IsPathRooted($BuilderRoot)) { $BuilderRoot = [IO.Path]::GetFullPath($BuilderRoot) } elseif (Test-Path -LiteralPath $BuilderRoot) { $BuilderRoot = (Resolve-Path $BuilderRoot).Path } else { $BuilderRoot = [IO.Path]::GetFullPath((Join-Path $BatmanRoot $BuilderRoot)) }
if ([string]::IsNullOrWhiteSpace($RetailFrontendPackagePath)) { $RetailFrontendPackagePath = Join-Path $BuilderRoot 'extracted\frontend-retail\Frontend.umap' } elseif (-not [IO.Path]::IsPathRooted($RetailFrontendPackagePath)) { $RetailFrontendPackagePath = [IO.Path]::GetFullPath((Join-Path (Get-Location) $RetailFrontendPackagePath)) }
$RetailFrontendPackagePath = [IO.Path]::GetFullPath($RetailFrontendPackagePath)
if ([string]::IsNullOrWhiteSpace($BatmanUserIniPath)) {
    $documentsPath = [Environment]::GetFolderPath([Environment+SpecialFolder]::MyDocuments)
    $BatmanUserIniPath = Join-Path $documentsPath 'Square Enix\Batman Arkham Asylum GOTY\BmGame\Config\BmEngine.ini'
} else {
    $BatmanUserIniPath = [IO.Path]::GetFullPath($BatmanUserIniPath)
}
if (-not (Test-Path -LiteralPath $BatmanUserIniPath -PathType Leaf)) { throw "Batman user INI was not found as a file: $BatmanUserIniPath" }

$match = & (Join-Path $PSScriptRoot 'Get-BatmanSteamBuildMatch.ps1')
if ($match.BuildId -cne 'steam-goty-1.0' -or $match.Executable -cne 'ShippingPC-BmGame.exe' -or [int64]$match.FileSize -ne 38758728 -or $match.Sha256 -cne '4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028') { throw 'Steam build match is not the verified 4DAC retail executable identity.' }
$baseInfo = Get-Item -LiteralPath $RetailFrontendPackagePath
$baseHash = (Get-FileHash -LiteralPath $RetailFrontendPackagePath -Algorithm SHA256).Hash
if ($baseInfo.Length -ne 2988548 -or $baseHash -cne '271916B888F83374122AF0FCCC5C685804F4C8286A92A772CD71E4F48A00F2CC') { throw 'Retail frontend input does not match the verified 2988548-byte base.' }

$builderProject = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj'
$patcherProject = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\BmGameGfxPatcher\BmGameGfxPatcher.csproj'
$ffdec = Join-Path $BuilderRoot 'extracted\ffdec\ffdec-cli.exe'
foreach ($path in @($builderProject, $patcherProject, $ffdec, $RetailFrontendPackagePath)) { if (-not (Test-Path -LiteralPath $path)) { throw "Required retail patch input not found: $path" } }

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('HelenBatmanGraphicsRetailPatch-' + [Guid]::NewGuid().ToString('N'))
$bootstrapFalseIniPath = Join-Path $tempRoot 'bootstrap-false.ini'
$bootstrapTrueIniPath = Join-Path $tempRoot 'bootstrap-true.ini'
$malformedIniPath = Join-Path $tempRoot 'bootstrap-malformed.ini'
$missingIniPath = Join-Path $tempRoot 'bootstrap-missing.ini'
$bootstrapFalseRoot = Join-Path $tempRoot 'prototype-false'
$bootstrapTrueRoot = Join-Path $tempRoot 'prototype-true'
$bootstrapFalseExportRoot = Join-Path $tempRoot 'export-false'
$bootstrapTrueExportRoot = Join-Path $tempRoot 'export-true'
$invalidMissingOutputRoot = Join-Path $tempRoot 'invalid-missing-output'
$invalidMalformedOutputRoot = Join-Path $tempRoot 'invalid-malformed-output'
$prototypeRoot = $bootstrapTrueRoot
$prototypeGfx = Join-Path $prototypeRoot 'MainV2-graphics-options.gfx'
$manifestPath = Join-Path $tempRoot 'patch.manifest.json'
$patchedPackage = Join-Path $tempRoot 'Frontend-graphics-options.umap'
$extractedGfx = Join-Path $tempRoot 'MainV2.gfx'
$xmlPath = Join-Path $tempRoot 'MainV2.xml'
$exportRoot = Join-Path $tempRoot 'export'
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
try {
    New-VsyncIniFixture -SourcePath $BatmanUserIniPath -DestinationPath $bootstrapFalseIniPath -Enabled $false
    New-VsyncIniFixture -SourcePath $BatmanUserIniPath -DestinationPath $bootstrapTrueIniPath -Enabled $true
    New-MalformedIniFixture -SourcePath $BatmanUserIniPath -DestinationPath $malformedIniPath
    Assert-BuilderRejectsIni -BuilderProject $builderProject -Configuration $Configuration -BuilderRoot $BuilderRoot -OutputDirectory $invalidMissingOutputRoot -FfdecPath $ffdec -IniPath $missingIniPath -ExpectedDiagnostic "Required path not found: $missingIniPath" -Context 'Missing INI validation'
    Assert-BuilderRejectsIni -BuilderProject $builderProject -Configuration $Configuration -BuilderRoot $BuilderRoot -OutputDirectory $invalidMalformedOutputRoot -FfdecPath $ffdec -IniPath $malformedIniPath -ExpectedDiagnostic "INI value 'SystemSettings.UseVsync' must be a boolean-like value but was 'Malformed'." -Context 'Malformed INI validation'
    Invoke-ShellBuild -BuilderProject $builderProject -Configuration $Configuration -BuilderRoot $BuilderRoot -OutputDirectory $bootstrapFalseRoot -FfdecPath $ffdec -IniPath $bootstrapFalseIniPath
    Invoke-ShellBuild -BuilderProject $builderProject -Configuration $Configuration -BuilderRoot $BuilderRoot -OutputDirectory $bootstrapTrueRoot -FfdecPath $ffdec -IniPath $bootstrapTrueIniPath
    Assert-ShellBootstrapExport -GfxPath (Join-Path $bootstrapFalseRoot 'MainV2-graphics-options.gfx') -ExportRoot $bootstrapFalseExportRoot -FfdecPath $ffdec -ExpectedVsync 0 -Context 'UseVsync=False bootstrap'
    Assert-ShellBootstrapExport -GfxPath (Join-Path $bootstrapTrueRoot 'MainV2-graphics-options.gfx') -ExportRoot $bootstrapTrueExportRoot -FfdecPath $ffdec -ExpectedVsync 1 -Context 'UseVsync=True bootstrap'
    if (-not (Test-Path -LiteralPath $prototypeGfx)) { throw "Shell prototype was not generated: $prototypeGfx" }

    $manifest = [ordered]@{ name = 'MainV2 graphics-options shell retail patch'; patches = @([ordered]@{ owner = 'MainMenu'; exportName = 'MainV2'; exportType = 'GFxMovieInfo'; replacementPath = $prototypeGfx; payloadMagic = 'GFX' }) }
    [IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
    $patch = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('run', '--project', $patcherProject, '-c', $Configuration, '--', 'patch', '--package', $RetailFrontendPackagePath, '--manifest', $manifestPath, '--output', $patchedPackage)
    if ($patch.ExitCode -ne 0) { throw "Retail patch failed: $($patch.Output -join [Environment]::NewLine)" }

    . (Join-Path $PSScriptRoot 'BatmanPackVerificationHelpers.ps1')
    $patchedStorage = Get-UnrealPackageStorageInfo -Path $patchedPackage
    if ($patchedStorage.CompressionChunkCount -le 0) { throw 'Retail patch output must remain chunk-compressed.' }
    $extract = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('run', '--project', $patcherProject, '-c', $Configuration, '--', 'extract-gfx', '--package', $patchedPackage, '--owner', 'MainMenu', '--name', 'MainV2', '--output', $extractedGfx)
    if ($extract.ExitCode -ne 0) { throw 'Failed to extract patched retail MainV2.' }
    $xmlResult = Invoke-ExternalProcess -FilePath $ffdec -Arguments @('-swf2xml', $extractedGfx, $xmlPath)
    if ($xmlResult.ExitCode -ne 0) { throw 'FFDec failed to reopen patched retail MainV2.' }
    $export = Invoke-ExternalProcess -FilePath $ffdec -Arguments @('-export', 'script', $exportRoot, $extractedGfx)
    if ($export.ExitCode -ne 0) { throw 'FFDec failed to export patched retail scripts.' }

    [xml]$document = Get-Content -LiteralPath $xmlPath -Raw
    $sprites = @($document.SelectNodes("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='600']"))
    if ($sprites.Count -ne 1) { throw "Expected exactly one shell sprite 600, found $($sprites.Count)." }
    if (@($document.SelectNodes("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='601']")).Count -ne 0) { throw 'Patched retail shell must not contain sprite 601.' }
    $exports = @(@($document.SelectNodes("/swf/tags/item[@type='ExportAssetsTag']")) | Where-Object { @($_.names.item | Where-Object { $_ -eq 'ScreenOptionsGraphics' }).Count -gt 0 })
    if ($exports.Count -ne 1 -or @($exports[0].tags.item | Where-Object { $_ -eq '600' }).Count -ne 1) { throw 'Patched retail shell must export exactly sprite 600 as ScreenOptionsGraphics.' }

    $scriptsRoot = Join-Path $exportRoot 'scripts'
    $scriptFiles = @(Get-ChildItem -LiteralPath $scriptsRoot -Recurse -Filter *.as -File)
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
    foreach ($required in @('Graphics Options', 'CancelScreen', 'ReturnFromScreen', 'FE_SetActiveScreenName","Graphics Options')) { Assert-ContainsOrdinal $screenText $required 'Options Graphics screen script' }
    Assert-RowShellContract -ScreenDirectory $screenDirectory.FullName
    foreach ($required in @('rs.ui.BatmanGraphicsVsyncController', 'InitialVsync', 'DraftVsync', 'ApplyWasDispatched = false')) { Assert-ContainsOrdinal -Text $screenText -Token $required -Context 'Options Graphics screen script' }
    if ($screenText -notmatch 'IncrementVsync\s*=\s*function\s*\(\)\s*\{\s*this\.SetVsync\(this\.DraftVsync\s*==\s*0\s*\?\s*1\s*:\s*0,true\);\s*\}') { throw 'IncrementVsync must wrap DraftVsync through the guarded setter.' }
    if ($screenText -notmatch 'DecrementVsync\s*=\s*function\s*\(\)\s*\{\s*this\.SetVsync\(this\.DraftVsync\s*==\s*0\s*\?\s*1\s*:\s*0,false\);\s*\}') { throw 'DecrementVsync must wrap DraftVsync through the guarded setter.' }
    Assert-ContainsOrdinal -Text $screenText -Token 'setInterval(this,"CompleteApply",100)' -Context 'Options Graphics screen timer'
    Assert-ContainsOrdinal -Text $screenText -Token 'this.Screen.BlockInput(true);' -Context 'Options Graphics apply input block'
    Assert-ContainsOrdinal -Text $screenText -Token 'this.Screen.BlockInput(false);' -Context 'Options Graphics apply input unblock'
    if ($screenText -notmatch 'FE_SetControlType",4210\s*\+\s*this\.DraftVsync') { throw 'Options Graphics screen script must dispatch FE_SetControlType 4210 plus DraftVsync.' }
    if ($screenText -notmatch 'FE_SetControlType",4990\s*\+\s*this\.ApplySignalToggle') { throw 'Options Graphics screen script must dispatch FE_SetControlType 4990 plus ApplySignalToggle.' }
    foreach ($forbidden in @('Helen_GetInt', 'Helen_SetInt', 'Helen_RunCommand', 'Helen_ApplyBatmanGraphicsDraft', 'GraphicsExitPrompt', 'CaptureInitialState')) { Assert-NotContainsOrdinal -Text $screenText -Token $forbidden -Context 'Options Graphics screen script' }
    foreach ($forbidden in @('Helen_', 'GraphicsExitPrompt', 'loadBatmanGraphicsDraftIntoConfig', 'applyBatmanGraphicsDraft', 'Unsaved graphics changes', 'Some changes require a restart')) { foreach ($file in $scriptFiles) { Assert-NotContainsOrdinal -Text (Get-Content -LiteralPath $file.FullName -Raw) -Token $forbidden -Context $file.Name } }
}
finally {
    if (Test-Path -LiteralPath $tempRoot) { Remove-Item -LiteralPath $tempRoot -Recurse -Force }
}

Write-Output 'PASS'
