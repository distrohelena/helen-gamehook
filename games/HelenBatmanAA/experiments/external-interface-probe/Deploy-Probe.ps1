param([Parameter(Mandatory = $true)][string]$PackageRoot)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$gameBin = 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries'
. (Join-Path $repoRoot 'games\HelenBatmanAA\scripts\Deploy-BatmanGraphicsOptionsExperiment.ps1') -GameBin $gameBin -FunctionsOnly
if (Get-Process ShippingPC-BmGame -ErrorAction SilentlyContinue) { throw 'Close Batman before installing the probe.' }
$packageParent = Assert-SafeDeploymentPath -Path $PackageRoot -AllowedRoots @((Join-Path $repoRoot 'output\batman-direct-probe')) -RequireExisting
$sourcePack = Join-Path $packageParent 'batman-aa-graphics-options'
$sourceDll = Join-Path $repoRoot 'output\batman-direct-probe\bin\HelenGameHook.dll'
& (Join-Path $PSScriptRoot 'Inspect-Dispatch.ps1') -ExecutablePath (Join-Path $gameBin 'ShippingPC-BmGame.exe')
& (Join-Path $repoRoot 'output\batman-direct-probe\verify\VerifyPackage.exe') $packageParent (Join-Path $repoRoot 'games\HelenBatmanAA\builder\extracted\frontend-retail\Frontend.umap')
if ($LASTEXITCODE -ne 0) { throw 'Probe package failed native validation.' }
$livePack = Join-Path $gameBin 'helengamehook\packs\batman-aa-graphics-options'
$liveConfig = Join-Path $gameBin 'helengamehook\config\packs.json'
$liveDll = Join-Path $gameBin 'HelenGameHook.dll'
$liveProxy = Join-Path $gameBin 'dinput8.dll'
$subtitleRoot = Join-Path $gameBin 'helengamehook\packs\batman-aa-subtitles'
Assert-BatmanGraphicsPackConfig -Path $liveConfig -Context 'Pre-probe'
$subtitleSnapshot = @(Get-DirectorySnapshot -Root $subtitleRoot)
$sourceSnapshot = @(Get-DirectorySnapshot -Root $sourcePack)
$dllHash = (Get-FileHash -LiteralPath $sourceDll).Hash
$proxyHash = (Get-FileHash -LiteralPath $liveProxy).Hash
$configHash = (Get-FileHash -LiteralPath $liveConfig).Hash
$backupRoot = Join-Path $repoRoot ('output\batman-direct-probe\rollback-' + [Guid]::NewGuid().ToString('N'))
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
    Assert-DirectorySnapshotEqual -Expected $sourceSnapshot -ActualRoot $pack -Context 'Direct probe pack'
    Assert-DirectorySnapshotEqual -Expected $subtitleSnapshot -ActualRoot $subtitleRoot -Context 'Unchanged subtitles'
    if ((Get-FileHash -LiteralPath $dll).Hash -ne $dllHash) { throw 'Probe DLL hash mismatch.' }
    if ((Get-FileHash -LiteralPath $proxy).Hash -ne $proxyHash) { throw 'Proxy changed unexpectedly.' }
    if ((Get-FileHash -LiteralPath $config).Hash -ne $configHash) { throw 'Enabled pack configuration changed.' }
}
if (Get-Process ShippingPC-BmGame -ErrorAction SilentlyContinue) { throw 'Batman started during staging; installation cancelled.' }
Invoke-AtomicGraphicsDeployment -GameBin $gameBin -LivePackRoot $livePack -LiveConfigPath $liveConfig -LiveHelenGameHookPath $liveDll -LiveProxyPath $liveProxy -StagedPackRoot $stagePack -StagedConfigPath (Join-Path $stageRoot 'packs.json') -StagedHelenGameHookPath (Join-Path $stageRoot 'HelenGameHook.dll') -StagedProxyPath (Join-Path $stageRoot 'dinput8.dll') -PackBackupRoot (Join-Path $recoveryRoot 'pack') -ConfigBackupPath (Join-Path $recoveryRoot 'packs.json') -HelenGameHookBackupPath (Join-Path $recoveryRoot 'HelenGameHook.dll') -ProxyBackupPath (Join-Path $recoveryRoot 'dinput8.dll') -RecoveryRoot $recoveryRoot -StagingRoot $stageRoot -VerifyStagedPublication $verify -VerifyPublication $verify
& $verify $livePack $liveConfig $liveDll $liveProxy
if (@(Get-ChildItem -LiteralPath $stageRoot -Force).Count -ne 0) { throw 'Unexpected files remain in committed probe staging.' }
Remove-Item -LiteralPath $stageRoot
Write-Output 'DIRECT_PROBE_DEPLOYED_AND_HASH_VERIFIED'
