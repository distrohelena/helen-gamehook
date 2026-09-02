param(
    [string]$BatmanRoot,
    [string]$BuilderRoot,
    [string]$Configuration = 'Release',
    [string]$PackRootOverride,
    [string]$TargetPathOverride,
    [switch]$StagedPackageValidation
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'BatmanBuilderWorkspaceHelpers.ps1')
. (Join-Path $PSScriptRoot 'BatmanPackVerificationHelpers.ps1')

function Assert-ExactProperties {
    param(
        [Parameter(Mandatory = $true)] [psobject]$Object,
        [Parameter(Mandatory = $true)] [string[]]$Names,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    $actual = @($Object.PSObject.Properties.Name | Sort-Object)
    $expected = @($Names | Sort-Object)
    if (($actual -join '|') -cne ($expected -join '|')) {
        throw "$Context properties drifted. Expected '$($expected -join ', ')' but found '$($actual -join ', ')'."
    }
}

function Assert-ExactOrderedProperties {
    param(
        [Parameter(Mandatory = $true)] [psobject]$Object,
        [Parameter(Mandatory = $true)] [string[]]$Names,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    $actual = @($Object.PSObject.Properties.Name)
    if (($actual -join '|') -cne ($Names -join '|')) {
        throw "$Context properties drifted. Expected '$($Names -join ', ')' but found '$($actual -join ', ')'."
    }
}

function Assert-ContainsOrdinal {
    param(
        [Parameter(Mandatory = $true)] [AllowEmptyString()] [string]$Text,
        [Parameter(Mandatory = $true)] [string]$Token,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    if ($Text.IndexOf($Token, [System.StringComparison]::Ordinal) -lt 0) {
        throw "$Context is missing required token '$Token'."
    }
}

function Assert-NotContainsOrdinal {
    param(
        [Parameter(Mandatory = $true)] [AllowEmptyString()] [string]$Text,
        [Parameter(Mandatory = $true)] [string]$Token,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    if ($Text.IndexOf($Token, [System.StringComparison]::Ordinal) -ge 0) {
        throw "$Context contains forbidden token '$Token'."
    }
}

function Invoke-ExternalProcess {
    param(
        [Parameter(Mandatory = $true)] [string]$FilePath,
        [Parameter(Mandatory = $true)] [string[]]$Arguments
    )

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { $output = @(& $FilePath @Arguments 2>&1) } finally { $ErrorActionPreference = $previousErrorActionPreference }
    return [pscustomobject]@{
        ExitCode = $LASTEXITCODE
        Output = $output
    }
}

function Read-UInt32LittleEndian {
    param([byte[]]$Bytes, [int]$Offset)
    return [BitConverter]::ToUInt32($Bytes, $Offset)
}

function Read-UInt64LittleEndian {
    param([byte[]]$Bytes, [int]$Offset)
    if ($Offset -lt 0 -or $Offset -gt ($Bytes.Length - 8)) { throw "HGDL read exceeded the byte buffer at offset $Offset." }
    return [BitConverter]::ToUInt64($Bytes, $Offset)
}

function Assert-ExactPackFileSet {
    param(
        [Parameter(Mandatory = $true)] [string]$Root,
        [Parameter(Mandatory = $true)] [string]$BuildDirectoryName
    )

    $expected = @(
        'pack.json',
        "builds\$BuildDirectoryName\build.json",
        "builds\$BuildDirectoryName\bindings.json",
        "builds\$BuildDirectoryName\commands.json",
        "builds\$BuildDirectoryName\hooks.json",
        "builds\$BuildDirectoryName\files.json",
        "builds\$BuildDirectoryName\assets\deltas\Frontend-graphics-options.hgdelta"
    ) | Sort-Object
    $rootPrefix = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $actual = @(Get-ChildItem -LiteralPath $Root -Recurse -Force -File | ForEach-Object {
        $_.FullName.Substring($rootPrefix.Length).Replace('/', '\')
    } | Sort-Object)
    if (($actual -join '|') -cne ($expected -join '|')) {
        throw "Graphics-options pack file set drifted. Expected '$($expected -join ', ')' but found '$($actual -join ', ')'."
    }
}

function Assert-ExactGeneratedTargetFileSet {
    param([Parameter(Mandatory = $true)] [string]$Root, [Parameter(Mandatory = $true)] [string]$TargetPath)
    if (-not (Test-Path -LiteralPath $Root -PathType Container)) { throw "Stable generated output root was not found: $Root" }
    $rootPrefix = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $expected = @([IO.Path]::GetFileName($TargetPath))
    $actual = @(Get-ChildItem -LiteralPath $Root -Recurse -Force -File | ForEach-Object { $_.FullName.Substring($rootPrefix.Length).Replace('/', '\') } | Sort-Object)
    if (($actual -join '|') -cne ($expected -join '|')) {
        $expectedText = $expected -join ', '
        $actualText = $actual -join ', '
        throw "Stable generated output file set drifted. Expected '$expectedText' but found '$actualText'."
    }
}

function Assert-RebuildAtomicSourceContract {
    param([Parameter(Mandatory = $true)] [string]$ScriptPath)

    $source = Get-Content -LiteralPath $ScriptPath -Raw
    foreach ($token in @(
        '$stagedPackRoot', '$stagedTargetPath', '$stagedDeltaPath', '$packBackupRoot', '$targetBackupPath',
        'Assert-GraphicsPublicationSameVolume', 'Move-Item -LiteralPath', '-OutputFile $stagedDeltaPath', 'Restore-AtomicRebuild', 'catch', 'finally'
    )) {
        Assert-ContainsOrdinal -Text $source -Token $token -Context 'atomic graphics-options rebuild source'
    }
    if ($source.IndexOf('-OutputFile $deltaPath', [StringComparison]::Ordinal) -ge 0) {
        throw 'Rebuild must never write hgdelta output directly into the checked-in pack.'
    }
    if ($source.IndexOf('$stagedPackRoot = $packRoot', [StringComparison]::OrdinalIgnoreCase) -ge 0 -or
        $source.IndexOf('$stagedTargetPath = $targetPath', [StringComparison]::OrdinalIgnoreCase) -ge 0) {
        throw 'Rebuild staging paths must differ from live pack and target paths.'
    }
    if ($source -notmatch '(?s)catch\s*\{.*Restore-AtomicRebuild') {
        throw 'Rebuild catch path must restore the live pack and target after activation failure.'
    }
}

function Get-HgdeltaFunctionBody {
    param(
        [Parameter(Mandatory = $true)] [string]$Text,
        [Parameter(Mandatory = $true)] [string]$FunctionAssignment
    )

    $match = [regex]::Match($Text, [regex]::Escape($FunctionAssignment) + '\s*=\s*function\s*\(\s*\)\s*\{(?<body>.*?)\}', [Text.RegularExpressions.RegexOptions]::Singleline)
    if (-not $match.Success) { throw "Missing no-op function '$FunctionAssignment'." }
    return $match.Groups['body'].Value.Trim()
}

function Get-VsyncSliceRowActionBody {
    param([string]$Text, [string]$Assignment)
    return Get-HgdeltaFunctionBody -Text $Text -FunctionAssignment $Assignment
}

function Assert-NoOpVsyncSliceRowActions {
    param([string]$Text, [string]$Context)
    foreach ($action in @('RunAction', 'Increment', 'Decrement', 'ShowPrompt')) {
        if ((Get-VsyncSliceRowActionBody -Text $Text -Assignment "this.$action") -ne '') { throw "$Context action $action must be a no-op." }
    }
}

function Assert-VsyncSliceRowContract {
    param([string]$Text, [int]$Index, [string]$Context)

    $labels = @('Fullscreen', 'Resolution', 'VSync', 'MSAA', 'Detail Level', 'Bloom', 'Dynamic Shadows', 'Motion Blur', 'Distortion', 'Fog Volumes', 'Spherical Harmonic Lighting', 'Ambient Occlusion', 'PhysX', 'Stereo 3D', 'Apply Changes')
    if ($Index -eq 2) {
        Assert-ContainsOrdinal -Text $Text -Token 'this.Names = new Array("Off","On");' -Context "$Context names"
        Assert-ContainsOrdinal -Text $Text -Token 'this.State = _parent.GraphicsVsyncController.DraftVsync;' -Context "$Context initial state"
        Assert-ContainsOrdinal -Text $Text -Token '_parent.GraphicsVsyncController.ToggleVsync();' -Context "$Context RunAction"
        Assert-ContainsOrdinal -Text $Text -Token '_parent.GraphicsVsyncController.IncrementVsync();' -Context "$Context Increment"
        Assert-ContainsOrdinal -Text $Text -Token '_parent.GraphicsVsyncController.DecrementVsync();' -Context "$Context Decrement"
        Assert-ContainsOrdinal -Text $Text -Token $labels[$Index] -Context "$Context label"
        Assert-ContainsOrdinal -Text $Text -Token 'this._visible = true;' -Context "$Context visibility"
        return
    }

    if ($Index -eq 14) {
        Assert-ContainsOrdinal -Text $Text -Token 'this.Names = new Array("");' -Context "$Context names"
        Assert-ContainsOrdinal -Text $Text -Token 'Apply Changes' -Context "$Context label"
        Assert-ContainsOrdinal -Text $Text -Token 'this.ItemText.text = "";' -Context "$Context value"
        Assert-ContainsOrdinal -Text $Text -Token 'this._visible = true;' -Context "$Context visibility"
        Assert-ContainsOrdinal -Text $Text -Token '_parent.GraphicsVsyncController.ApplyChanges();' -Context "$Context RunAction"
        foreach ($action in @('Increment', 'Decrement')) {
            if ((Get-VsyncSliceRowActionBody -Text $Text -Assignment "this.$action") -ne '') { throw "$Context action $action must be a no-op." }
        }
        return
    }

    Assert-ContainsOrdinal -Text $Text -Token 'this.Names = new Array("Not active");' -Context "$Context names"
    Assert-ContainsOrdinal -Text $Text -Token 'if(this.ItemText != undefined)' -Context "$Context ItemText guard"
    Assert-ContainsOrdinal -Text $Text -Token 'this.ItemText.text = "Not active";' -Context "$Context visible value"
    Assert-ContainsOrdinal -Text $Text -Token $labels[$Index] -Context "$Context label"
    Assert-ContainsOrdinal -Text $Text -Token 'this._visible = true;' -Context "$Context visibility"
    Assert-NoOpVsyncSliceRowActions -Text $Text -Context $Context
}

function Assert-ScopedExportedShellContract {
    param([Parameter(Mandatory = $true)] [string]$ExportRoot)

    $scriptsRoot = Join-Path $ExportRoot 'scripts'
    $scriptFiles = @(Get-ChildItem -LiteralPath $scriptsRoot -Recurse -Filter *.as -File)
    if ($scriptFiles.Count -eq 0) { throw 'FFDec exported no ActionScript files.' }
    $screenDirectories = @(Get-ChildItem -LiteralPath $scriptsRoot -Recurse -Directory | Where-Object { $_.Name -eq 'DefineSprite_600_ScreenOptionsGraphics' })
    if ($screenDirectories.Count -ne 1) { throw "Expected exactly one DefineSprite_600_ScreenOptionsGraphics directory, found $($screenDirectories.Count)." }
    $screenDirectory = $screenDirectories[0]
    $screenScriptPath = Join-Path $screenDirectory.FullName 'frame_1\DoAction.as'
    if (-not (Test-Path -LiteralPath $screenScriptPath)) { throw "Missing known Options Graphics screen script: $screenScriptPath" }
    $screenText = Get-Content -LiteralPath $screenScriptPath -Raw

    $menuScriptPath = Join-Path $scriptsRoot 'DefineSprite_333_ScreenOptionsMenu\frame_1\PlaceObject2_117_GenericButton_37\CLIPACTIONRECORD onClipEvent(load).as'
    if (-not (Test-Path -LiteralPath $menuScriptPath)) { throw "Missing known Options menu script: $menuScriptPath" }
    $menuText = Get-Content -LiteralPath $menuScriptPath -Raw
    Assert-ContainsOrdinal -Text $menuText -Token 'GotoScreen("OptionsGraphics")' -Context 'Options menu script'
    Assert-ContainsOrdinal -Text $menuText -Token 'Graphics Options' -Context 'Options menu script'

    foreach ($token in @('Graphics Options', 'CancelScreen', 'ReturnFromScreen', 'FE_SetActiveScreenName","Graphics Options')) {
        Assert-ContainsOrdinal -Text $screenText -Token $token -Context 'Options Graphics screen script'
    }
    foreach ($token in @('rs.ui.BatmanGraphicsVsyncController', 'InitialVsync', 'DraftVsync', 'InitialStateResolved', 'InitialStateFailed', 'return this.InitialStateResolved && !this.ApplyInProgress && this.IsDirty();', 'this.InitialVsync = this.DraftVsync;', 'FE_SetControlType",4200', 'FE_GetControlType')) {
        Assert-ContainsOrdinal -Text $screenText -Token $token -Context 'Options Graphics screen script'
    }
    if ($screenText -notmatch 'IncrementVsync\s*=\s*function\s*\(\)\s*\{\s*this\.SetVsync\(this\.DraftVsync\s*==\s*0\s*\?\s*1\s*:\s*0,true\);\s*\}') { throw 'IncrementVsync must wrap DraftVsync through the guarded setter.' }
    if ($screenText -notmatch 'DecrementVsync\s*=\s*function\s*\(\)\s*\{\s*this\.SetVsync\(this\.DraftVsync\s*==\s*0\s*\?\s*1\s*:\s*0,false\);\s*\}') { throw 'DecrementVsync must wrap DraftVsync through the guarded setter.' }
    Assert-ContainsOrdinal -Text $screenText -Token 'setInterval(this,"CompleteApply",1000)' -Context 'Options Graphics screen timer'
    Assert-ContainsOrdinal -Text $screenText -Token 'this.Screen.BlockInput(true);' -Context 'Options Graphics apply input block'
    Assert-ContainsOrdinal -Text $screenText -Token 'this.Screen.BlockInput(false);' -Context 'Options Graphics apply input unblock'
    if ($screenText -notmatch 'FE_SetControlType",4210\s*\+\s*this\.DraftVsync\s*,\s*""\s*\)') { throw 'Options Graphics screen script must dispatch FE_SetControlType 4210 plus DraftVsync with an empty second argument.' }
    if ($screenText -notmatch 'FE_SetControlType",4990\s*\+\s*this\.ApplySignalToggle\s*,\s*""\s*\)') { throw 'Options Graphics screen script must dispatch FE_SetControlType 4990 plus ApplySignalToggle with an empty second argument.' }

    $rowDepths = @('141', '133', '125', '117', '109', '101', '93', '85', '77', '69', '61', '53', '45', '37', '29')
    for ($index = 0; $index -lt $rowDepths.Count; $index++) {
        $rowPath = Join-Path $screenDirectory.FullName "frame_1\PlaceObject2_290_List_Template_$($rowDepths[$index])\CLIPACTIONRECORD onClipEvent(load).as"
        if (-not (Test-Path -LiteralPath $rowPath)) { throw "Missing known Graphics Options row script: $rowPath" }
        $rowText = Get-Content -LiteralPath $rowPath -Raw
        if ($index -lt 15) {
            if ($rowText -notmatch 'this\.(?:Label\.)?Label\.Text\.text\s*=\s*"' -and $rowText -notmatch 'this\.Label\.Text\.text\s*=\s*"') { throw "Graphics row $($index + 1) is missing its fixed label assignment." }
            Assert-VsyncSliceRowContract -Text $rowText -Index $index -Context "Graphics row $($index + 1)"
        } else {
            throw 'Unexpected graphics row index.'
        }
    }

    foreach ($forbidden in @('Helen_GetInt', 'Helen_SetInt', 'Helen_RunCommand', 'Helen_ApplyBatmanGraphicsDraft', 'GraphicsExitPrompt', 'CaptureInitialState', 'ApplyWasDispatched')) {
        Assert-NotContainsOrdinal -Text $screenText -Token $forbidden -Context 'Options Graphics screen script'
    }
    foreach ($forbidden in @('Helen_', 'GraphicsExitPrompt', 'loadBatmanGraphicsDraftIntoConfig', 'applyBatmanGraphicsDraft', 'Unsaved graphics changes', 'Some changes require a restart')) {
        foreach ($scriptFile in $scriptFiles) { Assert-NotContainsOrdinal -Text (Get-Content -LiteralPath $scriptFile.FullName -Raw) -Token $forbidden -Context "exported script $($scriptFile.Name)" }
    }
}

function Set-HgdeltaUInt32Fixture {
    param([byte[]]$Bytes, [int]$Offset, [uint32]$Value)
    [Buffer]::BlockCopy([BitConverter]::GetBytes($Value), 0, $Bytes, $Offset, 4)
}

function Set-HgdeltaUInt64Fixture {
    param([byte[]]$Bytes, [int]$Offset, [uint64]$Value)
    [Buffer]::BlockCopy([BitConverter]::GetBytes($Value), 0, $Bytes, $Offset, 8)
}

function Assert-HgdeltaRejected {
    param(
        [Parameter(Mandatory = $true)] [string]$BasePath,
        [Parameter(Mandatory = $true)] [byte[]]$Bytes,
        [Parameter(Mandatory = $true)] [string]$Path,
        [Parameter(Mandatory = $true)] [string]$CaseName
    )
    [IO.File]::WriteAllBytes($Path, $Bytes)
    $rejected = $false
    try { $null = Reconstruct-HgdeltaTarget -BasePath $BasePath -DeltaPath $Path } catch { $rejected = $true }
    finally { if (Test-Path -LiteralPath $Path) { Remove-Item -LiteralPath $Path -Force } }
    if (-not $rejected) { throw "HGDL verifier accepted invalid $CaseName fixture." }
}

function Assert-AtomicPublicationRegression {
    $rebuildPath = Join-Path $PSScriptRoot 'Rebuild-BatmanGraphicsOptionsExperiment.ps1'
    $regressionRoot = Join-Path ([IO.Path]::GetTempPath()) ('HelenBatmanGraphicsAtomicRegression-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path $regressionRoot | Out-Null
    try {
        . $rebuildPath -FunctionsOnly
        if ($null -eq (Get-Command Invoke-AtomicGraphicsPublication -ErrorAction SilentlyContinue)) { throw 'Atomic publication helper was not loaded.' }
        if ($null -eq (Get-Command Assert-GraphicsPublicationSameVolume -ErrorAction SilentlyContinue)) { throw 'Same-volume publication guard was not loaded.' }
        $sameVolumeStagingRoot = Join-Path $regressionRoot 'same-volume-staging'
        $sameVolumeBackupRoot = Join-Path $regressionRoot 'same-volume-backup'
        $sameVolumeLivePackRoot = Join-Path $regressionRoot 'same-volume-live\pack'
        $sameVolumeLiveTargetPath = Join-Path $regressionRoot 'same-volume-live\Frontend.umap'
        Assert-GraphicsPublicationSameVolume -TempRoot $sameVolumeStagingRoot -BackupRoot $sameVolumeBackupRoot -LivePackRoot $sameVolumeLivePackRoot -LiveTargetPath $sameVolumeLiveTargetPath
        $stagingVolume = [IO.Path]::GetPathRoot([IO.Path]::GetFullPath($sameVolumeStagingRoot))
        $alternateVolumes = @(Get-PSDrive -PSProvider FileSystem | Where-Object { -not [string]::IsNullOrWhiteSpace($_.Root) -and -not [String]::Equals([IO.Path]::GetPathRoot([IO.Path]::GetFullPath($_.Root)), $stagingVolume, [StringComparison]::OrdinalIgnoreCase) })
        if ($alternateVolumes.Count -gt 0) {
            $alternateBackupRoot = Join-Path $alternateVolumes[0].Root 'HelenBatmanGraphicsCrossVolumeBackup'
            $crossVolumeRejected = $false
            try {
                Assert-GraphicsPublicationSameVolume -TempRoot $sameVolumeStagingRoot -BackupRoot $alternateBackupRoot -LivePackRoot $sameVolumeLivePackRoot -LiveTargetPath $sameVolumeLiveTargetPath
            } catch {
                $crossVolumeRejected = $true
            }
            if (-not $crossVolumeRejected) { throw 'Cross-volume graphics publication roots were accepted.' }
        }
        $preCommitFailures = @('AfterPackBackup', 'AfterTargetBackup', 'AfterPackActivation', 'AfterTargetActivation', 'AfterTargetVerification')
        foreach ($failureCase in $preCommitFailures) {
            $caseRoot = Join-Path $regressionRoot $failureCase
            $livePack = Join-Path $caseRoot 'live\pack'
            $liveTarget = Join-Path $caseRoot 'live\Frontend.umap'
            $backupRoot = Join-Path $caseRoot 'backup-sibling'
            $tempRoot = Join-Path $caseRoot 'staging-temp'
            $stagedPack = Join-Path $tempRoot 'pack'
            $stagedTarget = Join-Path $tempRoot 'Frontend.umap'
            New-Item -ItemType Directory -Force -Path $tempRoot, $livePack, $stagedPack, (Split-Path -Parent $liveTarget) | Out-Null
            [IO.File]::WriteAllText((Join-Path $livePack 'pack.json'), 'old-pack')
            [IO.File]::WriteAllText($liveTarget, 'old-target')
            [IO.File]::WriteAllText((Join-Path $stagedPack 'pack.json'), 'new-pack')
            [IO.File]::WriteAllText($stagedTarget, 'new-target')
            $oldPackHash = (Get-FileHash -LiteralPath (Join-Path $livePack 'pack.json') -Algorithm SHA256).Hash
            $oldTargetHash = (Get-FileHash -LiteralPath $liveTarget -Algorithm SHA256).Hash
            if ([IO.Path]::GetFullPath($backupRoot).StartsWith(([IO.Path]::GetFullPath($tempRoot).TrimEnd('\') + '\'), [StringComparison]::OrdinalIgnoreCase)) { throw "$failureCase backup path is inside the temp tree." }
            $verify = { param($LivePackRoot, $LiveTargetPath) if ((Get-Content -LiteralPath (Join-Path $LivePackRoot 'pack.json') -Raw) -cne 'new-pack' -or (Get-Content -LiteralPath $LiveTargetPath -Raw) -cne 'new-target') { throw 'Injected publication verification saw unexpected live contents.' } }
            $failed = $false
            try {
                Invoke-AtomicGraphicsPublication -LivePackRoot $livePack -LiveTargetPath $liveTarget -StagedPackRoot $stagedPack -StagedTargetPath $stagedTarget -BackupRoot $backupRoot -TempRoot $tempRoot -VerifyPublication $verify -FailureInjection $failureCase
            } catch { $failed = $true }
            if (-not $failed) { throw "$failureCase injection did not fail." }
            if ((Get-FileHash -LiteralPath (Join-Path $livePack 'pack.json') -Algorithm SHA256).Hash -cne $oldPackHash -or (Get-FileHash -LiteralPath $liveTarget -Algorithm SHA256).Hash -cne $oldTargetHash) { throw "$failureCase injection did not restore exact old live hashes." }
        }

        $firstBuildRoot = Join-Path $regressionRoot 'FirstPublicationWithoutLiveParents'
        $livePack = Join-Path $firstBuildRoot 'live\pack'
        $liveTarget = Join-Path $firstBuildRoot 'live\Frontend.umap'
        $tempRoot = Join-Path $firstBuildRoot 'staging-temp'
        $stagedPack = Join-Path $tempRoot 'pack'
        $stagedTarget = Join-Path $tempRoot 'Frontend.umap'
        $backupRoot = Join-Path $firstBuildRoot 'backup-sibling'
        New-Item -ItemType Directory -Force -Path $tempRoot, $stagedPack | Out-Null
        [IO.File]::WriteAllText((Join-Path $stagedPack 'pack.json'), 'new-pack')
        [IO.File]::WriteAllText($stagedTarget, 'new-target')
        $verify = { param($LivePackRoot, $LiveTargetPath) if ((Get-Content -LiteralPath (Join-Path $LivePackRoot 'pack.json') -Raw) -cne 'new-pack' -or (Get-Content -LiteralPath $LiveTargetPath -Raw) -cne 'new-target') { throw 'First-publication verification saw unexpected live contents.' } }
        Invoke-AtomicGraphicsPublication -LivePackRoot $livePack -LiveTargetPath $liveTarget -StagedPackRoot $stagedPack -StagedTargetPath $stagedTarget -BackupRoot $backupRoot -TempRoot $tempRoot -VerifyPublication $verify
        if (-not (Test-Path -LiteralPath (Join-Path $livePack 'pack.json')) -or -not (Test-Path -LiteralPath $liveTarget)) { throw 'First publication did not create missing live output parents and outputs.' }
        if (Test-Path -LiteralPath $backupRoot) { throw 'First publication left an unexpected backup root after success.' }

        $cleanupRoot = Join-Path $regressionRoot 'AfterBackupDeletion'
        $livePack = Join-Path $cleanupRoot 'live\pack'
        $liveTarget = Join-Path $cleanupRoot 'live\Frontend.umap'
        $backupRoot = Join-Path $cleanupRoot 'backup-sibling'
        $tempRoot = Join-Path $cleanupRoot 'staging-temp'
        $stagedPack = Join-Path $tempRoot 'pack'
        $stagedTarget = Join-Path $tempRoot 'Frontend.umap'
        New-Item -ItemType Directory -Force -Path $tempRoot, $livePack, $stagedPack, (Split-Path -Parent $liveTarget) | Out-Null
        [IO.File]::WriteAllText((Join-Path $livePack 'pack.json'), 'old-pack')
        [IO.File]::WriteAllText($liveTarget, 'old-target')
        [IO.File]::WriteAllText((Join-Path $stagedPack 'pack.json'), 'new-pack')
        [IO.File]::WriteAllText($stagedTarget, 'new-target')
        $verify = { param($LivePackRoot, $LiveTargetPath) if ((Get-Content -LiteralPath (Join-Path $LivePackRoot 'pack.json') -Raw) -cne 'new-pack' -or (Get-Content -LiteralPath $LiveTargetPath -Raw) -cne 'new-target') { throw 'Injected publication verification saw unexpected live contents.' } }
        $failed = $false
        try {
            Invoke-AtomicGraphicsPublication -LivePackRoot $livePack -LiveTargetPath $liveTarget -StagedPackRoot $stagedPack -StagedTargetPath $stagedTarget -BackupRoot $backupRoot -TempRoot $tempRoot -VerifyPublication $verify -FailureInjection 'AfterBackupDeletion'
        } catch { $failed = $true }
        if (-not $failed) { throw 'AfterBackupDeletion injection did not surface cleanup failure.' }
        if ((Get-Content -LiteralPath (Join-Path $livePack 'pack.json') -Raw) -cne 'new-pack' -or (Get-Content -LiteralPath $liveTarget -Raw) -cne 'new-target') { throw 'Backup cleanup failure did not leave verified new live outputs in place.' }
        $retainedTargetBackup = Join-Path $backupRoot 'target\Frontend-graphics-options.umap'
        if (-not (Test-Path -LiteralPath $backupRoot) -or -not (Test-Path -LiteralPath $retainedTargetBackup) -or (Get-Content -LiteralPath $retainedTargetBackup -Raw) -cne 'old-target') { throw 'Backup cleanup failure did not retain the failed sibling recovery copy.' }
        if ([IO.Path]::GetFullPath($backupRoot).StartsWith(([IO.Path]::GetFullPath($tempRoot).TrimEnd('\') + '\'), [StringComparison]::OrdinalIgnoreCase)) { throw 'Recovery copies were retained inside the temp tree.' }
    }
    finally {
        if (Test-Path -LiteralPath $regressionRoot) { Remove-Item -LiteralPath $regressionRoot -Recurse -Force }
    }
}

function Reconstruct-HgdeltaTarget {
    param(
        [Parameter(Mandatory = $true)] [string]$BasePath,
        [Parameter(Mandatory = $true)] [string]$DeltaPath
    )

    $base = [System.IO.File]::ReadAllBytes($BasePath)
    $delta = [System.IO.File]::ReadAllBytes($DeltaPath)
    if ($delta.Length -lt 116 -or [Text.Encoding]::ASCII.GetString($delta, 0, 4) -cne 'HGDL') { throw "Invalid HGDL delta: $DeltaPath" }
    $majorVersion = Read-UInt32LittleEndian $delta 4
    $minorVersion = Read-UInt32LittleEndian $delta 8
    $chunkSize = Read-UInt32LittleEndian $delta 12
    $baseSize = Read-UInt64LittleEndian $delta 16
    $targetSize64 = Read-UInt64LittleEndian $delta 24
    $baseHash = Convert-BytesToLowerHex -Bytes @($delta[32..63])
    $targetHash = Convert-BytesToLowerHex -Bytes @($delta[64..95])
    $chunkCount64 = Read-UInt32LittleEndian $delta 96
    $chunkTableOffset = Read-UInt64LittleEndian $delta 100
    $payloadOffset = Read-UInt64LittleEndian $delta 108
    if ($majorVersion -ne 1 -or $minorVersion -ne 0) { throw "HGDL version must be 1.0, found $majorVersion.$minorVersion." }
    if ($chunkSize -ne 65536) { throw "HGDL chunk size must be 65536, found $chunkSize." }
    if ($baseSize -ne [uint64]$base.Length) { throw "HGDL base size mismatch: header $baseSize, actual $($base.Length)." }
    if ($targetSize64 -gt [uint64][int]::MaxValue) { throw 'HGDL target is too large for verifier memory.' }
    if ($chunkCount64 -gt [uint64][int]::MaxValue) { throw 'HGDL chunk count is too large for verifier memory.' }
    $targetSize = [int]$targetSize64
    $chunkCount = [int]$chunkCount64
    $expectedChunkCount = [uint64][Math]::Ceiling($targetSize64 / [double]$chunkSize)
    if ($chunkCount64 -ne $expectedChunkCount) { throw "HGDL chunk count mismatch: expected $expectedChunkCount, found $chunkCount64." }
    if ($chunkTableOffset -lt 116) { throw "HGDL chunk table overlaps header at offset $chunkTableOffset." }
    $tableLength = $chunkCount64 * 20
    if ($tableLength -gt [uint64]$delta.Length - $chunkTableOffset) { throw 'HGDL chunk table exceeds delta file bounds.' }
    $tableEnd = $chunkTableOffset + $tableLength
    if ($payloadOffset -lt $tableEnd -or $payloadOffset -gt [uint64]$delta.Length) { throw 'HGDL payload offset is outside the delta file bounds.' }
    $actualBaseHash = (Get-FileHash -LiteralPath $BasePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($baseHash -cne $actualBaseHash) { throw "HGDL base SHA mismatch: header $baseHash, actual $actualBaseHash." }
    $result = [System.IO.MemoryStream]::new($targetSize)
    [uint64]$written = 0
    try {
        for ($index = 0; $index -lt $chunkCount; $index++) {
            $entry64 = $chunkTableOffset + ([uint64]$index * 20)
            if ($entry64 -gt [uint64][int]::MaxValue) { throw 'HGDL chunk entry offset exceeds verifier limits.' }
            $entry = [int]$entry64
            $kind = Read-UInt32LittleEndian $delta $entry
            $size64 = Read-UInt32LittleEndian $delta ($entry + 4)
            $entryPayloadOffset = Read-UInt64LittleEndian $delta ($entry + 8)
            $entryPayloadSize = Read-UInt32LittleEndian $delta ($entry + 16)
            if ($size64 -eq 0 -or $size64 -gt [uint64]$chunkSize) { throw "HGDL chunk $index has invalid reconstructed size $size64." }
            if ($written + $size64 -gt $targetSize64) { throw "HGDL chunk $index exceeds reconstructed target size." }
            $size = [int]$size64
            if ($kind -eq 0) {
                if ($entryPayloadOffset -ne 0 -or $entryPayloadSize -ne 0) { throw "HGDL base chunk $index contains replacement payload metadata." }
                $baseOffset = [uint64]$index * [uint64]$chunkSize
                if ($baseOffset + $size64 -gt [uint64]$base.Length) { throw "HGDL base chunk $index exceeds base file bounds." }
                $result.Write($base, [int]$baseOffset, $size)
            } elseif ($kind -eq 1) {
                if ($entryPayloadSize -ne $size64) { throw "HGDL replacement size mismatch at chunk $index." }
                if ($payloadOffset + $entryPayloadOffset -lt $payloadOffset -or $payloadOffset + $entryPayloadOffset + $size64 -gt [uint64]$delta.Length) { throw "HGDL replacement chunk $index exceeds payload bounds." }
                $sourceOffset = $payloadOffset + $entryPayloadOffset
                $result.Write($delta, [int]$sourceOffset, $size)
            } else {
                throw "HGDL contains unsupported chunk kind $kind at chunk $index."
            }
            $written += $size64
        }
        if ($written -ne $targetSize64 -or $result.Length -ne $targetSize) { throw "HGDL reconstructed size mismatch: expected $targetSize64, wrote $written." }
        $reconstructed = $result.ToArray()
        $actualTargetHash = (Get-FileHash -InputStream ([IO.MemoryStream]::new($reconstructed)) -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($targetHash -cne $actualTargetHash) { throw "HGDL reconstructed SHA mismatch: header $targetHash, actual $actualTargetHash." }
        return ,$reconstructed
    }
    finally {
        $result.Dispose()
    }
}

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}
if ([string]::IsNullOrWhiteSpace($BuilderRoot)) {
    $BuilderRoot = Join-Path $BatmanRoot 'builder'
} elseif ([IO.Path]::IsPathRooted($BuilderRoot)) {
    $BuilderRoot = [IO.Path]::GetFullPath($BuilderRoot)
} elseif (Test-Path -LiteralPath $BuilderRoot) {
    $BuilderRoot = (Resolve-Path $BuilderRoot).Path
} else {
    $BuilderRoot = [IO.Path]::GetFullPath((Join-Path $BatmanRoot $BuilderRoot))
}

$packRoot = if ([string]::IsNullOrWhiteSpace($PackRootOverride)) { Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-graphics-options' } else { [IO.Path]::GetFullPath($PackRootOverride) }
$buildRoot = Join-Path $packRoot 'builds\steam-goty-1.0'
$packJsonPath = Join-Path $packRoot 'pack.json'
$buildJsonPath = Join-Path $buildRoot 'build.json'
$bindingsJsonPath = Join-Path $buildRoot 'bindings.json'
$commandsJsonPath = Join-Path $buildRoot 'commands.json'
$filesJsonPath = Join-Path $buildRoot 'files.json'
$deltaPath = Join-Path $buildRoot 'assets\deltas\Frontend-graphics-options.hgdelta'
$hooksJsonPath = Join-Path $buildRoot 'hooks.json'
$texturesJsonPath = Join-Path $buildRoot 'textures.json'
$basePath = Join-Path $BuilderRoot 'extracted\frontend-retail\Frontend.umap'
$targetPath = if ([string]::IsNullOrWhiteSpace($TargetPathOverride)) { Join-Path $BuilderRoot 'generated\graphics-options-experiment\Frontend-graphics-options.umap' } else { [IO.Path]::GetFullPath($TargetPathOverride) }
$stableGeneratedRoot = Join-Path $BuilderRoot 'generated\graphics-options-experiment'
$ffdecPath = Join-Path $BuilderRoot 'extracted\ffdec\ffdec-cli.exe'
$patcherProjectPath = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\BmGameGfxPatcher\BmGameGfxPatcher.csproj'

foreach ($requiredPath in @($packJsonPath, $buildJsonPath, $bindingsJsonPath, $commandsJsonPath, $hooksJsonPath, $filesJsonPath, $deltaPath, $basePath, $targetPath, $ffdecPath, $patcherProjectPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Batman graphics-options package input not found: $requiredPath" }
}
Assert-RebuildAtomicSourceContract -ScriptPath (Join-Path $PSScriptRoot 'Rebuild-BatmanGraphicsOptionsExperiment.ps1')
Assert-AtomicPublicationRegression
Assert-ExactPackFileSet -Root $packRoot -BuildDirectoryName 'steam-goty-1.0'
if (-not $StagedPackageValidation) { Assert-ExactGeneratedTargetFileSet -Root $stableGeneratedRoot -TargetPath $targetPath }
if (-not (Test-Path -LiteralPath $hooksJsonPath -PathType Leaf)) { throw 'Graphics-options package must contain hooks.json.' }
if (Test-Path -LiteralPath $texturesJsonPath) { throw 'Graphics-options shell must not contain textures.json.' }

$pack = Get-Content -LiteralPath $packJsonPath -Raw | ConvertFrom-Json
Assert-ExactOrderedProperties -Object $pack -Names @('schemaVersion', 'id', 'name', 'targets', 'config', 'builds') -Context 'pack.json'
if ($pack.schemaVersion -ne 1 -or $pack.id -ne 'batman-aa-graphics-options' -or $pack.name -ne 'Batman Graphics Options') { throw 'pack.json graphics-options identity drifted.' }
if (@($pack.targets).Count -ne 1 -or $pack.targets[0].gameId -ne 'batman-arkham-asylum' -or @($pack.targets[0].executables).Count -ne 1 -or $pack.targets[0].executables[0] -ne 'ShippingPC-BmGame.exe') { throw 'pack.json target executable drifted.' }
Assert-ExactOrderedProperties -Object $pack.targets[0] -Names @('gameId', 'executables') -Context 'pack.json target'
Assert-ExactOrderedProperties -Object $pack.config[0] -Names @('key', 'type', 'defaultValue') -Context 'pack.json config entry'
$expectedConfigKeys = @('fullscreen', 'resolutionWidth', 'resolutionHeight', 'vsync', 'msaa', 'detailLevel', 'bloom', 'dynamicShadows', 'motionBlur', 'distortion', 'fogVolumes', 'sphericalHarmonicLighting', 'ambientOcclusion', 'physx', 'stereo', 'applySignal', 'rollbackSignal')
if (@($pack.config).Count -ne $expectedConfigKeys.Count) { throw "pack.json config must contain exactly $($expectedConfigKeys.Count) entries." }
for ($index = 0; $index -lt $expectedConfigKeys.Count; $index++) {
    $configEntry = $pack.config[$index]
    Assert-ExactOrderedProperties -Object $configEntry -Names @('key', 'type', 'defaultValue') -Context "pack.json config entry $($index + 1)"
    if ($configEntry.key -cne $expectedConfigKeys[$index] -or $configEntry.type -cne 'int' -or $configEntry.defaultValue -ne 0) {
        throw "pack.json config entry $($index + 1) drifted."
    }
}
if (@($pack.builds).Count -ne 1 -or $pack.builds[0] -ne 'steam-goty-1.0') { throw 'pack.json build list drifted.' }

$build = Get-Content -LiteralPath $buildJsonPath -Raw | ConvertFrom-Json
Assert-ExactOrderedProperties -Object $build -Names @('id', 'executable', 'match', 'startupCommands') -Context 'build.json'
$expectedMatch = & (Join-Path $PSScriptRoot 'Get-BatmanSteamBuildMatch.ps1')
if ($build.id -ne $expectedMatch.BuildId -or $build.executable -ne $expectedMatch.Executable -or $build.match.fileSize -ne $expectedMatch.FileSize -or $build.match.sha256 -cne $expectedMatch.Sha256) { throw 'build.json retail executable identity drifted.' }
Assert-ExactProperties -Object $build.match -Names @('fileSize', 'sha256') -Context 'build.json match'
if (@($build.startupCommands).Count -ne 1 -or $build.startupCommands[0] -cne 'loadBatmanGraphicsDraftIntoConfig') { throw 'build.json startup command drifted.' }

$bindings = Get-Content -LiteralPath $bindingsJsonPath -Raw | ConvertFrom-Json
Assert-ExactProperties -Object $bindings -Names @('bindings') -Context 'bindings.json'
if (@($bindings.bindings).Count -ne 0) { throw 'bindings.json must contain zero bindings.' }
$commands = Get-Content -LiteralPath $commandsJsonPath -Raw | ConvertFrom-Json
Assert-ExactProperties -Object $commands -Names @('commands') -Context 'commands.json'
if (@($commands.commands).Count -ne 2) { throw 'commands.json must contain exactly two commands.' }
$loadCommand = $commands.commands[0]
$applyCommand = $commands.commands[1]
Assert-ExactOrderedProperties -Object $loadCommand -Names @('id', 'name', 'steps') -Context 'commands.json load command'
Assert-ExactOrderedProperties -Object $applyCommand -Names @('id', 'name', 'steps') -Context 'commands.json apply command'
if ($loadCommand.id -cne 'loadBatmanGraphicsDraftIntoConfig' -or $loadCommand.name -cne 'Load Batman Graphics Draft Into Config' -or @($loadCommand.steps).Count -ne 1) { throw 'commands.json load command identity drifted.' }
if ($applyCommand.id -cne 'applyBatmanGraphicsDraft' -or $applyCommand.name -cne 'Apply Batman Graphics Draft' -or @($applyCommand.steps).Count -ne 2) { throw 'commands.json apply command identity drifted.' }
Assert-ExactOrderedProperties -Object $loadCommand.steps[0] -Names @('kind') -Context 'commands.json load step'
Assert-ExactOrderedProperties -Object $applyCommand.steps[0] -Names @('kind') -Context 'commands.json apply config step'
Assert-ExactOrderedProperties -Object $applyCommand.steps[1] -Names @('kind') -Context 'commands.json apply load step'
if ($loadCommand.steps[0].kind -cne 'load-batman-graphics-draft-into-config' -or $applyCommand.steps[0].kind -cne 'apply-batman-graphics-config' -or $applyCommand.steps[1].kind -cne 'load-batman-graphics-draft-into-config') { throw 'commands.json step kinds drifted.' }

$hooks = Get-Content -LiteralPath $hooksJsonPath -Raw | ConvertFrom-Json
Assert-ExactOrderedProperties -Object $hooks -Names @('runtimeSlots', 'stateObservers', 'hooks') -Context 'hooks.json'
if (@($hooks.runtimeSlots).Count -ne 0 -or @($hooks.hooks).Count -ne 0 -or @($hooks.stateObservers).Count -ne 6) { throw 'hooks.json runtime slots, observers, or hooks count drifted.' }

function Assert-GraphicsCarrierChecks {
    param([Parameter(Mandatory = $true)] [psobject]$Observer, [Parameter(Mandatory = $true)] [string]$Context)
    if (@($Observer.checks).Count -ne 11) { throw "$Context must contain exactly eleven checks." }
    $expectedConstantChecks = @(
        @{ offset = -16; expectedValue = 50 },
        @{ offset = -12; expectedValue = 100 },
        @{ offset = -8; expectedValue = 100 },
        @{ offset = -4; expectedValue = 100 },
        @{ offset = 0; expectedValue = 4102 },
        @{ offset = 4; expectedValue = 1 },
        @{ offset = 8; expectedValue = 0 },
        @{ offset = 16; expectedValue = 4102 },
        @{ offset = 20; expectedValue = 2 },
        @{ offset = 28; expectedValue = 3 },
        @{ offset = 32; expectedValue = 3 }
    )
    for ($index = 0; $index -lt $expectedConstantChecks.Count; $index++) {
        $check = $Observer.checks[$index]
        Assert-ExactOrderedProperties -Object $check -Names @('comparison', 'offset', 'expectedValue') -Context "$Context check $($index + 1)"
        $expected = $expectedConstantChecks[$index]
        if ($check.comparison -cne 'equals-constant' -or $check.offset -ne $expected.offset -or $check.expectedValue -ne $expected.expectedValue) { throw "$Context constant check $($index + 1) drifted." }
    }
}

$expectedObserverProperties = @('id', 'addressGroup', 'scanStartAddress', 'scanEndAddress', 'scanStride', 'valueOffset', 'pollIntervalMs', 'targetConfigKey', 'addressMatchValues', 'checks', 'mappings', 'responseRequestValue', 'responseMappings', 'acknowledgementMappings', 'failureResponseValue')
$expectedCommandObserverProperties = @('id', 'addressGroup', 'scanStartAddress', 'scanEndAddress', 'scanStride', 'valueOffset', 'pollIntervalMs', 'targetConfigKey', 'addressMatchValues', 'checks', 'mappings', 'acknowledgementMappings', 'failureResponseValue', 'command')
$expectedObserverIds = @('graphicsObserverVsync', 'graphicsObserverMsaa', 'graphicsObserverPhysx', 'graphicsObserverStereo', 'graphicsObserverApplySignal', 'graphicsObserverRollbackSignal')
$expectedObserverTargets = @('vsync', 'msaa', 'physx', 'stereo', 'applySignal', 'rollbackSignal')
$expectedAddressMatchValues = @(
    4200, 4210, 4211, 4220, 4221, 4230, 4231, 4299,
    4300, 4310, 4311, 4312, 4313, 4314, 4320, 4321, 4322, 4323, 4324, 4330, 4331, 4332, 4333, 4334, 4399,
    4400, 4410, 4411, 4412, 4420, 4421, 4422, 4430, 4431, 4432, 4499,
    4500, 4510, 4511, 4520, 4521, 4530, 4531, 4599,
    4960, 4961, 4969, 4970, 4971, 4980, 4981, 4989, 4990, 4991
)
for ($observerIndex = 0; $observerIndex -lt $expectedObserverIds.Count; $observerIndex++) {
    $observer = $hooks.stateObservers[$observerIndex]
    $expectedProperties = if ($observerIndex -lt 4) { $expectedObserverProperties } else { $expectedCommandObserverProperties }
    Assert-ExactOrderedProperties -Object $observer -Names $expectedProperties -Context "hooks.json $($expectedObserverIds[$observerIndex])"
    if ($observer.id -cne $expectedObserverIds[$observerIndex] -or $observer.targetConfigKey -cne $expectedObserverTargets[$observerIndex]) { throw "hooks.json observer $($observerIndex + 1) identity drifted." }
    if ($observer.addressGroup -cne 'batmanFrontendControlType') { throw "hooks.json $($observer.id) address group drifted." }
    if ($observer.scanStartAddress -cne '0x10000000' -or $observer.scanEndAddress -cne '0x30000000' -or $observer.scanStride -ne 4 -or $observer.valueOffset -ne 12 -or $observer.pollIntervalMs -ne 50) { throw "hooks.json $($observer.id) scan geometry drifted." }
    if (@($observer.addressMatchValues).Count -ne $expectedAddressMatchValues.Count -or (@($observer.addressMatchValues) -join ',') -cne ($expectedAddressMatchValues -join ',')) { throw "hooks.json $($observer.id) addressMatchValues drifted." }
    Assert-GraphicsCarrierChecks -Observer $observer -Context "hooks.json $($observer.id)"
    $mappingNames = if ($observerIndex -lt 4) { @('mappings', 'responseMappings', 'acknowledgementMappings') } else { @('mappings', 'acknowledgementMappings') }
    foreach ($mappingName in $mappingNames) {
        foreach ($entry in @($observer.$mappingName)) { Assert-ExactOrderedProperties -Object $entry -Names @('match', 'value') -Context "hooks.json $($observer.id) $mappingName" }
    }
}

$expectedGraphicsProtocols = @(
    [pscustomobject]@{ Id = 'graphicsObserverVsync'; Read = 4200; Responses = @(4210, 4211); Writes = @(4220, 4221); Acks = @(4230, 4231); Failure = 4299; ConfigValues = @(0, 1) },
    [pscustomobject]@{ Id = 'graphicsObserverMsaa'; Read = 4300; Responses = @(4310, 4311, 4312, 4313, 4314); Writes = @(4320, 4321, 4322, 4323, 4324); Acks = @(4330, 4331, 4332, 4333, 4334); Failure = 4399; ConfigValues = @(0, 1, 2, 3, 5) },
    [pscustomobject]@{ Id = 'graphicsObserverPhysx'; Read = 4400; Responses = @(4410, 4411, 4412); Writes = @(4420, 4421, 4422); Acks = @(4430, 4431, 4432); Failure = 4499; ConfigValues = @(0, 1, 2) },
    [pscustomobject]@{ Id = 'graphicsObserverStereo'; Read = 4500; Responses = @(4510, 4511); Writes = @(4520, 4521); Acks = @(4530, 4531); Failure = 4599; ConfigValues = @(0, 1) }
)
for ($protocolIndex = 0; $protocolIndex -lt $expectedGraphicsProtocols.Count; $protocolIndex++) {
    $protocol = $expectedGraphicsProtocols[$protocolIndex]
    $observer = $hooks.stateObservers[$protocolIndex]
    if (@($observer.mappings).Count -ne $protocol.Writes.Count -or @($observer.responseMappings).Count -ne $protocol.Responses.Count -or @($observer.acknowledgementMappings).Count -ne $protocol.Acks.Count) { throw "hooks.json $($protocol.Id) mapping counts drifted." }
    if ($observer.responseRequestValue -ne $protocol.Read -or $observer.failureResponseValue -ne $protocol.Failure) { throw "hooks.json $($protocol.Id) request/failure values drifted." }
    for ($mappingIndex = 0; $mappingIndex -lt $protocol.Writes.Count; $mappingIndex++) {
        if ($observer.mappings[$mappingIndex].match -ne $protocol.Writes[$mappingIndex] -or $observer.mappings[$mappingIndex].value -ne $protocol.ConfigValues[$mappingIndex]) { throw "hooks.json $($protocol.Id) write mapping drifted." }
        if ($observer.responseMappings[$mappingIndex].match -ne $protocol.ConfigValues[$mappingIndex] -or $observer.responseMappings[$mappingIndex].value -ne $protocol.Responses[$mappingIndex]) { throw "hooks.json $($protocol.Id) response mapping drifted." }
        if ($observer.acknowledgementMappings[$mappingIndex].match -ne $protocol.Writes[$mappingIndex] -or $observer.acknowledgementMappings[$mappingIndex].value -ne $protocol.Acks[$mappingIndex]) { throw "hooks.json $($protocol.Id) acknowledgement mapping drifted." }
    }
}

$vsyncObserver = $hooks.stateObservers[0]
$msaaObserver = $hooks.stateObservers[1]
$physxObserver = $hooks.stateObservers[2]
$stereoObserver = $hooks.stateObservers[3]
$applyObserver = $hooks.stateObservers[4]
$rollbackObserver = $hooks.stateObservers[5]
if ($applyObserver.id -cne 'graphicsObserverApplySignal' -or $applyObserver.targetConfigKey -cne 'applySignal' -or $applyObserver.command -cne 'applyBatmanGraphicsDraft') { throw 'hooks.json apply observer identity drifted.' }
if ($rollbackObserver.id -cne 'graphicsObserverRollbackSignal' -or $rollbackObserver.targetConfigKey -cne 'rollbackSignal' -or $rollbackObserver.command -cne 'loadBatmanGraphicsDraftIntoConfig') { throw 'hooks.json rollback observer identity drifted.' }
if ($applyObserver.mappings[0].match -ne 4990 -or $applyObserver.mappings[0].value -ne 0 -or $applyObserver.mappings[1].match -ne 4991 -or $applyObserver.mappings[1].value -ne 1) { throw 'hooks.json apply mappings drifted.' }
if ($applyObserver.acknowledgementMappings[0].match -ne 4990 -or $applyObserver.acknowledgementMappings[0].value -ne 4980 -or $applyObserver.acknowledgementMappings[1].match -ne 4991 -or $applyObserver.acknowledgementMappings[1].value -ne 4981 -or $applyObserver.failureResponseValue -ne 4989) { throw 'hooks.json apply acknowledgement/failure mappings drifted.' }
if ($rollbackObserver.mappings[0].match -ne 4970 -or $rollbackObserver.mappings[0].value -ne 0 -or $rollbackObserver.mappings[1].match -ne 4971 -or $rollbackObserver.mappings[1].value -ne 1) { throw 'hooks.json rollback mappings drifted.' }
if ($rollbackObserver.acknowledgementMappings[0].match -ne 4970 -or $rollbackObserver.acknowledgementMappings[0].value -ne 4960 -or $rollbackObserver.acknowledgementMappings[1].match -ne 4971 -or $rollbackObserver.acknowledgementMappings[1].value -ne 4961 -or $rollbackObserver.failureResponseValue -ne 4969) { throw 'hooks.json rollback acknowledgement/failure mappings drifted.' }

$files = Get-Content -LiteralPath $filesJsonPath -Raw | ConvertFrom-Json
Assert-ExactProperties -Object $files -Names @('virtualFiles') -Context 'files.json'
if (@($files.virtualFiles).Count -ne 1) { throw 'files.json must contain exactly one virtual file.' }
Assert-HgdeltaVirtualFileContract -Context 'graphics-options shell' -VirtualFile $files.virtualFiles[0] -ExpectedId 'frontendGraphicsOptionsPackage' -ExpectedPath 'BmGame/CookedPC/Maps/Frontend/Frontend.umap' -ExpectedMode 'delta-on-read' -ExpectedKind 'delta-file' -ExpectedDeltaRelativePath 'assets/deltas/Frontend-graphics-options.hgdelta' -BasePath $basePath -TargetPath $targetPath -DeltaFilePath $deltaPath -ChunkSize 65536 -ChunkTableOffset 116

$baseInfo = Get-Item -LiteralPath $basePath
$baseHash = (Get-FileHash -LiteralPath $basePath -Algorithm SHA256).Hash
if ($baseInfo.Length -ne 2988548 -or $baseHash -cne '271916B888F83374122AF0FCCC5C685804F4C8286A92A772CD71E4F48A00F2CC') { throw 'Retail Frontend.umap base identity is not the verified retail input.' }
$targetInfo = Get-UnrealPackageStorageInfo -Path $targetPath
if ($targetInfo.CompressionChunkCount -le 0) { throw 'Generated shell target must retain chunk-compressed Unreal storage.' }

$verificationRoot = Join-Path ([IO.Path]::GetTempPath()) ('HelenBatmanGraphicsPackageVerification-' + [Guid]::NewGuid().ToString('N'))
$extractedGfxPath = Join-Path $verificationRoot 'MainV2.gfx'
$xmlPath = Join-Path $verificationRoot 'MainV2.xml'
$exportRoot = Join-Path $verificationRoot 'export'
New-Item -ItemType Directory -Force -Path $verificationRoot | Out-Null
try {
    $extract = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('run', '--project', $patcherProjectPath, '-c', $Configuration, '--', 'extract-gfx', '--package', $targetPath, '--owner', 'MainMenu', '--name', 'MainV2', '--output', $extractedGfxPath)
    if ($extract.ExitCode -ne 0) { throw "Failed to extract current-run MainV2: $($extract.Output -join [Environment]::NewLine)" }
    $xml = Invoke-ExternalProcess -FilePath $ffdecPath -Arguments @('-swf2xml', $extractedGfxPath, $xmlPath)
    if ($xml.ExitCode -ne 0) { throw 'FFDec failed to reopen the generated MainV2 shell.' }
    $export = Invoke-ExternalProcess -FilePath $ffdecPath -Arguments @('-export', 'script', $exportRoot, $extractedGfxPath)
    if ($export.ExitCode -ne 0) { throw 'FFDec failed to export the generated MainV2 shell scripts.' }

    [xml]$document = Get-Content -LiteralPath $xmlPath -Raw
    $sprites600 = @($document.SelectNodes("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='600']"))
    if ($sprites600.Count -ne 1) { throw "Expected exactly one DefineSprite_600_ScreenOptionsGraphics sprite, found $($sprites600.Count)." }
    if (@($document.SelectNodes("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='601']")).Count -ne 0) { throw 'Shell target must not contain sprite 601.' }
    $exports = @(@($document.SelectNodes("/swf/tags/item[@type='ExportAssetsTag']")) | Where-Object { @($_.names.item | Where-Object { $_ -eq 'ScreenOptionsGraphics' }).Count -gt 0 })
    if ($exports.Count -ne 1 -or @($exports[0].tags.item | Where-Object { $_ -eq '600' }).Count -ne 1) { throw 'Shell target must export exactly sprite 600 as ScreenOptionsGraphics.' }

    Assert-ScopedExportedShellContract -ExportRoot $exportRoot

    $reconstructed = Reconstruct-HgdeltaTarget -BasePath $basePath -DeltaPath $deltaPath
    $targetBytes = [IO.File]::ReadAllBytes($targetPath)
    if ($reconstructed.Length -ne $targetBytes.Length -or -not [Linq.Enumerable]::SequenceEqual($reconstructed, $targetBytes)) { throw 'HGDL delta reconstruction does not match the current-run target byte-for-byte.' }
    $deltaBytes = [IO.File]::ReadAllBytes($deltaPath)
    $invalidDeltaPath = Join-Path $verificationRoot 'invalid.hgdelta'
    [byte[]]$badBytes = $deltaBytes.Clone()
    $badBytes[0] = [byte][char]'X'
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'header magic'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt32Fixture -Bytes $badBytes -Offset 4 -Value 2
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'version'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt32Fixture -Bytes $badBytes -Offset 12 -Value 1
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'chunk size'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt32Fixture -Bytes $badBytes -Offset 96 -Value 0
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'chunk count'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt64Fixture -Bytes $badBytes -Offset 100 -Value 115
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'chunk-table bounds'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt64Fixture -Bytes $badBytes -Offset 108 -Value ([uint64]$deltaBytes.Length + 1)
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'payload bounds'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt32Fixture -Bytes $badBytes -Offset 116 -Value 9
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'unsupported chunk kind'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt32Fixture -Bytes $badBytes -Offset 120 -Value 65537
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'chunk size/count bounds'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt64Fixture -Bytes $badBytes -Offset 124 -Value ([uint64]$deltaBytes.Length)
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'replacement payload bounds'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt32Fixture -Bytes $badBytes -Offset 116 -Value 1
    Set-HgdeltaUInt64Fixture -Bytes $badBytes -Offset 124 -Value ([uint64]::MaxValue)
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'chunk offset overflow'
    [byte[]]$badBytes = $deltaBytes.Clone()
    Set-HgdeltaUInt32Fixture -Bytes $badBytes -Offset 116 -Value 0
    Set-HgdeltaUInt32Fixture -Bytes $badBytes -Offset 120 -Value 65535
    Set-HgdeltaUInt64Fixture -Bytes $badBytes -Offset 124 -Value 0
    Set-HgdeltaUInt32Fixture -Bytes $badBytes -Offset 132 -Value 0
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'reconstructed size'
    [byte[]]$badBytes = $deltaBytes.Clone()
    $badBytes[64] = $badBytes[64] -bxor 255
    Assert-HgdeltaRejected -BasePath $basePath -Bytes $badBytes -Path $invalidDeltaPath -CaseName 'reconstructed SHA'
}
finally {
    if (Test-Path -LiteralPath $verificationRoot) { Remove-Item -LiteralPath $verificationRoot -Recurse -Force }
}

Write-Output 'PASS'
