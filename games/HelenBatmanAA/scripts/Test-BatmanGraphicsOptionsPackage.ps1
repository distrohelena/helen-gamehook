param(
    [string]$BatmanRoot,
    [string]$BuilderRoot,
    [string]$Configuration = 'Release',
    [string]$PackRootOverride,
    [string]$TargetPathOverride,
    [switch]$StagedPackageValidation
)

$ErrorActionPreference = 'Stop'
$ExpectedGraphicsShellSha256 = '74C7453CD4D4F194C28E5BC3B689AC1F47BB97B2C3F296345E32B01E438D5679'

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

function Assert-StrictJsonInteger {
    <# Accept only non-null integral CLR values produced for JSON integers. #>
    param(
        [Parameter(Mandatory = $true)] [AllowNull()] [object]$Value,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    if ($null -eq $Value) { throw "$Context must be a non-null JSON integer." }
    $integralTypes = @([byte], [sbyte], [int16], [uint16], [int32], [uint32], [int64], [uint64])
    if ($integralTypes -notcontains $Value.GetType()) { throw "$Context must be an integral JSON number, not $($Value.GetType().FullName)." }
    [decimal]$decimalValue = $Value
    if ($decimalValue -gt [decimal][long]::MaxValue) { throw "$Context is outside the signed 64-bit JSON integer range." }
    return [long]$Value
}

function Assert-StrictJsonString {
    <# Accept only non-null JSON strings, preserving ordinal identity checks for callers. #>
    param(
        [Parameter(Mandatory = $true)] [AllowNull()] [object]$Value,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    if ($null -eq $Value -or $Value -isnot [string]) { throw "$Context must be a non-null JSON string." }
    return [string]$Value
}

function Assert-StrictJsonArray {
    <# Require an actual JSON array so scalar objects cannot masquerade as one-item arrays. #>
    param(
        [Parameter(Mandatory = $true)] [AllowNull()] [object]$Value,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    if ($null -eq $Value -or $Value -isnot [Array]) { throw "$Context must be a JSON array." }
}

function Assert-StrictJsonIntegerEquals {
    param(
        [Parameter(Mandatory = $true)] [AllowNull()] [object]$Value,
        [Parameter(Mandatory = $true)] [long]$Expected,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    $actual = Assert-StrictJsonInteger -Value $Value -Context $Context
    if ($actual -ne $Expected) { throw "$Context drifted. Expected $Expected but found $actual." }
}

function Assert-StrictJsonStringEquals {
    param(
        [Parameter(Mandatory = $true)] [AllowNull()] [object]$Value,
        [Parameter(Mandatory = $true)] [string]$Expected,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    $actual = Assert-StrictJsonString -Value $Value -Context $Context
    if (-not [String]::Equals($actual, $Expected, [StringComparison]::Ordinal)) { throw "$Context drifted. Expected '$Expected' but found '$actual'." }
}

function Assert-StrictJsonIntegerArrayEquals {
    param(
        [Parameter(Mandatory = $true)] [AllowNull()] [object]$Values,
        [Parameter(Mandatory = $true)] [long[]]$Expected,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    Assert-StrictJsonArray -Value $Values -Context $Context
    $actualValues = @($Values)
    if ($actualValues.Count -ne $Expected.Count) { throw "$Context count drifted. Expected $($Expected.Count) but found $($actualValues.Count)." }
    for ($index = 0; $index -lt $Expected.Count; $index++) {
        Assert-StrictJsonIntegerEquals -Value $actualValues[$index] -Expected $Expected[$index] -Context "$Context value $($index + 1)"
    }
}

function Assert-StrictJsonMappingEquals {
    param(
        [Parameter(Mandatory = $true)] [AllowNull()] [object]$Mapping,
        [Parameter(Mandatory = $true)] [long]$ExpectedMatch,
        [Parameter(Mandatory = $true)] [long]$ExpectedValue,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    if ($null -eq $Mapping) { throw "$Context must be an object." }
    Assert-ExactOrderedProperties -Object $Mapping -Names @('match', 'value') -Context $Context
    Assert-StrictJsonIntegerEquals -Value $Mapping.match -Expected $ExpectedMatch -Context "$Context match"
    Assert-StrictJsonIntegerEquals -Value $Mapping.value -Expected $ExpectedValue -Context "$Context value"
}

function Assert-GraphicsFilesJsonShape {
    <# Enforce the complete generated virtual-file schema before binary validation. #>
    param(
        [Parameter(Mandatory = $true)] [AllowNull()] [object]$Files,
        [Parameter(Mandatory = $true)] [long]$ExpectedBaseSize,
        [Parameter(Mandatory = $true)] [string]$ExpectedBaseSha256,
        [Parameter(Mandatory = $true)] [long]$ExpectedTargetSize,
        [Parameter(Mandatory = $true)] [string]$ExpectedTargetSha256
    )

    if ($null -eq $Files) { throw 'files.json must contain an object.' }
    Assert-ExactOrderedProperties -Object $Files -Names @('virtualFiles') -Context 'files.json'
    Assert-StrictJsonArray -Value $Files.virtualFiles -Context 'files.json virtualFiles'
    if (@($Files.virtualFiles).Count -ne 1) { throw 'files.json must contain exactly one virtual file.' }
    $virtualFile = @($Files.virtualFiles)[0]
    Assert-ExactOrderedProperties -Object $virtualFile -Names @('id', 'path', 'mode', 'source') -Context 'files.json virtualFiles[0]'
    Assert-StrictJsonStringEquals -Value $virtualFile.id -Expected 'frontendGraphicsOptionsPackage' -Context 'files.json virtualFiles[0].id'
    Assert-StrictJsonStringEquals -Value $virtualFile.path -Expected 'BmGame/CookedPC/Maps/Frontend/Frontend.umap' -Context 'files.json virtualFiles[0].path'
    Assert-StrictJsonStringEquals -Value $virtualFile.mode -Expected 'delta-on-read' -Context 'files.json virtualFiles[0].mode'
    if ($null -eq $virtualFile.source) { throw 'files.json virtualFiles[0].source must be an object.' }
    Assert-ExactOrderedProperties -Object $virtualFile.source -Names @('kind', 'path', 'base', 'target', 'chunkSize') -Context 'files.json virtualFiles[0].source'
    Assert-StrictJsonStringEquals -Value $virtualFile.source.kind -Expected 'delta-file' -Context 'files.json virtualFiles[0].source.kind'
    Assert-StrictJsonStringEquals -Value $virtualFile.source.path -Expected 'assets/deltas/Frontend-graphics-options.hgdelta' -Context 'files.json virtualFiles[0].source.path'
    Assert-StrictJsonIntegerEquals -Value $virtualFile.source.chunkSize -Expected 65536 -Context 'files.json virtualFiles[0].source.chunkSize'

    $expectedNestedSizes = [ordered]@{ base = $ExpectedBaseSize; target = $ExpectedTargetSize }
    $expectedNestedHashes = [ordered]@{ base = $ExpectedBaseSha256; target = $ExpectedTargetSha256 }
    foreach ($nestedName in @('base', 'target')) {
        $nested = $virtualFile.source.$nestedName
        if ($null -eq $nested) { throw "files.json virtualFiles[0].source.$nestedName must be an object." }
        Assert-ExactOrderedProperties -Object $nested -Names @('size', 'sha256') -Context "files.json virtualFiles[0].source.$nestedName"
        Assert-StrictJsonIntegerEquals -Value $nested.size -Expected $expectedNestedSizes[$nestedName] -Context "files.json virtualFiles[0].source.$nestedName.size"
        Assert-StrictJsonStringEquals -Value $nested.sha256 -Expected $expectedNestedHashes[$nestedName] -Context "files.json virtualFiles[0].source.$nestedName.sha256"
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

function Assert-ExpectedSha256 {
    <# Require both a complete digest and the independently reviewed graphics shell identity. #>
    param(
        [Parameter(Mandatory = $true)] [string]$Hash,
        [Parameter(Mandatory = $true)] [string]$Expected,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    if ($Hash -notmatch '\A[0-9A-Fa-f]{64}\z') { throw "$Context must be a full 64-character SHA-256 digest, found '$Hash'." }
    if ($Hash -cne $Expected) { throw "$Context drifted. Expected '$Expected', found '$Hash'." }
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

function Assert-CurrentGraphicsSourceProvenance {
    <# Prove the exact production shell call graph and keep legacy builders outside that graph. #>
    param(
        [Parameter(Mandatory = $true)] [string]$RebuildPath,
        [Parameter(Mandatory = $true)] [string]$ShellTemplatePath,
        [Parameter(Mandatory = $true)] [string]$ShellBuilderPath,
        [Parameter(Mandatory = $true)] [string]$BuilderProgramPath,
        [Parameter(Mandatory = $true)] [string]$XmlPatcherPath
    )

    foreach ($path in @($RebuildPath, $ShellTemplatePath, $ShellBuilderPath, $BuilderProgramPath, $XmlPatcherPath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Current graphics shell provenance source was not found: $path"
        }
    }

    $rebuildText = Get-Content -LiteralPath $RebuildPath -Raw
    $shellTemplateText = Get-Content -LiteralPath $ShellTemplatePath -Raw
    $shellBuilderText = Get-Content -LiteralPath $ShellBuilderPath -Raw
    $builderProgramText = Get-Content -LiteralPath $BuilderProgramPath -Raw
    $xmlPatcherText = Get-Content -LiteralPath $XmlPatcherPath -Raw
    $legacyTemplatePath = Join-Path (Split-Path -Parent $ShellTemplatePath) 'GraphicsOptionsScriptTemplates.cs'
    if (-not (Test-Path -LiteralPath $legacyTemplatePath -PathType Leaf)) { throw "Legacy graphics template source was not found: $legacyTemplatePath" }
    $legacyTemplateText = Get-Content -LiteralPath $legacyTemplatePath -Raw
    $legacySourceText = $legacyTemplateText + $shellBuilderText + $xmlPatcherText
    $shellBuilderShellMatch = [regex]::Match($shellBuilderText, '(?s)private static void PatchFrontendShellScripts\(.*?(?=\r?\n\s*/// <summary>)')
    if (-not $shellBuilderShellMatch.Success) { throw 'Current shell builder shell patch method could not be isolated for provenance validation.' }
    $shellBuilderShellText = $shellBuilderShellMatch.Value
    $buildShellMatch = [regex]::Match($shellBuilderText, '(?s)public static void BuildShell\(.*?(?=\r?\n\s*/// <summary>)')
    if (-not $buildShellMatch.Success) { throw 'Current shell builder BuildShell method could not be isolated for provenance validation.' }
    $buildShellText = $buildShellMatch.Value
    $programDispatchMatch = [regex]::Match($builderProgramText, '(?s)"build-main-menu-graphics-shell"\s*=>\s*RunBuildMainMenuGraphicsShell\(tail\)')
    if (-not $programDispatchMatch.Success) { throw 'NativeSubtitleExePatcher does not dispatch the production shell command to RunBuildMainMenuGraphicsShell.' }
    $programShellMatch = [regex]::Match($builderProgramText, '(?s)private static int RunBuildMainMenuGraphicsShell\(.*?(?=\r?\n\s*/// <summary>)')
    if (-not $programShellMatch.Success) { throw 'NativeSubtitleExePatcher shell command method could not be isolated for provenance validation.' }
    $programShellText = $programShellMatch.Value
    $patchShellMatch = [regex]::Match($xmlPatcherText, '(?s)public static void PatchShell\(.*?(?=\r?\n\s*/// <summary>)')
    if (-not $patchShellMatch.Success) { throw 'GraphicsOptionsXmlPatcher PatchShell method could not be isolated for provenance validation.' }
    $patchShellText = $patchShellMatch.Value
    $shellSpriteMatch = [regex]::Match($xmlPatcherText, '(?s)private static void AppendGraphicsShellSpriteAndExport\(.*?(?=\r?\n\s*/// <summary>)')
    if (-not $shellSpriteMatch.Success) { throw 'GraphicsOptionsXmlPatcher shell sprite method could not be isolated for provenance validation.' }
    $shellSpriteText = $shellSpriteMatch.Value

    foreach ($required in @(
        'build-main-menu-graphics-shell', '--output-dir', '--ffdec', '--ini',
        'GraphicsOptionsAssetBuilder.BuildShell(paths)'
    )) {
        Assert-ContainsOrdinal -Text ($rebuildText + $programShellText) -Token $required -Context 'production graphics shell call graph'
    }
    foreach ($required in @(
        'GraphicsOptionsShellBuildPaths paths = GraphicsOptionsShellBuildPaths.FromRoot',
        'GraphicsOptionsAssetBuilder.BuildShell(paths)'
    )) {
        Assert-ContainsOrdinal -Text $programShellText -Token $required -Context 'NativeSubtitleExePatcher graphics shell route'
    }
    foreach ($required in @(
        'ValidateShellInputs(paths)',
        'BatmanGraphicsIniBootstrapLoader.Load(paths.BatmanUserIniPath)',
        'PatchFrontendShellScripts(paths.FrontendWorkingScriptsPath, bootstrapSnapshot)',
        'ValidateShellPatchedScriptSet(paths.FrontendWorkingScriptsPath)',
        'GraphicsOptionsXmlPatcher.PatchShell(paths.FrontendXmlPath, paths.FrontendPatchedXmlPath)',
        '"-importScript"', 'paths.FrontendOutputGfxPath', 'paths.FrontendWorkingScriptsPath'
    )) {
        Assert-ContainsOrdinal -Text $buildShellText -Token $required -Context 'GraphicsOptionsAssetBuilder BuildShell route'
    }
    Assert-ContainsOrdinal -Text $shellBuilderShellText -Token 'GraphicsOptionsShellScriptTemplates' -Context 'shell script template route'
    Assert-ContainsOrdinal -Text $patchShellText -Token 'AppendGraphicsShellSpriteAndExport(tags, optionsGamePcSprite)' -Context 'selective sprite-600 patch route'
    foreach ($required in @('ScreenOptionsGraphicsSpriteId', 'PatchGraphicsScreenSprite', 'CreateExportAssetsTag', 'CloneDoInitActionTagForSprite')) {
        Assert-ContainsOrdinal -Text $shellSpriteText -Token $required -Context 'selective sprite-600 import route'
    }
    foreach ($legacyToken in @('Helen_', 'GraphicsExitPrompt', 'DefineSprite_601')) {
        Assert-ContainsOrdinal -Text $legacySourceText -Token $legacyToken -Context "legacy full-controller source ($legacyToken)"
    }
    foreach ($legacyToken in @('public static void Patch(string inputXmlPath, string outputXmlPath)', 'AppendGraphicsSpritesAndExports', 'GraphicsExitPromptSpriteId')) {
        Assert-ContainsOrdinal -Text $xmlPatcherText -Token $legacyToken -Context "legacy XML patch route ($legacyToken)"
    }

    foreach ($forbidden in @(
        'F:\helenhook.7z', 'F:/helenhook.7z', 'batma/', 'batma\', 'Program Files',
        'GraphicsVsyncController', 'InitialVsync', 'DraftVsync', 'GraphicsExitPrompt',
        'DefineSprite_601', 'Helen_', 'prompt export', 'prompt route', 'old package', 'historical',
        'PatchFrontendScripts(', 'GraphicsOptionsScriptTemplates', 'AppendGraphicsSpritesAndExports',
        'GraphicsExitPromptSpriteId', 'YesNoPrompt', 'Patch(inputXmlPath, outputXmlPath)'
    )) {
        foreach ($source in @(
            [pscustomobject]@{ Name = 'rebuild'; Text = $rebuildText },
            [pscustomobject]@{ Name = 'current shell templates'; Text = $shellTemplateText },
            [pscustomobject]@{ Name = 'shell builder BuildShell'; Text = $buildShellText },
            [pscustomobject]@{ Name = 'shell builder shell method'; Text = $shellBuilderShellText },
            [pscustomobject]@{ Name = 'NativeSubtitleExePatcher shell route'; Text = $programShellText },
            [pscustomobject]@{ Name = 'XmlPatcher PatchShell'; Text = $patchShellText },
            [pscustomobject]@{ Name = 'XmlPatcher selective sprite route'; Text = $shellSpriteText }
        )) {
            Assert-NotContainsOrdinal -Text $source.Text -Token $forbidden -Context "$($source.Name) provenance source"
        }
    }

    if ($shellBuilderText -notmatch '(?s)ShellPatchedScriptRelativePaths\s*=\s*\[.*?\];') {
        throw 'Current shell builder must declare its exact patched source file allow-list.'
    }
    $sourceAllowList = [regex]::Match($shellBuilderText, '(?s)ShellPatchedScriptRelativePaths\s*=\s*\[(?<items>.*?)\];').Groups['items'].Value
    $allowListCount = ([regex]::Matches($sourceAllowList, '"')).Count / 2
    if ($allowListCount -ne 21) { throw "Current shell builder source allow-list must contain exactly 21 files, found $allowListCount." }
}

function Assert-BatmanRuntimeDisplayProviderContract {
    <# Validate the runtime provider as a multi-part source contract so lifetime and snapshot semantics cannot drift independently. #>
    param([Parameter(Mandatory = $true)] [string]$RuntimeSourcePath)
    if (-not (Test-Path -LiteralPath $RuntimeSourcePath -PathType Leaf)) { throw "Batman runtime source was not found: $RuntimeSourcePath" }
    $source = Get-Content -LiteralPath $RuntimeSourcePath -Raw
    $displayConstruction = $source.IndexOf('g_batman_display_mode_service = std::make_unique<helen::BatmanDisplayModeService>()', [StringComparison]::Ordinal)
    $graphicsConstruction = $source.IndexOf('g_batman_graphics_config_service = std::make_unique<helen::BatmanGraphicsConfigService>', [StringComparison]::Ordinal)
    if ($displayConstruction -lt 0 -or $graphicsConstruction -lt 0 -or $displayConstruction -ge $graphicsConstruction) { throw 'Batman display service must be constructed before the graphics config service.' }
    $callbackMatch = [regex]::Match($source, '(?s)const\s+helen::MemoryStateObserverDynamicResponseCallback\s+dynamic_response_callback\s*=\s*(?<body>.*?);\s*g_build_runtime_coordinator\s*=')
    if (-not $callbackMatch.Success) { throw 'Batman runtime dynamic provider callback could not be isolated.' }
    $callback = $callbackMatch.Groups['body'].Value
    foreach ($required in @('provider_id != BatmanDisplayModeProviderId', 'raw_request == BatmanDisplayModeCatalogRequest', 'TryGetIntPair', 'display_mode_service.Refresh()', 'raw_request == BatmanDisplayModeCurrentWidthRequest || raw_request == BatmanDisplayModeCurrentHeightRequest', 'raw_request == BatmanDisplayModeCurrentWidthRequest', 'QueryCatalogScalar')) {
        Assert-ContainsOrdinal -Text $callback -Token $required -Context "Batman runtime dynamic provider callback ($required)"
    }
    foreach ($required in @('const std::optional<int> catalog_value = display_mode_service.QueryCatalogScalar(raw_request)', 'if (!catalog_value.has_value())', 'display_catalog_current_pair->reset()')) {
        Assert-ContainsOrdinal -Text $callback -Token $required -Context "Batman runtime dynamic provider failure cleanup ($required)"
    }
    foreach ($required in @('BatmanDisplayModeProviderId = "batmanDisplayModes"', 'BatmanDisplayModeCatalogRequest = 4700', 'BatmanDisplayModeCurrentWidthRequest = 4897', 'BatmanDisplayModeCurrentHeightRequest = 4898')) {
        Assert-ContainsOrdinal -Text $source -Token $required -Context "Batman runtime display protocol constant ($required)"
    }
    if (([regex]::Matches($callback, 'display_catalog_current_pair->reset\(\)')).Count -lt 2 -or $source.IndexOf('std::make_shared<std::optional<helen::CommandIntPair>>()', [StringComparison]::Ordinal) -lt 0) { throw 'Batman runtime dynamic provider must clear the owned current-pair snapshot on refresh and after the height response.' }
    $coordinatorMatch = [regex]::Match($source, '(?s)g_build_runtime_coordinator\s*=\s*std::make_unique<helen::BuildRuntimeCoordinator>\(.*?\);')
    if (-not $coordinatorMatch.Success -or $coordinatorMatch.Value.IndexOf('dynamic_response_callback', [StringComparison]::Ordinal) -lt 0) { throw 'Batman runtime coordinator must receive the dynamic provider callback.' }
    $resetMatch = [regex]::Match($source, '(?s)void ResetPackRuntimeState\(\).*?\n    \}')
    if (-not $resetMatch.Success) { throw 'Batman runtime pack reset function could not be isolated.' }
    $reset = $resetMatch.Value
    $coordinatorReset = $reset.IndexOf('g_build_runtime_coordinator.reset()', [StringComparison]::Ordinal)
    $graphicsReset = $reset.IndexOf('g_batman_graphics_config_service.reset()', [StringComparison]::Ordinal)
    $displayReset = $reset.IndexOf('g_batman_display_mode_service.reset()', [StringComparison]::Ordinal)
    if ($coordinatorReset -lt 0 -or $graphicsReset -lt 0 -or $displayReset -lt 0 -or $coordinatorReset -ge $graphicsReset -or $graphicsReset -ge $displayReset) { throw 'Batman runtime reset must stop the coordinator before graphics and display services.' }
}

function Assert-GraphicsDynamicResponseParameterContract {
    <# Exercise the generator's strict numeric boundary so PowerShell coercion cannot alter protocol bounds. #>
    param([Parameter(Mandatory = $true)] [string]$RebuildPath)
    . $RebuildPath -FunctionsOnly
    $parameters = @{
        Id = 'catalog'
        AddressMatchValues = @(4700, 4899)
        DynamicResponseProvider = 'batmanDisplayModes'
        DynamicResponseRequests = @(4700, 4898)
        DynamicResponseMinimumValue = 1
        DynamicResponseMaximumValue = 32767
        FailureResponseValue = 4899
    }
    $valid = New-GraphicsCarrierObserver @parameters
    $validJson = $valid | ConvertTo-Json -Depth 6 | ConvertFrom-Json
    Assert-StrictJsonIntegerEquals -Value $validJson.dynamicResponse.minimumValue -Expected 1 -Context 'dynamic generator minimumValue'
    Assert-StrictJsonIntegerEquals -Value $validJson.dynamicResponse.maximumValue -Expected 32767 -Context 'dynamic generator maximumValue'
    foreach ($invalidBounds in @(
        [pscustomobject]@{ Minimum = 1.5; Maximum = 32767 },
        [pscustomobject]@{ Minimum = '1'; Maximum = 32767 },
        [pscustomobject]@{ Minimum = $true; Maximum = 32767 },
        [pscustomobject]@{ Minimum = $null; Maximum = 32767 },
        [pscustomobject]@{ Minimum = 0; Maximum = 32767 },
        [pscustomobject]@{ Minimum = 1; Maximum = 32768 }
    )) {
        $invalidParameters = $parameters.Clone()
        $invalidParameters['DynamicResponseMinimumValue'] = $invalidBounds.Minimum
        $invalidParameters['DynamicResponseMaximumValue'] = $invalidBounds.Maximum
        $rejected = $false
        try { $null = New-GraphicsCarrierObserver @invalidParameters } catch { $rejected = $true }
        if (-not $rejected) { throw "Dynamic response bounds accepted invalid values '$($invalidBounds.Minimum)'/'$($invalidBounds.Maximum)'." }
    }
    $commandParameters = $parameters.Clone()
    $commandParameters['Command'] = 'setBatmanGraphicsResolutionMode'
    $commandRejected = $false
    try { $null = New-GraphicsCarrierObserver @commandParameters } catch { $commandRejected = $true }
    if (-not $commandRejected) { throw 'Dynamic response observer accepted a non-empty command even though response-only observers cannot declare commands.' }
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
    if ($Index -in @(0, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13)) {
        $activeRows = @{
            0 = [pscustomobject]@{ RowIndex = 1; Values = 'this.Names = new Array("Windowed","Fullscreen");' }
            2 = [pscustomobject]@{ RowIndex = 3; Values = 'this.Names = new Array("Off","On");' }
            3 = [pscustomobject]@{ RowIndex = 4; Values = 'this.Names = new Array("Off","2x","4x","8x","16x");' }
            4 = [pscustomobject]@{ RowIndex = 5; Values = 'this.Names = new Array("Off","On");' }
            5 = [pscustomobject]@{ RowIndex = 6; Values = 'this.Names = new Array("Off","On");' }
            6 = [pscustomobject]@{ RowIndex = 7; Values = 'this.Names = new Array("Off","On");' }
            7 = [pscustomobject]@{ RowIndex = 8; Values = 'this.Names = new Array("Off","On");' }
            8 = [pscustomobject]@{ RowIndex = 9; Values = 'this.Names = new Array("Off","On");' }
            9 = [pscustomobject]@{ RowIndex = 10; Values = 'this.Names = new Array("Off","On");' }
            10 = [pscustomobject]@{ RowIndex = 11; Values = 'this.Names = new Array("Off","On");' }
            11 = [pscustomobject]@{ RowIndex = 12; Values = 'this.Names = new Array("Off","On");' }
            12 = [pscustomobject]@{ RowIndex = 13; Values = 'this.Names = new Array("Off","Normal","High");' }
            13 = [pscustomobject]@{ RowIndex = 14; Values = 'this.Names = new Array("Off","On");' }
        }
        $activeRow = $activeRows[$Index]
        Assert-ContainsOrdinal -Text $Text -Token $activeRow.Values -Context "$Context names"
        Assert-ContainsOrdinal -Text $Text -Token "this.RowIndex = $($activeRow.RowIndex);" -Context "$Context row index"
        Assert-ContainsOrdinal -Text $Text -Token 'this.State = _parent.GraphicsOptionsController.GetDraftIndex(this.RowIndex);' -Context "$Context initial state"
        Assert-ContainsOrdinal -Text $Text -Token '_parent.GraphicsOptionsController.ToggleSetting(this.RowIndex);' -Context "$Context RunAction"
        Assert-ContainsOrdinal -Text $Text -Token '_parent.GraphicsOptionsController.IncrementSetting(this.RowIndex);' -Context "$Context Increment"
        Assert-ContainsOrdinal -Text $Text -Token '_parent.GraphicsOptionsController.DecrementSetting(this.RowIndex);' -Context "$Context Decrement"
        Assert-ContainsOrdinal -Text $Text -Token $labels[$Index] -Context "$Context label"
        Assert-ContainsOrdinal -Text $Text -Token 'this._visible = true;' -Context "$Context visibility"
        return
    }

    if ($Index -eq 1) {
        Assert-ContainsOrdinal -Text $Text -Token 'this.Names = new Array();' -Context "$Context names"
        Assert-ContainsOrdinal -Text $Text -Token 'this.RowIndex = 2;' -Context "$Context row index"
        Assert-ContainsOrdinal -Text $Text -Token 'GetResolutionDraftIndex();' -Context "$Context draft state"
        Assert-ContainsOrdinal -Text $Text -Token 'ToggleResolution();' -Context "$Context RunAction"
        Assert-ContainsOrdinal -Text $Text -Token 'IncrementResolution();' -Context "$Context Increment"
        Assert-ContainsOrdinal -Text $Text -Token 'DecrementResolution();' -Context "$Context Decrement"
        Assert-ContainsOrdinal -Text $Text -Token 'ResolutionModes' -Context "$Context catalog labels"
        Assert-ContainsOrdinal -Text $Text -Token '.Label' -Context "$Context catalog labels"
        Assert-ContainsOrdinal -Text $Text -Token 'this._visible = true;' -Context "$Context visibility"
        if ($Text.IndexOf('this.Names = new Array("Not active");', [StringComparison]::Ordinal) -ge 0) { throw "$Context must not be inactive." }
        return
    }

    if ($Index -eq 4) {
        Assert-ContainsOrdinal -Text $Text -Token 'this.Names = new Array("Low","Medium","High","Very High","Custom");' -Context "$Context names"
        Assert-ContainsOrdinal -Text $Text -Token 'this.State = -1;' -Context "$Context state"
        Assert-ContainsOrdinal -Text $Text -Token 'this.Initial = -1;' -Context "$Context initial state"
        Assert-ContainsOrdinal -Text $Text -Token 'this.Default = -1;' -Context "$Context default state"
        Assert-ContainsOrdinal -Text $Text -Token 'GetDetailLevelDraftIndex();' -Context "$Context draft state"
        Assert-ContainsOrdinal -Text $Text -Token 'GetDetailLevelInitialIndex();' -Context "$Context initial state"
        Assert-ContainsOrdinal -Text $Text -Token 'ToggleDetailPreset();' -Context "$Context RunAction"
        Assert-ContainsOrdinal -Text $Text -Token 'IncrementDetailPreset();' -Context "$Context Increment"
        Assert-ContainsOrdinal -Text $Text -Token 'DecrementDetailPreset();' -Context "$Context Decrement"
        if ($Text.IndexOf('ToggleSetting', [StringComparison]::Ordinal) -ge 0 -or $Text.IndexOf('IncrementSetting', [StringComparison]::Ordinal) -ge 0 -or $Text.IndexOf('DecrementSetting', [StringComparison]::Ordinal) -ge 0) { throw "$Context must delegate only to detail preset methods." }
        Assert-ContainsOrdinal -Text $Text -Token 'this._visible = true;' -Context "$Context visibility"
        return
    }

    if ($Index -eq 14) {
        Assert-ContainsOrdinal -Text $Text -Token 'this.Names = new Array("");' -Context "$Context names"
        Assert-ContainsOrdinal -Text $Text -Token 'Apply Changes' -Context "$Context label"
        Assert-ContainsOrdinal -Text $Text -Token 'this.ItemText.text = "";' -Context "$Context value"
        Assert-ContainsOrdinal -Text $Text -Token 'this._visible = true;' -Context "$Context visibility"
        Assert-ContainsOrdinal -Text $Text -Token '_parent.GraphicsOptionsController.ApplyChanges();' -Context "$Context RunAction"
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
    param(
        [Parameter(Mandatory = $true)] [string]$ExportRoot,
        [Parameter(Mandatory = $true)] [string]$XmlPath
    )

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
    foreach ($token in @('rs.ui.BatmanGraphicsOptionsController', 'InitializationComplete', 'InitializationFailed', 'BeginInitialization', 'PollInitialization', 'GetDraftIndex', 'GetInitialIndex', 'ApplyChanges', 'BeginRollback', 'CompleteRollback', 'FailRollback', 'ReadRequest:4200', 'ReadRequest:4300', 'ReadRequest:4400', 'ReadRequest:4500', 'FE_GetControlType')) {
        Assert-ContainsOrdinal -Text $screenText -Token $token -Context 'Options Graphics screen script'
    }
    Assert-ContainsOrdinal -Text $screenText -Token 'this.Screen.BlockInput(true);' -Context 'Options Graphics apply input block'
    Assert-ContainsOrdinal -Text $screenText -Token 'this.Screen.BlockInput(false);' -Context 'Options Graphics apply input unblock'
    if ($screenText -notmatch 'this\.CurrentPendingSetting\.WriteRequestBase\s*\+\s*this\.CurrentPendingSetting\.DraftIndex') { throw 'Options Graphics screen script must dispatch the current setting write request through its draft index.' }
    if ($screenText -notmatch 'FE_SetControlType",4990\s*\+\s*this\.ApplySignalToggle\s*,\s*""\s*\)') { throw 'Options Graphics screen script must dispatch FE_SetControlType 4990 plus ApplySignalToggle with an empty second argument.' }
    foreach ($staleToken in @('GraphicsVsyncController', 'InitialVsync', 'DraftVsync', 'InitialStateResolved', 'InitialStateFailed', 'CompleteApply', 'SetVsync', 'IncrementVsync', 'DecrementVsync')) {
        Assert-NotContainsOrdinal -Text $screenText -Token $staleToken -Context 'Options Graphics screen script'
    }

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
    $xmlText = Get-Content -LiteralPath $XmlPath -Raw
    foreach ($forbidden in @('Helen_', 'GraphicsExitPrompt', 'GraphicsVsyncController', 'InitialVsync', 'DraftVsync', 'DefineSprite_601')) {
        Assert-NotContainsOrdinal -Text $xmlText -Token $forbidden -Context 'generated shell XML artifact'
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
$rebuildSourcePath = Join-Path $PSScriptRoot 'Rebuild-BatmanGraphicsOptionsExperiment.ps1'
$runtimeSourcePath = Join-Path (Split-Path -Parent $PSScriptRoot | Split-Path -Parent | Split-Path -Parent) 'HelenGameHook\HelenGameHook.cpp'
Assert-RebuildAtomicSourceContract -ScriptPath $rebuildSourcePath
Assert-GraphicsDynamicResponseParameterContract -RebuildPath $rebuildSourcePath
Assert-BatmanRuntimeDisplayProviderContract -RuntimeSourcePath $runtimeSourcePath
Assert-CurrentGraphicsSourceProvenance -RebuildPath $rebuildSourcePath -ShellTemplatePath (Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\GraphicsOptionsShellScriptTemplates.cs') -ShellBuilderPath (Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\GraphicsOptionsAssetBuilder.cs') -BuilderProgramPath (Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\Program.cs') -XmlPatcherPath (Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\GraphicsOptionsXmlPatcher.cs')
Assert-AtomicPublicationRegression
Assert-ExactPackFileSet -Root $packRoot -BuildDirectoryName 'steam-goty-1.0'
if (-not $StagedPackageValidation) { Assert-ExactGeneratedTargetFileSet -Root $stableGeneratedRoot -TargetPath $targetPath }
if (-not (Test-Path -LiteralPath $hooksJsonPath -PathType Leaf)) { throw 'Graphics-options package must contain hooks.json.' }
if (Test-Path -LiteralPath $texturesJsonPath) { throw 'Graphics-options shell must not contain textures.json.' }

$pack = Get-Content -LiteralPath $packJsonPath -Raw | ConvertFrom-Json
Assert-ExactOrderedProperties -Object $pack -Names @('schemaVersion', 'id', 'name', 'targets', 'config', 'builds') -Context 'pack.json'
Assert-StrictJsonIntegerEquals -Value $pack.schemaVersion -Expected 1 -Context 'pack.json schemaVersion'
Assert-StrictJsonStringEquals -Value $pack.id -Expected 'batman-aa-graphics-options' -Context 'pack.json id'
Assert-StrictJsonStringEquals -Value $pack.name -Expected 'Batman Graphics Options' -Context 'pack.json name'
Assert-StrictJsonArray -Value $pack.targets -Context 'pack.json targets'
if (@($pack.targets).Count -ne 1) { throw 'pack.json must contain exactly one target.' }
Assert-ExactOrderedProperties -Object $pack.targets[0] -Names @('gameId', 'executables') -Context 'pack.json target'
Assert-StrictJsonStringEquals -Value $pack.targets[0].gameId -Expected 'batman-arkham-asylum' -Context 'pack.json target gameId'
Assert-StrictJsonArray -Value $pack.targets[0].executables -Context 'pack.json target executables'
if (@($pack.targets[0].executables).Count -ne 1) { throw 'pack.json target must contain exactly one executable.' }
Assert-StrictJsonStringEquals -Value $pack.targets[0].executables[0] -Expected 'ShippingPC-BmGame.exe' -Context 'pack.json target executable'
$expectedConfigKeys = @('fullscreen', 'resolutionWidth', 'resolutionHeight', 'resolutionModeIndex', 'vsync', 'msaa', 'detailLevel', 'bloom', 'dynamicShadows', 'motionBlur', 'distortion', 'fogVolumes', 'sphericalHarmonicLighting', 'ambientOcclusion', 'physx', 'stereo', 'applySignal', 'rollbackSignal')
Assert-StrictJsonArray -Value $pack.config -Context 'pack.json config'
if (@($pack.config).Count -ne $expectedConfigKeys.Count) { throw "pack.json config must contain exactly $($expectedConfigKeys.Count) entries; the checked-in package is stale for the Task 8 display protocol." }
for ($index = 0; $index -lt $expectedConfigKeys.Count; $index++) {
    $configEntry = $pack.config[$index]
    Assert-ExactOrderedProperties -Object $configEntry -Names @('key', 'type', 'defaultValue') -Context "pack.json config entry $($index + 1)"
    Assert-StrictJsonStringEquals -Value $configEntry.key -Expected $expectedConfigKeys[$index] -Context "pack.json config entry $($index + 1) key"
    Assert-StrictJsonStringEquals -Value $configEntry.type -Expected 'int' -Context "pack.json config entry $($index + 1) type"
    $expectedDefault = if ($configEntry.key -eq 'resolutionModeIndex') { -1 } else { 0 }
    Assert-StrictJsonIntegerEquals -Value $configEntry.defaultValue -Expected $expectedDefault -Context "pack.json config entry $($index + 1) defaultValue"
}
Assert-StrictJsonArray -Value $pack.builds -Context 'pack.json builds'
if (@($pack.builds).Count -ne 1) { throw 'pack.json build list must contain exactly one build.' }
Assert-StrictJsonStringEquals -Value $pack.builds[0] -Expected 'steam-goty-1.0' -Context 'pack.json build list entry'

$build = Get-Content -LiteralPath $buildJsonPath -Raw | ConvertFrom-Json
Assert-ExactOrderedProperties -Object $build -Names @('id', 'executable', 'match', 'startupCommands') -Context 'build.json'
$expectedMatch = & (Join-Path $PSScriptRoot 'Get-BatmanSteamBuildMatch.ps1')
Assert-StrictJsonStringEquals -Value $build.id -Expected $expectedMatch.BuildId -Context 'build.json id'
Assert-StrictJsonStringEquals -Value $build.executable -Expected $expectedMatch.Executable -Context 'build.json executable'
Assert-StrictJsonIntegerEquals -Value $build.match.fileSize -Expected $expectedMatch.FileSize -Context 'build.json match fileSize'
Assert-StrictJsonStringEquals -Value $build.match.sha256 -Expected $expectedMatch.Sha256 -Context 'build.json match sha256'
Assert-ExactProperties -Object $build.match -Names @('fileSize', 'sha256') -Context 'build.json match'
Assert-StrictJsonArray -Value $build.startupCommands -Context 'build.json startupCommands'
if (@($build.startupCommands).Count -ne 1) { throw 'build.json startup command list must contain exactly one command.' }
Assert-StrictJsonStringEquals -Value $build.startupCommands[0] -Expected 'loadBatmanGraphicsDraftIntoConfig' -Context 'build.json startup command'

$bindings = Get-Content -LiteralPath $bindingsJsonPath -Raw | ConvertFrom-Json
Assert-ExactProperties -Object $bindings -Names @('bindings') -Context 'bindings.json'
Assert-StrictJsonArray -Value $bindings.bindings -Context 'bindings.json bindings'
if (@($bindings.bindings).Count -ne 0) { throw 'bindings.json must contain zero bindings.' }
$commands = Get-Content -LiteralPath $commandsJsonPath -Raw | ConvertFrom-Json
Assert-ExactProperties -Object $commands -Names @('commands') -Context 'commands.json'
Assert-StrictJsonArray -Value $commands.commands -Context 'commands.json commands'
if (@($commands.commands).Count -ne 4) { throw 'commands.json must contain exactly four commands.' }
$loadCommand = $commands.commands[0]
$syncCommand = $commands.commands[1]
$resolutionCommand = $commands.commands[2]
$applyCommand = $commands.commands[3]
Assert-ExactOrderedProperties -Object $loadCommand -Names @('id', 'name', 'steps') -Context 'commands.json load command'
Assert-ExactOrderedProperties -Object $syncCommand -Names @('id', 'name', 'steps') -Context 'commands.json sync command'
Assert-ExactOrderedProperties -Object $resolutionCommand -Names @('id', 'name', 'steps') -Context 'commands.json resolution command'
Assert-ExactOrderedProperties -Object $applyCommand -Names @('id', 'name', 'steps') -Context 'commands.json apply command'
Assert-StrictJsonStringEquals -Value $loadCommand.id -Expected 'loadBatmanGraphicsDraftIntoConfig' -Context 'commands.json load command id'
Assert-StrictJsonStringEquals -Value $loadCommand.name -Expected 'Load Batman Graphics Draft Into Config' -Context 'commands.json load command name'
Assert-StrictJsonArray -Value $loadCommand.steps -Context 'commands.json load steps'
if (@($loadCommand.steps).Count -ne 1) { throw 'commands.json load command must contain exactly one step.' }
Assert-StrictJsonStringEquals -Value $syncCommand.id -Expected 'syncBatmanGraphicsDetailLevel' -Context 'commands.json sync command id'
Assert-StrictJsonStringEquals -Value $syncCommand.name -Expected 'Sync Batman Graphics Detail Level' -Context 'commands.json sync command name'
Assert-StrictJsonArray -Value $syncCommand.steps -Context 'commands.json sync steps'
if (@($syncCommand.steps).Count -ne 1) { throw 'commands.json sync command must contain exactly one step.' }
Assert-StrictJsonStringEquals -Value $resolutionCommand.id -Expected 'setBatmanGraphicsResolutionMode' -Context 'commands.json resolution command id'
Assert-StrictJsonStringEquals -Value $resolutionCommand.name -Expected 'Set Batman Graphics Resolution Mode' -Context 'commands.json resolution command name'
Assert-StrictJsonArray -Value $resolutionCommand.steps -Context 'commands.json resolution steps'
if (@($resolutionCommand.steps).Count -ne 1) { throw 'commands.json resolution command must contain exactly one step.' }
Assert-StrictJsonStringEquals -Value $applyCommand.id -Expected 'applyBatmanGraphicsDraft' -Context 'commands.json apply command id'
Assert-StrictJsonStringEquals -Value $applyCommand.name -Expected 'Apply Batman Graphics Draft' -Context 'commands.json apply command name'
Assert-StrictJsonArray -Value $applyCommand.steps -Context 'commands.json apply steps'
if (@($applyCommand.steps).Count -ne 2) { throw 'commands.json apply command must contain exactly two steps.' }
Assert-ExactOrderedProperties -Object $loadCommand.steps[0] -Names @('kind') -Context 'commands.json load step'
Assert-ExactOrderedProperties -Object $syncCommand.steps[0] -Names @('kind') -Context 'commands.json sync step'
Assert-ExactOrderedProperties -Object $resolutionCommand.steps[0] -Names @('kind') -Context 'commands.json resolution step'
Assert-ExactOrderedProperties -Object $applyCommand.steps[0] -Names @('kind') -Context 'commands.json apply config step'
Assert-ExactOrderedProperties -Object $applyCommand.steps[1] -Names @('kind') -Context 'commands.json apply load step'
Assert-StrictJsonStringEquals -Value $loadCommand.steps[0].kind -Expected 'load-batman-graphics-draft-into-config' -Context 'commands.json load step kind'
Assert-StrictJsonStringEquals -Value $syncCommand.steps[0].kind -Expected 'sync-batman-graphics-detail-level' -Context 'commands.json sync step kind'
Assert-StrictJsonStringEquals -Value $resolutionCommand.steps[0].kind -Expected 'set-batman-graphics-resolution-mode' -Context 'commands.json resolution step kind'
Assert-StrictJsonStringEquals -Value $applyCommand.steps[0].kind -Expected 'apply-batman-graphics-config' -Context 'commands.json apply config step kind'
Assert-StrictJsonStringEquals -Value $applyCommand.steps[1].kind -Expected 'load-batman-graphics-draft-into-config' -Context 'commands.json apply load step kind'

$hooks = Get-Content -LiteralPath $hooksJsonPath -Raw | ConvertFrom-Json
Assert-ExactOrderedProperties -Object $hooks -Names @('runtimeSlots', 'stateObservers', 'hooks') -Context 'hooks.json'
Assert-StrictJsonArray -Value $hooks.runtimeSlots -Context 'hooks.json runtimeSlots'
Assert-StrictJsonArray -Value $hooks.stateObservers -Context 'hooks.json stateObservers'
Assert-StrictJsonArray -Value $hooks.hooks -Context 'hooks.json hooks'
if (@($hooks.runtimeSlots).Count -ne 0 -or @($hooks.hooks).Count -ne 0 -or @($hooks.stateObservers).Count -ne 16) { throw 'hooks.json runtime slots, observers, or hooks count drifted: the checked-in package is stale for the Task 8 display protocol.' }

function Assert-GraphicsCarrierChecks {
    param([Parameter(Mandatory = $true)] [psobject]$Observer, [Parameter(Mandatory = $true)] [string]$Context)
    Assert-StrictJsonArray -Value $Observer.checks -Context "$Context checks"
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
        Assert-StrictJsonStringEquals -Value $check.comparison -Expected 'equals-constant' -Context "$Context check $($index + 1) comparison"
        Assert-StrictJsonIntegerEquals -Value $check.offset -Expected $expected.offset -Context "$Context check $($index + 1) offset"
        Assert-StrictJsonIntegerEquals -Value $check.expectedValue -Expected $expected.expectedValue -Context "$Context check $($index + 1) expectedValue"
    }
}

$expectedObserverProperties = @('id', 'addressGroup', 'scanStartAddress', 'scanEndAddress', 'scanStride', 'valueOffset', 'pollIntervalMs', 'targetConfigKey', 'addressMatchValues', 'checks', 'mappings', 'responseRequestValue', 'responseMappings', 'acknowledgementMappings', 'failureResponseValue')
$expectedCommandObserverProperties = @('id', 'addressGroup', 'scanStartAddress', 'scanEndAddress', 'scanStride', 'valueOffset', 'pollIntervalMs', 'targetConfigKey', 'addressMatchValues', 'checks', 'mappings', 'acknowledgementMappings', 'failureResponseValue', 'command')
$expectedCommandSettingObserverProperties = @('id', 'addressGroup', 'scanStartAddress', 'scanEndAddress', 'scanStride', 'valueOffset', 'pollIntervalMs', 'targetConfigKey', 'addressMatchValues', 'checks', 'mappings', 'responseRequestValue', 'responseMappings', 'acknowledgementMappings', 'failureResponseValue', 'command')
$expectedDynamicObserverProperties = @('id', 'addressGroup', 'scanStartAddress', 'scanEndAddress', 'scanStride', 'valueOffset', 'pollIntervalMs', 'addressMatchValues', 'checks', 'dynamicResponse', 'failureResponseValue')
$expectedResolutionObserverProperties = @('id', 'addressGroup', 'scanStartAddress', 'scanEndAddress', 'scanStride', 'valueOffset', 'pollIntervalMs', 'targetConfigKey', 'addressMatchValues', 'checks', 'mappings', 'acknowledgementMappings', 'failureResponseValue', 'command')
$expectedObserverIds = @('graphicsObserverFullscreen', 'graphicsObserverDisplayModeCatalog', 'graphicsObserverResolutionModeIndex', 'graphicsObserverVsync', 'graphicsObserverMsaa', 'graphicsObserverPhysx', 'graphicsObserverStereo', 'graphicsObserverBloom', 'graphicsObserverDynamicShadows', 'graphicsObserverMotionBlur', 'graphicsObserverDistortion', 'graphicsObserverFogVolumes', 'graphicsObserverSphericalHarmonicLighting', 'graphicsObserverAmbientOcclusion', 'graphicsObserverApplySignal', 'graphicsObserverRollbackSignal')
$expectedObserverTargets = @('fullscreen', '', 'resolutionModeIndex', 'vsync', 'msaa', 'physx', 'stereo', 'bloom', 'dynamicShadows', 'motionBlur', 'distortion', 'fogVolumes', 'sphericalHarmonicLighting', 'ambientOcclusion', 'applySignal', 'rollbackSignal')
$expectedAddressMatchValues = @(
    4670, 4671, 4672, 4673, 4674, 4675, 4676, 4679,
    4960, 4961, 4969, 4970, 4971, 4980, 4981, 4989, 4990, 4991,
    5199,
    4200, 4210, 4211, 4220, 4221, 4230, 4231, 4299,
    4300, 4310, 4311, 4312, 4313, 4314, 4320, 4321, 4322, 4323, 4324, 4330, 4331, 4332, 4333, 4334, 4399,
    4400, 4410, 4411, 4412, 4420, 4421, 4422, 4430, 4431, 4432, 4499,
    4500, 4510, 4511, 4520, 4521, 4530, 4531, 4599,
    4600, 4601, 4602, 4603, 4604, 4605, 4606, 4609,
    4610, 4611, 4612, 4613, 4614, 4615, 4616, 4619,
    4620, 4621, 4622, 4623, 4624, 4625, 4626, 4629,
    4630, 4631, 4632, 4633, 4634, 4635, 4636, 4639,
    4640, 4641, 4642, 4643, 4644, 4645, 4646, 4649,
    4650, 4651, 4652, 4653, 4654, 4655, 4656, 4659,
    4660, 4661, 4662, 4663, 4664, 4665, 4666, 4669
)
$expectedAddressMatchValues = @($expectedAddressMatchValues + (4700..4899) + (5000..5097) + (5100..5197) | Sort-Object -Unique)
for ($observerIndex = 0; $observerIndex -lt $expectedObserverIds.Count; $observerIndex++) {
    $observer = $hooks.stateObservers[$observerIndex]
    $expectedProperties = if ($observerIndex -eq 1) { $expectedDynamicObserverProperties } elseif ($observerIndex -eq 2) { $expectedResolutionObserverProperties } elseif ($observerIndex -eq 0 -or $observerIndex -ge 3 -and $observerIndex -le 6) { $expectedObserverProperties } elseif ($observerIndex -ge 7 -and $observerIndex -le 13) { $expectedCommandSettingObserverProperties } else { $expectedCommandObserverProperties }
    Assert-ExactOrderedProperties -Object $observer -Names $expectedProperties -Context "hooks.json $($expectedObserverIds[$observerIndex])"
    Assert-StrictJsonStringEquals -Value $observer.id -Expected $expectedObserverIds[$observerIndex] -Context "hooks.json observer $($observerIndex + 1) id"
    if ($observerIndex -ne 1) { Assert-StrictJsonStringEquals -Value $observer.targetConfigKey -Expected $expectedObserverTargets[$observerIndex] -Context 'hooks.json observer targetConfigKey' }
    elseif ($observer.PSObject.Properties.Name -contains 'targetConfigKey') { throw 'hooks.json catalog dynamic observer must omit targetConfigKey.' }
    Assert-StrictJsonStringEquals -Value $observer.addressGroup -Expected 'batmanFrontendControlType' -Context "hooks.json observer $($observerIndex + 1) addressGroup"
    Assert-StrictJsonStringEquals -Value $observer.scanStartAddress -Expected '0x10000000' -Context "hooks.json observer $($observerIndex + 1) scanStartAddress"
    Assert-StrictJsonStringEquals -Value $observer.scanEndAddress -Expected '0x30000000' -Context "hooks.json observer $($observerIndex + 1) scanEndAddress"
    Assert-StrictJsonIntegerEquals -Value $observer.scanStride -Expected 4 -Context "hooks.json observer $($observerIndex + 1) scanStride"
    Assert-StrictJsonIntegerEquals -Value $observer.valueOffset -Expected 12 -Context "hooks.json observer $($observerIndex + 1) valueOffset"
    Assert-StrictJsonIntegerEquals -Value $observer.pollIntervalMs -Expected 50 -Context "hooks.json observer $($observerIndex + 1) pollIntervalMs"
    Assert-StrictJsonIntegerArrayEquals -Values $observer.addressMatchValues -Expected $expectedAddressMatchValues -Context "hooks.json observer $($observerIndex + 1) addressMatchValues"
    foreach ($addressValue in @($observer.addressMatchValues)) {
        if ([long]$addressValue -le 0) { throw "hooks.json $($observer.id) addressMatchValues must contain positive discovery values only; ordinal-tagged dynamic negatives are transient responses." }
    }
    Assert-GraphicsCarrierChecks -Observer $observer -Context "hooks.json $($observer.id)"
    $mappingNames = if ($observerIndex -eq 1) { @() } elseif ($observerIndex -eq 2) { @('mappings', 'acknowledgementMappings') } elseif ($observerIndex -lt 11) { @('mappings', 'responseMappings', 'acknowledgementMappings') } else { @('mappings', 'acknowledgementMappings') }
    foreach ($mappingName in $mappingNames) {
        Assert-StrictJsonArray -Value $observer.$mappingName -Context "hooks.json $($observer.id) $mappingName"
        foreach ($entry in @($observer.$mappingName)) {
            if ($null -eq $entry) { throw "hooks.json $($observer.id) $mappingName contains a null mapping." }
            Assert-ExactOrderedProperties -Object $entry -Names @('match', 'value') -Context "hooks.json $($observer.id) $mappingName"
            $null = Assert-StrictJsonInteger -Value $entry.match -Context "hooks.json $($observer.id) $mappingName match"
            $null = Assert-StrictJsonInteger -Value $entry.value -Context "hooks.json $($observer.id) $mappingName value"
        }
    }
}

$catalogObserver = $hooks.stateObservers[1]
$fullscreenObserver = $hooks.stateObservers[0]
Assert-StrictJsonIntegerEquals -Value $fullscreenObserver.responseRequestValue -Expected 4670 -Context 'hooks.json fullscreen responseRequestValue'
Assert-StrictJsonMappingEquals -Mapping $fullscreenObserver.mappings[0] -ExpectedMatch 4673 -ExpectedValue 0 -Context 'hooks.json fullscreen off write mapping'
Assert-StrictJsonMappingEquals -Mapping $fullscreenObserver.mappings[1] -ExpectedMatch 4674 -ExpectedValue 1 -Context 'hooks.json fullscreen on write mapping'
Assert-StrictJsonMappingEquals -Mapping $fullscreenObserver.responseMappings[0] -ExpectedMatch 0 -ExpectedValue 4671 -Context 'hooks.json fullscreen off response mapping'
Assert-StrictJsonMappingEquals -Mapping $fullscreenObserver.responseMappings[1] -ExpectedMatch 1 -ExpectedValue 4672 -Context 'hooks.json fullscreen on response mapping'
Assert-StrictJsonMappingEquals -Mapping $fullscreenObserver.acknowledgementMappings[0] -ExpectedMatch 4673 -ExpectedValue 4675 -Context 'hooks.json fullscreen off acknowledgement mapping'
Assert-StrictJsonMappingEquals -Mapping $fullscreenObserver.acknowledgementMappings[1] -ExpectedMatch 4674 -ExpectedValue 4676 -Context 'hooks.json fullscreen on acknowledgement mapping'
Assert-StrictJsonIntegerEquals -Value $fullscreenObserver.failureResponseValue -Expected 4679 -Context 'hooks.json fullscreen failureResponseValue'
foreach ($forbiddenCatalogProperty in @('targetConfigKey', 'mappings', 'responseRequestValue', 'responseMappings', 'acknowledgementMappings', 'command')) {
    if ($catalogObserver.PSObject.Properties.Name -contains $forbiddenCatalogProperty) { throw "hooks.json catalog dynamic observer must omit forbidden static property '$forbiddenCatalogProperty'." }
}
Assert-ExactOrderedProperties -Object $catalogObserver.dynamicResponse -Names @('provider', 'requests', 'minimumValue', 'maximumValue') -Context 'hooks.json catalog dynamicResponse'
Assert-StrictJsonStringEquals -Value $catalogObserver.dynamicResponse.provider -Expected 'batmanDisplayModes' -Context 'hooks.json catalog provider'
Assert-StrictJsonIntegerArrayEquals -Values $catalogObserver.dynamicResponse.requests -Expected @(4700..4898) -Context 'hooks.json catalog requests'
Assert-StrictJsonIntegerEquals -Value $catalogObserver.dynamicResponse.minimumValue -Expected 1 -Context 'hooks.json catalog minimumValue'
Assert-StrictJsonIntegerEquals -Value $catalogObserver.dynamicResponse.maximumValue -Expected 32767 -Context 'hooks.json catalog maximumValue'
Assert-StrictJsonIntegerEquals -Value $catalogObserver.failureResponseValue -Expected 4899 -Context 'hooks.json catalog failureResponseValue'
$resolutionObserver = $hooks.stateObservers[2]
if (@($resolutionObserver.mappings).Count -ne 98 -or @($resolutionObserver.acknowledgementMappings).Count -ne 98) { throw 'hooks.json resolution observer must contain exactly 98 generated mappings.' }
for ($resolutionIndex = 0; $resolutionIndex -lt 98; $resolutionIndex++) {
    Assert-StrictJsonMappingEquals -Mapping $resolutionObserver.mappings[$resolutionIndex] -ExpectedMatch (5000 + $resolutionIndex) -ExpectedValue $resolutionIndex -Context "hooks.json resolution write mapping $($resolutionIndex + 1)"
    Assert-StrictJsonMappingEquals -Mapping $resolutionObserver.acknowledgementMappings[$resolutionIndex] -ExpectedMatch (5000 + $resolutionIndex) -ExpectedValue (5100 + $resolutionIndex) -Context "hooks.json resolution acknowledgement mapping $($resolutionIndex + 1)"
}
Assert-StrictJsonIntegerEquals -Value $resolutionObserver.failureResponseValue -Expected 5199 -Context 'hooks.json resolution failureResponseValue'
Assert-StrictJsonStringEquals -Value $resolutionObserver.command -Expected 'setBatmanGraphicsResolutionMode' -Context 'hooks.json resolution command'

$expectedGraphicsProtocols = @(
    [pscustomobject]@{ Id = 'graphicsObserverVsync'; Read = 4200; Responses = @(4210, 4211); Writes = @(4220, 4221); Acks = @(4230, 4231); Failure = 4299; ConfigValues = @(0, 1); Command = '' },
    [pscustomobject]@{ Id = 'graphicsObserverMsaa'; Read = 4300; Responses = @(4310, 4311, 4312, 4313, 4314); Writes = @(4320, 4321, 4322, 4323, 4324); Acks = @(4330, 4331, 4332, 4333, 4334); Failure = 4399; ConfigValues = @(0, 1, 2, 3, 5); Command = '' },
    [pscustomobject]@{ Id = 'graphicsObserverPhysx'; Read = 4400; Responses = @(4410, 4411, 4412); Writes = @(4420, 4421, 4422); Acks = @(4430, 4431, 4432); Failure = 4499; ConfigValues = @(0, 1, 2); Command = '' },
    [pscustomobject]@{ Id = 'graphicsObserverStereo'; Read = 4500; Responses = @(4510, 4511); Writes = @(4520, 4521); Acks = @(4530, 4531); Failure = 4599; ConfigValues = @(0, 1); Command = '' },
    [pscustomobject]@{ Id = 'graphicsObserverBloom'; Read = 4600; Responses = @(4601, 4602); Writes = @(4603, 4604); Acks = @(4605, 4606); Failure = 4609; ConfigValues = @(0, 1); Command = 'syncBatmanGraphicsDetailLevel' },
    [pscustomobject]@{ Id = 'graphicsObserverDynamicShadows'; Read = 4610; Responses = @(4611, 4612); Writes = @(4613, 4614); Acks = @(4615, 4616); Failure = 4619; ConfigValues = @(0, 1); Command = 'syncBatmanGraphicsDetailLevel' },
    [pscustomobject]@{ Id = 'graphicsObserverMotionBlur'; Read = 4620; Responses = @(4621, 4622); Writes = @(4623, 4624); Acks = @(4625, 4626); Failure = 4629; ConfigValues = @(0, 1); Command = 'syncBatmanGraphicsDetailLevel' },
    [pscustomobject]@{ Id = 'graphicsObserverDistortion'; Read = 4630; Responses = @(4631, 4632); Writes = @(4633, 4634); Acks = @(4635, 4636); Failure = 4639; ConfigValues = @(0, 1); Command = 'syncBatmanGraphicsDetailLevel' },
    [pscustomobject]@{ Id = 'graphicsObserverFogVolumes'; Read = 4640; Responses = @(4641, 4642); Writes = @(4643, 4644); Acks = @(4645, 4646); Failure = 4649; ConfigValues = @(0, 1); Command = 'syncBatmanGraphicsDetailLevel' },
    [pscustomobject]@{ Id = 'graphicsObserverSphericalHarmonicLighting'; Read = 4650; Responses = @(4651, 4652); Writes = @(4653, 4654); Acks = @(4655, 4656); Failure = 4659; ConfigValues = @(0, 1); Command = 'syncBatmanGraphicsDetailLevel' },
    [pscustomobject]@{ Id = 'graphicsObserverAmbientOcclusion'; Read = 4660; Responses = @(4661, 4662); Writes = @(4663, 4664); Acks = @(4665, 4666); Failure = 4669; ConfigValues = @(0, 1); Command = 'syncBatmanGraphicsDetailLevel' }
)
for ($protocolIndex = 0; $protocolIndex -lt $expectedGraphicsProtocols.Count; $protocolIndex++) {
    $protocol = $expectedGraphicsProtocols[$protocolIndex]
    $observer = $hooks.stateObservers[$protocolIndex + 3]
    if ($null -eq $observer.mappings -or @($observer.mappings).Count -ne $protocol.Writes.Count -or $null -eq $observer.responseMappings -or @($observer.responseMappings).Count -ne $protocol.Responses.Count -or $null -eq $observer.acknowledgementMappings -or @($observer.acknowledgementMappings).Count -ne $protocol.Acks.Count) { throw "hooks.json $($protocol.Id) mapping counts drifted." }
    Assert-StrictJsonIntegerEquals -Value $observer.responseRequestValue -Expected $protocol.Read -Context "hooks.json $($protocol.Id) responseRequestValue"
    Assert-StrictJsonIntegerEquals -Value $observer.failureResponseValue -Expected $protocol.Failure -Context "hooks.json $($protocol.Id) failureResponseValue"
    if ($protocolIndex -ge 4 -and $protocolIndex -lt 11) {
        Assert-StrictJsonStringEquals -Value $observer.command -Expected 'syncBatmanGraphicsDetailLevel' -Context "hooks.json $($protocol.Id) command"
    } elseif ($protocolIndex -lt 4 -and $observer.PSObject.Properties.Name -contains 'command') {
        throw "hooks.json $($protocol.Id) must not declare a command."
    }
    for ($mappingIndex = 0; $mappingIndex -lt $protocol.Writes.Count; $mappingIndex++) {
        Assert-StrictJsonMappingEquals -Mapping $observer.mappings[$mappingIndex] -ExpectedMatch $protocol.Writes[$mappingIndex] -ExpectedValue $protocol.ConfigValues[$mappingIndex] -Context "hooks.json $($protocol.Id) write mapping $($mappingIndex + 1)"
        Assert-StrictJsonMappingEquals -Mapping $observer.responseMappings[$mappingIndex] -ExpectedMatch $protocol.ConfigValues[$mappingIndex] -ExpectedValue $protocol.Responses[$mappingIndex] -Context "hooks.json $($protocol.Id) response mapping $($mappingIndex + 1)"
        Assert-StrictJsonMappingEquals -Mapping $observer.acknowledgementMappings[$mappingIndex] -ExpectedMatch $protocol.Writes[$mappingIndex] -ExpectedValue $protocol.Acks[$mappingIndex] -Context "hooks.json $($protocol.Id) acknowledgement mapping $($mappingIndex + 1)"
    }
}

$vsyncObserver = $hooks.stateObservers[3]
$msaaObserver = $hooks.stateObservers[4]
$physxObserver = $hooks.stateObservers[5]
$stereoObserver = $hooks.stateObservers[6]
$applyObserver = $hooks.stateObservers[14]
$rollbackObserver = $hooks.stateObservers[15]
Assert-StrictJsonStringEquals -Value $applyObserver.id -Expected 'graphicsObserverApplySignal' -Context 'hooks.json apply observer id'
Assert-StrictJsonStringEquals -Value $applyObserver.targetConfigKey -Expected 'applySignal' -Context 'hooks.json apply observer targetConfigKey'
Assert-StrictJsonStringEquals -Value $applyObserver.command -Expected 'applyBatmanGraphicsDraft' -Context 'hooks.json apply observer command'
Assert-StrictJsonStringEquals -Value $rollbackObserver.id -Expected 'graphicsObserverRollbackSignal' -Context 'hooks.json rollback observer id'
Assert-StrictJsonStringEquals -Value $rollbackObserver.targetConfigKey -Expected 'rollbackSignal' -Context 'hooks.json rollback observer targetConfigKey'
Assert-StrictJsonStringEquals -Value $rollbackObserver.command -Expected 'loadBatmanGraphicsDraftIntoConfig' -Context 'hooks.json rollback observer command'
$expectedApplyMappingMatches = @(4990, 4991)
$expectedApplyMappingValues = @(0, 1)
$expectedApplyAcknowledgementMatches = @(4990, 4991)
$expectedApplyAcknowledgementValues = @(4980, 4981)
$expectedRollbackMappingMatches = @(4970, 4971)
$expectedRollbackMappingValues = @(0, 1)
$expectedRollbackAcknowledgementMatches = @(4970, 4971)
$expectedRollbackAcknowledgementValues = @(4960, 4961)
if ($null -eq $applyObserver.mappings -or @($applyObserver.mappings).Count -ne $expectedApplyMappingMatches.Count -or $null -eq $applyObserver.acknowledgementMappings -or @($applyObserver.acknowledgementMappings).Count -ne $expectedApplyAcknowledgementMatches.Count) { throw 'hooks.json apply mapping counts drifted.' }
for ($mappingIndex = 0; $mappingIndex -lt $expectedApplyMappingMatches.Count; $mappingIndex++) {
    Assert-StrictJsonMappingEquals -Mapping $applyObserver.mappings[$mappingIndex] -ExpectedMatch $expectedApplyMappingMatches[$mappingIndex] -ExpectedValue $expectedApplyMappingValues[$mappingIndex] -Context "hooks.json apply mapping $($mappingIndex + 1)"
    Assert-StrictJsonMappingEquals -Mapping $applyObserver.acknowledgementMappings[$mappingIndex] -ExpectedMatch $expectedApplyAcknowledgementMatches[$mappingIndex] -ExpectedValue $expectedApplyAcknowledgementValues[$mappingIndex] -Context "hooks.json apply acknowledgement mapping $($mappingIndex + 1)"
}
Assert-StrictJsonIntegerEquals -Value $applyObserver.failureResponseValue -Expected 4989 -Context 'hooks.json apply failureResponseValue'
if ($null -eq $rollbackObserver.mappings -or @($rollbackObserver.mappings).Count -ne $expectedRollbackMappingMatches.Count -or $null -eq $rollbackObserver.acknowledgementMappings -or @($rollbackObserver.acknowledgementMappings).Count -ne $expectedRollbackAcknowledgementMatches.Count) { throw 'hooks.json rollback mapping counts drifted.' }
for ($mappingIndex = 0; $mappingIndex -lt $expectedRollbackMappingMatches.Count; $mappingIndex++) {
    Assert-StrictJsonMappingEquals -Mapping $rollbackObserver.mappings[$mappingIndex] -ExpectedMatch $expectedRollbackMappingMatches[$mappingIndex] -ExpectedValue $expectedRollbackMappingValues[$mappingIndex] -Context "hooks.json rollback mapping $($mappingIndex + 1)"
    Assert-StrictJsonMappingEquals -Mapping $rollbackObserver.acknowledgementMappings[$mappingIndex] -ExpectedMatch $expectedRollbackAcknowledgementMatches[$mappingIndex] -ExpectedValue $expectedRollbackAcknowledgementValues[$mappingIndex] -Context "hooks.json rollback acknowledgement mapping $($mappingIndex + 1)"
}
Assert-StrictJsonIntegerEquals -Value $rollbackObserver.failureResponseValue -Expected 4969 -Context 'hooks.json rollback failureResponseValue'

$files = Get-Content -LiteralPath $filesJsonPath -Raw | ConvertFrom-Json
$baseInfo = Get-Item -LiteralPath $basePath
$baseHash = (Get-FileHash -LiteralPath $basePath -Algorithm SHA256).Hash
if ($baseInfo.Length -ne 2988548 -or $baseHash -cne '271916B888F83374122AF0FCCC5C685804F4C8286A92A772CD71E4F48A00F2CC') { throw 'Retail Frontend.umap base identity is not the verified retail input.' }
$targetFileInfo = Get-Item -LiteralPath $targetPath
$targetFileHash = (Get-FileHash -LiteralPath $targetPath -Algorithm SHA256).Hash.ToLowerInvariant()
Assert-GraphicsFilesJsonShape -Files $files -ExpectedBaseSize $baseInfo.Length -ExpectedBaseSha256 $baseHash.ToLowerInvariant() -ExpectedTargetSize $targetFileInfo.Length -ExpectedTargetSha256 $targetFileHash
Assert-HgdeltaVirtualFileContract -Context 'graphics-options shell' -VirtualFile $files.virtualFiles[0] -ExpectedId 'frontendGraphicsOptionsPackage' -ExpectedPath 'BmGame/CookedPC/Maps/Frontend/Frontend.umap' -ExpectedMode 'delta-on-read' -ExpectedKind 'delta-file' -ExpectedDeltaRelativePath 'assets/deltas/Frontend-graphics-options.hgdelta' -BasePath $basePath -TargetPath $targetPath -DeltaFilePath $deltaPath -ChunkSize 65536 -ChunkTableOffset 116

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
    $extractedGfxHash = (Get-FileHash -LiteralPath $extractedGfxPath -Algorithm SHA256).Hash
    Assert-ExpectedSha256 -Hash $extractedGfxHash -Expected $ExpectedGraphicsShellSha256 -Context 'generated MainV2 shell GFX hash'
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

    Assert-ScopedExportedShellContract -ExportRoot $exportRoot -XmlPath $xmlPath

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
