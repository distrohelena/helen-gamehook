param()
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$batmanRoot = Join-Path $repoRoot 'games\HelenBatmanAA'
$builderRoot = Join-Path $batmanRoot 'builder'
$gfxPath = Join-Path $builderRoot 'generated\external-interface-probe\MainV2-direct-probe.gfx'
$sourcePack = Join-Path $batmanRoot 'helengamehook\packs\batman-aa-graphics-options'
$outputRoot = Join-Path $repoRoot ('output\batman-direct-probe\package-' + [Guid]::NewGuid().ToString('N'))
$packRoot = Join-Path $outputRoot 'batman-aa-graphics-options'
$buildRoot = Join-Path $packRoot 'builds\steam-goty-1.0'
$basePath = Join-Path $builderRoot 'extracted\frontend-retail\Frontend.umap'
if ((Get-FileHash -LiteralPath $basePath).Hash -ne '271916B888F83374122AF0FCCC5C685804F4C8286A92A772CD71E4F48A00F2CC') { throw 'Unverified retail frontend.' }
& (Join-Path $PSScriptRoot 'Inspect-Dispatch.ps1')
New-Item -ItemType Directory -Path (Join-Path $buildRoot 'assets\native') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $buildRoot 'assets\deltas') -Force | Out-Null
# Copy declarative source metadata only. No installed files or previous delta are build inputs.
foreach ($source in Get-ChildItem -LiteralPath $sourcePack -File -Recurse -Filter '*.json') {
    $relative = $source.FullName.Substring($sourcePack.Length + 1)
    Copy-Item -LiteralPath $source.FullName -Destination (Join-Path $packRoot $relative)
}
$manifestPath = Join-Path $outputRoot 'patch.json'
$manifest = @{name='Isolated direct Fullscreen probe'; patches=@(@{owner='MainMenu'; exportName='MainV2'; exportType='GFxMovieInfo'; replacementPath=$gfxPath; payloadMagic='GFX'})}
[IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
$targetPath = Join-Path $outputRoot 'Frontend-direct-probe.umap'
& dotnet (Join-Path $builderRoot 'tools\NativeSubtitleExePatcher\BmGameGfxPatcher\bin\Release\net8.0\BmGameGfxPatcher.dll') patch --package $basePath --manifest $manifestPath --output $targetPath
if ($LASTEXITCODE -ne 0) { throw 'Probe frontend package patch failed.' }
& (Join-Path $batmanRoot 'scripts\Build-Hgdelta.ps1') -BaseFile $basePath -TargetFile $targetPath -OutputFile (Join-Path $buildRoot 'assets\deltas\Frontend-graphics-options.hgdelta') -ChunkSize 65536 | Out-Null
$filesPath = Join-Path $buildRoot 'files.json'
$files = Get-Content -LiteralPath $filesPath -Raw | ConvertFrom-Json
$files.virtualFiles[0].source.target.size = (Get-Item -LiteralPath $targetPath).Length
$files.virtualFiles[0].source.target.sha256 = (Get-FileHash -LiteralPath $targetPath).Hash.ToLowerInvariant()
[IO.File]::WriteAllText($filesPath, ($files | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
$hooksPath = Join-Path $buildRoot 'hooks.json'
$hooks = Get-Content -LiteralPath $hooksPath -Raw | ConvertFrom-Json
$hooks.stateObservers = @($hooks.stateObservers | Where-Object { $_.id -ne 'graphicsObserverFullscreen' })
# mov edx, callback; jmp resume. The original argument pushes/call and result copy remain in Batman.
[IO.File]::WriteAllBytes((Join-Path $buildRoot 'assets\native\direct-dispatch.bin'), [byte[]]@(0xBA,0,0,0,0,0xE9,0,0,0,0))
$hooks.hooks = @(@{
    id='directFullscreenProbe'; module='ShippingPC-BmGame.exe'; rva='0x015FD9D0'; expectedBytes='8B118B5204';
    action='inline-jump-to-pack-blob'; overwriteLength=5; resumeOffsetFromTarget=5;
    blob=@{assetPath='assets/native/direct-dispatch.bin'; entryOffset=0; relocations=@(
        @{offset=1; encoding='abs32'; source=@{kind='module-export'; module='HelenGameHook.dll'; export='@HelenProbeDispatch@24'}},
        @{offset=6; encoding='rel32'; source=@{kind='hook-resume'}}
    )}
})
[IO.File]::WriteAllText($hooksPath, ($hooks | ConvertTo-Json -Depth 20), [Text.UTF8Encoding]::new($false))
Write-Output "STAGED_PROBE_PACKAGE: $outputRoot"
