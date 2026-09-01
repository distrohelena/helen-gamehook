param(
    [string]$BatmanRoot,
    [string]$BuilderRoot,
    [string]$Configuration = 'Release'
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
    return [BitConverter]::ToUInt64($Bytes, $Offset)
}

function Reconstruct-HgdeltaTarget {
    param(
        [Parameter(Mandatory = $true)] [string]$BasePath,
        [Parameter(Mandatory = $true)] [string]$DeltaPath
    )

    $base = [System.IO.File]::ReadAllBytes($BasePath)
    $delta = [System.IO.File]::ReadAllBytes($DeltaPath)
    if ($delta.Length -lt 116 -or [Text.Encoding]::ASCII.GetString($delta, 0, 4) -cne 'HGDL') { throw "Invalid HGDL delta: $DeltaPath" }
    $chunkCount = [int](Read-UInt32LittleEndian $delta 96)
    $chunkTableOffset = [int](Read-UInt64LittleEndian $delta 100)
    $payloadOffset = [int](Read-UInt64LittleEndian $delta 108)
    $targetSize = [int](Read-UInt64LittleEndian $delta 24)
    $result = [System.IO.MemoryStream]::new($targetSize)
    try {
        for ($index = 0; $index -lt $chunkCount; $index++) {
            $entry = $chunkTableOffset + ($index * 20)
            $kind = Read-UInt32LittleEndian $delta $entry
            $size = [int](Read-UInt32LittleEndian $delta ($entry + 4))
            $entryPayloadOffset = [int](Read-UInt64LittleEndian $delta ($entry + 8))
            $entryPayloadSize = [int](Read-UInt32LittleEndian $delta ($entry + 16))
            if ($kind -eq 0) {
                if ($size -gt 0) { $result.Write($base, $index * 65536, $size) }
            } elseif ($kind -eq 1) {
                if ($entryPayloadSize -ne $size) { throw "HGDL replacement size mismatch at chunk $index." }
                if ($size -gt 0) { $result.Write($delta, $payloadOffset + $entryPayloadOffset, $size) }
            } else {
                throw "HGDL contains unsupported chunk kind $kind at chunk $index."
            }
        }
        return ,$result.ToArray()
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

$packRoot = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-graphics-options'
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
$targetPath = Join-Path $BuilderRoot 'generated\graphics-options-experiment\Frontend-graphics-options.umap'
$ffdecPath = Join-Path $BuilderRoot 'extracted\ffdec\ffdec-cli.exe'
$patcherProjectPath = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\BmGameGfxPatcher\BmGameGfxPatcher.csproj'

foreach ($requiredPath in @($packJsonPath, $buildJsonPath, $bindingsJsonPath, $commandsJsonPath, $filesJsonPath, $deltaPath, $basePath, $targetPath, $ffdecPath, $patcherProjectPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Batman graphics-options package input not found: $requiredPath" }
}
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

    $scriptFiles = @(Get-ChildItem -LiteralPath (Join-Path $exportRoot 'scripts') -Recurse -Filter *.as -File)
    if ($scriptFiles.Count -eq 0) { throw 'FFDec exported no ActionScript files.' }
    $screenScripts = @($scriptFiles | Where-Object { $_.FullName -like '*DefineSprite_600_ScreenOptionsGraphics*' })
    $screenScript = (($screenScripts | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join [Environment]::NewLine)
    $allScriptText = (($scriptFiles | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join [Environment]::NewLine)
    foreach ($token in @('Graphics Options', 'GotoScreen("OptionsGraphics")', 'CancelScreen', 'ReturnFromScreen', 'Not active')) { Assert-ContainsOrdinal -Text $allScriptText -Token $token -Context 'exported graphics shell' }
    foreach ($forbidden in @('Helen_', 'ApplyChanges', 'GraphicsExitPrompt', 'loadBatmanGraphicsDraftIntoConfig', 'applyBatmanGraphicsDraft', 'Unsaved graphics changes', 'Some changes require a restart')) {
        foreach ($scriptFile in $scriptFiles) { Assert-NotContainsOrdinal -Text (Get-Content -LiteralPath $scriptFile.FullName -Raw) -Token $forbidden -Context "exported script $($scriptFile.Name)" }
    }

    $reconstructed = Reconstruct-HgdeltaTarget -BasePath $basePath -DeltaPath $deltaPath
    $targetBytes = [IO.File]::ReadAllBytes($targetPath)
    if ($reconstructed.Length -ne $targetBytes.Length -or -not [Linq.Enumerable]::SequenceEqual($reconstructed, $targetBytes)) { throw 'HGDL delta reconstruction does not match the current-run target byte-for-byte.' }
}
finally {
    if (Test-Path -LiteralPath $verificationRoot) { Remove-Item -LiteralPath $verificationRoot -Recurse -Force }
}

Write-Output 'PASS'
