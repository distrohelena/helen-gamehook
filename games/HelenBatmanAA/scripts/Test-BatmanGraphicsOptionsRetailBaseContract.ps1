param(
    [string]$BatmanRoot,
    [string]$BuilderRoot
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'BatmanPackVerificationHelpers.ps1')

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) { $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path } else { $BatmanRoot = (Resolve-Path $BatmanRoot).Path }
if ([string]::IsNullOrWhiteSpace($BuilderRoot)) { $BuilderRoot = Join-Path $BatmanRoot 'builder' } elseif ([IO.Path]::IsPathRooted($BuilderRoot)) { $BuilderRoot = [IO.Path]::GetFullPath($BuilderRoot) } elseif (Test-Path -LiteralPath $BuilderRoot) { $BuilderRoot = (Resolve-Path $BuilderRoot).Path } else { $BuilderRoot = [IO.Path]::GetFullPath((Join-Path $BatmanRoot $BuilderRoot)) }

$basePath = Join-Path $BuilderRoot 'extracted\frontend-retail\Frontend.umap'
if (-not (Test-Path -LiteralPath $basePath)) { throw "Verified retail base was not found: $basePath" }
$baseInfo = Get-Item -LiteralPath $basePath
$baseHash = (Get-FileHash -LiteralPath $basePath -Algorithm SHA256).Hash
if ($baseInfo.Length -ne 2988548 -or $baseHash -cne '271916B888F83374122AF0FCCC5C685804F4C8286A92A772CD71E4F48A00F2CC') { throw 'Frontend.umap is not the verified 2988548-byte retail base.' }

$match = & (Join-Path $PSScriptRoot 'Get-BatmanSteamBuildMatch.ps1')
if ($match.BuildId -ne 'steam-goty-1.0' -or $match.Executable -ne 'ShippingPC-BmGame.exe' -or $match.FileSize -ne 38758728 -or $match.Sha256 -cne '4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028') { throw 'Steam GOTY executable build identity is not the verified retail identity.' }

$buildRoot = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-graphics-options\builds\steam-goty-1.0'
$filesPath = Join-Path $buildRoot 'files.json'
$deltaPath = Join-Path $buildRoot 'assets\deltas\Frontend-graphics-options.hgdelta'
$targetPath = Join-Path $BuilderRoot 'generated\graphics-options-experiment\Frontend-graphics-options.umap'
if (-not (Test-Path -LiteralPath $filesPath)) { throw "Graphics shell files manifest was not found: $filesPath" }
$manifest = Get-Content -LiteralPath $filesPath -Raw | ConvertFrom-Json
if (@($manifest.virtualFiles).Count -ne 1) { throw 'Graphics shell must contain exactly one virtual file.' }
Assert-HgdeltaVirtualFileContract -Context 'graphics-options retail base' -VirtualFile $manifest.virtualFiles[0] -ExpectedId 'frontendGraphicsOptionsPackage' -ExpectedPath 'BmGame/CookedPC/Maps/Frontend/Frontend.umap' -ExpectedMode 'delta-on-read' -ExpectedKind 'delta-file' -ExpectedDeltaRelativePath 'assets/deltas/Frontend-graphics-options.hgdelta' -BasePath $basePath -TargetPath $targetPath -DeltaFilePath $deltaPath -ChunkSize 65536 -ChunkTableOffset 116

Write-Output 'PASS'
