param(
    [Parameter(Mandatory = $true)][string]$FilePath,
    [Parameter(Mandatory = $true)][AllowEmptyCollection()][string[]]$Arguments
)

$ErrorActionPreference = 'Stop'
# The child inherits this process-local error mode. No machine-wide WER setting is changed.
if (-not ('HelenBatmanConsoleErrorMode' -as [type])) {
    Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
/** Suppresses operating-system crash dialogs for console build children. */
public static class HelenBatmanConsoleErrorMode {
    /** Changes only this process's inheritable error mode and returns its previous value. */
    [DllImport("kernel32.dll")]
    public static extern uint SetErrorMode(uint mode);
}
'@
}
$previousMode = [HelenBatmanConsoleErrorMode]::SetErrorMode(0x8003)
try {
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Console tool '$FilePath' exited with code $LASTEXITCODE." }
}
finally {
    [HelenBatmanConsoleErrorMode]::SetErrorMode($previousMode) | Out-Null
}
