param(
    [string]$GameExe = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe'
)

$ErrorActionPreference = 'Stop'

Start-Process $GameExe

for ($index = 0; $index -lt 40; $index++) {
    $process = Get-Process ShippingPC-BmGame -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -ne $process) {
        Write-Output 'PROCESS_STARTED'
        exit 0
    }

    Start-Sleep -Milliseconds 500
}

throw 'Batman process did not start.'
