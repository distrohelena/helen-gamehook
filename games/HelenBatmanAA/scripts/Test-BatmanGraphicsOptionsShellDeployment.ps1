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
$PackSourcePath = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-graphics-options'
$PackageVerifierPath = Join-Path $BatmanRoot 'scripts\Test-BatmanGraphicsOptionsPackage.ps1'
$BuilderRootPath = Join-Path $BatmanRoot 'builder'

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
        [Parameter(Mandatory = $true)] [bool]$IncludeExistingGraphicsPack,
        [bool]$IncludeStagedStateSentinel = $true
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

    foreach ($sourceChild in @(Get-ChildItem -LiteralPath $PackSourcePath -Force)) {
        Copy-Item -LiteralPath $sourceChild.FullName -Destination $StagedPackRoot -Recurse -Force
    }
    if ($IncludeStagedStateSentinel) {
        [IO.File]::WriteAllText((Join-Path $StagedPackRoot 'state.txt'), 'new-pack')
    }
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
    '-BuilderRoot $BuilderRoot',
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
    'VerifyStagedPublication',
    'Move-Item -LiteralPath',
    'Assert-SafeDeploymentPath',
    'Get-SafeDeploymentItems',
    'Get-ExistingDeploymentItem',
    'Assert-DeploymentStagingRootAvailable',
    'Initialize-DeploymentRoots',
    'AfterRecoveryRootCreation',
    'Get-DirectorySnapshot',
    'Assert-BatmanGraphicsVsyncHooks',
    'Assert-BatmanGraphicsCommands',
    'Assert-BatmanGraphicsPackFileSet',
    'Remove-DeploymentStagingRoot',
    'GetPathRoot',
    'Split-Path -Parent $PackDestination',
    'Split-Path -Parent $ConfigDestinationPath'
)) {
    Assert-ContainsOrdinal -Text $DeployScriptText -Token $RequiredToken -Context 'Batman graphics shell deploy source'
}

Assert-NotContainsOrdinal -Text $DeployScriptText -Token '$ConflictingPackDestination' -Context 'Batman graphics shell deploy source'
Assert-NotContainsOrdinal -Text $DeployScriptText -Token 'GetTempPath' -Context 'Batman graphics shell deploy source'
Assert-NotContainsOrdinal -Text $DeployScriptText -Token '}.GetNewClosure()' -Context 'Batman graphics shell deploy source verifier'
foreach ($forbiddenToken in @('F:\helenhook.7z', 'F:/helenhook.7z', 'batma/', 'batma\', 'Program Files', 'GraphicsVsyncController', 'InitialVsync', 'DraftVsync', 'GraphicsExitPrompt', 'DefineSprite_601', 'Helen_', 'full-controller', 'prompt export', 'prompt route', 'old package')) {
    Assert-NotContainsOrdinal -Text $DeployScriptText -Token $forbiddenToken -Context 'Batman graphics shell deploy source provenance'
}
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
$LiveTargetPath = Join-Path $GameBin 'Frontend-graphics-options.umap'
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
    Assert-BatmanGraphicsPackFileSet -PackRoot $PackSourcePath -Context 'Repository source'

    $ResolverRepoRoot = Join-Path $TestRoot 'resolver-repo'
    $ResolverBatmanRoot = Join-Path $ResolverRepoRoot 'games\HelenBatmanAA'
    $ResolverCwdBuilderRoot = Join-Path $ResolverBatmanRoot 'builder'
    $ResolverFallbackBuilderRoot = Join-Path $ResolverBatmanRoot 'fallback-builder'
    $ResolverAbsoluteBuilderRoot = Join-Path $ResolverRepoRoot 'absolute-builder'
    New-Item -ItemType Directory -Force -Path $ResolverCwdBuilderRoot, $ResolverFallbackBuilderRoot, $ResolverAbsoluteBuilderRoot | Out-Null
    Push-Location $ResolverRepoRoot
    try {
        $cwdRelativeBuilderRoot = Resolve-OptionalBuilderRoot -BatmanRootPath $ResolverBatmanRoot -BuilderRootPath '.\games\HelenBatmanAA\builder'
        $fallbackBuilderRoot = Resolve-OptionalBuilderRoot -BatmanRootPath $ResolverBatmanRoot -BuilderRootPath 'fallback-builder'
        $absoluteBuilderRoot = Resolve-OptionalBuilderRoot -BatmanRootPath $ResolverBatmanRoot -BuilderRootPath $ResolverAbsoluteBuilderRoot
        $omittedBuilderRoot = Resolve-OptionalBuilderRoot -BatmanRootPath $ResolverBatmanRoot
    } finally {
        Pop-Location
    }
    foreach ($resolvedPath in @(
        [pscustomobject]@{ Actual = $cwdRelativeBuilderRoot; Expected = $ResolverCwdBuilderRoot; Context = 'current-directory-relative BuilderRoot' },
        [pscustomobject]@{ Actual = $fallbackBuilderRoot; Expected = $ResolverFallbackBuilderRoot; Context = 'Batman-root-relative BuilderRoot' },
        [pscustomobject]@{ Actual = $absoluteBuilderRoot; Expected = $ResolverAbsoluteBuilderRoot; Context = 'absolute BuilderRoot' },
        [pscustomobject]@{ Actual = $omittedBuilderRoot; Expected = $ResolverCwdBuilderRoot; Context = 'omitted BuilderRoot' }
    )) {
        if (-not [String]::Equals([IO.Path]::GetFullPath($resolvedPath.Actual), [IO.Path]::GetFullPath($resolvedPath.Expected), [StringComparison]::OrdinalIgnoreCase)) {
            throw "$($resolvedPath.Context) resolved to '$($resolvedPath.Actual)' instead of '$($resolvedPath.Expected)'."
        }
    }
    if ([IO.Path]::GetPathRoot($GameBin) -cne [IO.Path]::GetPathRoot($StagingRoot) -or [IO.Path]::GetPathRoot($GameBin) -cne [IO.Path]::GetPathRoot($RecoveryRoot)) {
        throw 'Deployment staging and recovery roots must share the GameBin volume.'
    }
    Reset-DeploymentFixture -LivePackRoot $LivePackRoot -LiveConfigPath $LiveConfigPath -LiveHelenGameHookPath $LiveHelenGameHookPath -LiveProxyPath $LiveProxyPath -SubtitlePackRoot $SubtitlePackRoot -StagingRoot $StagingRoot -RecoveryRoot $RecoveryRoot -StagedPackRoot $StagedPackRoot -StagedConfigPath $StagedConfigPath -StagedHelenGameHookPath $StagedHelenGameHookPath -StagedProxyPath $StagedProxyPath -IncludeExistingGraphicsPack $true
    [IO.File]::WriteAllBytes($LiveTargetPath, [Text.Encoding]::ASCII.GetBytes('old-target'))
    Assert-BatmanGraphicsVsyncHooks -PackRoot $StagedPackRoot -Context 'Staged deployment fixture'
    Assert-BatmanGraphicsCommands -PackRoot $StagedPackRoot -Context 'Staged deployment fixture'
    $validStagedHooksText = Get-Content -LiteralPath (Join-Path $StagedPackRoot 'builds\steam-goty-1.0\hooks.json') -Raw
    $mutatedQualityHooksPath = Join-Path $StagedPackRoot 'builds\steam-goty-1.0\hooks.json'
    $protocolMutations = @(
        [pscustomobject]@{ Name = 'catalog request omission'; Mutate = { param($hooks) $hooks.stateObservers[1].dynamicResponse.requests = @($hooks.stateObservers[1].dynamicResponse.requests | Select-Object -SkipLast 1) } },
        [pscustomobject]@{ Name = 'duplicate resolution write code'; Mutate = { param($hooks) $hooks.stateObservers[2].mappings[0].match = $hooks.stateObservers[2].mappings[1].match } },
        [pscustomobject]@{ Name = 'dynamic scalar bound'; Mutate = { param($hooks) $hooks.stateObservers[1].dynamicResponse.minimumValue = 0 } },
        [pscustomobject]@{ Name = 'resolution acknowledgement'; Mutate = { param($hooks) $hooks.stateObservers[2].acknowledgementMappings[0].value = 5199 } },
        [pscustomobject]@{ Name = 'quality response'; Mutate = { param($hooks) $hooks.stateObservers[7].responseRequestValue = 4698 } }
    )
    foreach ($mutation in $protocolMutations) {
        $mutatedHooks = $validStagedHooksText | ConvertFrom-Json
        & $mutation.Mutate $mutatedHooks
        [IO.File]::WriteAllText($mutatedQualityHooksPath, ($mutatedHooks | ConvertTo-Json -Depth 100), [Text.UTF8Encoding]::new($false))
        $mutationRejected = $false
        try {
            Assert-BatmanGraphicsVsyncHooks -PackRoot $StagedPackRoot -Context "Mutated $($mutation.Name) fixture"
        } catch {
            $mutationRejected = $true
        }
        if (-not $mutationRejected) { throw "Deploy validator accepted a mutated $($mutation.Name) declaration." }
    }
    [IO.File]::WriteAllText($mutatedQualityHooksPath, $validStagedHooksText, [Text.UTF8Encoding]::new($false))
    $ExpectedHelenGameHookHash = (Get-FileHash -LiteralPath $StagedHelenGameHookPath -Algorithm SHA256).Hash
    $ExpectedProxyHash = (Get-FileHash -LiteralPath $StagedProxyPath -Algorithm SHA256).Hash

    $StaleStagingRoot = Join-Path $GameBin '.helengamehook-staging-stale'
    New-Item -ItemType Directory -Force -Path (Join-Path $StaleStagingRoot 'nested') | Out-Null
    [IO.File]::WriteAllText((Join-Path $StaleStagingRoot 'normal.txt'), 'stale')
    $hiddenStalePath = Join-Path (Join-Path $StaleStagingRoot 'nested') 'hidden.txt'
    [IO.File]::WriteAllText($hiddenStalePath, 'stale-hidden')
    (Get-Item -LiteralPath $hiddenStalePath -Force).Attributes = [IO.FileAttributes]::Hidden
    $staleStagingFailureObserved = $false
    $staleStagingFailureMessage = ''
    try {
        Initialize-DeploymentRoots -GameBin $GameBin -StagingRoot $StaleStagingRoot -RecoveryRoot (Join-Path $GameBin '.helengamehook-recovery-stale') -PackStagingDestination (Join-Path $StaleStagingRoot 'pack') -ConfigStagingPath (Join-Path (Join-Path $StaleStagingRoot 'config') 'packs.json') -PackParent (Split-Path -Parent $LivePackRoot) -ConfigParent (Split-Path -Parent $LiveConfigPath) | Out-Null
    } catch {
        $staleStagingFailureObserved = $true
        $staleStagingFailureMessage = $_.Exception.Message
    }
    if (-not $staleStagingFailureObserved -or $staleStagingFailureMessage -notmatch 'already exists') { throw 'Pre-existing staging root was not rejected.' }
    if ((Get-Content -LiteralPath (Join-Path $LivePackRoot 'state.txt') -Raw) -cne 'old-pack') { throw 'Pre-existing staging root check changed the live graphics pack.' }
    if (-not (Test-Path -LiteralPath (Join-Path $StaleStagingRoot 'normal.txt')) -or -not (Test-Path -LiteralPath (Join-Path (Join-Path $StaleStagingRoot 'nested') 'hidden.txt'))) { throw 'Pre-existing staging root check removed stale files.' }
    Remove-Item -LiteralPath $StaleStagingRoot -Recurse -Force

    New-Item -ItemType Directory -Force -Path $StaleStagingRoot | Out-Null
    $emptyStagingFailureObserved = $false
    $emptyStagingFailureMessage = ''
    try {
        Initialize-DeploymentRoots -GameBin $GameBin -StagingRoot $StaleStagingRoot -RecoveryRoot (Join-Path $GameBin '.helengamehook-recovery-empty-stale') -PackStagingDestination (Join-Path $StaleStagingRoot 'pack') -ConfigStagingPath (Join-Path (Join-Path $StaleStagingRoot 'config') 'packs.json') -PackParent (Split-Path -Parent $LivePackRoot) -ConfigParent (Split-Path -Parent $LiveConfigPath) | Out-Null
    } catch {
        $emptyStagingFailureObserved = $true
        $emptyStagingFailureMessage = $_.Exception.Message
    }
    if (-not $emptyStagingFailureObserved -or $emptyStagingFailureMessage -notmatch 'already exists') { throw 'Empty pre-existing staging root was not rejected.' }
    if (-not (Test-Path -LiteralPath $StaleStagingRoot -PathType Container)) { throw 'Empty pre-existing staging root was removed.' }
    Remove-Item -LiteralPath $StaleStagingRoot -Recurse -Force

    $SetupFailureStagingRoot = Join-Path $GameBin '.helengamehook-staging-setup-failure'
    $SetupFailureRecoveryRoot = Join-Path $GameBin '.helengamehook-recovery-setup-failure'
    $SetupFailurePackStagingRoot = Join-Path $SetupFailureStagingRoot 'pack'
    $SetupFailureConfigStagingPath = Join-Path (Join-Path $SetupFailureStagingRoot 'config') 'packs.json'
    $SetupFailureObserved = $false
    $SetupFailureMessage = ''
    try {
        Initialize-DeploymentRoots -GameBin $GameBin -StagingRoot $SetupFailureStagingRoot -RecoveryRoot $SetupFailureRecoveryRoot -PackStagingDestination $SetupFailurePackStagingRoot -ConfigStagingPath $SetupFailureConfigStagingPath -PackParent (Split-Path -Parent $LivePackRoot) -ConfigParent (Split-Path -Parent $LiveConfigPath) -FailureInjection 'AfterRecoveryRootCreation' | Out-Null
    } catch {
        $SetupFailureObserved = $true
        $SetupFailureMessage = $_.Exception.Message
    }
    if (-not $SetupFailureObserved -or $SetupFailureMessage -notmatch 'AfterRecoveryRootCreation') { throw 'Setup failure injection did not preserve the primary error.' }
    if (Test-Path -LiteralPath $SetupFailureStagingRoot) { throw 'Setup failure left its staging root behind.' }
    if (Test-Path -LiteralPath $SetupFailureRecoveryRoot) { throw 'Setup failure left an empty recovery root behind.' }

    $VerifyPublication = {
        param($LivePackPath, $LiveConfigPathForVerification, $LiveHelenPath, $LiveProxyPath)
        if ((Get-Content -LiteralPath (Join-Path $LivePackPath 'state.txt') -Raw) -cne 'new-pack') { throw 'Verifier saw the wrong graphics pack.' }
        Assert-BatmanGraphicsVsyncHooks -PackRoot $LivePackPath -Context 'Activated deployment fixture'
        Assert-BatmanGraphicsCommands -PackRoot $LivePackPath -Context 'Activated deployment fixture'
        $config = Get-Content -LiteralPath $LiveConfigPathForVerification -Raw | ConvertFrom-Json
        $packs = @($config.enabledPacksByExecutable.'ShippingPC-BmGame.exe')
        if ($packs.Count -ne 2 -or $packs[0] -ne 'batman-aa-subtitles' -or $packs[1] -ne 'batman-aa-graphics-options') { throw 'Verifier saw the wrong pack order.' }
        if ((Get-FileHash -LiteralPath $LiveHelenPath -Algorithm SHA256).Hash -cne $ExpectedHelenGameHookHash) { throw 'Verifier saw the wrong HelenGameHook.dll.' }
        if ((Get-FileHash -LiteralPath $LiveProxyPath -Algorithm SHA256).Hash -cne $ExpectedProxyHash) { throw 'Verifier saw the wrong dinput8.dll.' }
    }.GetNewClosure()

    Reset-DeploymentFixture -LivePackRoot $LivePackRoot -LiveConfigPath $LiveConfigPath -LiveHelenGameHookPath $LiveHelenGameHookPath -LiveProxyPath $LiveProxyPath -SubtitlePackRoot $SubtitlePackRoot -StagingRoot $StagingRoot -RecoveryRoot $RecoveryRoot -StagedPackRoot $StagedPackRoot -StagedConfigPath $StagedConfigPath -StagedHelenGameHookPath $StagedHelenGameHookPath -StagedProxyPath $StagedProxyPath -IncludeExistingGraphicsPack $true -IncludeStagedStateSentinel $false
    $preVerifierFailurePack = @(Get-DirectorySnapshot -Root $LivePackRoot)
    $preVerifierFailureConfigHash = (Get-FileHash -LiteralPath $LiveConfigPath -Algorithm SHA256).Hash
    $preVerifierFailureHelenHash = (Get-FileHash -LiteralPath $LiveHelenGameHookPath -Algorithm SHA256).Hash
    $preVerifierFailureProxyHash = (Get-FileHash -LiteralPath $LiveProxyPath -Algorithm SHA256).Hash
    $preVerifierFailureTargetHash = (Get-FileHash -LiteralPath $LiveTargetPath -Algorithm SHA256).Hash
    $preVerifierFailureSubtitle = @(Get-DirectorySnapshot -Root $SubtitlePackRoot)
    $stagedHooksPath = Join-Path $StagedPackRoot 'builds\steam-goty-1.0\hooks.json'
    [IO.File]::WriteAllText($stagedHooksPath, '{"runtimeSlots":[],"stateObservers":[],"hooks":[]}', [Text.UTF8Encoding]::new($false))
    $stagedVerifier = {
        param($StagedPackPath, $StagedConfigPathForVerification, $StagedHelenPath, $StagedProxyPath)
        $previousErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            $output = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PackageVerifierPath -BatmanRoot $BatmanRoot -BuilderRoot $BuilderRootPath -Configuration Release -PackRootOverride $StagedPackPath -TargetPathOverride (Join-Path $BuilderRootPath 'generated\graphics-options-experiment\Frontend-graphics-options.umap') -StagedPackageValidation 2>&1)
            $exitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $previousErrorActionPreference
        }
        if ($exitCode -eq 0) { throw 'STAGED_VERIFIER_UNEXPECTED_PASS: corrupt hooks.json was accepted.' }
        throw "STAGED_VERIFIER_REJECTED: exit=$exitCode output=$($output -join [Environment]::NewLine)"
    }.GetNewClosure()
    $stagedVerifierFailureObserved = $false
    $stagedVerifierFailureMessage = ''
    try {
        Invoke-AtomicGraphicsDeployment -GameBin $GameBin -LivePackRoot $LivePackRoot -LiveConfigPath $LiveConfigPath -LiveHelenGameHookPath $LiveHelenGameHookPath -LiveProxyPath $LiveProxyPath -StagedPackRoot $StagedPackRoot -StagedConfigPath $StagedConfigPath -StagedHelenGameHookPath $StagedHelenGameHookPath -StagedProxyPath $StagedProxyPath -PackBackupRoot $PackBackupRoot -ConfigBackupPath $ConfigBackupPath -HelenGameHookBackupPath $HelenGameHookBackupPath -ProxyBackupPath $ProxyBackupPath -RecoveryRoot $RecoveryRoot -StagingRoot $StagingRoot -VerifyPublication $VerifyPublication -VerifyStagedPublication $stagedVerifier
    } catch {
        $stagedVerifierFailureObserved = $true
        $stagedVerifierFailureMessage = $_.Exception.Message
    }
    if (-not $stagedVerifierFailureObserved -or $stagedVerifierFailureMessage -notmatch 'STAGED_VERIFIER_REJECTED') { throw "Corrupt staged package did not produce a true verifier rejection: $stagedVerifierFailureMessage" }
    Assert-DirectorySnapshotEqual -Expected $preVerifierFailurePack -ActualRoot $LivePackRoot -Context 'Staged verifier rejection live pack'
    if ((Get-FileHash -LiteralPath $LiveConfigPath -Algorithm SHA256).Hash -cne $preVerifierFailureConfigHash -or (Get-FileHash -LiteralPath $LiveHelenGameHookPath -Algorithm SHA256).Hash -cne $preVerifierFailureHelenHash -or (Get-FileHash -LiteralPath $LiveProxyPath -Algorithm SHA256).Hash -cne $preVerifierFailureProxyHash -or (Get-FileHash -LiteralPath $LiveTargetPath -Algorithm SHA256).Hash -cne $preVerifierFailureTargetHash) { throw 'Staged verifier rejection changed a live config, target, or runtime hash.' }
    Assert-DirectorySnapshotEqual -Expected $preVerifierFailureSubtitle -ActualRoot $SubtitlePackRoot -Context 'Staged verifier rejection subtitle pack'
    foreach ($backupPath in @($PackBackupRoot, $ConfigBackupPath, $HelenGameHookBackupPath, $ProxyBackupPath)) { if (Test-Path -LiteralPath $backupPath) { throw "Staged verifier rejection left backup state: $backupPath" } }
    if ((Test-Path -LiteralPath $RecoveryRoot -PathType Container) -and @(Get-ChildItem -LiteralPath $RecoveryRoot -Force).Count -ne 0) { throw "Staged verifier rejection left recovery state: $RecoveryRoot" }
    Remove-DeploymentStagingRoot -StagingRoot $StagingRoot -GameBin $GameBin

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

    $ReparseTarget = Join-Path $TestRoot 'reparse-target'
    $ReparseLink = Join-Path $GameBin 'reparse-link'
    $ReparseCleanupStagingRoot = Join-Path $GameBin '.reparse-cleanup-staging'
    $ReparseCleanupTarget = Join-Path $TestRoot 'reparse-cleanup-target'
    New-Item -ItemType Directory -Force -Path $ReparseTarget, $ReparseCleanupStagingRoot, $ReparseCleanupTarget | Out-Null
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
    $reparseItem = Get-Item -LiteralPath $ReparseLink -Force
    if (($reparseItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) { throw 'Reparse fixture does not expose the ReparsePoint attribute.' }
    Remove-Item -LiteralPath $ReparseTarget -Recurse -Force
    $reparseCandidate = Join-Path $ReparseLink 'payload.txt'
    if (-not (Test-PathWithinRoot -Path $reparseCandidate -Root $GameBin)) { throw 'Reparse candidate is not lexically beneath GameBin.' }
    $reparseRejected = $false
    $reparseMessage = ''
    try { Assert-SafeDeploymentPath -Path $reparseCandidate -AllowedRoots @($GameBin) } catch { $reparseRejected = $true; $reparseMessage = $_.Exception.Message }
    if (-not $reparseRejected -or $reparseMessage -notmatch 'reparse point') { throw "Safe path validator did not reject the in-root dangling reparse traversal: $reparseMessage" }
    $reparseCleanupLink = Join-Path $ReparseCleanupStagingRoot 'reparse-link'
    try {
        New-Item -ItemType Junction -Path $reparseCleanupLink -Target $ReparseCleanupTarget -ErrorAction Stop | Out-Null
    } catch {
        try {
            New-Item -ItemType SymbolicLink -Path $reparseCleanupLink -Target $ReparseCleanupTarget -ErrorAction Stop | Out-Null
        } catch {
            throw "Unable to evaluate recursive reparse cleanup fixture: $($_.Exception.Message)"
        }
    }
    $reparseCleanupItem = Get-Item -LiteralPath $reparseCleanupLink -Force
    if (($reparseCleanupItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) { throw 'Recursive reparse fixture does not expose the ReparsePoint attribute.' }
    $reparseCleanupRejected = $false
    $reparseCleanupMessage = ''
    try { Remove-DeploymentStagingRoot -StagingRoot $ReparseCleanupStagingRoot -GameBin $GameBin } catch { $reparseCleanupRejected = $true; $reparseCleanupMessage = $_.Exception.Message }
    if (-not $reparseCleanupRejected -or $reparseCleanupMessage -notmatch 'reparse point') { throw "Recursive staging cleanup accepted a reparse point: $reparseCleanupMessage" }
    Remove-Item -LiteralPath $reparseCleanupLink -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $ReparseCleanupStagingRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $ReparseLink -Force -ErrorAction SilentlyContinue

    Write-Output 'PASS'
}
finally {
    if (Test-Path -LiteralPath $TestRoot) { Remove-Item -LiteralPath $TestRoot -Recurse -Force }
}
