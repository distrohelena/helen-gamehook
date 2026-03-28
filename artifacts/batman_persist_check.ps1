$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win32HelenPersist {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);
}
"@
function Get-GameProcess {
    Get-Process ShippingPC-BmGame -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
}
function Focus-GameWindow([System.Diagnostics.Process]$Process) {
    [void][Win32HelenPersist]::ShowWindow($Process.MainWindowHandle, 9)
    Start-Sleep -Milliseconds 250
    [void][Win32HelenPersist]::SetForegroundWindow($Process.MainWindowHandle)
    Start-Sleep -Milliseconds 500
}
function Send-GameKeys([System.Diagnostics.Process]$Process, [string]$Keys, [int]$DelayMs = 700) {
    Focus-GameWindow $Process
    [System.Windows.Forms.SendKeys]::SendWait($Keys)
    Start-Sleep -Milliseconds $DelayMs
}
function Click-WindowCenter([System.Diagnostics.Process]$Process) {
    $rect = New-Object Win32HelenPersist+RECT
    [void][Win32HelenPersist]::GetWindowRect($Process.MainWindowHandle, [ref]$rect)
    $x = [int](($rect.Left + $rect.Right) / 2)
    $y = [int](($rect.Top + $rect.Bottom) / 2)
    [void][Win32HelenPersist]::SetCursorPos($x, $y)
    Start-Sleep -Milliseconds 150
    [Win32HelenPersist]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 50
    [Win32HelenPersist]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 500
}
function Save-WindowShot([System.Diagnostics.Process]$Process, [string]$Path) {
    $rect = New-Object Win32HelenPersist+RECT
    [void][Win32HelenPersist]::GetWindowRect($Process.MainWindowHandle, [ref]$rect)
    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    $bitmap = New-Object System.Drawing.Bitmap($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
    $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    $graphics.Dispose()
    $bitmap.Dispose()
}
$gameExe = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe'
$logDir = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\helengamehook\logs'
$artifacts = 'C:\dev\helenhook\artifacts'
Get-Process ShippingPC-BmGame -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2
Get-ChildItem $logDir -ErrorAction SilentlyContinue | Remove-Item -Force
Start-Process $gameExe
$process = $null
for ($i = 0; $i -lt 120; $i++) {
    $process = Get-GameProcess
    if ($null -ne $process) { break }
    Start-Sleep -Milliseconds 500
}
if ($null -eq $process) { throw 'Batman window did not appear.' }
Start-Sleep -Seconds 8
Send-GameKeys $process ' '
Send-GameKeys $process '{ENTER}'
Click-WindowCenter $process
Start-Sleep -Seconds 5
Send-GameKeys $process '{ESC}' 1000
Start-Sleep -Seconds 2
Send-GameKeys $process '{UP}' 700
Send-GameKeys $process '{ENTER}' 1000
Start-Sleep -Seconds 20
Send-GameKeys $process '{ESC}' 1000
Send-GameKeys $process '{ENTER}' 1000
Start-Sleep -Seconds 8
Send-GameKeys $process '{ESC}' 1000
Send-GameKeys $process '{DOWN}' 500
Send-GameKeys $process '{DOWN}' 500
Send-GameKeys $process '{DOWN}' 500
Send-GameKeys $process '{ENTER}' 1000
Start-Sleep -Seconds 2
Save-WindowShot $process (Join-Path $artifacts 'batman-persist-audio-options.png')
