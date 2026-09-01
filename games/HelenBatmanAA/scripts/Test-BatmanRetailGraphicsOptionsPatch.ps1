param(
    [string]$RetailFrontendPackagePath,
    [string]$BatmanRoot,
    [string]$Configuration = 'Debug',
    [string]$BuilderRoot
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

function Assert-RowShellContract {
    param([string]$ScreenDirectory)
    $depths = @('141', '133', '125', '117', '109', '101', '93', '85', '77', '69', '61', '53', '45', '37', '29')
    $labels = @('Fullscreen', 'Resolution', 'VSync', 'MSAA', 'Detail Level', 'Bloom', 'Dynamic Shadows', 'Motion Blur', 'Distortion', 'Fog Volumes', 'Spherical Harmonic Lighting', 'Ambient Occlusion', 'PhysX', 'Stereo 3D', '')
    for ($index = 0; $index -lt $depths.Count; $index++) {
        $rowPath = Join-Path $ScreenDirectory "frame_1\PlaceObject2_290_List_Template_$($depths[$index])\CLIPACTIONRECORD onClipEvent(load).as"
        if (-not (Test-Path -LiteralPath $rowPath)) { throw "Missing known row script $rowPath." }
        $rowText = Get-Content -LiteralPath $rowPath -Raw
        if ($index -lt 14) {
            Assert-ContainsOrdinal $rowText 'this.Names = new Array("Not active");' "row $($index + 1)"
            if ($rowText -notmatch 'this\.(?:Label\.)?Label\.Text\.text\s*=\s*"' -and $rowText -notmatch 'this\.Label\.Text\.text\s*=\s*"') { throw "row $($index + 1) is missing its fixed label assignment." }
            Assert-ContainsOrdinal $rowText $labels[$index] "row $($index + 1) label"
            Assert-ContainsOrdinal $rowText 'this._visible = true;' "row $($index + 1) visibility"
        } else {
            Assert-ContainsOrdinal $rowText 'this.Names = new Array("");' 'row 15 names'
            Assert-ContainsOrdinal $rowText 'this._visible = false;' 'row 15 visibility'
            Assert-ContainsOrdinal $rowText 'this.ItemText.text = "";' 'row 15 value'
        }
        foreach ($action in @('RunAction', 'Increment', 'Decrement', 'ShowPrompt')) {
            if ((Get-ActionFunctionBody $rowText "this.$action") -ne '') { throw "row $($index + 1) action $action must be a no-op." }
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

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) { $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path } else { $BatmanRoot = (Resolve-Path $BatmanRoot).Path }
if ([string]::IsNullOrWhiteSpace($BuilderRoot)) { $BuilderRoot = Join-Path $BatmanRoot 'builder' } elseif ([IO.Path]::IsPathRooted($BuilderRoot)) { $BuilderRoot = [IO.Path]::GetFullPath($BuilderRoot) } elseif (Test-Path -LiteralPath $BuilderRoot) { $BuilderRoot = (Resolve-Path $BuilderRoot).Path } else { $BuilderRoot = [IO.Path]::GetFullPath((Join-Path $BatmanRoot $BuilderRoot)) }
if ([string]::IsNullOrWhiteSpace($RetailFrontendPackagePath)) { $RetailFrontendPackagePath = Join-Path $BuilderRoot 'extracted\frontend-retail\Frontend.umap' } elseif (-not [IO.Path]::IsPathRooted($RetailFrontendPackagePath)) { $RetailFrontendPackagePath = [IO.Path]::GetFullPath((Join-Path (Get-Location) $RetailFrontendPackagePath)) }
$RetailFrontendPackagePath = [IO.Path]::GetFullPath($RetailFrontendPackagePath)

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
$prototypeRoot = Join-Path $tempRoot 'prototype'
$prototypeGfx = Join-Path $prototypeRoot 'MainV2-graphics-options.gfx'
$manifestPath = Join-Path $tempRoot 'patch.manifest.json'
$patchedPackage = Join-Path $tempRoot 'Frontend-graphics-options.umap'
$extractedGfx = Join-Path $tempRoot 'MainV2.gfx'
$xmlPath = Join-Path $tempRoot 'MainV2.xml'
$exportRoot = Join-Path $tempRoot 'export'
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
try {
    $build = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('run', '--project', $builderProject, '-c', $Configuration, '--', 'build-main-menu-graphics-shell', '--root', $BuilderRoot, '--output-dir', $prototypeRoot, '--ffdec', $ffdec)
    if ($build.ExitCode -ne 0) { throw "Shell prototype build failed: $($build.Output -join [Environment]::NewLine)" }
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
    foreach ($forbidden in @('Helen_', 'ApplyChanges', 'GraphicsExitPrompt', 'loadBatmanGraphicsDraftIntoConfig', 'applyBatmanGraphicsDraft', 'Unsaved graphics changes', 'Some changes require a restart')) { foreach ($file in $scriptFiles) { Assert-NotContainsOrdinal -Text (Get-Content -LiteralPath $file.FullName -Raw) -Token $forbidden -Context $file.Name } }
}
finally {
    if (Test-Path -LiteralPath $tempRoot) { Remove-Item -LiteralPath $tempRoot -Recurse -Force }
}

Write-Output 'PASS'
