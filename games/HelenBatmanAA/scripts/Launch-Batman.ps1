param(
    [string]$GameExe = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe'
)

$ErrorActionPreference = 'Stop'

Start-Process $GameExe
Start-Sleep -Seconds 12
Write-Output 'LAUNCHED'
