param(
    [string]$BatmanRoot,
    [string]$BuilderRoot,
    [string]$Configuration = 'Release',
    [string]$PackRootOverride,
    [string]$TargetPathOverride
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'BatmanBuilderWorkspaceHelpers.ps1')
. (Join-Path $PSScriptRoot 'BatmanPackVerificationHelpers.ps1')

function Assert-ExactProperties {
    param(
        [Parameter(Mandatory = $true)] [psobject]$Object,
        [Parameter(Mandatory = $true)] [string[]]$Names,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    $actual = @($Object.PSObject.Properties.Name | Sort-Object)
    $expected = @($Names | Sort-Object)
    if (($actual -join '|') -cne ($expected -join '|')) {
        throw "$Context properties drifted. Expected '$($expected -join ', ')' but found '$($actual -join ', ')'."
    }
}

function Assert-ContainsOrdinal {
    param(
        [Parameter(Mandatory = $true)] [AllowEmptyString()] [string]$Text,
        [Parameter(Mandatory = $true)] [string]$Token,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    if ($Text.IndexOf($Token, [System.StringComparison]::Ordinal) -lt 0) {
        throw "$Context is missing required token '$Token'."
    }
}

function Assert-NotContainsOrdinal {
    param(
        [Parameter(Mandatory = $true)] [AllowEmptyString()] [string]$Text,
        [Parameter(Mandatory = $true)] [string]$Token,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    if ($Text.IndexOf($Token, [System.StringComparison]::Ordinal) -ge 0) {
        throw "$Context contains forbidden token '$Token'."
    }
}

function Invoke-ExternalProcess {
    param(
        [Parameter(Mandatory = $true)] [string]$FilePath,
        [Parameter(Mandatory = $true)] [string[]]$Arguments
    )

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { $output = @(& $FilePath @Arguments 2>&1) } finally { $ErrorActionPreference = $previousErrorActionPreference }
    return [pscustomobject]@{
        ExitCode = $LASTEXITCODE
        Output = $output
    }
}

function Read-UInt32LittleEndian {
    param([byte[]]$Bytes, [int]$Offset)
    return [BitConverter]::ToUInt32($Bytes, $Offset)
}

function Read-UInt64LittleEndian {
    param([byte[]]$Bytes, [int]$Offset)
    if ($Offset -lt 0 -or $Offset -gt ($Bytes.Length - 8)) { throw "HGDL read exceeded the byte buffer at offset $Offset." }
    return [BitConverter]::ToUInt64($Bytes, $Offset)
}

function Assert-ExactPackFileSet {
    param(
        [Parameter(Mandatory = $true)] [string]$Root,
        [Parameter(Mandatory = $true)] [string]$BuildDirectoryName
    )

    $expected = @(
        'pack.json',
        "builds\$BuildDirectoryName\build.json",
        "builds\$BuildDirectoryName\bindings.json",
        "builds\$BuildDirectoryName\commands.json",
        "builds\$BuildDirectoryName\files.json",
        "builds\$BuildDirectoryName\assets\deltas\Frontend-graphics-options.hgdelta"
    ) | Sort-Object
    $rootPrefix = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $actual = @(Get-ChildItem -LiteralPath $Root -Recurse -File | ForEach-Object {
        $_.FullName.Substring($rootPrefix.Length).Replace('/', '\')
    } | Sort-Object)
    if (($actual -join '|') -cne ($expected -join '|')) {
        throw "Graphics-options pack file set drifted. Expected '$($expected -join ', ')' but found '$($actual -join ', ')'."
    }
}

function Assert-RebuildAtomicSourceContract {
    param([Parameter(Mandatory = $true)] [string]$ScriptPath)

    $source = Get-Content -LiteralPath $ScriptPath -Raw
    foreach ($token in @(
        '$stagedPackRoot', '$stagedTargetPath', '$stagedDeltaPath', '$packBackupRoot', '$targetBackupPath',
        'Move-Item -LiteralPath', '-OutputFile $stagedDeltaPath', 'Restore-AtomicRebuild', 'catch', 'finally'
    )) {
        Assert-ContainsOrdinal -Text $source -Token $token -Context 'atomic graphics-options rebuild source'
    }
    if ($source.IndexOf('-OutputFile $deltaPath', [StringComparison]::Ordinal) -ge 0) {
        throw 'Rebuild must never write hgdelta output directly into the checked-in pack.'
    }
    if ($source.IndexOf('$stagedPackRoot = $packRoot', [StringComparison]::OrdinalIgnoreCase) -ge 0 -or
        $source.IndexOf('$stagedTargetPath = $targetPath', [StringComparison]::OrdinalIgnoreCase) -ge 0) {
        throw 'Rebuild staging paths must differ from live pack and target paths.'
    }
    if ($source -notmatch '(?s)catch\s*\{.*Restore-AtomicRebuild') {
        throw 'Rebuild catch path must restore the live pack and target after activation failure.'
    }
}

function Get-HgdeltaFunctionBody {
    param(
        [Parameter(Mandatory = $true)] [string]$Text,
        [Parameter(Mandatory = $true)] [string]$FunctionAssignment
    )

    $match = [regex]::Match($Text, [regex]::Escape($FunctionAssignment) + '\s*=\s*function\s*\(\s*\)\s*\{(?<body>.*?)\}', [Text.RegularExpressions.RegexOptions]::Singleline)
    if (-not $match.Success) { throw "Missing no-op function '$FunctionAssignment'." }
    return $match.Groups['body'].Value.Trim()
}

function Assert-NoOpRowActions {
    param([Parameter(Mandatory = $true)] [string]$Text, [Parameter(Mandatory = $true)] [string]$Context)
    foreach ($action in @('RunAction', 'Increment', 'Decrement', 'ShowPrompt')) {
        if ((Get-HgdeltaFunctionBody -Text $Text -FunctionAssignment "this.$action") -ne '') {
            throw "$Context action $action must be a no-op."
        }
    }
}

function Assert-ScopedExportedShellContract {
    param([Parameter(Mandatory = $true)] [string]$ExportRoot)

    $scriptsRoot = Join-Path $ExportRoot 'scripts'
    $scriptFiles = @(Get-ChildItem -LiteralPath $scriptsRoot -Recurse -Filter *.as -File)
    if ($scriptFiles.Count -eq 0) { throw 'FFDec exported no ActionScript files.' }
    $screenDirectories = @(Get-ChildItem -LiteralPath $scriptsRoot -Recurse -Directory | Where-Object { $_.Name -eq 'DefineSprite_600_ScreenOptionsGraphics' })
    if ($screenDirectories.Count -ne 1) { throw "Expected exactly one DefineSprite_600_ScreenOptionsGraphics directory, found $($screenDirectories.Count)." }
    $screenDirectory = $screenDirectories[0]
    $screenScriptPath = Join-Path $screenDirectory.FullName 'frame_1\DoAction.as'
    if (-not (Test-Path -LiteralPath $screenScriptPath)) { throw "Missing known Options Graphics screen script: $screenScriptPath" }
    $screenText = Get-Content -LiteralPath $screenScriptPath -Raw

    $menuScriptPath = Join-Path $scriptsRoot 'DefineSprite_333_ScreenOptionsMenu\frame_1\PlaceObject2_117_GenericButton_37\CLIPACTIONRECORD onClipEvent(load).as'
    if (-not (Test-Path -LiteralPath $menuScriptPath)) { throw "Missing known Options menu script: $menuScriptPath" }
    $menuText = Get-Content -LiteralPath $menuScriptPath -Raw
    Assert-ContainsOrdinal -Text $menuText -Token 'GotoScreen("OptionsGraphics")' -Context 'Options menu script'
    Assert-ContainsOrdinal -Text $menuText -Token 'Graphics Options' -Context 'Options menu script'

    foreach ($token in @('Graphics Options', 'CancelScreen', 'ReturnFromScreen', 'FE_SetActiveScreenName","Graphics Options')) {
        Assert-ContainsOrdinal -Text $screenText -Token $token -Context 'Options Graphics screen script'
    }

    $rowDepths = @('141', '133', '125', '117', '109', '101', '93', '85', '77', '69', '61', '53', '45', '37', '29')
    $rowLabels = @('Fullscreen', 'Resolution', 'VSync', 'MSAA', 'Detail Level', 'Bloom', 'Dynamic Shadows', 'Motion Blur', 'Distortion', 'Fog Volumes', 'Spherical Harmonic Lighting', 'Ambient Occlusion', 'PhysX', 'Stereo 3D', '')
    for ($index = 0; $index -lt $rowDepths.Count; $index++) {
        $rowPath = Join-Path $screenDirectory.FullName "frame_1\PlaceObject2_290_List_Template_$($rowDepths[$index])\CLIPACTIONRECORD onClipEvent(load).as"
        if (-not (Test-Path -LiteralPath $rowPath)) { throw "Missing known Graphics Options row script: $rowPath" }
        $rowText = Get-Content -LiteralPath $rowPath -Raw
        if ($index -lt 14) {
            Assert-ContainsOrdinal -Text $rowText -Token 'this.Names = new Array("Not active");' -Context "Graphics row $($index + 1)"
            if ($rowText -notmatch 'this\.(?:Label\.)?Label\.Text\.text\s*=\s*"' -and $rowText -notmatch 'this\.Label\.Text\.text\s*=\s*"') { throw "Graphics row $($index + 1) is missing its fixed label assignment." }
            Assert-ContainsOrdinal -Text $rowText -Token $rowLabels[$index] -Context "Graphics row $($index + 1) label"
            Assert-ContainsOrdinal -Text $rowText -Token 'this._visible = true;' -Context "Graphics row $($index + 1) visibility"
        } else {
            Assert-ContainsOrdinal -Text $rowText -Token 'this.Names = new Array("");' -Context 'Graphics row 15 names'
            Assert-ContainsOrdinal -Text $rowText -Token 'this._visible = false;' -Context 'Graphics row 15 visibility'
            Assert-ContainsOrdinal -Text $rowText -Token 'this.ItemText.text = "";' -Context 'Graphics row 15 value'
        }
        Assert-NoOpRowActions -Text $rowText -Context "Graphics row $($index + 1)"
    }
}

function Set-HgdeltaUInt32Fixture {
    param([byte[]]$Bytes, [int]$Offset, [uint32]$Value)
    [Buffer]::BlockCopy([BitConverter]::GetBytes($Value), 0, $Bytes, $Offset, 4)
}

function Set-HgdeltaUInt64Fixture {
    param([byte[]]$Bytes, [int]$Offset, [uint64]$Value)
    [Buffer]::BlockCopy([BitConverter]::GetBytes($Value), 0, $Bytes, $Offset, 8)
}

function Assert-HgdeltaRejected {
    param(
        [Parameter(Mandatory = $true)] [string]$BasePath,
        [Parameter(Mandatory = $true)] [byte[]]$Bytes,
        [Parameter(Mandatory = $true)] [string]$Path,
        [Parameter(Mandatory = $true)] [string]$CaseName
    )
    [IO.File]::WriteAllBytes($Path, $Bytes)
    $rejected = $false
    try { $null = Reconstruct-HgdeltaTarget -BasePath $BasePath -DeltaPath $Path } catch { $rejected = $true }
    finally { if (Test-Path -LiteralPath $Path) { Remove-Item -LiteralPath $Path -Force } }
    if (-not $rejected) { throw "HGDL verifier accepted invalid $CaseName fixture." }
}

function Reconstruct-HgdeltaTarget {
    param(
        [Parameter(Mandatory = $true)] [string]$BasePath,
        [Parameter(Mandatory = $true)] [string]$DeltaPath
    )

    $base = [System.IO.File]::ReadAllBytes($BasePath)
    $delta = [System.IO.File]::ReadAllBytes($DeltaPath)
    if ($delta.Length -lt 116 -or [Text.Encoding]::ASCII.GetString($delta, 0, 4) -cne 'HGDL') { throw "Invalid HGDL delta: $DeltaPath" }
    $majorVersion = Read-UInt32LittleEndian $delta 4
    $minorVersion = Read-UInt32LittleEndian $delta 8
    $chunkSize = Read-UInt32LittleEndian $delta 12
    $baseSize = Read-UInt64LittleEndian $delta 16
    $targetSize64 = Read-UInt64LittleEndian $delta 24
    $baseHash = Convert-BytesToLowerHex -Bytes @($delta[32..63])
    $targetHash = Convert-BytesToLowerHex -Bytes @($delta[64..95])
    $chunkCount64 = Read-UInt32LittleEndian $delta 96
    $chunkTableOffset = Read-UInt64LittleEndian $delta 100
    $payloadOffset = Read-UInt64LittleEndian $delta 108
    if ($majorVersion -ne 1 -or $minorVersion -ne 0) { throw "HGDL version must be 1.0, found $majorVersion.$minorVersion." }
    if ($chunkSize -ne 65536) { throw "HGDL chunk size must be 65536, found $chunkSize." }
    if ($baseSize -ne [uint64]$base.Length) { throw "HGDL base size mismatch: header $baseSize, actual $($base.Length)." }
    if ($targetSize64 -gt [uint64][int]::MaxValue) { throw 'HGDL target is too large for verifier memory.' }
    if ($chunkCount64 -gt [uint64][int]::MaxValue) { throw 'HGDL chunk count is too large for verifier memory.' }
    $targetSize = [int]$targetSize64
    $chunkCount = [int]$chunkCount64
    $expectedChunkCount = [uint64][Math]::Ceiling($targetSize64 / [double]$chunkSize)
    if ($chunkCount64 -ne $expectedChunkCount) { throw "HGDL chunk count mismatch: expected $expectedChunkCount, found $chunkCount64." }
    if ($chunkTableOffset -lt 116) { throw "HGDL chunk table overlaps header at offset $chunkTableOffset." }
    $tableLength = $chunkCount64 * 20
    if ($tableLength -gt [uint64]$delta.Length - $chunkTableOffset) { throw 'HGDL chunk table exceeds delta file bounds.' }
    $tableEnd = $chunkTableOffset + $tableLength
    if ($payloadOffset -lt $tableEnd -or $payloadOffset -gt [uint64]$delta.Length) { throw 'HGDL payload offset is outside the delta file bounds.' }
    $actualBaseHash = (Get-FileHash -LiteralPath $BasePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($baseHash -cne $actualBaseHash) { throw "HGDL base SHA mismatch: header $baseHash, actual $actualBaseHash." }
    $result = [System.IO.MemoryStream]::new($targetSize)
    [uint64]$written = 0
    try {
        for ($index = 0; $index -lt $chunkCount; $index++) {
            $entry64 = $chunkTableOffset + ([uint64]$index * 20)
            if ($entry64 -gt [uint64][int]::MaxValue) { throw 'HGDL chunk entry offset exceeds verifier limits.' }
            $entry = [int]$entry64
            $kind = Read-UInt32LittleEndian $delta $entry
            $size64 = Read-UInt32LittleEndian $delta ($entry + 4)
            $entryPayloadOffset = Read-UInt64LittleEndian $delta ($entry + 8)
            $entryPayloadSize = Read-UInt32LittleEndian $delta ($entry + 16)
            if ($size64 -eq 0 -or $size64 -gt [uint64]$chunkSize) { throw "HGDL chunk $index has invalid reconstructed size $size64." }
            if ($written + $size64 -gt $targetSize64) { throw "HGDL chunk $index exceeds reconstructed target size." }
            $size = [int]$size64
            if ($kind -eq 0) {
                if ($entryPayloadOffset -ne 0 -or $entryPayloadSize -ne 0) { throw "HGDL base chunk $index contains replacement payload metadata." }
                $baseOffset = [uint64]$index * [uint64]$chunkSize
                if ($baseOffset + $size64 -gt [uint64]$base.Length) { throw "HGDL base chunk $index exceeds base file bounds." }
                $result.Write($base, [int]$baseOffset, $size)
            } elseif ($kind -eq 1) {
                if ($entryPayloadSize -ne $size64) { throw "HGDL replacement size mismatch at chunk $index." }
                if ($payloadOffset + $entryPayloadOffset -lt $payloadOffset -or $payloadOffset + $entryPayloadOffset + $size64 -gt [uint64]$delta.Length) { throw "HGDL replacement chunk $index exceeds payload bounds." }
                $sourceOffset = $payloadOffset + $entryPayloadOffset
                $result.Write($delta, [int]$sourceOffset, $size)
            } else {
                throw "HGDL contains unsupported chunk kind $kind at chunk $index."
            }
            $written += $size64
        }
        if ($written -ne $targetSize64 -or $result.Length -ne $targetSize) { throw "HGDL reconstructed size mismatch: expected $targetSize64, wrote $written." }
        $reconstructed = $result.ToArray()
        $actualTargetHash = (Get-FileHash -InputStream ([IO.MemoryStream]::new($reconstructed)) -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($targetHash -cne $actualTargetHash) { throw "HGDL reconstructed SHA mismatch: header $targetHash, actual $actualTargetHash." }
        return ,$reconstructed
    }
    finally {
        $result.Dispose()
    }
}

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}
if ([string]::IsNullOrWhiteSpace($BuilderRoot)) {
    $BuilderRoot = Join-Path $BatmanRoot 'builder'
} elseif ([IO.Path]::IsPathRooted($BuilderRoot)) {
    $BuilderRoot = [IO.Path]::GetFullPath($BuilderRoot)
} elseif (Test-Path -LiteralPath $BuilderRoot) {
    $BuilderRoot = (Resolve-Path $BuilderRoot).Path
} else {
    $BuilderRoot = [IO.Path]::GetFullPath((Join-Path $BatmanRoot $BuilderRoot))
}

$packRoot = if ([string]::IsNullOrWhiteSpace($PackRootOverride)) { Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-graphics-options' } else { [IO.Path]::GetFullPath($PackRootOverride) }
$buildRoot = Join-Path $packRoot 'builds\steam-goty-1.0'
$packJsonPath = Join-Path $packRoot 'pack.json'
$buildJsonPath = Join-Path $buildRoot 'build.json'
$bindingsJsonPath = Join-Path $buildRoot 'bindings.json'
$commandsJsonPath = Join-Path $buildRoot 'commands.json'
$filesJsonPath = Join-Path $buildRoot 'files.json'
$deltaPath = Join-Path $buildRoot 'assets\deltas\Frontend-graphics-options.hgdelta'
$hooksJsonPath = Join-Path $buildRoot 'hooks.json'
$texturesJsonPath = Join-Path $buildRoot 'textures.json'
$basePath = Join-Path $BuilderRoot 'extracted\frontend-retail\Frontend.umap'
$targetPath = if ([string]::IsNullOrWhiteSpace($TargetPathOverride)) { Join-Path $BuilderRoot 'generated\graphics-options-experiment\Frontend-graphics-options.umap' } else { [IO.Path]::GetFullPath($TargetPathOverride) }
$ffdecPath = Join-Path $BuilderRoot 'extracted\ffdec\ffdec-cli.exe'
$patcherProjectPath = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\BmGameGfxPatcher\BmGameGfxPatcher.csproj'

foreach ($requiredPath in @($packJsonPath, $buildJsonPath, $bindingsJsonPath, $commandsJsonPath, $filesJsonPath, $deltaPath, $basePath, $targetPath, $ffdecPath, $patcherProjectPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Batman graphics-options package input not found: $requiredPath" }
}
Assert-RebuildAtomicSourceContract -ScriptPath (Join-Path $PSScriptRoot 'Rebuild-BatmanGraphicsOptionsExperiment.ps1')
Assert-ExactPackFileSet -Root $packRoot -BuildDirectoryName 'steam-goty-1.0'
if (Test-Path -LiteralPath $hooksJsonPath) { throw 'Graphics-options shell must not contain hooks.json.' }
if (Test-Path -LiteralPath $texturesJsonPath) { throw 'Graphics-options shell must not contain textures.json.' }

$pack = Get-Content -LiteralPath $packJsonPath -Raw | ConvertFrom-Json
Assert-ExactProperties -Object $pack -Names @('schemaVersion', 'id', 'name', 'targets', 'builds') -Context 'pack.json'
if ($pack.schemaVersion -ne 1 -or $pack.id -ne 'batman-aa-graphics-options' -or $pack.name -ne 'Batman Graphics Options Shell') { throw 'pack.json shell identity drifted.' }
if (@($pack.targets).Count -ne 1 -or $pack.targets[0].gameId -ne 'batman-arkham-asylum' -or @($pack.targets[0].executables).Count -ne 1 -or $pack.targets[0].executables[0] -ne 'ShippingPC-BmGame.exe') { throw 'pack.json target executable drifted.' }
if (@($pack.builds).Count -ne 1 -or $pack.builds[0] -ne 'steam-goty-1.0') { throw 'pack.json build list drifted.' }

$build = Get-Content -LiteralPath $buildJsonPath -Raw | ConvertFrom-Json
Assert-ExactProperties -Object $build -Names @('id', 'executable', 'match') -Context 'build.json'
$expectedMatch = & (Join-Path $PSScriptRoot 'Get-BatmanSteamBuildMatch.ps1')
if ($build.id -ne $expectedMatch.BuildId -or $build.executable -ne $expectedMatch.Executable -or $build.match.fileSize -ne $expectedMatch.FileSize -or $build.match.sha256 -cne $expectedMatch.Sha256) { throw 'build.json retail executable identity drifted.' }
Assert-ExactProperties -Object $build.match -Names @('fileSize', 'sha256') -Context 'build.json match'

$bindings = Get-Content -LiteralPath $bindingsJsonPath -Raw | ConvertFrom-Json
Assert-ExactProperties -Object $bindings -Names @('bindings') -Context 'bindings.json'
if (@($bindings.bindings).Count -ne 0) { throw 'bindings.json must contain zero bindings.' }
$commands = Get-Content -LiteralPath $commandsJsonPath -Raw | ConvertFrom-Json
Assert-ExactProperties -Object $commands -Names @('commands') -Context 'commands.json'
if (@($commands.commands).Count -ne 0) { throw 'commands.json must contain zero commands.' }

$files = Get-Content -LiteralPath $filesJsonPath -Raw | ConvertFrom-Json
Assert-ExactProperties -Object $files -Names @('virtualFiles') -Context 'files.json'
if (@($files.virtualFiles).Count -ne 1) { throw 'files.json must contain exactly one virtual file.' }
Assert-HgdeltaVirtualFileContract -Context 'graphics-options shell' -VirtualFile $files.virtualFiles[0] -ExpectedId 'frontendGraphicsOptionsPackage' -ExpectedPath 'BmGame/CookedPC/Maps/Frontend/Frontend.umap' -ExpectedMode 'delta-on-read' -ExpectedKind 'delta-file' -ExpectedDeltaRelativePath 'assets/deltas/Frontend-graphics-options.hgdelta' -BasePath $basePath -TargetPath $targetPath -DeltaFilePath $deltaPath -ChunkSize 65536 -ChunkTableOffset 116

$baseInfo = Get-Item -LiteralPath $basePath
$baseHash = (Get-FileHash -LiteralPath $basePath -Algorithm SHA256).Hash
if ($baseInfo.Length -ne 2988548 -or $baseHash -cne '271916B888F83374122AF0FCCC5C685804F4C8286A92A772CD71E4F48A00F2CC') { throw 'Retail Frontend.umap base identity is not the verified retail input.' }
$targetInfo = Get-UnrealPackageStorageInfo -Path $targetPath
if ($targetInfo.CompressionChunkCount -le 0) { throw 'Generated shell target must retain chunk-compressed Unreal storage.' }

$verificationRoot = Join-Path ([IO.Path]::GetTempPath()) ('HelenBatmanGraphicsPackageVerification-' + [Guid]::NewGuid().ToString('N'))
$extractedGfxPath = Join-Path $verificationRoot 'MainV2.gfx'
$xmlPath = Join-Path $verificationRoot 'MainV2.xml'
$exportRoot = Join-Path $verificationRoot 'export'
New-Item -ItemType Directory -Force -Path $verificationRoot | Out-Null
try {
    $extract = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('run', '--project', $patcherProjectPath, '-c', $Configuration, '--', 'extract-gfx', '--package', $targetPath, '--owner', 'MainMenu', '--name', 'MainV2', '--output', $extractedGfxPath)
    if ($extract.ExitCode -ne 0) { throw "Failed to extract current-run MainV2: $($extract.Output -join [Environment]::NewLine)" }
    $xml = Invoke-ExternalProcess -FilePath $ffdecPath -Arguments @('-swf2xml', $extractedGfxPath, $xmlPath)
    if ($xml.ExitCode -ne 0) { throw 'FFDec failed to reopen the generated MainV2 shell.' }
    $export = Invoke-ExternalProcess -FilePath $ffdecPath -Arguments @('-export', 'script', $exportRoot, $extractedGfxPath)
    if ($export.ExitCode -ne 0) { throw 'FFDec failed to export the generated MainV2 shell scripts.' }

    [xml]$document = Get-Content -LiteralPath $xmlPath -Raw
    $sprites600 = @($document.SelectNodes("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='600']"))
    if ($sprites600.Count -ne 1) { throw "Expected exactly one DefineSprite_600_ScreenOptionsGraphics sprite, found $($sprites600.Count)." }
    if (@($document.SelectNodes("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='601']")).Count -ne 0) { throw 'Shell target must not contain sprite 601.' }
    $exports = @(@($document.SelectNodes("/swf/tags/item[@type='ExportAssetsTag']")) | Where-Object { @($_.names.item | Where-Object { $_ -eq 'ScreenOptionsGraphics' }).Count -gt 0 })
    if ($exports.Count -ne 1 -or @($exports[0].tags.item | Where-Object { $_ -eq '600' }).Count -ne 1) { throw 'Shell target must export exactly sprite 600 as ScreenOptionsGraphics.' }

    Assert-ScopedExportedShellContract -ExportRoot $exportRoot
    $scriptFiles = @(Get-ChildItem -LiteralPath (Join-Path $exportRoot 'scripts') -Recurse -Filter *.as -File)
    foreach ($forbidden in @('Helen_', 'ApplyChanges', 'GraphicsExitPrompt', 'loadBatmanGraphicsDraftIntoConfig', 'applyBatmanGraphicsDraft', 'Unsaved graphics changes', 'Some changes require a restart')) {
        foreach ($scriptFile in $scriptFiles) { Assert-NotContainsOrdinal -Text (Get-Content -LiteralPath $scriptFile.FullName -Raw) -Token $forbidden -Context "exported script $($scriptFile.Name)" }
    }

    $reconstructed = Reconstruct-HgdeltaTarget -BasePath $basePath -DeltaPath $deltaPath
    $targetBytes = [IO.File]::ReadAllBytes($targetPath)
    if ($reconstructed.Length -ne $targetBytes.Length -or -not [Linq.Enumerable]::SequenceEqual($reconstructed, $targetBytes)) { throw 'HGDL delta reconstruction does not match the current-run target byte-for-byte.' }
    $deltaBytes = [IO.File]::ReadAllBytes($deltaPath)
    $invalidDeltaPath = Join-Path $verificationRoot 'invalid.hgdelta'
    [byte[]]$badBytes = $deltaBytes.Clone()
    $badBytes[0] = [byte][char]'X'
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'header magic'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt32Fixture -Bytes $badBytes -Offset 4 -Value 2
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'version'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt32Fixture -Bytes $badBytes -Offset 12 -Value 1
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'chunk size'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt32Fixture -Bytes $badBytes -Offset 96 -Value 0
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'chunk count'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt64Fixture -Bytes $badBytes -Offset 100 -Value 115
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'chunk-table bounds'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt64Fixture -Bytes $badBytes -Offset 108 -Value ([uint64]$deltaBytes.Length + 1)
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'payload bounds'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt32Fixture -Bytes $badBytes -Offset 116 -Value 9
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'unsupported chunk kind'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt32Fixture -Bytes $badBytes -Offset 120 -Value 65537
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'chunk size/count bounds'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt64Fixture -Bytes $badBytes -Offset 124 -Value ([uint64]$deltaBytes.Length)
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'replacement payload bounds'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt32Fixture -Bytes $badBytes -Offset 116 -Value 1
    Set-HgdeltaUInt64Fixture -Bytes $badBytes -Offset 124 -Value ([uint64]::MaxValue)
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'chunk offset overflow'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt32Fixture -Bytes $badBytes -Offset 116 -Value 0
    Set-HgdeltaUInt32Fixture -Bytes $badBytes -Offset 120 -Value 65535
    Set-HgdeltaUInt64Fixture -Bytes $badBytes -Offset 124 -Value 0
    Set-HgdeltaUInt32Fixture -Bytes $badBytes -Offset 132 -Value 0
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'reconstructed size'
    [byte[]]$badBytes = $deltaBytes.Clone()
    $badBytes[64] = $badBytes[64] -bxor 255
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'reconstructed SHA'
}
finally {
    if (Test-Path -LiteralPath $verificationRoot) { Remove-Item -LiteralPath $verificationRoot -Recurse -Force }
}

Write-Output 'PASS'
