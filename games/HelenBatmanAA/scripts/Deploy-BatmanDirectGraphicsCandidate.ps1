param(
    [Parameter(Mandatory = $true)][string]$PackageRoot,
    [Parameter(Mandatory = $true)][string]$GameBin,
    [Parameter(Mandatory = $true)][string]$RuntimeLibraryPath
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
. (Join-Path $repoRoot 'games\HelenBatmanAA\scripts\Deploy-BatmanGraphicsOptionsExperiment.ps1') -GameBin $gameBin -FunctionsOnly
if (Get-Process ShippingPC-BmGame -ErrorAction SilentlyContinue) { throw 'Close Batman before installing the candidate.' }
$packageParent = Assert-SafeDeploymentPath -Path $PackageRoot -AllowedRoots @((Join-Path $repoRoot 'output\batman-direct-graphics')) -RequireExisting
$sourcePack = Join-Path $packageParent 'packs\batman-aa-graphics-options'
$sourceDll = Join-Path $packageParent 'HelenGameHook.dll'
if ((Get-FileHash -LiteralPath (Join-Path $gameBin 'ShippingPC-BmGame.exe')).Hash -cne '4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028') {
    throw 'Refusing to install direct graphics on an unverified executable.'
}
& (Join-Path $repoRoot 'games\HelenBatmanAA\experiments\external-interface-probe\Inspect-Dispatch.ps1') -ExecutablePath (Join-Path $gameBin 'ShippingPC-BmGame.exe')
& (Join-Path $PSScriptRoot 'Test-BatmanDirectGraphicsPackage.ps1') -PackParent (Join-Path $packageParent 'packs') -BasePath (Join-Path $repoRoot 'games\HelenBatmanAA\builder\extracted\frontend-retail\Frontend.umap') -TargetPath (Join-Path $packageParent 'Frontend-direct-graphics.umap') -NativeDllPath $sourceDll -RuntimeLibraryPath $RuntimeLibraryPath
$livePack = Join-Path $gameBin 'helengamehook\packs\batman-aa-graphics-options'
$liveConfig = Join-Path $gameBin 'helengamehook\config\packs.json'
$liveDll = Join-Path $gameBin 'HelenGameHook.dll'
$liveProxy = Join-Path $gameBin 'dinput8.dll'
$subtitleRoot = Join-Path $gameBin 'helengamehook\packs\batman-aa-subtitles'
Assert-BatmanGraphicsPackConfig -Path $liveConfig -Context 'Pre-candidate'
$subtitleSnapshot = @(Get-DirectorySnapshot -Root $subtitleRoot)
$sourceSnapshot = @(Get-DirectorySnapshot -Root $sourcePack)
$dllHash = (Get-FileHash -LiteralPath $sourceDll).Hash
$proxyHash = (Get-FileHash -LiteralPath $liveProxy).Hash
$configHash = (Get-FileHash -LiteralPath $liveConfig).Hash
$backupRoot = Join-Path $repoRoot ('output\batman-direct-graphics\rollback-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $backupRoot | Out-Null
Copy-Item -LiteralPath $livePack -Destination (Join-Path $backupRoot 'batman-aa-graphics-options') -Recurse
Copy-Item -LiteralPath $liveConfig,$liveDll,$liveProxy -Destination $backupRoot
Copy-Item -LiteralPath (Join-Path $gameBin 'helengamehook\logs') -Destination (Join-Path $backupRoot 'logs') -Recurse
Write-Output "ROLLBACK_COPY: $backupRoot"
$stageRoot = Join-Path $gameBin ('.helengamehook-staging-' + [Guid]::NewGuid().ToString('N'))
$recoveryRoot = Join-Path $gameBin ('.helengamehook-recovery-' + [Guid]::NewGuid().ToString('N'))
$stagePack = Join-Path $stageRoot 'pack'
Assert-SafeDeploymentPath -Path $stageRoot -AllowedRoots @($gameBin) | Out-Null
New-Item -ItemType Directory -Path $stagePack -Force | Out-Null
foreach ($item in Get-ChildItem -LiteralPath $sourcePack -Force) { Copy-Item -LiteralPath $item.FullName -Destination $stagePack -Recurse }
Copy-Item -LiteralPath $sourceDll -Destination (Join-Path $stageRoot 'HelenGameHook.dll')
Copy-Item -LiteralPath $liveProxy,$liveConfig -Destination $stageRoot
$verify = {
    param($pack, $config, $dll, $proxy)
    Assert-DirectorySnapshotEqual -Expected $sourceSnapshot -ActualRoot $pack -Context 'Direct candidate pack'
    Assert-DirectorySnapshotEqual -Expected $subtitleSnapshot -ActualRoot $subtitleRoot -Context 'Unchanged subtitles'
    if ((Get-FileHash -LiteralPath $dll).Hash -ne $dllHash) { throw 'Candidate DLL hash mismatch.' }
    if ((Get-FileHash -LiteralPath $proxy).Hash -ne $proxyHash) { throw 'Proxy changed unexpectedly.' }
    if ((Get-FileHash -LiteralPath $config).Hash -ne $configHash) { throw 'Enabled pack configuration changed.' }
}
if (Get-Process ShippingPC-BmGame -ErrorAction SilentlyContinue) { throw 'Batman started during staging; installation cancelled.' }
Invoke-AtomicGraphicsDeployment -GameBin $gameBin -LivePackRoot $livePack -LiveConfigPath $liveConfig -LiveHelenGameHookPath $liveDll -LiveProxyPath $liveProxy -StagedPackRoot $stagePack -StagedConfigPath (Join-Path $stageRoot 'packs.json') -StagedHelenGameHookPath (Join-Path $stageRoot 'HelenGameHook.dll') -StagedProxyPath (Join-Path $stageRoot 'dinput8.dll') -PackBackupRoot (Join-Path $recoveryRoot 'pack') -ConfigBackupPath (Join-Path $recoveryRoot 'packs.json') -HelenGameHookBackupPath (Join-Path $recoveryRoot 'HelenGameHook.dll') -ProxyBackupPath (Join-Path $recoveryRoot 'dinput8.dll') -RecoveryRoot $recoveryRoot -StagingRoot $stageRoot -VerifyStagedPublication $verify -VerifyPublication $verify
& $verify $livePack $liveConfig $liveDll $liveProxy
if (@(Get-ChildItem -LiteralPath $stageRoot -Force).Count -ne 0) { throw 'Unexpected files remain in committed candidate staging.' }
Remove-Item -LiteralPath $stageRoot
Write-Output 'DIRECT_GRAPHICS_CANDIDATE_DEPLOYED_AND_HASH_VERIFIED'
