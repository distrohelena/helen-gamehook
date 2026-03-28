Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win32HelenStep {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
}
"@
$p = Get-Process ShippingPC-BmGame -ErrorAction Stop | Select-Object -First 1
[void][Win32HelenStep]::ShowWindow($p.MainWindowHandle, 9)
Start-Sleep -Milliseconds 250
[void][Win32HelenStep]::SetForegroundWindow($p.MainWindowHandle)
Start-Sleep -Milliseconds 500
[System.Windows.Forms.SendKeys]::SendWait('{ESC}')
Start-Sleep -Seconds 2
[System.Windows.Forms.SendKeys]::SendWait('{DOWN}')
Start-Sleep -Milliseconds 500
[System.Windows.Forms.SendKeys]::SendWait('{DOWN}')
Start-Sleep -Milliseconds 500
[System.Windows.Forms.SendKeys]::SendWait('{DOWN}')
Start-Sleep -Milliseconds 500
[System.Windows.Forms.SendKeys]::SendWait('{ENTER}')
Start-Sleep -Seconds 2
$rect = New-Object Win32HelenStep+RECT
[void][Win32HelenStep]::GetWindowRect($p.MainWindowHandle, [ref]$rect)
$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
$bitmap = New-Object System.Drawing.Bitmap($width, $height)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
$bitmap.Save('C:\dev\helenhook\artifacts\batman-step-audio-options-confirm.png', [System.Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose()
$bitmap.Dispose()
