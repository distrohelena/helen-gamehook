param(
    [string]$BatmanRoot
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path -LiteralPath $BatmanRoot).Path
}

$DeployScriptPath = Join-Path $BatmanRoot 'scripts\Deploy-BatmanGraphicsOptionsExperiment.ps1'
$ConfigSourcePath = Join-Path $BatmanRoot 'helengamehook\config\packs.json'

function Assert-ContainsOrdinal {
    param(
        [Parameter(Mandatory = $true)] [string]$Text,
        [Parameter(Mandatory = $true)] [string]$Token,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    if ($Text.IndexOf($Token, [StringComparison]::Ordinal) -lt 0) {
        throw "$Context is missing required token '$Token'."
    }
}

function Assert-NotContainsOrdinal {
    param(
        [Parameter(Mandatory = $true)] [string]$Text,
        [Parameter(Mandatory = $true)] [string]$Token,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    if ($Text.IndexOf($Token, [StringComparison]::Ordinal) -ge 0) {
        throw "$Context contains forbidden token '$Token'."
    }
}

function Reset-DeploymentFixture {
    param(
        [Parameter(Mandatory = $true)] [string]$LivePackRoot,
        [Parameter(Mandatory = $true)] [string]$LiveConfigPath,
        [Parameter(Mandatory = $true)] [string]$LiveHelenGameHookPath,
        [Parameter(Mandatory = $true)] [string]$LiveProxyPath,
        [Parameter(Mandatory = $true)] [string]$SubtitlePackRoot,
        [Parameter(Mandatory = $true)] [string]$StagingRoot,
        [Parameter(Mandatory = $true)] [string]$RecoveryRoot,
        [Parameter(Mandatory = $true)] [string]$StagedPackRoot,
        [Parameter(Mandatory = $true)] [string]$StagedConfigPath,
        [Parameter(Mandatory = $true)] [string]$StagedHelenGameHookPath,
        [Parameter(Mandatory = $true)] [string]$StagedProxyPath,
        [Parameter(Mandatory = $true)] [bool]$IncludeExistingGraphicsPack
    )

    foreach ($path in @($LivePackRoot, $LiveConfigPath, $LiveHelenGameHookPath, $LiveProxyPath, $SubtitlePackRoot, $StagingRoot, $RecoveryRoot)) {
        if (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Recurse -Force
        }
    }

    New-Item -ItemType Directory -Force -Path $LivePackRoot, (Split-Path -Parent $LiveConfigPath), (Split-Path -Parent $LiveHelenGameHookPath), $SubtitlePackRoot, $StagingRoot, $RecoveryRoot, $StagedPackRoot, (Split-Path -Parent $StagedConfigPath) | Out-Null
    [IO.File]::WriteAllText($LiveConfigPath, '{"enabledPacksByExecutable":{"ShippingPC-BmGame.exe":["old-pack"]}}')
    [IO.File]::WriteAllBytes($LiveHelenGameHookPath, [Text.Encoding]::ASCII.GetBytes('old-helen-game-hook'))
    [IO.File]::WriteAllBytes($LiveProxyPath, [Text.Encoding]::ASCII.GetBytes('old-proxy'))
    [IO.File]::WriteAllText((Join-Path $SubtitlePackRoot 'subtitle.txt'), 'working subtitle')
    New-Item -ItemType Directory -Force -Path (Join-Path $SubtitlePackRoot 'nested') | Out-Null
    [IO.File]::WriteAllText((Join-Path $SubtitlePackRoot 'nested\layout.txt'), 'subtitle layout')

    if ($IncludeExistingGraphicsPack) {
        [IO.File]::WriteAllText((Join-Path $LivePackRoot 'state.txt'), 'old-pack')
    } else {
        Remove-Item -LiteralPath $LivePackRoot -Recurse -Force
    }

    [IO.File]::WriteAllText((Join-Path $StagedPackRoot 'state.txt'), 'new-pack')
    [IO.File]::WriteAllText($StagedConfigPath, (Get-Content -LiteralPath $ConfigSourcePath -Raw))
    [IO.File]::WriteAllBytes($StagedHelenGameHookPath, [Text.Encoding]::ASCII.GetBytes('new-helen-game-hook'))
    [IO.File]::WriteAllBytes($StagedProxyPath, [Text.Encoding]::ASCII.GetBytes('new-proxy'))
}

function Get-DirectorySnapshot {
    param(
        [Parameter(Mandatory = $true)] [string]$Root
    )

    if (-not (Test-Path -LiteralPath $Root -PathType Container)) {
        throw "Snapshot root is not a directory: $Root"
    }

    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    return @(
        Get-ChildItem -LiteralPath $Root -Recurse -Force | ForEach-Object {
            $relativePath = $_.FullName.Substring($fullRoot.Length).Replace('/', '\')
            if ($_.PSIsContainer) {
                [pscustomobject]@{ RelativePath = $relativePath; Kind = 'Directory'; Length = [int64]0; Sha256 = '' }
            } else {
                [pscustomobject]@{ RelativePath = $relativePath; Kind = 'File'; Length = [int64]$_.Length; Sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
            }
        } | Sort-Object Kind, RelativePath
    )
}

function Assert-DirectorySnapshotEqual {
    param(
        [Parameter(Mandatory = $true)] [object[]]$Expected,
        [Parameter(Mandatory = $true)] [string]$ActualRoot,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    $actual = @(Get-DirectorySnapshot -Root $ActualRoot)
    if ($Expected.Count -ne $actual.Count) {
        throw "$Context subtitle directory entry count changed. Expected $($Expected.Count), found $($actual.Count)."
    }
    for ($index = 0; $index -lt $Expected.Count; $index++) {
        foreach ($property in @('RelativePath', 'Kind', 'Length', 'Sha256')) {
            if ($Expected[$index].$property -cne $actual[$index].$property) {
                throw "$Context subtitle directory entry changed at index $index property $property."
            }
        }
    }
}

if (-not (Test-Path -LiteralPath $DeployScriptPath)) {
    throw "Batman graphics shell deploy script not found: $DeployScriptPath"
}
if (-not (Test-Path -LiteralPath $ConfigSourcePath)) {
    throw "Batman repository pack config not found: $ConfigSourcePath"
}

$DeployScriptText = Get-Content -LiteralPath $DeployScriptPath -Raw
foreach ($RequiredToken in @(
    '$RebuildScriptPath',
    '& $RebuildScriptPath',
    '$ConfigSourcePath',
    '$ConfigDestinationPath',
    '$ConfigStagingPath',
    '$ConfigBackupPath',
    '$HelenGameHookStagingPath',
    '$ProxyStagingPath',
    '$HelenGameHookBackupPath',
    '$ProxyBackupPath',
    'batman-aa-subtitles',
    'batman-aa-graphics-options',
    'Invoke-AtomicGraphicsDeployment',
    'Move-Item -LiteralPath',
    'Assert-SafeDeploymentPath',
    'Get-SafeDeploymentItems',
    'Get-DirectorySnapshot',
    'Remove-DeploymentStagingRoot',
    'GetPathRoot',
    'Split-Path -Parent $PackDestination',
    'Split-Path -Parent $ConfigDestinationPath'
)) {
    Assert-ContainsOrdinal -Text $DeployScriptText -Token $RequiredToken -Context 'Batman graphics shell deploy source'
}

Assert-NotContainsOrdinal -Text $DeployScriptText -Token '$ConflictingPackDestination' -Context 'Batman graphics shell deploy source'
Assert-NotContainsOrdinal -Text $DeployScriptText -Token 'GetTempPath' -Context 'Batman graphics shell deploy source'
if ($DeployScriptText -match '(?im)Remove-Item[^`r`n]*batman-aa-subtitles') {
    throw 'Batman graphics shell deploy source must not remove the subtitle pack.'
}
$RebuildIndex = $DeployScriptText.IndexOf('& $RebuildScriptPath', [StringComparison]::Ordinal)
$VerifierIndex = $DeployScriptText.IndexOf('& $VerifierPath', [StringComparison]::Ordinal)
if ($RebuildIndex -lt 0 -or $VerifierIndex -lt 0 -or $RebuildIndex -gt $VerifierIndex) {
    throw 'Batman graphics shell deployment must rebuild before package verification.'
}

$ConfigJson = Get-Content -LiteralPath $ConfigSourcePath -Raw | ConvertFrom-Json
$EnabledPacks = @($ConfigJson.enabledPacksByExecutable.'ShippingPC-BmGame.exe')
if ($EnabledPacks.Count -ne 2 -or $EnabledPacks[0] -ne 'batman-aa-subtitles' -or $EnabledPacks[1] -ne 'batman-aa-graphics-options') {
    throw 'Repository Batman pack config must enable exactly subtitles then graphics options.'
}

$TestRoot = Join-Path ([IO.Path]::GetTempPath()) ('HelenBatmanGraphicsDeploymentContract-' + [Guid]::NewGuid().ToString('N'))
$GameBin = Join-Path $TestRoot 'game\Binaries'
$LivePackRoot = Join-Path $GameBin 'helengamehook\packs\batman-aa-graphics-options'
$LiveConfigPath = Join-Path $GameBin 'helengamehook\config\packs.json'
$LiveHelenGameHookPath = Join-Path $GameBin 'HelenGameHook.dll'
$LiveProxyPath = Join-Path $GameBin 'dinput8.dll'
$SubtitlePackRoot = Join-Path $GameBin 'helengamehook\packs\batman-aa-subtitles'
$StagingRoot = Join-Path $GameBin ('.helengamehook-staging-' + [Guid]::NewGuid().ToString('N'))
$RecoveryRoot = Join-Path $GameBin ('.helengamehook-recovery-' + [Guid]::NewGuid().ToString('N'))
$StagedPackRoot = Join-Path $StagingRoot 'pack'
$StagedConfigPath = Join-Path $StagingRoot 'config\packs.json'
$StagedHelenGameHookPath = Join-Path $StagingRoot 'HelenGameHook.dll'
$StagedProxyPath = Join-Path $StagingRoot 'dinput8.dll'
$PackBackupRoot = Join-Path $RecoveryRoot 'graphics-options'
$ConfigBackupPath = Join-Path $RecoveryRoot 'packs.json'
$HelenGameHookBackupPath = Join-Path $RecoveryRoot 'HelenGameHook.dll'
$ProxyBackupPath = Join-Path $RecoveryRoot 'dinput8.dll'

try {
    New-Item -ItemType Directory -Force -Path $GameBin | Out-Null
    . $DeployScriptPath -GameBin $GameBin -BuilderRoot (Join-Path $BatmanRoot 'builder') -FunctionsOnly
    if ([IO.Path]::GetPathRoot($GameBin) -cne [IO.Path]::GetPathRoot($StagingRoot) -or [IO.Path]::GetPathRoot($GameBin) -cne [IO.Path]::GetPathRoot($RecoveryRoot)) {
        throw 'Deployment staging and recovery roots must share the GameBin volume.'
    }
    Reset-DeploymentFixture -LivePackRoot $LivePackRoot -LiveConfigPath $LiveConfigPath -LiveHelenGameHookPath $LiveHelenGameHookPath -LiveProxyPath $LiveProxyPath -SubtitlePackRoot $SubtitlePackRoot -StagingRoot $StagingRoot -RecoveryRoot $RecoveryRoot -StagedPackRoot $StagedPackRoot -StagedConfigPath $StagedConfigPath -StagedHelenGameHookPath $StagedHelenGameHookPath -StagedProxyPath $StagedProxyPath -IncludeExistingGraphicsPack $true
    $ExpectedHelenGameHookHash = (Get-FileHash -LiteralPath $StagedHelenGameHookPath -Algorithm SHA256).Hash
    $ExpectedProxyHash = (Get-FileHash -LiteralPath $StagedProxyPath -Algorithm SHA256).Hash

    $VerifyPublication = {
        param($LivePackPath, $LiveConfigPathForVerification, $LiveHelenPath, $LiveProxyPath)
        if ((Get-Content -LiteralPath (Join-Path $LivePackPath 'state.txt') -Raw) -cne 'new-pack') { throw 'Verifier saw the wrong graphics pack.' }
        $config = Get-Content -LiteralPath $LiveConfigPathForVerification -Raw | ConvertFrom-Json
        $packs = @($config.enabledPacksByExecutable.'ShippingPC-BmGame.exe')
        if ($packs.Count -ne 2 -or $packs[0] -ne 'batman-aa-subtitles' -or $packs[1] -ne 'batman-aa-graphics-options') { throw 'Verifier saw the wrong pack order.' }
        if ((Get-FileHash -LiteralPath $LiveHelenPath -Algorithm SHA256).Hash -cne $ExpectedHelenGameHookHash) { throw 'Verifier saw the wrong HelenGameHook.dll.' }
        if ((Get-FileHash -LiteralPath $LiveProxyPath -Algorithm SHA256).Hash -cne $ExpectedProxyHash) { throw 'Verifier saw the wrong dinput8.dll.' }
    }.GetNewClosure()

    $FailureStages = @(
        'AfterPackBackup', 'AfterConfigBackup', 'AfterHelenGameHookBackup', 'AfterProxyBackup',
        'AfterPackActivation', 'AfterConfigActivation', 'AfterHelenGameHookActivation', 'AfterProxyActivation', 'AfterVerification'
    )
    foreach ($FailureInjection in $FailureStages) {
        Reset-DeploymentFixture -LivePackRoot $LivePackRoot -LiveConfigPath $LiveConfigPath -LiveHelenGameHookPath $LiveHelenGameHookPath -LiveProxyPath $LiveProxyPath -SubtitlePackRoot $SubtitlePackRoot -StagingRoot $StagingRoot -RecoveryRoot $RecoveryRoot -StagedPackRoot $StagedPackRoot -StagedConfigPath $StagedConfigPath -StagedHelenGameHookPath $StagedHelenGameHookPath -StagedProxyPath $StagedProxyPath -IncludeExistingGraphicsPack $true
        $subtitleBefore = @(Get-DirectorySnapshot -Root $SubtitlePackRoot)
        $oldHashes = @{
            Helen = (Get-FileHash -LiteralPath $LiveHelenGameHookPath -Algorithm SHA256).Hash
            Proxy = (Get-FileHash -LiteralPath $LiveProxyPath -Algorithm SHA256).Hash
        }
        $failureObserved = $false
        try {
            Invoke-AtomicGraphicsDeployment -GameBin $GameBin -LivePackRoot $LivePackRoot -LiveConfigPath $LiveConfigPath -LiveHelenGameHookPath $LiveHelenGameHookPath -LiveProxyPath $LiveProxyPath -StagedPackRoot $StagedPackRoot -StagedConfigPath $StagedConfigPath -StagedHelenGameHookPath $StagedHelenGameHookPath -StagedProxyPath $StagedProxyPath -PackBackupRoot $PackBackupRoot -ConfigBackupPath $ConfigBackupPath -HelenGameHookBackupPath $HelenGameHookBackupPath -ProxyBackupPath $ProxyBackupPath -RecoveryRoot $RecoveryRoot -StagingRoot $StagingRoot -VerifyPublication $VerifyPublication -FailureInjection $FailureInjection
        } catch { $failureObserved = $true }
        if (-not $failureObserved) { throw "Failure injection '$FailureInjection' did not fail." }
        if ((Get-Content -LiteralPath (Join-Path $LivePackRoot 'state.txt') -Raw) -cne 'old-pack') { throw "Failure injection '$FailureInjection' did not restore graphics pack." }
        $restoredConfig = Get-Content -LiteralPath $LiveConfigPath -Raw | ConvertFrom-Json
        $restoredPacks = @($restoredConfig.enabledPacksByExecutable.'ShippingPC-BmGame.exe')
        if ($restoredPacks.Count -ne 1 -or $restoredPacks[0] -ne 'old-pack') { throw "Failure injection '$FailureInjection' did not restore config." }
        if ((Get-FileHash -LiteralPath $LiveHelenGameHookPath -Algorithm SHA256).Hash -cne $oldHashes.Helen -or (Get-FileHash -LiteralPath $LiveProxyPath -Algorithm SHA256).Hash -cne $oldHashes.Proxy) { throw "Failure injection '$FailureInjection' did not restore runtime DLLs." }
        Assert-DirectorySnapshotEqual -Expected $subtitleBefore -ActualRoot $SubtitlePackRoot -Context "Failure injection '$FailureInjection'"
        if ((Test-Path -LiteralPath $RecoveryRoot) -and @(Get-ChildItem -LiteralPath $RecoveryRoot -Recurse -Force).Count -ne 0) { throw "Failure injection '$FailureInjection' left recovery entries after rollback: $((Get-ChildItem -LiteralPath $RecoveryRoot -Recurse -Force | Select-Object -ExpandProperty FullName) -join '; ')" }
    }

    Reset-DeploymentFixture -LivePackRoot $LivePackRoot -LiveConfigPath $LiveConfigPath -LiveHelenGameHookPath $LiveHelenGameHookPath -LiveProxyPath $LiveProxyPath -SubtitlePackRoot $SubtitlePackRoot -StagingRoot $StagingRoot -RecoveryRoot $RecoveryRoot -StagedPackRoot $StagedPackRoot -StagedConfigPath $StagedConfigPath -StagedHelenGameHookPath $StagedHelenGameHookPath -StagedProxyPath $StagedProxyPath -IncludeExistingGraphicsPack $true
    $subtitleBefore = @(Get-DirectorySnapshot -Root $SubtitlePackRoot)
    Invoke-AtomicGraphicsDeployment -GameBin $GameBin -LivePackRoot $LivePackRoot -LiveConfigPath $LiveConfigPath -LiveHelenGameHookPath $LiveHelenGameHookPath -LiveProxyPath $LiveProxyPath -StagedPackRoot $StagedPackRoot -StagedConfigPath $StagedConfigPath -StagedHelenGameHookPath $StagedHelenGameHookPath -StagedProxyPath $StagedProxyPath -PackBackupRoot $PackBackupRoot -ConfigBackupPath $ConfigBackupPath -HelenGameHookBackupPath $HelenGameHookBackupPath -ProxyBackupPath $ProxyBackupPath -RecoveryRoot $RecoveryRoot -StagingRoot $StagingRoot -VerifyPublication $VerifyPublication
    Assert-DirectorySnapshotEqual -Expected $subtitleBefore -ActualRoot $SubtitlePackRoot -Context 'Successful deployment'
    if ((Test-Path -LiteralPath $RecoveryRoot) -and @(Get-ChildItem -LiteralPath $RecoveryRoot -Recurse -Force).Count -ne 0) { throw 'Successful deployment left recovery entries.' }

    Reset-DeploymentFixture -LivePackRoot $LivePackRoot -LiveConfigPath $LiveConfigPath -LiveHelenGameHookPath $LiveHelenGameHookPath -LiveProxyPath $LiveProxyPath -SubtitlePackRoot $SubtitlePackRoot -StagingRoot $StagingRoot -RecoveryRoot $RecoveryRoot -StagedPackRoot $StagedPackRoot -StagedConfigPath $StagedConfigPath -StagedHelenGameHookPath $StagedHelenGameHookPath -StagedProxyPath $StagedProxyPath -IncludeExistingGraphicsPack $false
    $subtitleBefore = @(Get-DirectorySnapshot -Root $SubtitlePackRoot)
    $cleanupFailureObserved = $false
    $cleanupFailureMessage = ''
    try {
        Invoke-AtomicGraphicsDeployment -GameBin $GameBin -LivePackRoot $LivePackRoot -LiveConfigPath $LiveConfigPath -LiveHelenGameHookPath $LiveHelenGameHookPath -LiveProxyPath $LiveProxyPath -StagedPackRoot $StagedPackRoot -StagedConfigPath $StagedConfigPath -StagedHelenGameHookPath $StagedHelenGameHookPath -StagedProxyPath $StagedProxyPath -PackBackupRoot $PackBackupRoot -ConfigBackupPath $ConfigBackupPath -HelenGameHookBackupPath $HelenGameHookBackupPath -ProxyBackupPath $ProxyBackupPath -RecoveryRoot $RecoveryRoot -StagingRoot $StagingRoot -VerifyPublication $VerifyPublication -FailureInjection 'AfterConfigBackupDeletion'
    } catch {
        $cleanupFailureObserved = $true
        $cleanupFailureMessage = $_.Exception.Message
    }
    if (-not $cleanupFailureObserved) { throw 'Absent-graphics cleanup failure injection did not fail.' }
    if ($cleanupFailureMessage -notmatch 'Remaining recovery paths:') { throw 'Cleanup failure did not report remaining recovery paths.' }
    if ($cleanupFailureMessage -match [Regex]::Escape($ConfigBackupPath)) { throw 'Cleanup failure falsely reported the deleted config backup as recoverable.' }
    if (Test-Path -LiteralPath $ConfigBackupPath) { throw 'Cleanup failure unexpectedly retained the deleted config backup.' }
    foreach ($actualRecoveryPath in @(Get-ChildItem -LiteralPath $RecoveryRoot -Recurse -Force -File | Select-Object -ExpandProperty FullName)) {
        if ($cleanupFailureMessage.IndexOf($actualRecoveryPath, [StringComparison]::OrdinalIgnoreCase) -lt 0) { throw "Cleanup failure omitted surviving recovery path: $actualRecoveryPath" }
    }
    if ((Test-Path -LiteralPath $LivePackRoot) -and (Test-Path -LiteralPath $LiveConfigPath) -and (Test-Path -LiteralPath $LiveHelenGameHookPath) -and (Test-Path -LiteralPath $LiveProxyPath)) {
        Assert-DirectorySnapshotEqual -Expected $subtitleBefore -ActualRoot $SubtitlePackRoot -Context 'Absent-graphics cleanup failure'
    } else {
        throw 'Absent-graphics cleanup failure did not leave all verified new artifacts active.'
    }

    $OuterStagingRoot = Join-Path $GameBin '.outer-staging-fixture'
    $OuterRecoveryRoot = Join-Path $GameBin '.outer-recovery-fixture'
    New-Item -ItemType Directory -Force -Path (Join-Path $OuterStagingRoot 'nested'), $OuterRecoveryRoot | Out-Null
    [IO.File]::WriteAllText((Join-Path $OuterStagingRoot 'nested\staged.txt'), 'staging')
    [IO.File]::WriteAllText((Join-Path $OuterRecoveryRoot 'survivor.txt'), 'recoverable')
    Remove-DeploymentStagingRoot -StagingRoot $OuterStagingRoot -GameBin $GameBin
    if (Test-Path -LiteralPath $OuterStagingRoot) { throw 'Outer staging cleanup did not remove staging root.' }
    if (-not (Test-Path -LiteralPath (Join-Path $OuterRecoveryRoot 'survivor.txt'))) { throw 'Outer staging cleanup removed recovery state.' }

    $ReparseRoot = Join-Path $TestRoot 'reparse'
    $ReparseTarget = Join-Path $ReparseRoot 'target'
    $ReparseLink = Join-Path $ReparseRoot 'link'
    New-Item -ItemType Directory -Force -Path $ReparseTarget | Out-Null
    $reparseCreated = $false
    try {
        New-Item -ItemType Junction -Path $ReparseLink -Target $ReparseTarget -ErrorAction Stop | Out-Null
        $reparseCreated = $true
    } catch {
        try {
            New-Item -ItemType SymbolicLink -Path $ReparseLink -Target $ReparseTarget -ErrorAction Stop | Out-Null
            $reparseCreated = $true
        } catch {
            throw "Unable to evaluate the required reparse-point fixture: $($_.Exception.Message)"
        }
    }
    if (-not $reparseCreated) { throw 'Required reparse-point fixture was not created.' }
    $reparseRejected = $false
    try { Assert-SafeDeploymentPath -Path $ReparseLink -AllowedRoots @($GameBin) -RequireExisting } catch { $reparseRejected = $true }
    if (-not $reparseRejected) { throw 'Safe path validator accepted a reparse-point target.' }
    $reparseDescendantRejected = $false
    try { Assert-SafeDeploymentPath -Path (Join-Path $ReparseLink 'payload.txt') -AllowedRoots @($GameBin) } catch { $reparseDescendantRejected = $true }
    if (-not $reparseDescendantRejected) { throw 'Safe path validator accepted a path through a reparse-point ancestor.' }

    Write-Output 'PASS'
}
finally {
    if (Test-Path -LiteralPath $TestRoot) { Remove-Item -LiteralPath $TestRoot -Recurse -Force }
}
