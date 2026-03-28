$ErrorActionPreference = 'Stop'
$gameBin = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries'
$packSrc = 'C:\dev\helenhook\games\HelenBatmanAA\helengamehook\packs\batman-aa-subtitles'
$packDst = Join-Path $gameBin 'helengamehook\packs\batman-aa-subtitles'
Get-Process ShippingPC-BmGame -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2
Copy-Item 'C:\dev\helenhook\bin\Win32\Debug\HelenGameHook.dll' (Join-Path $gameBin 'HelenGameHook.dll') -Force
Copy-Item 'C:\dev\helenhook\bin\Win32\Debug\dinput8.dll' (Join-Path $gameBin 'dinput8.dll') -Force
Copy-Item (Join-Path $packSrc '*') $packDst -Recurse -Force
Get-ChildItem (Join-Path $gameBin 'helengamehook\logs') -ErrorAction SilentlyContinue | Remove-Item -Force
Write-Output 'DEPLOYED'
