param(
    [string]$GameBin = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries',
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'

$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$RepoRoot = (Resolve-Path (Join-Path $BatmanRoot '..\..')).Path
$PackSource = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-subtitles'
$PackDestination = Join-Path $GameBin 'helengamehook\packs\batman-aa-subtitles'
$PackBuildRoot = Join-Path $PackDestination 'builds\steam-goty-1.0'
$DeployedFilesJsonPath = Join-Path $PackBuildRoot 'files.json'
$DeployedDeltaPath = Join-Path $PackBuildRoot 'assets\deltas\BmGame-subtitle-signal.hgdelta'
$SourceDeltaPath = Join-Path $PackSource 'builds\steam-goty-1.0\assets\deltas\BmGame-subtitle-signal.hgdelta'
$VerifierPath = Join-Path $PSScriptRoot 'Test-BatmanKnownGoodGameplayPackage.ps1'
$HelenGameHookPath = Join-Path $RepoRoot "bin\Win32\$Configuration\HelenGameHook.dll"
$ProxyPath = Join-Path $RepoRoot "bin\Win32\$Configuration\dinput8.dll"
$ExpectedVirtualFileId = 'bmgameGameplayPackage'
$ExpectedVirtualFilePath = 'BmGame/CookedPC/BmGame.u'
$ExpectedVirtualFileMode = 'delta-on-read'
$ExpectedVirtualFileKind = 'delta-file'
$ExpectedDeltaPath = 'assets/deltas/BmGame-subtitle-signal.hgdelta'
$ExpectedGameplayDeltaHash = (Get-FileHash -LiteralPath $SourceDeltaPath -Algorithm SHA256).Hash

& $VerifierPath -BatmanRoot $BatmanRoot

Get-Process ShippingPC-BmGame -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2

New-Item -ItemType Directory -Force -Path $PackDestination | Out-Null
Copy-Item -LiteralPath $HelenGameHookPath -Destination (Join-Path $GameBin 'HelenGameHook.dll') -Force
Copy-Item -LiteralPath $ProxyPath -Destination (Join-Path $GameBin 'dinput8.dll') -Force
Copy-Item -Path (Join-Path $PackSource '*') -Destination $PackDestination -Recurse -Force
Get-ChildItem (Join-Path $GameBin 'helengamehook\logs') -ErrorAction SilentlyContinue | Remove-Item -Force

if (-not (Test-Path -LiteralPath $DeployedFilesJsonPath)) {
    throw "Batman deployment manifest not found: $DeployedFilesJsonPath"
}

if (-not (Test-Path -LiteralPath $DeployedDeltaPath)) {
    throw "Batman deployment delta not found: $DeployedDeltaPath"
}

$DeployedManifest = Get-Content -LiteralPath $DeployedFilesJsonPath -Raw | ConvertFrom-Json
$VirtualFiles = @($DeployedManifest.virtualFiles)
if ($VirtualFiles.Count -ne 1) {
    throw "Batman deployment manifest expected exactly one virtual file, found $($VirtualFiles.Count)."
}

$GameplayFile = $VirtualFiles[0]
if ($GameplayFile.id -ne $ExpectedVirtualFileId) {
    throw "Batman deployment virtual file id mismatch. Expected $ExpectedVirtualFileId but found $($GameplayFile.id)."
}

if ($GameplayFile.path -ne $ExpectedVirtualFilePath) {
    throw "Batman deployment virtual file path mismatch. Expected $ExpectedVirtualFilePath but found $($GameplayFile.path)."
}

if ($GameplayFile.mode -ne $ExpectedVirtualFileMode) {
    throw "Batman deployment virtual file mode mismatch. Expected $ExpectedVirtualFileMode but found $($GameplayFile.mode)."
}

if ($GameplayFile.source.kind -ne $ExpectedVirtualFileKind) {
    throw "Batman deployment source kind mismatch. Expected $ExpectedVirtualFileKind but found $($GameplayFile.source.kind)."
}

if ($GameplayFile.source.path -ne $ExpectedDeltaPath) {
    throw "Batman deployment delta path mismatch. Expected $ExpectedDeltaPath but found $($GameplayFile.source.path)."
}

$ActualGameplayDeltaHash = (Get-FileHash -LiteralPath $DeployedDeltaPath -Algorithm SHA256).Hash
if (-not [string]::Equals($ActualGameplayDeltaHash, $ExpectedGameplayDeltaHash, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Batman deployment delta hash mismatch. Expected $ExpectedGameplayDeltaHash but found $ActualGameplayDeltaHash."
}

Write-Output 'DEPLOYED'
