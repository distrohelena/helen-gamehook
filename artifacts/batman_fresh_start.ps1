Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win32HelenFresh {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);
}
"@
function Get-GameProcess { Get-Process ShippingPC-BmGame -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1 }
function Snap([System.Diagnostics.Process]$p, [string]$path) {
  $rect = New-Object Win32HelenFresh+RECT
  [void][Win32HelenFresh]::GetWindowRect($p.MainWindowHandle, [ref]$rect)
  $bitmap = New-Object System.Drawing.Bitmap(($rect.Right-$rect.Left), ($rect.Bottom-$rect.Top))
  $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
  $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $graphics.Dispose(); $bitmap.Dispose()
}
$gameExe = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe'
Get-Process ShippingPC-BmGame -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2
Start-Process $gameExe
$p = $null
for ($i=0; $i -lt 120; $i++) { $p = Get-GameProcess; if ($null -ne $p) { break }; Start-Sleep -Milliseconds 500 }
if ($null -eq $p) { throw 'Batman window did not appear.' }
Start-Sleep -Seconds 8
[void][Win32HelenFresh]::ShowWindow($p.MainWindowHandle, 9)
Start-Sleep -Milliseconds 250
[void][Win32HelenFresh]::SetForegroundWindow($p.MainWindowHandle)
Start-Sleep -Milliseconds 500
[System.Windows.Forms.SendKeys]::SendWait(' ')
Start-Sleep -Milliseconds 800
[System.Windows.Forms.SendKeys]::SendWait('{ENTER}')
Start-Sleep -Milliseconds 800
$rect = New-Object Win32HelenFresh+RECT
[void][Win32HelenFresh]::GetWindowRect($p.MainWindowHandle, [ref]$rect)
$x = [int](($rect.Left + $rect.Right) / 2)
$y = [int](($rect.Top + $rect.Bottom) / 2)
[void][Win32HelenFresh]::SetCursorPos($x, $y)
Start-Sleep -Milliseconds 150
[Win32HelenFresh]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 50
[Win32HelenFresh]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
Start-Sleep -Seconds 5
Snap $p 'C:\dev\helenhook\artifacts\batman-fresh-after-start.png'
