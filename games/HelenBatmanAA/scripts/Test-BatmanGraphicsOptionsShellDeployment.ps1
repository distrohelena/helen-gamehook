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
    'batman-aa-subtitles',
    'batman-aa-graphics-options',
    'Invoke-AtomicGraphicsDeployment',
    'Move-Item -LiteralPath'
)) {
    Assert-ContainsOrdinal -Text $DeployScriptText -Token $RequiredToken -Context 'Batman graphics shell deploy source'
}

Assert-NotContainsOrdinal -Text $DeployScriptText -Token '$ConflictingPackDestination' -Context 'Batman graphics shell deploy source'
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
if ($EnabledPacks.Count -ne 2) {
    throw "Repository Batman pack config must contain exactly two enabled packs, found $($EnabledPacks.Count)."
}
if ($EnabledPacks[0] -ne 'batman-aa-subtitles' -or $EnabledPacks[1] -ne 'batman-aa-graphics-options') {
    throw 'Repository Batman pack config must enable subtitles before graphics options.'
}

$TempRoot = Join-Path ([IO.Path]::GetTempPath()) ('HelenBatmanGraphicsDeploymentContract-' + [Guid]::NewGuid().ToString('N'))
$GameBin = Join-Path $TempRoot 'game\Binaries'
$LivePackRoot = Join-Path $GameBin 'helengamehook\packs\batman-aa-graphics-options'
$LiveConfigPath = Join-Path $GameBin 'helengamehook\config\packs.json'
$SubtitlePackRoot = Join-Path $GameBin 'helengamehook\packs\batman-aa-subtitles'
$StagedPackRoot = Join-Path $TempRoot 'staging\pack'
$StagedConfigPath = Join-Path $TempRoot 'staging\config\packs.json'
$BackupRoot = Join-Path ([IO.Path]::GetTempPath()) ('HelenBatmanGraphicsDeploymentRecovery-' + [Guid]::NewGuid().ToString('N'))

try {
    New-Item -ItemType Directory -Force -Path $LivePackRoot, $StagedPackRoot, (Split-Path -Parent $LiveConfigPath), (Split-Path -Parent $StagedConfigPath), $SubtitlePackRoot, $BackupRoot | Out-Null
    [IO.File]::WriteAllText((Join-Path $LivePackRoot 'state.txt'), 'old-pack')
    [IO.File]::WriteAllText($LiveConfigPath, '{"enabledPacksByExecutable":{"ShippingPC-BmGame.exe":["old-pack"]}}')
    [IO.File]::WriteAllText((Join-Path $SubtitlePackRoot 'state.txt'), 'subtitle-pack')
    [IO.File]::WriteAllText((Join-Path $StagedPackRoot 'state.txt'), 'new-pack')
    [IO.File]::WriteAllText($StagedConfigPath, (Get-Content -LiteralPath $ConfigSourcePath -Raw))

    . $DeployScriptPath -GameBin $GameBin -BuilderRoot (Join-Path $BatmanRoot 'builder') -FunctionsOnly

    $verify = {
        param($LivePackRootForVerification, $LiveConfigPathForVerification)
        if ((Get-Content -LiteralPath (Join-Path $LivePackRootForVerification 'state.txt') -Raw) -cne 'new-pack') {
            throw 'Injected deployment verifier saw the wrong graphics pack.'
        }
        $config = Get-Content -LiteralPath $LiveConfigPathForVerification -Raw | ConvertFrom-Json
        $packs = @($config.enabledPacksByExecutable.'ShippingPC-BmGame.exe')
        if ($packs.Count -ne 2 -or $packs[0] -ne 'batman-aa-subtitles' -or $packs[1] -ne 'batman-aa-graphics-options') {
            throw 'Injected deployment verifier saw the wrong pack order.'
        }
    }

    foreach ($FailureInjection in @('AfterPackBackup', 'AfterConfigBackup', 'AfterPackActivation', 'AfterConfigActivation', 'AfterVerification')) {
        Remove-Item -LiteralPath $LivePackRoot -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $LiveConfigPath -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force -Path $LivePackRoot, (Split-Path -Parent $LiveConfigPath) | Out-Null
        [IO.File]::WriteAllText((Join-Path $LivePackRoot 'state.txt'), 'old-pack')
        [IO.File]::WriteAllText($LiveConfigPath, '{"enabledPacksByExecutable":{"ShippingPC-BmGame.exe":["old-pack"]}}')

        Remove-Item -LiteralPath $StagedPackRoot -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $StagedConfigPath -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force -Path $StagedPackRoot, (Split-Path -Parent $StagedConfigPath) | Out-Null
        [IO.File]::WriteAllText((Join-Path $StagedPackRoot 'state.txt'), 'new-pack')
        [IO.File]::WriteAllText($StagedConfigPath, (Get-Content -LiteralPath $ConfigSourcePath -Raw))

        $FailureObserved = $false
        try {
            Invoke-AtomicGraphicsDeployment -LivePackRoot $LivePackRoot -LiveConfigPath $LiveConfigPath -StagedPackRoot $StagedPackRoot -StagedConfigPath $StagedConfigPath -PackBackupRoot (Join-Path $BackupRoot 'pack') -ConfigBackupPath (Join-Path $BackupRoot 'packs.json') -BackupRoot $BackupRoot -TempRoot $TempRoot -VerifyPublication $verify -FailureInjection $FailureInjection
        } catch {
            $FailureObserved = $true
        }
        if (-not $FailureObserved) {
            throw "Failure injection '$FailureInjection' did not fail the deployment transaction."
        }
        if ((Get-Content -LiteralPath (Join-Path $LivePackRoot 'state.txt') -Raw) -cne 'old-pack') {
            throw "Failure injection '$FailureInjection' did not restore the previous graphics pack."
        }
        $RestoredConfig = Get-Content -LiteralPath $LiveConfigPath -Raw | ConvertFrom-Json
        $RestoredPacks = @($RestoredConfig.enabledPacksByExecutable.'ShippingPC-BmGame.exe')
        if ($RestoredPacks.Count -ne 1 -or $RestoredPacks[0] -ne 'old-pack') {
            throw "Failure injection '$FailureInjection' did not restore the previous pack config."
        }
        if (-not (Test-Path -LiteralPath $SubtitlePackRoot)) {
            throw "Failure injection '$FailureInjection' removed the subtitle pack."
        }
    }

    Remove-Item -LiteralPath $LivePackRoot -Recurse -Force
    Remove-Item -LiteralPath $LiveConfigPath -Force
    New-Item -ItemType Directory -Force -Path $LivePackRoot, (Split-Path -Parent $LiveConfigPath) | Out-Null
    [IO.File]::WriteAllText((Join-Path $LivePackRoot 'state.txt'), 'old-pack')
    [IO.File]::WriteAllText($LiveConfigPath, '{"enabledPacksByExecutable":{"ShippingPC-BmGame.exe":["old-pack"]}}')
    Remove-Item -LiteralPath $StagedPackRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $StagedConfigPath -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $StagedPackRoot, (Split-Path -Parent $StagedConfigPath) | Out-Null
    [IO.File]::WriteAllText((Join-Path $StagedPackRoot 'state.txt'), 'new-pack')
    [IO.File]::WriteAllText($StagedConfigPath, (Get-Content -LiteralPath $ConfigSourcePath -Raw))
    Invoke-AtomicGraphicsDeployment -LivePackRoot $LivePackRoot -LiveConfigPath $LiveConfigPath -StagedPackRoot $StagedPackRoot -StagedConfigPath $StagedConfigPath -PackBackupRoot (Join-Path $BackupRoot 'pack') -ConfigBackupPath (Join-Path $BackupRoot 'packs.json') -BackupRoot $BackupRoot -TempRoot $TempRoot -VerifyPublication $verify
    if ((Get-Content -LiteralPath (Join-Path $LivePackRoot 'state.txt') -Raw) -cne 'new-pack') {
        throw 'Successful deployment did not activate the new graphics pack.'
    }
    $SuccessfulConfig = Get-Content -LiteralPath $LiveConfigPath -Raw | ConvertFrom-Json
    $SuccessfulPacks = @($SuccessfulConfig.enabledPacksByExecutable.'ShippingPC-BmGame.exe')
    if ($SuccessfulPacks.Count -ne 2 -or $SuccessfulPacks[0] -ne 'batman-aa-subtitles' -or $SuccessfulPacks[1] -ne 'batman-aa-graphics-options') {
        throw 'Successful deployment did not activate the expected pack order.'
    }
    if (Test-Path -LiteralPath $BackupRoot) {
        throw 'Successful deployment left recovery backups behind.'
    }

    Remove-Item -LiteralPath $LivePackRoot -Recurse -Force
    Remove-Item -LiteralPath $LiveConfigPath -Force
    New-Item -ItemType Directory -Force -Path $LivePackRoot, (Split-Path -Parent $LiveConfigPath) | Out-Null
    [IO.File]::WriteAllText((Join-Path $LivePackRoot 'state.txt'), 'old-pack')
    [IO.File]::WriteAllText($LiveConfigPath, '{"enabledPacksByExecutable":{"ShippingPC-BmGame.exe":["old-pack"]}}')
    Remove-Item -LiteralPath $StagedPackRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $StagedConfigPath -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $StagedPackRoot, (Split-Path -Parent $StagedConfigPath) | Out-Null
    [IO.File]::WriteAllText((Join-Path $StagedPackRoot 'state.txt'), 'new-pack')
    [IO.File]::WriteAllText($StagedConfigPath, (Get-Content -LiteralPath $ConfigSourcePath -Raw))

    $CleanupFailureObserved = $false
    try {
        Invoke-AtomicGraphicsDeployment -LivePackRoot $LivePackRoot -LiveConfigPath $LiveConfigPath -StagedPackRoot $StagedPackRoot -StagedConfigPath $StagedConfigPath -PackBackupRoot (Join-Path $BackupRoot 'pack') -ConfigBackupPath (Join-Path $BackupRoot 'packs.json') -BackupRoot $BackupRoot -TempRoot $TempRoot -VerifyPublication $verify -FailureInjection 'AfterBackupDeletion'
    } catch {
        $CleanupFailureObserved = $true
    }
    if (-not $CleanupFailureObserved) {
        throw 'Backup-cleanup failure injection did not fail the deployment transaction.'
    }
    if ((Get-Content -LiteralPath (Join-Path $LivePackRoot 'state.txt') -Raw) -cne 'new-pack') {
        throw 'Backup-cleanup failure did not leave the verified graphics pack active.'
    }
    if (-not (Test-Path -LiteralPath (Join-Path $BackupRoot 'packs.json'))) {
        throw 'Backup-cleanup failure did not retain the recoverable config backup.'
    }
    if ([IO.Path]::GetFullPath($BackupRoot).StartsWith(([IO.Path]::GetFullPath($TempRoot).TrimEnd('\') + '\'), [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Recovery backups must not be placed inside the staging tree.'
    }

    Write-Output 'PASS'
}
finally {
    if (Test-Path -LiteralPath $TempRoot) {
        Remove-Item -LiteralPath $TempRoot -Recurse -Force
    }
    if (Test-Path -LiteralPath $BackupRoot) {
        Remove-Item -LiteralPath $BackupRoot -Recurse -Force
    }
}
