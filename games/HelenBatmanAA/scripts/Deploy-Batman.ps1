param(
    [string]$GameBin = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries',
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'

$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$RepoRoot = (Resolve-Path (Join-Path $BatmanRoot '..\..')).Path
$PackSource = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-subtitles'
$PackDestination = Join-Path $GameBin 'helengamehook\packs\batman-aa-subtitles'
$VerifierPath = Join-Path $PSScriptRoot 'Test-BatmanKnownGoodGameplayPackage.ps1'
$HelenGameHookPath = Join-Path $RepoRoot "bin\Win32\$Configuration\HelenGameHook.dll"
$ProxyPath = Join-Path $RepoRoot "bin\Win32\$Configuration\dinput8.dll"

& $VerifierPath -BatmanRoot $BatmanRoot

Get-Process ShippingPC-BmGame -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2

New-Item -ItemType Directory -Force -Path $PackDestination | Out-Null
Copy-Item -LiteralPath $HelenGameHookPath -Destination (Join-Path $GameBin 'HelenGameHook.dll') -Force
Copy-Item -LiteralPath $ProxyPath -Destination (Join-Path $GameBin 'dinput8.dll') -Force
Copy-Item -Path (Join-Path $PackSource '*') -Destination $PackDestination -Recurse -Force
Get-ChildItem (Join-Path $GameBin 'helengamehook\logs') -ErrorAction SilentlyContinue | Remove-Item -Force

Write-Output 'DEPLOYED'
