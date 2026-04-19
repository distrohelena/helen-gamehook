if (-not ('BatmanWindowNative' -as [type])) {
    Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class BatmanWindowNative {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumChildWindows(IntPtr hWndParent, EnumWindowsProc callback, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowTextLength(IntPtr hWnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int maxCount);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassName(IntPtr hWnd, StringBuilder className, int maxCount);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
}
"@
}

function Get-WindowTextValue {
    param(
        [Parameter(Mandatory = $true)]
        [System.IntPtr]$WindowHandle
    )

    $TextLength = [BatmanWindowNative]::GetWindowTextLength($WindowHandle)
    $Builder = New-Object System.Text.StringBuilder ($TextLength + 1)
    [BatmanWindowNative]::GetWindowText($WindowHandle, $Builder, $Builder.Capacity) | Out-Null
    return $Builder.ToString()
}

function Get-WindowClassNameValue {
    param(
        [Parameter(Mandatory = $true)]
        [System.IntPtr]$WindowHandle
    )

    $Builder = New-Object System.Text.StringBuilder 256
    [BatmanWindowNative]::GetClassName($WindowHandle, $Builder, $Builder.Capacity) | Out-Null
    return $Builder.ToString()
}

function Get-ChildWindowSnapshot {
    param(
        [Parameter(Mandatory = $true)]
        [System.IntPtr]$WindowHandle
    )

    $Rows = New-Object System.Collections.Generic.List[object]

    [BatmanWindowNative]::EnumChildWindows($WindowHandle, {
        param($ChildHandle, $LParam)

        if (-not [BatmanWindowNative]::IsWindowVisible($ChildHandle)) {
            return $true
        }

        $Text = Get-WindowTextValue -WindowHandle $ChildHandle
        $ClassName = Get-WindowClassNameValue -WindowHandle $ChildHandle

        if (-not [string]::IsNullOrWhiteSpace($Text)) {
            $Rows.Add([pscustomobject]@{
                Handle = ('0x{0:X16}' -f $ChildHandle.ToInt64())
                ClassName = $ClassName
                Text = $Text
            }) | Out-Null
        }

        return $true
    }, [System.IntPtr]::Zero) | Out-Null

    return $Rows
}

function Get-VisibleWindowSnapshot {
    param(
        [string[]]$ProcessNames,
        [string]$TitleRegex,
        [string]$ChildTextRegex
    )

    $Rows = New-Object System.Collections.Generic.List[object]
    $ProcessNameSet = @{}

    if ($null -ne $ProcessNames) {
        foreach ($ProcessName in $ProcessNames) {
            if (-not [string]::IsNullOrWhiteSpace($ProcessName)) {
                $ProcessNameSet[$ProcessName.ToLowerInvariant()] = $true
            }
        }
    }

    [BatmanWindowNative]::EnumWindows({
        param($WindowHandle, $LParam)

        if (-not [BatmanWindowNative]::IsWindowVisible($WindowHandle)) {
            return $true
        }

        [uint32]$ProcessId = 0
        [BatmanWindowNative]::GetWindowThreadProcessId($WindowHandle, [ref]$ProcessId) | Out-Null
        $Process = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
        if ($null -eq $Process) {
            return $true
        }

        if ($ProcessNameSet.Count -gt 0 -and -not $ProcessNameSet.ContainsKey($Process.ProcessName.ToLowerInvariant())) {
            return $true
        }

        $Title = Get-WindowTextValue -WindowHandle $WindowHandle
        $ChildWindows = @(Get-ChildWindowSnapshot -WindowHandle $WindowHandle)
        $ChildTexts = @($ChildWindows | ForEach-Object { $_.Text })
        $AllText = ((@($Title) + $ChildTexts) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }) -join "`n"

        if (-not [string]::IsNullOrWhiteSpace($TitleRegex) -and $Title -notmatch $TitleRegex) {
            return $true
        }

        if (-not [string]::IsNullOrWhiteSpace($ChildTextRegex) -and $AllText -notmatch $ChildTextRegex) {
            return $true
        }

        $Rows.Add([pscustomobject]@{
            Handle = ('0x{0:X16}' -f $WindowHandle.ToInt64())
            HandleValue = $WindowHandle
            ProcessName = $Process.ProcessName
            ProcessId = $ProcessId
            ClassName = (Get-WindowClassNameValue -WindowHandle $WindowHandle)
            Title = $Title
            ChildTexts = $ChildTexts
            ChildWindows = $ChildWindows
            AllText = $AllText
        }) | Out-Null

        return $true
    }, [System.IntPtr]::Zero) | Out-Null

    return $Rows
}

function Get-BatmanMainWindowSnapshot {
    $Windows = @(Get-VisibleWindowSnapshot -ProcessNames @('ShippingPC-BmGame'))
    return @($Windows | Where-Object { $_.ClassName -ne '#32770' }) | Select-Object -First 1
}

function Get-BatmanDialogSnapshot {
    $DialogRegex = 'Microsoft Visual C\+\+|Runtime Error|Fatal|Assertion|Unreal|failed|crash|error|warning'
    $Windows = @(Get-VisibleWindowSnapshot)

    return @(
        $Windows |
            Where-Object {
                $_.ProcessName -eq 'WerFault' -or
                (($_.ClassName -eq '#32770') -and ($_.Title -match $DialogRegex -or $_.AllText -match $DialogRegex)) -or
                (($_.ProcessName -eq 'ShippingPC-BmGame') -and ($_.ClassName -eq '#32770'))
            } |
            Where-Object {
                -not [string]::IsNullOrWhiteSpace($_.Title) -or
                $_.ChildTexts.Count -gt 0
            }
    )
}

function Export-WindowSnapshotJson {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [object[]]$Windows
    )

    $Directory = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($Directory)) {
        New-Item -ItemType Directory -Force -Path $Directory | Out-Null
    }

    $Windows | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Path
}

function Format-WindowSnapshotSummary {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Window
    )

    $SummaryLines = @()

    if (-not [string]::IsNullOrWhiteSpace($Window.ProcessName)) {
        $SummaryLines += "Process=$($Window.ProcessName)($($Window.ProcessId))"
    }

    if (-not [string]::IsNullOrWhiteSpace($Window.Title)) {
        $SummaryLines += "Title=$($Window.Title)"
    }

    if (-not [string]::IsNullOrWhiteSpace($Window.AllText)) {
        $FlattenedText = (($Window.AllText -split '\r?\n') | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }) -join ' | '
        $SummaryLines += "Text=$FlattenedText"
    }

    return ($SummaryLines -join '; ')
}

function Get-WindowHandleHex {
    param(
        [Parameter(Mandatory = $true)]
        [System.IntPtr]$WindowHandle
    )

    return ('0x{0:X16}' -f $WindowHandle.ToInt64())
}

function Get-BatmanTesseractExecutablePath {
    $PreferredPaths = @(
        'C:\Program Files\Tesseract-OCR\tesseract.exe',
        'C:\Program Files (x86)\Tesseract-OCR\tesseract.exe'
    )

    foreach ($CandidatePath in $PreferredPaths) {
        if (Test-Path -LiteralPath $CandidatePath) {
            return $CandidatePath
        }
    }

    throw 'Batman OCR requires Tesseract, but no supported tesseract.exe installation was found.'
}

function New-BatmanRecognitionOcrConfig {
    param(
        [ValidateSet('tesseract', 'windows_native')]
        [string]$PreferredEngine = 'tesseract'
    )

    if ($PreferredEngine -eq 'tesseract') {
        return @{
            ocr = @{
                engines = @(
                    @{
                        type = 'tesseract'
                        exePath = (Get-BatmanTesseractExecutablePath)
                    }
                )
            }
        }
    }

    return @{
        ocr = @{
            engines = @(
                @{
                    type = 'windows_native'
                }
            )
        }
    }
}

function Write-BatmanRecognitionOcrConfig {
    param(
        [Parameter(Mandatory = $true)]
        [string]$OutputPath,
        [ValidateSet('tesseract', 'windows_native')]
        [string]$PreferredEngine = 'tesseract'
    )

    $Directory = Split-Path -Parent $OutputPath
    if (-not [string]::IsNullOrWhiteSpace($Directory)) {
        New-Item -ItemType Directory -Force -Path $Directory | Out-Null
    }

    $Config = New-BatmanRecognitionOcrConfig -PreferredEngine $PreferredEngine
    $Config | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $OutputPath
}

function Convert-CommandOutputToJsonObject {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$OutputLines
    )

    $JoinedOutput = ($OutputLines -join [Environment]::NewLine).Trim()
    $JsonStart = $JoinedOutput.IndexOf('{')
    $JsonEnd = $JoinedOutput.LastIndexOf('}')

    if ($JsonStart -lt 0 -or $JsonEnd -lt $JsonStart) {
        throw "Expected JSON output but found: $JoinedOutput"
    }

    $JsonText = $JoinedOutput.Substring($JsonStart, ($JsonEnd - $JsonStart) + 1)
    return $JsonText | ConvertFrom-Json
}

function Invoke-ScreenshotCliWindowCapture {
    param(
        [Parameter(Mandatory = $true)]
        [System.IntPtr]$WindowHandle,
        [Parameter(Mandatory = $true)]
        [string]$OutputPath,
        [Parameter(Mandatory = $true)]
        [string]$ScreenshotCliProject
    )

    $ProjectFilePath = Join-Path $ScreenshotCliProject 'src\ScreenshotCli\ScreenshotCli.csproj'
    if (-not (Test-Path $ProjectFilePath)) {
        throw "Screenshot CLI project not found at '$ProjectFilePath'."
    }

    $OutputDirectory = Split-Path -Parent $OutputPath
    if (-not [string]::IsNullOrWhiteSpace($OutputDirectory)) {
        New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    }

    $HandleHex = Get-WindowHandleHex -WindowHandle $WindowHandle
    $CaptureOutput = & dotnet run --project $ProjectFilePath -v q -- capture --handle $HandleHex --output $OutputPath 2>&1
    $CaptureResult = Convert-CommandOutputToJsonObject -OutputLines @($CaptureOutput)

    if ($LASTEXITCODE -ne 0 -or -not $CaptureResult.success) {
        $ErrorMessage = if ($null -ne $CaptureResult.error) { $CaptureResult.error.message } else { ($CaptureOutput -join [Environment]::NewLine) }
        throw "screenshot-cli capture failed for handle $HandleHex. $ErrorMessage"
    }

    return $CaptureResult
}

function Invoke-RecognitionCliAnalyzeImage {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ImagePath,
        [Parameter(Mandatory = $true)]
        [string]$RecognitionCliProject,
        [Parameter(Mandatory = $true)]
        [string]$BatmanProjectPath,
        [Parameter(Mandatory = $true)]
        [string]$OcrConfigPath
    )

    $ProjectFilePath = Join-Path $RecognitionCliProject 'src\RecognitionCli\RecognitionCli.csproj'
    if (-not (Test-Path $ProjectFilePath)) {
        throw "Recognition CLI project not found at '$ProjectFilePath'."
    }

    if (-not (Test-Path $BatmanProjectPath)) {
        throw "Batman HelenUI project not found at '$BatmanProjectPath'."
    }

    if (-not (Test-Path $OcrConfigPath)) {
        throw "OCR config not found at '$OcrConfigPath'."
    }

    $RecognitionOutput = & dotnet run --project $ProjectFilePath -v q -- analyze --project $BatmanProjectPath --image $ImagePath --config $OcrConfigPath 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "recognition-cli analyze failed for '$ImagePath'. $($RecognitionOutput -join [Environment]::NewLine)"
    }

    return Convert-CommandOutputToJsonObject -OutputLines @($RecognitionOutput)
}

function Get-RecognitionRawTextSummary {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Recognition
    )

    $OcrResults = @($Recognition.diagnostics.ocr_results)
    foreach ($OcrResult in $OcrResults) {
        if ($OcrResult.success -and -not [string]::IsNullOrWhiteSpace($OcrResult.raw_text)) {
            $Lines = @(
                $OcrResult.raw_text -split '\r?\n' |
                    Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
            )
            return ($Lines -join ' | ')
        }
    }

    return ''
}

function Get-BatmanRecognitionScreenScore {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Recognition,
        [Parameter(Mandatory = $true)]
        [string]$ScreenName
    )

    $CandidateScreens = @()
    if ($null -ne $Recognition -and
        $null -ne $Recognition.diagnostics -and
        $null -ne $Recognition.diagnostics.candidate_screen_scores) {
        $CandidateScreens = @($Recognition.diagnostics.candidate_screen_scores)
    }

    $Screen = @($CandidateScreens | Where-Object { $_.screen_name -eq $ScreenName }) | Select-Object -First 1
    if ($null -eq $Screen) {
        return 0
    }

    return [double]$Screen.score
}

function Get-BatmanRecognitionMatchedScreenName {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Recognition
    )

    if ($null -eq $Recognition.screen_match -or
        [string]::IsNullOrWhiteSpace($Recognition.screen_match.screen_name)) {
        return 'Unknown'
    }

    return $Recognition.screen_match.screen_name
}

function Get-BatmanRecognitionTopCandidateSummary {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Recognition,
        [int]$MaxCount = 4
    )

    if ($MaxCount -le 0) {
        throw 'Get-BatmanRecognitionTopCandidateSummary requires MaxCount to be positive.'
    }

    $CandidateScreens = @()
    if ($null -ne $Recognition -and
        $null -ne $Recognition.diagnostics -and
        $null -ne $Recognition.diagnostics.candidate_screen_scores) {
        $CandidateScreens = @($Recognition.diagnostics.candidate_screen_scores)
    }

    if ($CandidateScreens.Count -eq 0) {
        return '<none>'
    }

    $TopCandidates = @($CandidateScreens | Sort-Object score -Descending | Select-Object -First $MaxCount)
    return (($TopCandidates | ForEach-Object { '{0}={1}' -f $_.screen_name, ([double]$_.score) }) -join ', ')
}

function Assert-BatmanRecognitionExpectedScreen {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Recognition,
        [Parameter(Mandatory = $true)]
        [string]$ExpectedScreenName,
        [double]$MinimumScore = 1,
        [string]$Context = 'Batman screen checkpoint'
    )

    if ($MinimumScore -le 0) {
        throw 'Assert-BatmanRecognitionExpectedScreen requires MinimumScore to be positive.'
    }

    $Score = Get-BatmanRecognitionScreenScore -Recognition $Recognition -ScreenName $ExpectedScreenName
    if ($Score -ge $MinimumScore) {
        return
    }

    $MatchedScreen = Get-BatmanRecognitionMatchedScreenName -Recognition $Recognition
    $TopCandidates = Get-BatmanRecognitionTopCandidateSummary -Recognition $Recognition
    throw "$Context expected '$ExpectedScreenName' but found '$MatchedScreen'. Top candidates: $TopCandidates"
}

function Wait-ForBatmanMainWindowExpectedScreen {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ExpectedScreenName,
        [Parameter(Mandatory = $true)]
        [string]$Context,
        [Parameter(Mandatory = $true)]
        [string]$ArtifactsRoot,
        [Parameter(Mandatory = $true)]
        [string]$Label,
        [Parameter(Mandatory = $true)]
        [string]$ScreenshotCliProject,
        [Parameter(Mandatory = $true)]
        [string]$RecognitionCliProject,
        [Parameter(Mandatory = $true)]
        [string]$BatmanProjectPath,
        [Parameter(Mandatory = $true)]
        [string]$OcrConfigPath,
        [int]$TimeoutMilliseconds = 10000,
        [int]$PollMilliseconds = 500,
        [double]$MinimumScore = 1
    )

    if ($TimeoutMilliseconds -le 0) {
        throw 'Wait-ForBatmanMainWindowExpectedScreen requires TimeoutMilliseconds to be positive.'
    }

    if ($PollMilliseconds -le 0) {
        throw 'Wait-ForBatmanMainWindowExpectedScreen requires PollMilliseconds to be positive.'
    }

    if ($MinimumScore -le 0) {
        throw 'Wait-ForBatmanMainWindowExpectedScreen requires MinimumScore to be positive.'
    }

    New-Item -ItemType Directory -Force -Path $ArtifactsRoot | Out-Null

    $SanitizedLabel = ($Label -replace '[^A-Za-z0-9_-]+', '-').Trim('-')
    if ([string]::IsNullOrWhiteSpace($SanitizedLabel)) {
        $SanitizedLabel = 'batman-screen-checkpoint'
    }

    $ScreenshotPath = Join-Path $ArtifactsRoot "$SanitizedLabel.png"
    $Deadline = (Get-Date).AddMilliseconds($TimeoutMilliseconds)
    $LastRecognition = $null

    while ($true) {
        Assert-NoBatmanDialog `
            -ArtifactsRoot $ArtifactsRoot `
            -Label "$SanitizedLabel-dialog-check" `
            -ScreenshotCliProject $ScreenshotCliProject `
            -RecognitionCliProject $RecognitionCliProject `
            -BatmanProjectPath $BatmanProjectPath `
            -OcrConfigPath $OcrConfigPath

        Capture-BatmanMainWindowImage `
            -OutputPath $ScreenshotPath `
            -ScreenshotCliProject $ScreenshotCliProject | Out-Null

        $LastRecognition = Invoke-RecognitionCliAnalyzeImage `
            -ImagePath $ScreenshotPath `
            -RecognitionCliProject $RecognitionCliProject `
            -BatmanProjectPath $BatmanProjectPath `
            -OcrConfigPath $OcrConfigPath

        $Score = Get-BatmanRecognitionScreenScore -Recognition $LastRecognition -ScreenName $ExpectedScreenName
        if ($Score -ge $MinimumScore) {
            return [pscustomobject]@{
                Recognition = $LastRecognition
                ScreenshotPath = $ScreenshotPath
            }
        }

        if ((Get-Date) -ge $Deadline) {
            break
        }

        Start-Sleep -Milliseconds $PollMilliseconds
    }

    $MatchedScreen = Get-BatmanRecognitionMatchedScreenName -Recognition $LastRecognition
    $TopCandidates = Get-BatmanRecognitionTopCandidateSummary -Recognition $LastRecognition
    throw "$Context expected '$ExpectedScreenName' but found '$MatchedScreen'. Top candidates: $TopCandidates. Screenshot: $ScreenshotPath"
}

function Write-JsonArtifact {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [object]$Value
    )

    $Directory = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($Directory)) {
        New-Item -ItemType Directory -Force -Path $Directory | Out-Null
    }

    $Value | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $Path
}

function Get-BatmanDialogFailure {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ArtifactsRoot,
        [Parameter(Mandatory = $true)]
        [string]$Label,
        [Parameter(Mandatory = $true)]
        [string]$ScreenshotCliProject,
        [Parameter(Mandatory = $true)]
        [string]$RecognitionCliProject,
        [Parameter(Mandatory = $true)]
        [string]$BatmanProjectPath,
        [Parameter(Mandatory = $true)]
        [string]$OcrConfigPath,
        [object]$DialogSnapshot
    )

    $Dialogs = if ($PSBoundParameters.ContainsKey('DialogSnapshot') -and $null -ne $DialogSnapshot) {
        @($DialogSnapshot)
    } else {
        @(Get-BatmanDialogSnapshot)
    }

    if ($Dialogs.Count -eq 0) {
        return $null
    }

    New-Item -ItemType Directory -Force -Path $ArtifactsRoot | Out-Null

    $Timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $SnapshotJsonPath = Join-Path $ArtifactsRoot "$Label-dialog-snapshot-$Timestamp.json"
    $DialogImagePath = Join-Path $ArtifactsRoot "$Label-dialog-$Timestamp.png"
    $RecognitionJsonPath = Join-Path $ArtifactsRoot "$Label-dialog-recognition-$Timestamp.json"

    Export-WindowSnapshotJson -Path $SnapshotJsonPath -Windows $Dialogs

    $Dialog = $Dialogs | Select-Object -First 1
    Invoke-ScreenshotCliWindowCapture `
        -WindowHandle $Dialog.HandleValue `
        -OutputPath $DialogImagePath `
        -ScreenshotCliProject $ScreenshotCliProject | Out-Null

    $Recognition = Invoke-RecognitionCliAnalyzeImage `
        -ImagePath $DialogImagePath `
        -RecognitionCliProject $RecognitionCliProject `
        -BatmanProjectPath $BatmanProjectPath `
        -OcrConfigPath $OcrConfigPath

    Write-JsonArtifact -Path $RecognitionJsonPath -Value $Recognition

    $RawTextSummary = Get-RecognitionRawTextSummary -Recognition $Recognition
    $ScreenName = if ($null -ne $Recognition.screen_match) { $Recognition.screen_match.screen_name } else { $null }
    $SummaryParts = @()

    if (-not [string]::IsNullOrWhiteSpace($ScreenName)) {
        $SummaryParts += "Screen=$ScreenName"
    }

    if (-not [string]::IsNullOrWhiteSpace($RawTextSummary)) {
        $SummaryParts += "OCR=$RawTextSummary"
    }

    $WindowSummary = Format-WindowSnapshotSummary -Window $Dialog
    if (-not [string]::IsNullOrWhiteSpace($WindowSummary)) {
        $SummaryParts += "Window=$WindowSummary"
    }

    return [pscustomobject]@{
        Dialog = $Dialog
        Recognition = $Recognition
        Summary = ($SummaryParts -join '; ')
        SnapshotJsonPath = $SnapshotJsonPath
        ImagePath = $DialogImagePath
        RecognitionJsonPath = $RecognitionJsonPath
    }
}

function Assert-NoBatmanDialog {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ArtifactsRoot,
        [Parameter(Mandatory = $true)]
        [string]$Label,
        [Parameter(Mandatory = $true)]
        [string]$ScreenshotCliProject,
        [Parameter(Mandatory = $true)]
        [string]$RecognitionCliProject,
        [Parameter(Mandatory = $true)]
        [string]$BatmanProjectPath,
        [Parameter(Mandatory = $true)]
        [string]$OcrConfigPath
    )

    $Failure = Get-BatmanDialogFailure `
        -ArtifactsRoot $ArtifactsRoot `
        -Label $Label `
        -ScreenshotCliProject $ScreenshotCliProject `
        -RecognitionCliProject $RecognitionCliProject `
        -BatmanProjectPath $BatmanProjectPath `
        -OcrConfigPath $OcrConfigPath

    if ($null -ne $Failure) {
        throw "Detected blocking Batman dialog. $($Failure.Summary). Artifacts: $($Failure.ImagePath), $($Failure.RecognitionJsonPath), $($Failure.SnapshotJsonPath)"
    }
}

function Capture-BatmanMainWindowImage {
    param(
        [Parameter(Mandatory = $true)]
        [string]$OutputPath,
        [Parameter(Mandatory = $true)]
        [string]$ScreenshotCliProject
    )

    $MainWindow = Get-BatmanMainWindowSnapshot
    if ($null -eq $MainWindow) {
        throw 'Batman main window not found.'
    }

    return Invoke-ScreenshotCliWindowCapture `
        -WindowHandle $MainWindow.HandleValue `
        -OutputPath $OutputPath `
        -ScreenshotCliProject $ScreenshotCliProject
}
