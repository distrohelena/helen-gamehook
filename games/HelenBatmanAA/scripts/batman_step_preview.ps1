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
    [DllImport("kernel32.dll", SetLastError=true)] public static extern IntPtr OpenProcess(uint access, bool inherit, int processId);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool ReadProcessMemory(IntPtr process, IntPtr address, byte[] buffer, int size, out IntPtr read);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool CloseHandle(IntPtr handle);
}
"@
function Save-WindowShot([System.Diagnostics.Process]$Process, [string]$Path) {
    $rect = New-Object Win32HelenStep+RECT
    [void][Win32HelenStep]::GetWindowRect($Process.MainWindowHandle, [ref]$rect)
    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    $bitmap = New-Object System.Drawing.Bitmap($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
    $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    $graphics.Dispose()
    $bitmap.Dispose()
}
$p = Get-Process ShippingPC-BmGame -ErrorAction Stop | Select-Object -First 1
[void][Win32HelenStep]::ShowWindow($p.MainWindowHandle, 9)
Start-Sleep -Milliseconds 250
[void][Win32HelenStep]::SetForegroundWindow($p.MainWindowHandle)
Start-Sleep -Milliseconds 500
[System.Windows.Forms.SendKeys]::SendWait('{DOWN}')
Start-Sleep -Seconds 2
Save-WindowShot $p 'C:\dev\helenhook\artifacts\batman-preview-large.png'
$state = Get-Content 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\helengamehook\logs\batman-debug-state.json' | ConvertFrom-Json
$slotHex = $state.slots[0].address
$slotAddress = [UInt32]::Parse($slotHex.Substring(2), [System.Globalization.NumberStyles]::HexNumber)
$handle = [Win32HelenStep]::OpenProcess(0x0010, $false, $p.Id)
$beforeBuffer = New-Object byte[] 4
$read = [IntPtr]::Zero
[void][Win32HelenStep]::ReadProcessMemory($handle, [IntPtr]([int64]$slotAddress), $beforeBuffer, 4, [ref]$read)
$beforeValue = [BitConverter]::ToSingle($beforeBuffer, 0)
[System.Windows.Forms.SendKeys]::SendWait('{ENTER}')
Start-Sleep -Seconds 2
Save-WindowShot $p 'C:\dev\helenhook\artifacts\batman-preview-small.png'
$afterBuffer = New-Object byte[] 4
[void][Win32HelenStep]::ReadProcessMemory($handle, [IntPtr]([int64]$slotAddress), $afterBuffer, 4, [ref]$read)
$afterValue = [BitConverter]::ToSingle($afterBuffer, 0)
[void][Win32HelenStep]::CloseHandle($handle)
[pscustomobject]@{ Before=$beforeValue; After=$afterValue } | ConvertTo-Json -Compress | Set-Content 'C:\dev\helenhook\artifacts\batman-slot-values.json'
