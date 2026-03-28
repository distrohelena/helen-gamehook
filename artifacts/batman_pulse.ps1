Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win32HelenPulse {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
}
"@
$p = Get-Process ShippingPC-BmGame -ErrorAction Stop | Select-Object -First 1
[void][Win32HelenPulse]::ShowWindow($p.MainWindowHandle, 9)
Start-Sleep -Milliseconds 250
[void][Win32HelenPulse]::SetForegroundWindow($p.MainWindowHandle)
Start-Sleep -Milliseconds 500
[System.Windows.Forms.SendKeys]::SendWait(' ')
Start-Sleep -Milliseconds 800
[System.Windows.Forms.SendKeys]::SendWait('{ENTER}')
Start-Sleep -Milliseconds 800
$rect = New-Object Win32HelenPulse+RECT
[void][Win32HelenPulse]::GetWindowRect($p.MainWindowHandle, [ref]$rect)
$x = [int](($rect.Left + $rect.Right) / 2)
$y = [int](($rect.Top + $rect.Bottom) / 2)
[void][Win32HelenPulse]::SetCursorPos($x, $y)
Start-Sleep -Milliseconds 150
[Win32HelenPulse]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 50
[Win32HelenPulse]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
