$gameExe = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe'
Start-Process $gameExe
for ($i = 0; $i -lt 40; $i++) {
    $p = Get-Process ShippingPC-BmGame -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -ne $p) { Write-Output 'PROCESS_STARTED'; exit 0 }
    Start-Sleep -Milliseconds 500
}
throw 'Batman process did not start.'
