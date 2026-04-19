param(
    [int]$TimeoutSeconds = 10
)

$ErrorActionPreference = 'Stop'

$HelperScriptPath = Join-Path $PSScriptRoot 'BatmanWindowHelpers.ps1'
. $HelperScriptPath

$OutputDir = 'C:\dev\helenhook\artifacts\dialog-recognition'
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
}

$TempOcrConfigPath = Join-Path $OutputDir 'temp-ocr-config.json'
Write-BatmanRecognitionOcrConfig -OutputPath $TempOcrConfigPath

$DialogTitle = 'Batman Fatal Error'
$DialogBody = 'Fatal error: synthetic smoke test dialog.'
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
        throw "Dialog recognition test did not find the visible message box '$DialogTitle'."
    }

    $Failure = Get-BatmanDialogFailure `
        -ArtifactsRoot $OutputDir `
        -Label 'synthetic-crash-dialog' `
        -ScreenshotCliProject 'C:\dev\helenui\plugins\screenshot-cli' `
        -RecognitionCliProject 'C:\dev\helenui\plugins\recognition-cli' `
        -BatmanProjectPath 'C:\dev\helenui\batman-aa.json' `
        -OcrConfigPath $TempOcrConfigPath `
        -DialogSnapshot $Match

    if ($null -eq $Failure) {
        throw 'Dialog recognition helper returned no failure object.'
    }

    if ($Failure.Recognition.screen_match.screen_name -ne 'CrashDialog') {
        throw "Expected CrashDialog screen match but found '$($Failure.Recognition.screen_match.screen_name)'."
    }

    if ($Failure.Summary -notmatch 'Fatal error') {
        throw "Expected OCR summary to contain 'Fatal error' but found '$($Failure.Summary)'."
    }

    Write-Output 'PASS'
}
finally {
    if ($null -ne $DialogProcess -and -not $DialogProcess.HasExited) {
        Stop-Process -Id $DialogProcess.Id -Force
    }

    if (Test-Path -LiteralPath $TempOcrConfigPath) {
        Remove-Item -LiteralPath $TempOcrConfigPath -Force -ErrorAction SilentlyContinue
    }
}
