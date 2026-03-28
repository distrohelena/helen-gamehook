Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win32HelenClick {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);
}
"@
function ClickAt([int]$x,[int]$y) {
 [void][Win32HelenClick]::SetCursorPos($x,$y)
 Start-Sleep -Milliseconds 150
 [Win32HelenClick]::mouse_event(0x0002,0,0,0,[UIntPtr]::Zero)
 Start-Sleep -Milliseconds 50
 [Win32HelenClick]::mouse_event(0x0004,0,0,0,[UIntPtr]::Zero)
}
function Snap([System.Diagnostics.Process]$p, [string]$path) {
  $rect = New-Object Win32HelenClick+RECT
  [void][Win32HelenClick]::GetWindowRect($p.MainWindowHandle, [ref]$rect)
  $bitmap = New-Object System.Drawing.Bitmap(($rect.Right-$rect.Left), ($rect.Bottom-$rect.Top))
  $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
  $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $graphics.Dispose(); $bitmap.Dispose()
}
$p = Get-Process ShippingPC-BmGame -ErrorAction Stop | Select-Object -First 1
[void][Win32HelenClick]::ShowWindow($p.MainWindowHandle, 9)
Start-Sleep -Milliseconds 250
[void][Win32HelenClick]::SetForegroundWindow($p.MainWindowHandle)
Start-Sleep -Milliseconds 500
$rect = New-Object Win32HelenClick+RECT
[void][Win32HelenClick]::GetWindowRect($p.MainWindowHandle, [ref]$rect)
$x = [int]($rect.Left + (($rect.Right-$rect.Left) * 0.52))
$y = [int]($rect.Top + (($rect.Bottom-$rect.Top) * 0.70))
ClickAt $x $y
Start-Sleep -Seconds 3
Snap $p 'C:\dev\helenhook\artifacts\batman-rva-after-title-click.png'
