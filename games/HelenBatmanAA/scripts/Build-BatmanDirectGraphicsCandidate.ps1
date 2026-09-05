param(
    [Parameter(Mandatory = $true)][string]$NativeDllPath,
    [Parameter(Mandatory = $true)][string]$RuntimeLibraryPath,
    [Parameter(Mandatory = $true)][string]$BatmanUserIniPath
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$builderRoot = Join-Path $repoRoot 'games\HelenBatmanAA\builder'
$candidateId = [Guid]::NewGuid().ToString('N')
$candidateRoot = Join-Path $repoRoot ('output\batman-direct-graphics\candidate-' + $candidateId)
$assetRoot = Join-Path $builderRoot ('generated\direct-graphics-' + $candidateId)
$packParent = Join-Path $candidateRoot 'packs'
$packRoot = Join-Path $packParent 'batman-aa-graphics-options'
$buildRoot = Join-Path $packRoot 'builds\steam-goty-1.0'
$basePath = Join-Path $builderRoot 'extracted\frontend-retail\Frontend.umap'
$targetPath = Join-Path $candidateRoot 'Frontend-direct-graphics.umap'
$consoleTool = Join-Path $PSScriptRoot 'Invoke-BatmanConsoleTool.ps1'
$builderProject = Join-Path $builderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj'
$patcherProject = Join-Path $builderRoot 'tools\NativeSubtitleExePatcher\BmGameGfxPatcher\BmGameGfxPatcher.csproj'
$builderDll = Join-Path (Split-Path -Parent $builderProject) 'bin\Release\net8.0\SubtitleSizeModBuilder.dll'
$patcherDll = Join-Path (Split-Path -Parent $patcherProject) 'bin\Release\net8.0\BmGameGfxPatcher.dll'
$ffdec = Join-Path $builderRoot 'extracted\ffdec\ffdec-cli.exe'

function Write-CandidateJson {
    <# Writes only newly constructed candidate metadata; never imports another pack's declarations. #>
    param([string]$Path, [object]$Value)
    [IO.File]::WriteAllText($Path, (ConvertTo-Json -InputObject $Value -Depth 12), [Text.UTF8Encoding]::new($false))
}

if ((Get-Item -LiteralPath $basePath).Length -ne 2988548 -or
    (Get-FileHash -LiteralPath $basePath).Hash -cne '271916B888F83374122AF0FCCC5C685804F4C8286A92A772CD71E4F48A00F2CC') {
    throw 'Candidate requires the verified retail Frontend.umap.'
}
foreach ($required in @($NativeDllPath, $RuntimeLibraryPath, $BatmanUserIniPath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required candidate input missing: $required" }
}
New-Item -ItemType Directory -Path (Join-Path $buildRoot 'assets\native') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $buildRoot 'assets\deltas') | Out-Null
Write-Output "CANDIDATE_ROOT: $candidateRoot"
# Each run compiles source and imports scripts into a new output tree. No previous GFX or pack is an input.
& $consoleTool -FilePath dotnet -Arguments @('build', $builderProject, '-c', 'Release', '--disable-build-servers', '-nr:false', '-p:UseSharedCompilation=false')
& $consoleTool -FilePath dotnet -Arguments @('build', $patcherProject, '-c', 'Release', '--disable-build-servers', '-nr:false', '-p:UseSharedCompilation=false')
& $consoleTool -FilePath dotnet -Arguments @($builderDll, 'build-main-menu-graphics-shell', '--root', $builderRoot, '--output-dir', $assetRoot, '--ffdec', $ffdec, '--ini', $BatmanUserIniPath)
$gfxPath = Join-Path $assetRoot 'MainV2-graphics-options.gfx'
$emittedController = Join-Path $assetRoot '_build\frontend-scripts\DefineSprite_600_ScreenOptionsGraphics\frame_1\DoAction.as'
& $consoleTool -FilePath node -Arguments @((Join-Path $PSScriptRoot 'Test-BatmanDirectGraphicsFrontend.js'), $emittedController)
$decompiledRoot = Join-Path $assetRoot 'decompiled'
& $consoleTool -FilePath $ffdec -Arguments @('-export', 'script', $decompiledRoot, $gfxPath) | Out-Null
$decompiledController = Join-Path $decompiledRoot 'scripts\DefineSprite_600_ScreenOptionsGraphics\frame_1\DoAction.as'
& $consoleTool -FilePath node -Arguments @((Join-Path $PSScriptRoot 'Test-BatmanDirectGraphicsFrontend.js'), $decompiledController)
$patchManifest = Join-Path $candidateRoot 'patch.json'
Write-CandidateJson $patchManifest ([ordered]@{ name = 'Direct graphics transactions'; patches = @([ordered]@{
    owner = 'MainMenu'; exportName = 'MainV2'; exportType = 'GFxMovieInfo'; replacementPath = $gfxPath; payloadMagic = 'GFX'
}) })
& $consoleTool -FilePath dotnet -Arguments @($patcherDll, 'patch', '--package', $basePath, '--manifest', $patchManifest, '--output', $targetPath)
$deltaPath = Join-Path $buildRoot 'assets\deltas\Frontend-graphics-options.hgdelta'
$delta = & (Join-Path $PSScriptRoot 'Build-Hgdelta.ps1') -BaseFile $basePath -TargetFile $targetPath -OutputFile $deltaPath -ChunkSize 65536
Write-CandidateJson (Join-Path $packRoot 'pack.json') ([ordered]@{
    schemaVersion = 1; id = 'batman-aa-graphics-options'; name = 'Batman Graphics Options'
    targets = @([ordered]@{ gameId = 'batman-arkham-asylum'; executables = @('ShippingPC-BmGame.exe') })
    config = @(); builds = @('steam-goty-1.0')
})
Write-CandidateJson (Join-Path $buildRoot 'build.json') ([ordered]@{
    id = 'steam-goty-1.0'; executable = 'ShippingPC-BmGame.exe'
    match = [ordered]@{ fileSize = 38758728; sha256 = '4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028' }
    startupCommands = @()
})
Write-CandidateJson (Join-Path $buildRoot 'bindings.json') ([ordered]@{ bindings = @() })
Write-CandidateJson (Join-Path $buildRoot 'commands.json') ([ordered]@{ commands = @() })
Write-CandidateJson (Join-Path $buildRoot 'hooks.json') ([ordered]@{
    runtimeSlots = @(); stateObservers = @(); hooks = @([ordered]@{
        id = 'directGraphicsDispatch'; module = 'ShippingPC-BmGame.exe'; rva = '0x015FD9D0'; expectedBytes = '8B118B5204'
        action = 'inline-jump-to-pack-blob'; overwriteLength = 5; resumeOffsetFromTarget = 5
        blob = [ordered]@{ assetPath = 'assets/native/direct-dispatch.bin'; entryOffset = 0; relocations = @(
            [ordered]@{ offset = 1; encoding = 'abs32'; source = [ordered]@{ kind = 'module-export'; module = 'HelenGameHook.dll'; export = '@HelenGraphicsDispatch@24' } },
            [ordered]@{ offset = 6; encoding = 'rel32'; source = [ordered]@{ kind = 'hook-resume' } }
        ) }
    })
})
[IO.File]::WriteAllBytes((Join-Path $buildRoot 'assets\native\direct-dispatch.bin'), [byte[]]@(0xBA,0,0,0,0,0xE9,0,0,0,0))
Write-CandidateJson (Join-Path $buildRoot 'files.json') ([ordered]@{ virtualFiles = @([ordered]@{
    id = 'frontendGraphicsOptionsPackage'; path = 'BmGame/CookedPC/Maps/Frontend/Frontend.umap'; mode = 'delta-on-read'
    source = [ordered]@{ kind = 'delta-file'; path = 'assets/deltas/Frontend-graphics-options.hgdelta'
        base = [ordered]@{ size = $delta.BaseSize; sha256 = $delta.BaseSha256 }
        target = [ordered]@{ size = $delta.TargetSize; sha256 = $delta.TargetSha256 }; chunkSize = 65536
    }
}) })
$candidateDll = Join-Path $candidateRoot 'HelenGameHook.dll'
Copy-Item -LiteralPath $NativeDllPath -Destination $candidateDll
& (Join-Path $PSScriptRoot 'Test-BatmanDirectGraphicsPackage.ps1') -PackParent $packParent -BasePath $basePath -TargetPath $targetPath -NativeDllPath $candidateDll -RuntimeLibraryPath $RuntimeLibraryPath
Write-CandidateJson (Join-Path $candidateRoot 'provenance.json') ([ordered]@{
    nativeInput = [IO.Path]::GetFullPath($NativeDllPath); nativeSha256 = (Get-FileHash -LiteralPath $candidateDll).Hash
    runtimeLibrary = [IO.Path]::GetFullPath($RuntimeLibraryPath); runtimeSha256 = (Get-FileHash -LiteralPath $RuntimeLibraryPath).Hash
    retailBaseSha256 = (Get-FileHash -LiteralPath $basePath).Hash; gfxSha256 = (Get-FileHash -LiteralPath $gfxPath).Hash
    targetSha256 = (Get-FileHash -LiteralPath $targetPath).Hash; deltaSha256 = (Get-FileHash -LiteralPath $deltaPath).Hash
    templateSha256 = (Get-FileHash -LiteralPath (Join-Path $builderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\GraphicsOptionsShellScriptTemplates.cs')).Hash
})
Write-Output "VERIFIED_DIRECT_GRAPHICS_CANDIDATE: $candidateRoot"
