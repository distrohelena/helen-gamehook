param(
    [string]$RetailFrontendPackagePath,
    [string]$BatmanRoot,
    [string]$Configuration = 'Debug',
    [string]$BuilderRoot,
    [string]$BatmanUserIniPath
)

$ErrorActionPreference = 'Stop'
$env:MSBUILDDISABLENODEREUSE = '1'
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
    if ($Index -in @(2, 3, 12, 13)) {
        $activeRows = @{
            2 = [pscustomobject]@{ RowIndex = 3; Values = 'this.Names = new Array("Off","On");' }
            3 = [pscustomobject]@{ RowIndex = 4; Values = 'this.Names = new Array("Off","2x","4x","8x","16x");' }
            12 = [pscustomobject]@{ RowIndex = 13; Values = 'this.Names = new Array("Off","Normal","High");' }
            13 = [pscustomobject]@{ RowIndex = 14; Values = 'this.Names = new Array("Off","On");' }
        }
        $activeRow = $activeRows[$Index]
        Assert-ContainsOrdinal $Text $activeRow.Values "$Context names"
        Assert-ContainsOrdinal $Text "this.RowIndex = $($activeRow.RowIndex);" "$Context row index"
        Assert-ContainsOrdinal $Text 'this.State = _parent.GraphicsOptionsController.GetDraftIndex(this.RowIndex);' "$Context initial state"
        Assert-ContainsOrdinal $Text '_parent.GraphicsOptionsController.ToggleSetting(this.RowIndex);' "$Context RunAction"
        Assert-ContainsOrdinal $Text '_parent.GraphicsOptionsController.IncrementSetting(this.RowIndex);' "$Context Increment"
        Assert-ContainsOrdinal $Text '_parent.GraphicsOptionsController.DecrementSetting(this.RowIndex);' "$Context Decrement"
        Assert-ContainsOrdinal $Text $labels[$Index] "$Context label"
        Assert-ContainsOrdinal $Text 'this._visible = true;' "$Context visibility"
        return
    }

    if ($Index -eq 14) {
        Assert-ContainsOrdinal $Text 'this.Names = new Array("");' "$Context names"
        Assert-ContainsOrdinal $Text 'Apply Changes' "$Context label"
        Assert-ContainsOrdinal $Text 'this.ItemText.text = "";' "$Context value"
        Assert-ContainsOrdinal $Text 'this._visible = true;' "$Context visibility"
        Assert-ContainsOrdinal $Text '_parent.GraphicsOptionsController.ApplyChanges();' "$Context RunAction"
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

function New-GraphicsOptionsIniFixture {
    <# Create an isolated four-setting bootstrap fixture without changing the user's INI. #>
    param(
        [Parameter(Mandatory = $true)] [string]$SourcePath,
        [Parameter(Mandatory = $true)] [string]$DestinationPath,
        [Parameter(Mandatory = $true)] [bool]$Vsync,
        [Parameter(Mandatory = $true)] [int]$MsaaSamples,
        [Parameter(Mandatory = $true)] [int]$PhysxLevel,
        [Parameter(Mandatory = $true)] [bool]$Stereo
    )

    $fixtureText = Get-Content -LiteralPath $SourcePath -Raw
    $assignments = @(
        [pscustomobject]@{ Name = 'UseVsync'; Pattern = '(?m)^UseVsync=(?:True|False|0|1)\r?$'; Value = if ($Vsync) { 'True' } else { 'False' } },
        [pscustomobject]@{ Name = 'MaxMultisamples'; Pattern = '(?m)^MaxMultisamples=-?\d+\r?$'; Value = [string]$MsaaSamples },
        [pscustomobject]@{ Name = 'PhysXLevel'; Pattern = '(?m)^PhysXLevel=-?\d+\r?$'; Value = [string]$PhysxLevel },
        [pscustomobject]@{ Name = 'Stereo'; Pattern = '(?m)^Stereo=(?:True|False|0|1)\r?$'; Value = if ($Stereo) { 'True' } else { 'False' } }
    )
    foreach ($assignment in $assignments) {
        $matches = [regex]::Matches($fixtureText, $assignment.Pattern)
        if ($matches.Count -ne 1) { throw "Expected exactly one $($assignment.Name) assignment in isolated INI fixture source, found $($matches.Count)." }
        $fixtureText = [regex]::Replace($fixtureText, $assignment.Pattern, "$($assignment.Name)=$($assignment.Value)", 1)
    }
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
    $result = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('run', '--no-build', '--project', $BuilderProject, '-c', $Configuration, '--', 'build-main-menu-graphics-shell', '--root', $BuilderRoot, '--output-dir', $OutputDirectory, '--ffdec', $FfdecPath, '--ini', $IniPath)
    if ($result.ExitCode -ne 0) { throw "Shell build failed for INI '$IniPath': $($result.Output -join [Environment]::NewLine)" }
}

function Invoke-RetailGraphicsPatch {
    <# Patch the verified retail frontend into an isolated output package. #>
    param(
        [Parameter(Mandatory = $true)] [string]$PatcherProject,
        [Parameter(Mandatory = $true)] [string]$Configuration,
        [Parameter(Mandatory = $true)] [string]$RetailPackagePath,
        [Parameter(Mandatory = $true)] [string]$ManifestPath,
        [Parameter(Mandatory = $true)] [string]$OutputPath
    )

    $result = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('run', '--no-build', '--project', $PatcherProject, '-c', $Configuration, '--', 'patch', '--package', $RetailPackagePath, '--manifest', $ManifestPath, '--output', $OutputPath)
    if ($result.ExitCode -ne 0) { throw "Retail graphics patch failed for '$OutputPath': $($result.Output -join [Environment]::NewLine)" }
    if (-not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) { throw "Retail graphics patch did not create '$OutputPath'." }
}

function Invoke-GraphicsHgdeltaBuild {
    <# Build one isolated HGDL delta from the verified retail base to one target package. #>
    param(
        [Parameter(Mandatory = $true)] [string]$BuildHgdeltaPath,
        [Parameter(Mandatory = $true)] [string]$RetailPackagePath,
        [Parameter(Mandatory = $true)] [string]$TargetPackagePath,
        [Parameter(Mandatory = $true)] [string]$OutputPath
    )

    $result = & $BuildHgdeltaPath -BaseFile $RetailPackagePath -TargetFile $TargetPackagePath -OutputFile $OutputPath -ChunkSize 65536
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) {
        throw "Graphics HGDL build failed for '$OutputPath': $($result -join [Environment]::NewLine)"
    }
}

function Assert-ShellLiveHandshakeExport {
    param([string]$GfxPath, [string]$ExportRoot, [string]$FfdecPath, [string]$Context)
    $export = Invoke-ExternalProcess -FilePath $FfdecPath -Arguments @('-export', 'script', $ExportRoot, $GfxPath)
    if ($export.ExitCode -ne 0) { throw "FFDec failed to export $Context shell scripts: $($export.Output -join [Environment]::NewLine)" }
    $screenDirectory = @(Get-ChildItem -LiteralPath (Join-Path $ExportRoot 'scripts') -Recurse -Directory | Where-Object { $_.Name -eq 'DefineSprite_600_ScreenOptionsGraphics' })
    if ($screenDirectory.Count -ne 1) { throw "$Context export did not contain exactly one graphics screen directory." }
    $screenText = Get-Content -LiteralPath (Join-Path $screenDirectory[0].FullName 'frame_1\DoAction.as') -Raw
    Assert-ContainsOrdinal -Text $screenText -Token 'this.GraphicsOptionsController = new rs.ui.BatmanGraphicsOptionsController(this);' -Context "$Context controller initialization"
    Assert-ContainsOrdinal -Text $screenText -Token 'flash.external.ExternalInterface.call("FE_SetControlType",this.Settings[this.InitializationIndex].ReadRequest,"");' -Context "$Context live-state request"
    Assert-ContainsOrdinal -Text $screenText -Token 'flash.external.ExternalInterface.call("FE_GetControlType")' -Context "$Context live-state response poll"
    $rowPath = Join-Path $screenDirectory[0].FullName 'frame_1\PlaceObject2_290_List_Template_125\CLIPACTIONRECORD onClipEvent(load).as'
    $rowText = Get-Content -LiteralPath $rowPath -Raw
    Assert-ContainsOrdinal -Text $rowText -Token 'this.State = _parent.GraphicsOptionsController.GetDraftIndex(this.RowIndex);' -Context "$Context live VSync row state"
    Assert-ContainsOrdinal -Text $rowText -Token '"Unavailable" : "Loading..."' -Context "$Context explicit unresolved state"
}

function Assert-BuilderRejectsIni {
    param([string]$BuilderProject, [string]$Configuration, [string]$BuilderRoot, [string]$OutputDirectory, [string]$FfdecPath, [string]$IniPath, [string]$ExpectedDiagnostic, [string]$Context)
    $result = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('run', '--no-build', '--project', $BuilderProject, '-c', $Configuration, '--', 'build-main-menu-graphics-shell', '--root', $BuilderRoot, '--output-dir', $OutputDirectory, '--ffdec', $FfdecPath, '--ini', $IniPath)
    if ($result.ExitCode -eq 0) { throw "$Context unexpectedly accepted INI '$IniPath'." }
    Assert-ContainsOrdinal -Text ($result.Output -join [Environment]::NewLine) -Token $ExpectedDiagnostic -Context "$Context diagnostic"
    $outputPath = Join-Path $OutputDirectory 'MainV2-graphics-options.gfx'
    if (Test-Path -LiteralPath $outputPath) { throw "$Context accepted output before rejecting INI '$IniPath': $outputPath" }
}

function Assert-CurrentGraphicsSourceProvenance {
    <# Inspect only current shell sources so historical exports cannot satisfy retail proof. #>
    param(
        [Parameter(Mandatory = $true)] [string]$RebuildPath,
        [Parameter(Mandatory = $true)] [string]$ShellTemplatePath,
        [Parameter(Mandatory = $true)] [string]$ShellBuilderPath
    )

    foreach ($path in @($RebuildPath, $ShellTemplatePath, $ShellBuilderPath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Current graphics source was not found: $path" }
    }
    $rebuildText = Get-Content -LiteralPath $RebuildPath -Raw
    $shellTemplateText = Get-Content -LiteralPath $ShellTemplatePath -Raw
    $shellBuilderText = Get-Content -LiteralPath $ShellBuilderPath -Raw
    $shellBuilderMatch = [regex]::Match($shellBuilderText, '(?s)private static void PatchFrontendShellScripts\(.*?private static void ValidateShellInputs')
    if (-not $shellBuilderMatch.Success) { throw 'Current shell builder source method could not be isolated.' }
    foreach ($source in @(
        [pscustomobject]@{ Name = 'rebuild'; Text = $rebuildText },
        [pscustomobject]@{ Name = 'shell template'; Text = $shellTemplateText },
        [pscustomobject]@{ Name = 'shell builder'; Text = $shellBuilderMatch.Value }
    )) {
        foreach ($forbidden in @('F:\helenhook.7z', 'F:/helenhook.7z', 'batma/', 'batma\', 'Program Files', 'full-controller', 'GraphicsVsyncController', 'InitialVsync', 'DraftVsync', 'GraphicsExitPrompt', 'DefineSprite_601', 'Helen_', 'prompt export', 'prompt route', 'old package', 'historical')) {
            Assert-NotContainsOrdinal -Text $source.Text -Token $forbidden -Context "$($source.Name) graphics provenance"
        }
    }
    $allowListMatch = [regex]::Match($shellBuilderText, '(?s)ShellPatchedScriptRelativePaths\s*=\s*\[(?<items>.*?)\];')
    if (-not $allowListMatch.Success -or (([regex]::Matches($allowListMatch.Groups['items'].Value, '"')).Count / 2) -ne 21) {
        throw 'Current shell builder must retain exactly 21 allow-listed patched source files.'
    }
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

foreach ($projectPath in @($builderProject, $patcherProject)) {
    $build = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('build', $projectPath, '-c', $Configuration, '--nologo', '--disable-build-servers', '-nr:false', '-p:UseSharedCompilation=false')
    if ($build.ExitCode -ne 0) { throw "Retail patch dependency build failed for '$projectPath': $($build.Output -join [Environment]::NewLine)" }
}

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('HelenBatmanGraphicsRetailPatch-' + [Guid]::NewGuid().ToString('N'))
$bootstrapAIniPath = Join-Path $tempRoot 'bootstrap-a.ini'
$bootstrapBIniPath = Join-Path $tempRoot 'bootstrap-b.ini'
$malformedIniPath = Join-Path $tempRoot 'bootstrap-malformed.ini'
$missingIniPath = Join-Path $tempRoot 'bootstrap-missing.ini'
$bootstrapARoot = Join-Path $tempRoot 'prototype-a'
$bootstrapBRoot = Join-Path $tempRoot 'prototype-b'
$normalRoot = Join-Path $tempRoot 'prototype-normal'
$bootstrapAExportRoot = Join-Path $tempRoot 'export-a'
$bootstrapBExportRoot = Join-Path $tempRoot 'export-b'
$invalidMissingOutputRoot = Join-Path $tempRoot 'invalid-missing-output'
$invalidMalformedOutputRoot = Join-Path $tempRoot 'invalid-malformed-output'
$bootstrapATarget = Join-Path $tempRoot 'Frontend-a.umap'
$bootstrapBTarget = Join-Path $tempRoot 'Frontend-b.umap'
$normalTarget = Join-Path $tempRoot 'Frontend-normal.umap'
$bootstrapADelta = Join-Path $tempRoot 'Frontend-a.hgdelta'
$bootstrapBDelta = Join-Path $tempRoot 'Frontend-b.hgdelta'
$normalDelta = Join-Path $tempRoot 'Frontend-normal.hgdelta'
$prototypeRoot = $bootstrapBRoot
$prototypeGfx = Join-Path $prototypeRoot 'MainV2-graphics-options.gfx'
$normalGfx = Join-Path $normalRoot 'MainV2-graphics-options.gfx'
$manifestPath = Join-Path $tempRoot 'patch.manifest.json'
$patchedPackage = $bootstrapBTarget
$extractedGfx = Join-Path $tempRoot 'MainV2.gfx'
$xmlPath = Join-Path $tempRoot 'MainV2.xml'
$exportRoot = Join-Path $tempRoot 'export'
$normalTargetPath = Join-Path $BuilderRoot 'generated\graphics-options-experiment\Frontend-graphics-options.umap'
$normalDeltaPath = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-graphics-options\builds\steam-goty-1.0\assets\deltas\Frontend-graphics-options.hgdelta'
$rebuildSourcePath = Join-Path $PSScriptRoot 'Rebuild-BatmanGraphicsOptionsExperiment.ps1'
$buildHgdeltaPath = Join-Path $PSScriptRoot 'Build-Hgdelta.ps1'
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
try {
    . $rebuildSourcePath -FunctionsOnly -BatmanRoot $BatmanRoot -BuilderRoot $BuilderRoot -BatmanUserIniPath $BatmanUserIniPath
    Assert-CurrentGraphicsSourceProvenance -RebuildPath $rebuildSourcePath -ShellTemplatePath (Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\GraphicsOptionsShellScriptTemplates.cs') -ShellBuilderPath (Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\GraphicsOptionsAssetBuilder.cs')
    New-GraphicsOptionsIniFixture -SourcePath $BatmanUserIniPath -DestinationPath $bootstrapAIniPath -Vsync $false -MsaaSamples 1 -PhysxLevel 0 -Stereo $false
    New-GraphicsOptionsIniFixture -SourcePath $BatmanUserIniPath -DestinationPath $bootstrapBIniPath -Vsync $true -MsaaSamples 16 -PhysxLevel 2 -Stereo $true
    New-MalformedIniFixture -SourcePath $BatmanUserIniPath -DestinationPath $malformedIniPath
    Assert-BuilderRejectsIni -BuilderProject $builderProject -Configuration $Configuration -BuilderRoot $BuilderRoot -OutputDirectory $invalidMissingOutputRoot -FfdecPath $ffdec -IniPath $missingIniPath -ExpectedDiagnostic "Required path not found: $missingIniPath" -Context 'Missing INI validation'
    Assert-BuilderRejectsIni -BuilderProject $builderProject -Configuration $Configuration -BuilderRoot $BuilderRoot -OutputDirectory $invalidMalformedOutputRoot -FfdecPath $ffdec -IniPath $malformedIniPath -ExpectedDiagnostic "INI value 'SystemSettings.UseVsync' must be a boolean-like value but was 'Malformed'." -Context 'Malformed INI validation'
    Invoke-ShellBuild -BuilderProject $builderProject -Configuration $Configuration -BuilderRoot $BuilderRoot -OutputDirectory $bootstrapARoot -FfdecPath $ffdec -IniPath $bootstrapAIniPath
    Invoke-ShellBuild -BuilderProject $builderProject -Configuration $Configuration -BuilderRoot $BuilderRoot -OutputDirectory $bootstrapBRoot -FfdecPath $ffdec -IniPath $bootstrapBIniPath
    Invoke-ShellBuild -BuilderProject $builderProject -Configuration $Configuration -BuilderRoot $BuilderRoot -OutputDirectory $normalRoot -FfdecPath $ffdec -IniPath $BatmanUserIniPath
    Assert-ShellLiveHandshakeExport -GfxPath (Join-Path $bootstrapARoot 'MainV2-graphics-options.gfx') -ExportRoot $bootstrapAExportRoot -FfdecPath $ffdec -Context 'Group 1 A (VSync Off, MSAA Off, PhysX Off, Stereo Off) input'
    Assert-ShellLiveHandshakeExport -GfxPath (Join-Path $bootstrapBRoot 'MainV2-graphics-options.gfx') -ExportRoot $bootstrapBExportRoot -FfdecPath $ffdec -Context 'Group 1 B (VSync On, MSAA 16x, PhysX High, Stereo On) input'
    $aShellHash = (Get-FileHash -LiteralPath (Join-Path $bootstrapARoot 'MainV2-graphics-options.gfx') -Algorithm SHA256).Hash
    $bShellHash = (Get-FileHash -LiteralPath (Join-Path $bootstrapBRoot 'MainV2-graphics-options.gfx') -Algorithm SHA256).Hash
    $normalShellHash = (Get-FileHash -LiteralPath $normalGfx -Algorithm SHA256).Hash
    if ($aShellHash -cne $bShellHash -or $aShellHash -cne $normalShellHash) { throw "Graphics shell bytes vary with build-time Group 1 values. A=$aShellHash B=$bShellHash normal=$normalShellHash" }
    if (-not (Test-Path -LiteralPath $prototypeGfx)) { throw "Shell prototype was not generated: $prototypeGfx" }

    $manifest = [ordered]@{ name = 'MainV2 graphics-options shell retail patch'; patches = @([ordered]@{ owner = 'MainMenu'; exportName = 'MainV2'; exportType = 'GFxMovieInfo'; replacementPath = (Join-Path $bootstrapARoot 'MainV2-graphics-options.gfx'); payloadMagic = 'GFX' }) }
    [IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
    Invoke-RetailGraphicsPatch -PatcherProject $patcherProject -Configuration $Configuration -RetailPackagePath $RetailFrontendPackagePath -ManifestPath $manifestPath -OutputPath $bootstrapATarget
    $manifest.patches[0].replacementPath = (Join-Path $bootstrapBRoot 'MainV2-graphics-options.gfx')
    [IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
    Invoke-RetailGraphicsPatch -PatcherProject $patcherProject -Configuration $Configuration -RetailPackagePath $RetailFrontendPackagePath -ManifestPath $manifestPath -OutputPath $bootstrapBTarget
    $manifest.patches[0].replacementPath = $normalGfx
    [IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
    Invoke-RetailGraphicsPatch -PatcherProject $patcherProject -Configuration $Configuration -RetailPackagePath $RetailFrontendPackagePath -ManifestPath $manifestPath -OutputPath $normalTarget
    Invoke-GraphicsHgdeltaBuild -BuildHgdeltaPath $buildHgdeltaPath -RetailPackagePath $RetailFrontendPackagePath -TargetPackagePath $bootstrapATarget -OutputPath $bootstrapADelta
    Invoke-GraphicsHgdeltaBuild -BuildHgdeltaPath $buildHgdeltaPath -RetailPackagePath $RetailFrontendPackagePath -TargetPackagePath $bootstrapBTarget -OutputPath $bootstrapBDelta
    Invoke-GraphicsHgdeltaBuild -BuildHgdeltaPath $buildHgdeltaPath -RetailPackagePath $RetailFrontendPackagePath -TargetPackagePath $normalTarget -OutputPath $normalDelta
    $hashes = @(
        [pscustomobject]@{ Name = 'shell GFX'; A = $aShellHash; B = $bShellHash; Normal = $normalShellHash },
        [pscustomobject]@{ Name = 'Frontend target'; A = (Get-FileHash -LiteralPath $bootstrapATarget -Algorithm SHA256).Hash; B = (Get-FileHash -LiteralPath $bootstrapBTarget -Algorithm SHA256).Hash; Normal = (Get-FileHash -LiteralPath $normalTarget -Algorithm SHA256).Hash },
        [pscustomobject]@{ Name = 'Frontend delta'; A = (Get-FileHash -LiteralPath $bootstrapADelta -Algorithm SHA256).Hash; B = (Get-FileHash -LiteralPath $bootstrapBDelta -Algorithm SHA256).Hash; Normal = (Get-FileHash -LiteralPath $normalDelta -Algorithm SHA256).Hash }
    )
    foreach ($hash in $hashes) {
        if ($hash.A -cne $hash.B -or $hash.A -cne $hash.Normal) { throw "$($hash.Name) provenance mismatch. A=$($hash.A) B=$($hash.B) normal=$($hash.Normal)" }
    }
    if (-not (Test-Path -LiteralPath $normalTargetPath -PathType Leaf) -or -not (Test-Path -LiteralPath $normalDeltaPath -PathType Leaf)) { throw 'Production graphics target/delta was not regenerated before retail provenance validation.' }
    $productionTargetHash = (Get-FileHash -LiteralPath $normalTargetPath -Algorithm SHA256).Hash
    $productionDeltaHash = (Get-FileHash -LiteralPath $normalDeltaPath -Algorithm SHA256).Hash
    if ($hashes[1].A -cne $productionTargetHash -or $hashes[2].A -cne $productionDeltaHash) { throw "Isolated outputs do not match the regenerated production artifact. isolatedTarget=$($hashes[1].A) productionTarget=$productionTargetHash isolatedDelta=$($hashes[2].A) productionDelta=$productionDeltaHash" }
    Write-Output "Reproducibility hashes: GFX=$aShellHash Target=$($hashes[1].A) Delta=$($hashes[2].A)"

    . (Join-Path $PSScriptRoot 'BatmanPackVerificationHelpers.ps1')
    $patchedStorage = Get-UnrealPackageStorageInfo -Path $patchedPackage
    if ($patchedStorage.CompressionChunkCount -le 0) { throw 'Retail patch output must remain chunk-compressed.' }
    $extract = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('run', '--no-build', '--project', $patcherProject, '-c', $Configuration, '--', 'extract-gfx', '--package', $patchedPackage, '--owner', 'MainMenu', '--name', 'MainV2', '--output', $extractedGfx)
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
    foreach ($required in @('rs.ui.BatmanGraphicsOptionsController', 'InitializationComplete', 'InitializationFailed', 'BeginInitialization', 'PollInitialization', 'GetDraftIndex', 'GetInitialIndex', 'ApplyChanges', 'BeginRollback', 'CompleteRollback', 'FailRollback', 'ReadRequest:4200', 'ReadRequest:4300', 'ReadRequest:4400', 'ReadRequest:4500', 'FE_GetControlType')) { Assert-ContainsOrdinal -Text $screenText -Token $required -Context 'Options Graphics screen script' }
    Assert-ContainsOrdinal -Text $screenText -Token 'this.Screen.BlockInput(true);' -Context 'Options Graphics apply input block'
    Assert-ContainsOrdinal -Text $screenText -Token 'this.Screen.BlockInput(false);' -Context 'Options Graphics apply input unblock'
    if ($screenText -notmatch 'this\.CurrentPendingSetting\.WriteRequestBase\s*\+\s*this\.CurrentPendingSetting\.DraftIndex') { throw 'Options Graphics screen script must dispatch the current setting write request through its draft index.' }
    if ($screenText -notmatch 'FE_SetControlType",4990\s*\+\s*this\.ApplySignalToggle\s*,\s*""\s*\)') { throw 'Options Graphics screen script must dispatch FE_SetControlType 4990 plus ApplySignalToggle with an empty second argument.' }
    foreach ($forbidden in @('Helen_GetInt', 'Helen_SetInt', 'Helen_RunCommand', 'Helen_ApplyBatmanGraphicsDraft', 'GraphicsExitPrompt', 'CaptureInitialState', 'GraphicsVsyncController', 'InitialVsync', 'DraftVsync', 'InitialStateResolved', 'InitialStateFailed', 'CompleteApply', 'SetVsync', 'IncrementVsync', 'DecrementVsync')) { Assert-NotContainsOrdinal -Text $screenText -Token $forbidden -Context 'Options Graphics screen script' }
    foreach ($forbidden in @('Helen_', 'GraphicsExitPrompt', 'loadBatmanGraphicsDraftIntoConfig', 'applyBatmanGraphicsDraft', 'Unsaved graphics changes', 'Some changes require a restart', 'ApplyWasDispatched')) { foreach ($file in $scriptFiles) { Assert-NotContainsOrdinal -Text (Get-Content -LiteralPath $file.FullName -Raw) -Token $forbidden -Context $file.Name } }
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        if ($null -eq (Get-Command Remove-SafeMutationTarget -ErrorAction SilentlyContinue)) {
            throw "Safe temporary cleanup helper was not loaded for '$tempRoot'."
        }
        Remove-SafeMutationTarget -Path $tempRoot -AllowedDescendantRoots @(([IO.Path]::GetFullPath([IO.Path]::GetTempPath())).TrimEnd('\'))
    }
}

Write-Output 'PASS'
