param(
    [int]$TimeoutSeconds = 10
)

$ErrorActionPreference = 'Stop'

$HelperScriptPath = Join-Path $PSScriptRoot 'BatmanWindowHelpers.ps1'
. $HelperScriptPath

$DialogTitle = 'Batman Helper Test'
$DialogBody = 'Dialog body for helper detection.'
$DialogProcess = $null

try {
    $DialogProcess = Start-Process powershell `
        -ArgumentList @(
            '-NoProfile',
            '-Command',
            "Add-Type -AssemblyName System.Windows.Forms; [System.Windows.Forms.MessageBox]::Show('$DialogBody', '$DialogTitle') | Out-Null"
        ) `
        -PassThru

    $Deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $Match = $null

    while ((Get-Date) -lt $Deadline -and $null -eq $Match) {
        $Match = Get-VisibleWindowSnapshot -TitleRegex "^$([Regex]::Escape($DialogTitle))$" |
            Select-Object -First 1

        if ($null -eq $Match) {
            Start-Sleep -Milliseconds 250
        }
    }

    if ($null -eq $Match) {
        throw "Dialog helper did not detect the visible message box '$DialogTitle'."
    }

    if ($Match.Title -ne $DialogTitle) {
        throw "Dialog helper title mismatch. Expected '$DialogTitle' but found '$($Match.Title)'."
    }

    if (($Match.ChildTexts -notcontains $DialogBody) -and ($Match.AllText -notmatch [Regex]::Escape($DialogBody))) {
        throw "Dialog helper body mismatch. Expected '$DialogBody' but found '$($Match.AllText)'."
    }

    Write-Output 'PASS'
}
finally {
    if ($null -ne $DialogProcess -and -not $DialogProcess.HasExited) {
        Stop-Process -Id $DialogProcess.Id -Force
    }
}
