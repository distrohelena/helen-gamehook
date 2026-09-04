param(
    [string]$Configuration = 'Release',
    [string]$BatmanRoot,
    [string]$BuilderRoot,
    [string]$BatmanUserIniPath,
    [switch]$FunctionsOnly
)

$ErrorActionPreference = 'Stop'
$env:MSBUILDDISABLENODEREUSE = '1'
. (Join-Path $PSScriptRoot 'BatmanBuilderWorkspaceHelpers.ps1')

function Write-Utf8TextFile {
    param([string]$Path, [string]$Contents)
    $directory = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($directory)) { New-Item -ItemType Directory -Force -Path $directory | Out-Null }
    [IO.File]::WriteAllText($Path, $Contents, [Text.UTF8Encoding]::new($false))
}

function New-GraphicsCarrierChecks {
    <#
    Return the ordered structural checks for the shared Batman graphics state carrier.
    The offsets and constants deliberately identify the verified retail structure;
    all shared graphics observers reuse this list so a raw state code controls only emission and
    never changes address resolution or causes observers to scan different shapes.
    #>
    return @(
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]-16
            expectedValue = [int]50
        },
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]-12
            expectedValue = [int]100
        },
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]-8
            expectedValue = [int]100
        },
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]-4
            expectedValue = [int]100
        },
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]0
            expectedValue = [int]4102
        },
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]4
            expectedValue = [int]1
        },
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]8
            expectedValue = [int]0
        },
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]16
            expectedValue = [int]4102
        },
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]20
            expectedValue = [int]2
        },
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]28
            expectedValue = [int]3
        },
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]32
            expectedValue = [int]3
        }
    )
}

function ConvertTo-StrictGraphicsIntegralValue {
    <#
    Validate one protocol value without accepting PowerShell coercions. Only the
    integral CLR numeric types that can represent the pack's signed int protocol
    values are accepted, and the result is range-checked before any int cast.
    #>
    param(
        [Parameter(Mandatory = $true)] [AllowNull()] [object]$Value,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    if ($null -eq $Value) { throw "$Context must be a non-null integral numeric value." }
    $integralTypes = @([byte], [sbyte], [int16], [uint16], [int32], [uint32], [int64], [uint64])
    if ($integralTypes -notcontains $Value.GetType()) { throw "$Context must be an integral CLR numeric value, not $($Value.GetType().FullName)." }
    [decimal]$decimalValue = $Value
    if ($decimalValue -lt [decimal][int]::MinValue -or $decimalValue -gt [decimal][int]::MaxValue) { throw "$Context is outside the signed 32-bit protocol range." }
    return [long]$decimalValue
}

function ConvertTo-StrictGraphicsIntegerArray {
    <# Validate and clone an array of raw protocol integers. #>
    param(
        [Parameter(Mandatory = $true)] [AllowNull()] [object[]]$Values,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    if ($null -eq $Values -or $Values.Count -eq 0) { throw "$Context must be a non-empty integer array." }
    $clonedValues = @(
        for ($index = 0; $index -lt $Values.Count; $index++) {
            [int](ConvertTo-StrictGraphicsIntegralValue -Value $Values[$index] -Context "$Context value $($index + 1)")
        }
    )
    return $clonedValues
}

function ConvertTo-StrictGraphicsMappingArray {
    <#
    Validate and clone mapping objects so generated manifests never retain caller
    references and never silently coerce strings, booleans, nulls, or fractions.
    #>
    param(
        [Parameter(Mandatory = $true)] [AllowNull()] [object[]]$Mappings,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    if ($null -eq $Mappings) { throw "$Context must be a non-null mapping array." }
    $clonedMappings = @(
        for ($index = 0; $index -lt $Mappings.Count; $index++) {
            $mapping = $Mappings[$index]
            if ($null -eq $mapping) { throw "$Context mapping $($index + 1) must be an object." }
            $propertyNames = if ($mapping -is [Collections.IDictionary]) { @($mapping.Keys | ForEach-Object { [string]$_ }) } else { @($mapping.PSObject.Properties.Name) }
            if ($propertyNames -notcontains 'match' -or $propertyNames -notcontains 'value') { throw "$Context mapping $($index + 1) must contain match and value members." }
            $matchValue = if ($mapping -is [Collections.IDictionary]) { $mapping['match'] } else { $mapping.PSObject.Properties['match'].Value }
            $configValue = if ($mapping -is [Collections.IDictionary]) { $mapping['value'] } else { $mapping.PSObject.Properties['value'].Value }
            $match = ConvertTo-StrictGraphicsIntegralValue -Value $matchValue -Context "$Context mapping $($index + 1) match"
            $value = ConvertTo-StrictGraphicsIntegralValue -Value $configValue -Context "$Context mapping $($index + 1) value"
            [ordered]@{ match = [int]$match; value = [int]$value }
        }
    )
    return $clonedMappings
}

$graphicsProtocol = @(
    [ordered]@{ Id='graphicsObserverFullscreen'; Key='fullscreen'; Read=4670; Responses=@(4671,4672); Writes=@(4673,4674); Acks=@(4675,4676); Failure=4679; ConfigValues=@(0,1); Command='' },
    [ordered]@{ Id='graphicsObserverVsync'; Key='vsync'; Read=4200; Responses=@(4210,4211); Writes=@(4220,4221); Acks=@(4230,4231); Failure=4299; ConfigValues=@(0,1); Command='' },
    [ordered]@{ Id='graphicsObserverMsaa'; Key='msaa'; Read=4300; Responses=@(4310,4311,4312,4313,4314); Writes=@(4320,4321,4322,4323,4324); Acks=@(4330,4331,4332,4333,4334); Failure=4399; ConfigValues=@(0,1,2,3,5); Command='' },
    [ordered]@{ Id='graphicsObserverPhysx'; Key='physx'; Read=4400; Responses=@(4410,4411,4412); Writes=@(4420,4421,4422); Acks=@(4430,4431,4432); Failure=4499; ConfigValues=@(0,1,2); Command='' },
    [ordered]@{ Id='graphicsObserverStereo'; Key='stereo'; Read=4500; Responses=@(4510,4511); Writes=@(4520,4521); Acks=@(4530,4531); Failure=4599; ConfigValues=@(0,1); Command='' },
    [ordered]@{ Id='graphicsObserverBloom'; Key='bloom'; Read=4600; Responses=@(4601,4602); Writes=@(4603,4604); Acks=@(4605,4606); Failure=4609; ConfigValues=@(0,1); Command='syncBatmanGraphicsDetailLevel' },
    [ordered]@{ Id='graphicsObserverDynamicShadows'; Key='dynamicShadows'; Read=4610; Responses=@(4611,4612); Writes=@(4613,4614); Acks=@(4615,4616); Failure=4619; ConfigValues=@(0,1); Command='syncBatmanGraphicsDetailLevel' },
    [ordered]@{ Id='graphicsObserverMotionBlur'; Key='motionBlur'; Read=4620; Responses=@(4621,4622); Writes=@(4623,4624); Acks=@(4625,4626); Failure=4629; ConfigValues=@(0,1); Command='syncBatmanGraphicsDetailLevel' },
    [ordered]@{ Id='graphicsObserverDistortion'; Key='distortion'; Read=4630; Responses=@(4631,4632); Writes=@(4633,4634); Acks=@(4635,4636); Failure=4639; ConfigValues=@(0,1); Command='syncBatmanGraphicsDetailLevel' },
    [ordered]@{ Id='graphicsObserverFogVolumes'; Key='fogVolumes'; Read=4640; Responses=@(4641,4642); Writes=@(4643,4644); Acks=@(4645,4646); Failure=4649; ConfigValues=@(0,1); Command='syncBatmanGraphicsDetailLevel' },
    [ordered]@{ Id='graphicsObserverSphericalHarmonicLighting'; Key='sphericalHarmonicLighting'; Read=4650; Responses=@(4651,4652); Writes=@(4653,4654); Acks=@(4655,4656); Failure=4659; ConfigValues=@(0,1); Command='syncBatmanGraphicsDetailLevel' },
    [ordered]@{ Id='graphicsObserverAmbientOcclusion'; Key='ambientOcclusion'; Read=4660; Responses=@(4661,4662); Writes=@(4663,4664); Acks=@(4665,4666); Failure=4669; ConfigValues=@(0,1); Command='syncBatmanGraphicsDetailLevel' }
)

$graphicsCommandValues = @(4960,4961,4969,4970,4971,4980,4981,4989,4990,4991)
$dynamicCatalogRequests = @(for ($request = 4700; $request -le 4898; $request++) { [int]$request })
$resolutionWriteRequests = @(for ($index = 0; $index -lt 98; $index++) { [int](5000 + $index) })
$resolutionAcknowledgementRequests = @(for ($index = 0; $index -lt 98; $index++) { [int](5100 + $index) })
$resolutionConfigValues = @(for ($index = 0; $index -lt 98; $index++) { [int]$index })
$dynamicCatalogFailure = [int]4899
$resolutionFailure = [int]5199
# Dynamic provider scalars are encoded by the native observer transport as negative
# ordinal-tagged magnitudes (ordinal * 32768 + value); these transient negatives
# are intentionally absent from every static address-match union.

function Assert-GraphicsProtocolTable {
    <#
    Validate the authoritative graphics-setting table and command signal values before
    any union, mapping, or observer generation can index malformed protocol data.
    #>
    param(
        [Parameter(Mandatory = $true)] [AllowNull()] [object[]]$Protocol,
        [Parameter(Mandatory = $true)] [AllowNull()] [object[]]$CommandValues
    )

    if ($null -eq $Protocol -or $Protocol.Count -eq 0) { throw 'Graphics protocol table must contain at least one setting.' }
    if ($null -eq $CommandValues -or $CommandValues.Count -eq 0) { throw 'Graphics command protocol must contain at least one raw value.' }
    $requiredPropertyNames = @('Id', 'Key', 'Read', 'Responses', 'Writes', 'Acks', 'Failure', 'ConfigValues', 'Command')
    $commandSet = [Collections.Generic.HashSet[int]]::new()
    for ($commandIndex = 0; $commandIndex -lt $CommandValues.Count; $commandIndex++) {
        $commandValue = ConvertTo-StrictGraphicsIntegralValue -Value $CommandValues[$commandIndex] -Context "Graphics command value $($commandIndex + 1)"
        if (-not $commandSet.Add([int]$commandValue)) { throw "Graphics command protocol contains duplicate raw value $commandValue." }
    }

    $settingRawSet = [Collections.Generic.HashSet[int]]::new()
    $settingIdSet = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $settingKeySet = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    for ($protocolIndex = 0; $protocolIndex -lt $Protocol.Count; $protocolIndex++) {
        $entry = $Protocol[$protocolIndex]
        $entryContext = "Graphics protocol entry $($protocolIndex + 1)"
        if ($null -eq $entry) { throw "$entryContext must be an object." }
        $actualPropertyNames = if ($entry -is [Collections.IDictionary]) { @($entry.Keys | ForEach-Object { [string]$_ } | Sort-Object) } else { @($entry.PSObject.Properties.Name | Sort-Object) }
        $expectedPropertyNames = @($requiredPropertyNames | Sort-Object)
        if (($actualPropertyNames -join '|') -cne ($expectedPropertyNames -join '|')) { throw "$entryContext must contain exactly Id, Key, Read, Responses, Writes, Acks, Failure, ConfigValues, and Command." }
        if ($entry.Id -isnot [string] -or [string]::IsNullOrWhiteSpace($entry.Id)) { throw "$entryContext Id must be a non-empty string." }
        if ($entry.Key -isnot [string] -or [string]::IsNullOrWhiteSpace($entry.Key)) { throw "$entryContext Key must be a non-empty string." }
        if ($entry.Command -isnot [string]) { throw "$entryContext Command must be a string." }
        if ($entry.Command.Length -gt 0 -and [string]::IsNullOrWhiteSpace($entry.Command)) { throw "$entryContext Command must not be whitespace-only when supplied." }
        if (-not $settingIdSet.Add($entry.Id)) { throw "$entryContext duplicates setting Id '$($entry.Id)'." }
        if (-not $settingKeySet.Add($entry.Key)) { throw "$entryContext duplicates setting Key '$($entry.Key)'." }
        $read = ConvertTo-StrictGraphicsIntegralValue -Value $entry.Read -Context "$entryContext Read"
        $failure = ConvertTo-StrictGraphicsIntegralValue -Value $entry.Failure -Context "$entryContext Failure"

        $categoryNames = @('Responses', 'Writes', 'Acks', 'ConfigValues')
        $categoryValues = [ordered]@{}
        foreach ($categoryName in $categoryNames) {
            $category = $entry.$categoryName
            if ($null -eq $category -or $category -isnot [Array] -or $category.Count -eq 0) { throw "$entryContext $categoryName must be a non-empty array." }
            $values = @()
            $categorySet = [Collections.Generic.HashSet[int]]::new()
            for ($valueIndex = 0; $valueIndex -lt $category.Count; $valueIndex++) {
                $value = ConvertTo-StrictGraphicsIntegralValue -Value $category[$valueIndex] -Context "$entryContext $categoryName value $($valueIndex + 1)"
                if (-not $categorySet.Add([int]$value)) { throw "$entryContext $categoryName contains duplicate value $value." }
                $values += [int]$value
            }
            $categoryValues[$categoryName] = $values
        }
        if ($categoryValues.Responses.Count -ne $categoryValues.Writes.Count -or $categoryValues.Responses.Count -ne $categoryValues.Acks.Count -or $categoryValues.Responses.Count -ne $categoryValues.ConfigValues.Count) { throw "$entryContext Responses, Writes, Acks, and ConfigValues must have equal cardinality." }

        $rawCategoryValues = [ordered]@{
            Read = @([int]$read)
            Responses = $categoryValues.Responses
            Writes = $categoryValues.Writes
            Acks = $categoryValues.Acks
            Failure = @([int]$failure)
        }
        $entryRawSet = [Collections.Generic.HashSet[int]]::new()
        foreach ($categoryName in @('Read', 'Responses', 'Writes', 'Acks', 'Failure')) {
            foreach ($rawValue in $rawCategoryValues[$categoryName]) {
                if (-not $entryRawSet.Add([int]$rawValue)) { throw "$entryContext raw protocol categories overlap at value $rawValue." }
                if ($commandSet.Contains([int]$rawValue)) { throw "$entryContext raw protocol value $rawValue conflicts with a command value." }
                if (-not $settingRawSet.Add([int]$rawValue)) { throw "$entryContext raw protocol value $rawValue duplicates another setting entry." }
            }
        }
    }
}

Assert-GraphicsProtocolTable -Protocol $graphicsProtocol -CommandValues $graphicsCommandValues

if ($dynamicCatalogRequests.Count -ne 199 -or $dynamicCatalogRequests[0] -ne 4700 -or $dynamicCatalogRequests[-1] -ne 4898) {
    throw 'Batman display catalog requests must be the ordered range 4700..4898.'
}
if ($resolutionWriteRequests.Count -ne 98 -or $resolutionAcknowledgementRequests.Count -ne 98 -or $resolutionConfigValues.Count -ne 98) {
    throw 'Batman resolution protocol must contain exactly 98 generated mappings.'
}
for ($resolutionIndex = 0; $resolutionIndex -lt 98; $resolutionIndex++) {
    if ($resolutionWriteRequests[$resolutionIndex] -ne 5000 + $resolutionIndex -or
        $resolutionAcknowledgementRequests[$resolutionIndex] -ne 5100 + $resolutionIndex -or
        $resolutionConfigValues[$resolutionIndex] -ne $resolutionIndex) {
        throw "Batman resolution protocol mapping drifted at index $resolutionIndex."
    }
}

function New-GraphicsCarrierObserver {
    <#
    Build one declarative observer for a graphics state code carried by the shared
    retail structure. Geometry and checks are fixed for collision-free discovery,
    mappings remain observer-specific, and optional response, acknowledgement, failure,
    and command fields are emitted only when their complete declarations are supplied.
    #>
    param(
        [Parameter(Mandatory = $true)] [string]$Id,
        [AllowEmptyString()] [string]$TargetConfigKey = '',
        [Parameter(Mandatory = $true)] [AllowNull()] [object[]]$AddressMatchValues,
        [AllowNull()] [object[]]$Mappings = @(),
        [AllowNull()] [object]$ResponseRequestValue = $null,
        [AllowNull()] [object[]]$ResponseMappings = @(),
        [AllowNull()] [object[]]$AcknowledgementMappings = @(),
        [AllowNull()] [object]$FailureResponseValue = $null,
        [AllowEmptyString()] [string]$Command = '',
        [AllowEmptyString()] [string]$DynamicResponseProvider = '',
        [AllowNull()] [object[]]$DynamicResponseRequests = @(),
        [int]$DynamicResponseMinimumValue = 0,
        [int]$DynamicResponseMaximumValue = 0
    )

    $dynamicResponseSupplied = $PSBoundParameters.ContainsKey('DynamicResponseProvider')
    $responseRequestSupplied = $PSBoundParameters.ContainsKey('ResponseRequestValue')
    $responseMappingsSupplied = $PSBoundParameters.ContainsKey('ResponseMappings')
    $acknowledgementMappingsSupplied = $PSBoundParameters.ContainsKey('AcknowledgementMappings')
    $failureResponseSupplied = $PSBoundParameters.ContainsKey('FailureResponseValue')
    if ($dynamicResponseSupplied) {
        if ([string]::IsNullOrWhiteSpace($DynamicResponseProvider) -or $null -eq $DynamicResponseRequests -or $DynamicResponseRequests.Count -eq 0) {
            throw "Graphics carrier observer '$Id' dynamic response must declare a provider and requests."
        }
        if ($DynamicResponseMinimumValue -le 0 -or $DynamicResponseMaximumValue -lt $DynamicResponseMinimumValue) {
            throw "Graphics carrier observer '$Id' dynamic response bounds are invalid."
        }
        if ($responseRequestSupplied -or $responseMappingsSupplied -or $acknowledgementMappingsSupplied -or $Mappings.Count -gt 0 -or -not [string]::IsNullOrWhiteSpace($TargetConfigKey)) {
            throw "Graphics carrier observer '$Id' dynamic response cannot include static mappings or a target key."
        }
        $dynamicRequestValuesClone = @(ConvertTo-StrictGraphicsIntegerArray -Values $DynamicResponseRequests -Context "Graphics carrier observer '$Id' dynamic response requests")
        $dynamicRequestSet = [Collections.Generic.HashSet[int]]::new()
        foreach ($requestValue in $dynamicRequestValuesClone) {
            if (-not $dynamicRequestSet.Add([int]$requestValue)) { throw "Graphics carrier observer '$Id' dynamic response requests contain duplicate value $requestValue." }
        }
        if ($null -eq $FailureResponseValue -or -not $failureResponseSupplied) { throw "Graphics carrier observer '$Id' dynamic response requires a failure response." }
    }
    if ($responseRequestSupplied) {
        $responseRequest = ConvertTo-StrictGraphicsIntegralValue -Value $ResponseRequestValue -Context "Graphics carrier observer '$Id' response request"
    }
    if ($responseMappingsSupplied) {
        if ($null -eq $ResponseMappings -or $ResponseMappings.Count -eq 0) { throw "Graphics carrier observer '$Id' declares empty response mappings." }
        $responseMappingsClone = @(ConvertTo-StrictGraphicsMappingArray -Mappings $ResponseMappings -Context "Graphics carrier observer '$Id' response mappings")
    }
    if ($responseRequestSupplied -ne $responseMappingsSupplied) { throw "Graphics carrier observer '$Id' must declare a response request and response mappings together." }

    if ($failureResponseSupplied) {
        $failureResponse = ConvertTo-StrictGraphicsIntegralValue -Value $FailureResponseValue -Context "Graphics carrier observer '$Id' failure response"
    }
    if ($acknowledgementMappingsSupplied) {
        if ($null -eq $AcknowledgementMappings -or $AcknowledgementMappings.Count -eq 0) { throw "Graphics carrier observer '$Id' declares empty acknowledgement mappings." }
        $acknowledgementMappingsClone = @(ConvertTo-StrictGraphicsMappingArray -Mappings $AcknowledgementMappings -Context "Graphics carrier observer '$Id' acknowledgement mappings")
    }
    if (-not $dynamicResponseSupplied -and $acknowledgementMappingsSupplied -ne $failureResponseSupplied) { throw "Graphics carrier observer '$Id' must declare acknowledgement mappings and a failure response together." }

    if (-not $dynamicResponseSupplied -and ($null -eq $Mappings -or $Mappings.Count -eq 0)) { throw "Graphics carrier observer '$Id' must declare non-empty mappings." }
    $addressMatchValuesClone = @(ConvertTo-StrictGraphicsIntegerArray -Values $AddressMatchValues -Context "Graphics carrier observer '$Id' address match values")
    $mappingsClone = if ($dynamicResponseSupplied) { @() } else { @(ConvertTo-StrictGraphicsMappingArray -Mappings $Mappings -Context "Graphics carrier observer '$Id' mappings") }

    $observer = [ordered]@{
        id = $Id
        addressGroup = 'batmanFrontendControlType'
        scanStartAddress = '0x10000000'
        scanEndAddress = '0x30000000'
        scanStride = [int]4
        valueOffset = [int]12
        pollIntervalMs = [int]50
    }
    if (-not $dynamicResponseSupplied) {
        $observer.targetConfigKey = $TargetConfigKey
    }
    $observer.addressMatchValues = $addressMatchValuesClone
    $observer.checks = @(New-GraphicsCarrierChecks)
    if (-not $dynamicResponseSupplied) { $observer.mappings = $mappingsClone }
    if ($dynamicResponseSupplied) {
        $observer.dynamicResponse = [ordered]@{
            provider = $DynamicResponseProvider
            requests = $dynamicRequestValuesClone
            minimumValue = [int]$DynamicResponseMinimumValue
            maximumValue = [int]$DynamicResponseMaximumValue
        }
    }
    if ($responseRequestSupplied) {
        $observer.responseRequestValue = [int]$responseRequest
        $observer.responseMappings = $responseMappingsClone
    }
    if ($acknowledgementMappingsSupplied) {
        $observer.acknowledgementMappings = $acknowledgementMappingsClone
        $observer.failureResponseValue = [int]$failureResponse
    }
    elseif ($dynamicResponseSupplied) {
        $observer.failureResponseValue = [int]$failureResponse
    }
    if (-not [string]::IsNullOrWhiteSpace($Command)) { $observer.command = $Command }
    return $observer
}

function Invoke-RequiredProcess {
    param([string]$FilePath, [string[]]$Arguments, [string]$FailureMessage)
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) { throw $FailureMessage }
}

function Get-SafeFullPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    if ($Path.StartsWith('\\?\', [StringComparison]::Ordinal) -or $Path.StartsWith('\\.\', [StringComparison]::Ordinal) -or $Path.StartsWith('\??\', [StringComparison]::Ordinal)) {
        throw "Device namespace paths are not allowed for graphics-shell cleanup: $Path"
    }
    return [IO.Path]::GetFullPath($Path)
}

function Test-StrictDescendantPath {
    param([Parameter(Mandatory = $true)][string]$Path, [Parameter(Mandatory = $true)][string]$Root)
    $fullPath = Get-SafeFullPath $Path
    $fullRoot = (Get-SafeFullPath $Root).TrimEnd('\') + '\'
    return $fullPath.StartsWith($fullRoot, [StringComparison]::OrdinalIgnoreCase)
}

function Assert-SafeMutationTarget {
    <#
    Resolve and validate a path before cleanup or movement. Temporary paths must
    be strict descendants of the system temp directory; live paths are limited to
    the exact checked-in pack root or generated target. Existing reparse points
    and device namespaces are rejected so a typo cannot redirect recursive work.
    #>
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [string[]]$AllowedDescendantRoots = @(),
        [string[]]$AllowedExactPaths = @()
    )
    $fullPath = Get-SafeFullPath $Path
    $isAllowed = $false
    foreach ($root in $AllowedDescendantRoots) {
        if (Test-StrictDescendantPath -Path $fullPath -Root $root) { $isAllowed = $true; break }
    }
    if (-not $isAllowed) {
        foreach ($exact in $AllowedExactPaths) {
            if ([String]::Equals($fullPath, (Get-SafeFullPath $exact), [StringComparison]::OrdinalIgnoreCase)) { $isAllowed = $true; break }
        }
    }
    if (-not $isAllowed) { throw "Refusing unsafe graphics-shell mutation target: $fullPath" }

    $current = $fullPath
    while (-not [string]::IsNullOrWhiteSpace($current)) {
        if (Test-Path -LiteralPath $current) {
            $item = Get-Item -LiteralPath $current -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Reparse-point cleanup target is not allowed: $current" }
        }
        $parent = Split-Path -Parent $current
        if ([string]::IsNullOrWhiteSpace($parent) -or [String]::Equals($parent, $current, [StringComparison]::OrdinalIgnoreCase)) { break }
        $current = $parent
    }
    if (Test-Path -LiteralPath $fullPath -PathType Container) {
        foreach ($child in @(Get-ChildItem -LiteralPath $fullPath -Recurse -Force)) {
            if (($child.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Reparse-point descendant is not allowed: $($child.FullName)" }
        }
    }
    return $fullPath
}

function Remove-SafeMutationTarget {
    <# Remove one validated file or directory using LiteralPath only. #>
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [string[]]$AllowedDescendantRoots = @(),
        [string[]]$AllowedExactPaths = @()
    )
    $fullPath = Assert-SafeMutationTarget -Path $Path -AllowedDescendantRoots $AllowedDescendantRoots -AllowedExactPaths $AllowedExactPaths
    if (Test-Path -LiteralPath $fullPath) { Remove-Item -LiteralPath $fullPath -Recurse -Force }
}

function Move-SafeMutationTarget {
    <# Move one validated path using LiteralPath only; both source and destination are constrained. #>
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination,
        [string[]]$SourceDescendantRoots = @(),
        [string[]]$SourceExactPaths = @(),
        [string[]]$DestinationDescendantRoots = @(),
        [string[]]$DestinationExactPaths = @()
    )
    $sourcePath = Assert-SafeMutationTarget -Path $Source -AllowedDescendantRoots $SourceDescendantRoots -AllowedExactPaths $SourceExactPaths
    $destinationPath = Assert-SafeMutationTarget -Path $Destination -AllowedDescendantRoots $DestinationDescendantRoots -AllowedExactPaths $DestinationExactPaths
    Move-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
}

function Ensure-SafeDirectoryPath {
    <# Create a missing destination directory and return only the directory paths created by this invocation. #>
    param([Parameter(Mandatory = $true)][string]$Path)
    $fullPath = Assert-SafeMutationTarget -Path $Path -AllowedExactPaths @($Path)
    if (Test-Path -LiteralPath $fullPath -PathType Container) { return @() }
    if (Test-Path -LiteralPath $fullPath) { throw "Graphics-shell destination parent is not a directory: $fullPath" }

    $missing = [Collections.Generic.List[string]]::new()
    $cursor = $fullPath
    while (-not (Test-Path -LiteralPath $cursor)) {
        $missing.Insert(0, $cursor)
        $parent = Split-Path -Parent $cursor
        if ([string]::IsNullOrWhiteSpace($parent) -or [String]::Equals($parent, $cursor, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Graphics-shell destination parent has no existing ancestor: $fullPath"
        }
        $cursor = $parent
    }
    foreach ($directory in $missing) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
    return @($missing)
}

function Assert-GraphicsPublicationSameVolume {
    <# Require staging, recovery, and both live graphics outputs to share a volume before any publication move. #>
    param(
        [Parameter(Mandatory = $true)] [string]$TempRoot,
        [Parameter(Mandatory = $true)] [string]$BackupRoot,
        [Parameter(Mandatory = $true)] [string]$LivePackRoot,
        [Parameter(Mandatory = $true)] [string]$LiveTargetPath
    )

    $paths = @(
        [pscustomobject]@{ Label = 'staging'; Path = $TempRoot },
        [pscustomobject]@{ Label = 'activation backup'; Path = $BackupRoot },
        [pscustomobject]@{ Label = 'live pack'; Path = $LivePackRoot },
        [pscustomobject]@{ Label = 'stable target'; Path = $LiveTargetPath }
    )
    $expectedVolume = [IO.Path]::GetPathRoot((Get-SafeFullPath $TempRoot))
    foreach ($entry in $paths) {
        $actualVolume = [IO.Path]::GetPathRoot((Get-SafeFullPath $entry.Path))
        if (-not [String]::Equals($actualVolume, $expectedVolume, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Rollback-safe graphics publication requires all paths on one Windows volume; $($entry.Label) '$($entry.Path)' is on '$actualVolume' while staging is on '$expectedVolume'."
        }
    }
}

function Restore-AtomicRebuild {
    <# Restore both live artifacts from the activation backup, tolerating absent originals. #>
    param(
        [Parameter(Mandatory = $true)][string]$LivePackRoot,
        [Parameter(Mandatory = $true)][string]$LiveTargetPath,
        [Parameter(Mandatory = $true)][string]$PackBackupRoot,
        [Parameter(Mandatory = $true)][string]$TargetBackupPath,
        [Parameter(Mandatory = $true)][string]$ActivationBackupRoot,
        [bool]$PackWasBackedUp = $false,
        [bool]$TargetWasBackedUp = $false,
        [bool]$PackWasInstalled = $false,
        [bool]$TargetWasInstalled = $false
    )
    $packBackupExists = Test-Path -LiteralPath $PackBackupRoot
    $targetBackupExists = Test-Path -LiteralPath $TargetBackupPath
    if ($PackWasInstalled -or $PackWasBackedUp -or $packBackupExists) {
        if (Test-Path -LiteralPath $LivePackRoot) { Remove-SafeMutationTarget -Path $LivePackRoot -AllowedExactPaths @($LivePackRoot) }
    }
    if (($PackWasBackedUp -or $packBackupExists) -and $packBackupExists) {
        Move-SafeMutationTarget -Source $PackBackupRoot -Destination $LivePackRoot -SourceDescendantRoots @($ActivationBackupRoot) -DestinationExactPaths @($LivePackRoot)
    }
    if ($TargetWasInstalled -or $TargetWasBackedUp -or $targetBackupExists) {
        if (Test-Path -LiteralPath $LiveTargetPath) { Remove-SafeMutationTarget -Path $LiveTargetPath -AllowedExactPaths @($LiveTargetPath) }
    }
    if (($TargetWasBackedUp -or $targetBackupExists) -and $targetBackupExists) {
        Move-SafeMutationTarget -Source $TargetBackupPath -Destination $LiveTargetPath -SourceDescendantRoots @($ActivationBackupRoot) -DestinationExactPaths @($LiveTargetPath)
    }
}

function Invoke-AtomicGraphicsPublication {
    <#
    Publish a fully verified pack and target through a rollback-safe transaction.
    The caller supplies a verifier and optional injected transition failure so
    tests exercise real file moves, restoration, and post-commit cleanup. Backups
    live in a unique sibling of the staging root and are never removed in finally.
    #>
    param(
        [Parameter(Mandatory = $true)][string]$LivePackRoot,
        [Parameter(Mandatory = $true)][string]$LiveTargetPath,
        [Parameter(Mandatory = $true)][string]$StagedPackRoot,
        [Parameter(Mandatory = $true)][string]$StagedTargetPath,
        [Parameter(Mandatory = $true)][string]$BackupRoot,
        [Parameter(Mandatory = $true)][string]$TempRoot,
        [Parameter(Mandatory = $true)][scriptblock]$VerifyPublication,
        [ValidateSet('', 'AfterPackBackup', 'AfterTargetBackup', 'AfterPackActivation', 'AfterTargetActivation', 'AfterTargetVerification', 'AfterBackupDeletion')]
        [string]$FailureInjection = ''
    )
    $livePack = Get-SafeFullPath $LivePackRoot
    $liveTarget = Get-SafeFullPath $LiveTargetPath
    $tempFull = Get-SafeFullPath $TempRoot
    $backupFull = Get-SafeFullPath $BackupRoot
    Assert-GraphicsPublicationSameVolume -TempRoot $tempFull -BackupRoot $backupFull -LivePackRoot $livePack -LiveTargetPath $liveTarget
    $stagedPack = Assert-SafeMutationTarget -Path $StagedPackRoot -AllowedDescendantRoots @($TempRoot)
    $stagedTarget = Assert-SafeMutationTarget -Path $StagedTargetPath -AllowedDescendantRoots @($TempRoot)
    if ([String]::Equals($backupFull, $tempFull, [StringComparison]::OrdinalIgnoreCase) -or $backupFull.StartsWith($tempFull.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Rollback-safe publication backups must be outside the staging temp tree.'
    }
    if (Test-Path -LiteralPath $backupFull) { throw "Rollback-safe publication backup root already exists: $backupFull" }
    $packBackupRoot = Join-Path $backupFull 'pack\batman-aa-graphics-options'
    $targetBackupPath = Join-Path $backupFull 'target\Frontend-graphics-options.umap'
    $packBackedUp = $false
    $targetBackedUp = $false
    $packInstalled = $false
    $targetInstalled = $false
    $committed = $false
    $createdDestinationParents = [Collections.Generic.List[string]]::new()
    try {
        Assert-SafeMutationTarget -Path $backupFull -AllowedDescendantRoots @([IO.Path]::GetTempPath()) | Out-Null
        New-Item -ItemType Directory -Force -Path (Join-Path $backupFull 'pack'), (Join-Path $backupFull 'target') | Out-Null
        foreach ($destinationParent in @((Split-Path -Parent $livePack), (Split-Path -Parent $liveTarget))) {
            foreach ($createdParent in @(Ensure-SafeDirectoryPath -Path $destinationParent)) {
                if (-not $createdDestinationParents.Contains($createdParent)) { $createdDestinationParents.Add($createdParent) }
            }
        }
        if (-not (Test-Path -LiteralPath $stagedPack) -or -not (Test-Path -LiteralPath $stagedTarget)) { throw 'Rollback-safe publication staged inputs disappeared before activation.' }
        if (Test-Path -LiteralPath $livePack) {
            Move-SafeMutationTarget -Source $livePack -Destination $packBackupRoot -SourceExactPaths @($livePack) -DestinationDescendantRoots @($backupFull)
            $packBackedUp = $true
        }
        if ($FailureInjection -eq 'AfterPackBackup') { throw 'Injected rollback-safe publication failure after pack backup.' }
        if (Test-Path -LiteralPath $liveTarget) {
            Move-SafeMutationTarget -Source $liveTarget -Destination $targetBackupPath -SourceExactPaths @($liveTarget) -DestinationDescendantRoots @($backupFull)
            $targetBackedUp = $true
        }
        if ($FailureInjection -eq 'AfterTargetBackup') { throw 'Injected rollback-safe publication failure after target backup.' }
        Move-SafeMutationTarget -Source $stagedPack -Destination $livePack -SourceDescendantRoots @($TempRoot) -DestinationExactPaths @($livePack)
        $packInstalled = $true
        if ($FailureInjection -eq 'AfterPackActivation') { throw 'Injected rollback-safe publication failure after pack activation.' }
        Move-SafeMutationTarget -Source $stagedTarget -Destination $liveTarget -SourceDescendantRoots @($TempRoot) -DestinationExactPaths @($liveTarget)
        $targetInstalled = $true
        if ($FailureInjection -eq 'AfterTargetActivation') { throw 'Injected rollback-safe publication failure after target activation.' }
        & $VerifyPublication $livePack $liveTarget
        if (-not $?) { throw 'Rollback-safe publication live-output verification failed.' }
        if ($FailureInjection -eq 'AfterTargetVerification') { throw 'Injected rollback-safe publication failure after target verification.' }
        $committed = $true

        if (Test-Path -LiteralPath $packBackupRoot) { Remove-SafeMutationTarget -Path $packBackupRoot -AllowedDescendantRoots @($backupFull) }
        if ($FailureInjection -eq 'AfterBackupDeletion') { throw "Injected rollback-safe publication backup cleanup failure while deleting '$targetBackupPath'." }
        if (Test-Path -LiteralPath $targetBackupPath) { Remove-SafeMutationTarget -Path $targetBackupPath -AllowedDescendantRoots @($backupFull) }
        if (Test-Path -LiteralPath $backupFull) { Remove-SafeMutationTarget -Path $backupFull -AllowedDescendantRoots @([IO.Path]::GetTempPath()) }
    }
    catch {
        $failure = $_
        if ($committed) {
            throw "Rollback-safe graphics-shell publication committed, but backup cleanup failed: $($failure.Exception.Message). Recovery copies are retained at $backupFull."
        }
        try {
            Restore-AtomicRebuild -LivePackRoot $livePack -LiveTargetPath $liveTarget -PackBackupRoot $packBackupRoot -TargetBackupPath $targetBackupPath -ActivationBackupRoot $backupFull -PackWasBackedUp $packBackedUp -TargetWasBackedUp $targetBackedUp -PackWasInstalled $packInstalled -TargetWasInstalled $targetInstalled
            if (Test-Path -LiteralPath $backupFull) { Remove-SafeMutationTarget -Path $backupFull -AllowedDescendantRoots @([IO.Path]::GetTempPath()) }
            for ($index = $createdDestinationParents.Count - 1; $index -ge 0; $index--) {
                $createdParent = $createdDestinationParents[$index]
                if (Test-Path -LiteralPath $createdParent -PathType Container -and @(Get-ChildItem -LiteralPath $createdParent -Force).Count -eq 0) {
                    Remove-SafeMutationTarget -Path $createdParent -AllowedExactPaths @($createdParent)
                }
            }
        }
        catch {
            throw "Rollback-safe graphics-shell publication failed: $($failure.Exception.Message); rollback failed: $($_.Exception.Message)"
        }
        throw $failure
    }
}

if ($FunctionsOnly) { return }

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) { $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path } else { $BatmanRoot = (Resolve-Path $BatmanRoot).Path }
if ([string]::IsNullOrWhiteSpace($BuilderRoot)) { $BuilderRoot = Join-Path $BatmanRoot 'builder' } elseif ([IO.Path]::IsPathRooted($BuilderRoot)) { $BuilderRoot = [IO.Path]::GetFullPath($BuilderRoot) } elseif (Test-Path -LiteralPath $BuilderRoot) { $BuilderRoot = (Resolve-Path $BuilderRoot).Path } else { $BuilderRoot = [IO.Path]::GetFullPath((Join-Path $BatmanRoot $BuilderRoot)) }
if ([string]::IsNullOrWhiteSpace($BatmanUserIniPath)) {
    $documentsPath = [Environment]::GetFolderPath([Environment+SpecialFolder]::MyDocuments)
    $BatmanUserIniPath = Join-Path $documentsPath 'Square Enix\Batman Arkham Asylum GOTY\BmGame\Config\BmEngine.ini'
} else {
    $BatmanUserIniPath = [IO.Path]::GetFullPath($BatmanUserIniPath)
}
if (-not (Test-Path -LiteralPath $BatmanUserIniPath -PathType Leaf)) { throw "Batman user INI was not found as a file: $BatmanUserIniPath" }

$retailBasePath = Join-Path $BuilderRoot 'extracted\frontend-retail\Frontend.umap'
$expectedRetailSize = 2988548
$expectedRetailHash = '271916B888F83374122AF0FCCC5C685804F4C8286A92A772CD71E4F48A00F2CC'
if (-not (Test-Path -LiteralPath $retailBasePath)) { throw "Verified retail Frontend.umap was not found: $retailBasePath" }
$retailInfo = Get-Item -LiteralPath $retailBasePath
$retailHash = (Get-FileHash -LiteralPath $retailBasePath -Algorithm SHA256).Hash
if ($retailInfo.Length -ne $expectedRetailSize -or $retailHash -cne $expectedRetailHash) { throw "Refusing to rebuild against an unverified retail Frontend.umap. Expected $expectedRetailSize bytes/$expectedRetailHash, found $($retailInfo.Length) bytes/$retailHash." }

$builderProjectPath = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj'
$patcherProjectPath = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\BmGameGfxPatcher\BmGameGfxPatcher.csproj'
$ffdecPath = Join-Path $BuilderRoot 'extracted\ffdec\ffdec-cli.exe'
$buildHgdeltaPath = Join-Path $PSScriptRoot 'Build-Hgdelta.ps1'
$buildMatchPath = Join-Path $PSScriptRoot 'Get-BatmanSteamBuildMatch.ps1'
$packageVerifierPath = Join-Path $PSScriptRoot 'Test-BatmanGraphicsOptionsPackage.ps1'
foreach ($path in @($builderProjectPath, $patcherProjectPath, $ffdecPath, $buildHgdeltaPath, $buildMatchPath, $packageVerifierPath)) { if (-not (Test-Path -LiteralPath $path)) { throw "Graphics shell build input was not found: $path" } }

$stableExperimentRoot = Join-Path $BuilderRoot 'generated\graphics-options-experiment'
$packRoot = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-graphics-options'
$stableTargetPath = Join-Path $stableExperimentRoot 'Frontend-graphics-options.umap'

$systemTempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
$tempRoot = Join-Path $systemTempRoot ('HelenBatmanGraphicsShell-' + [Guid]::NewGuid().ToString('N'))
$prototypeOutputRoot = Join-Path $tempRoot 'prototype'
$prototypeGfxPath = Join-Path $prototypeOutputRoot 'MainV2-graphics-options.gfx'
$patchManifestPath = Join-Path $tempRoot 'MainV2-graphics-options.manifest.json'
$tempTargetPath = Join-Path $tempRoot 'Frontend-graphics-options.umap'
$stagedTargetPath = Join-Path $tempRoot 'target\Frontend-graphics-options.umap'
$stagedPackRoot = Join-Path $tempRoot 'pack\batman-aa-graphics-options'
$stagedPackBuildRoot = Join-Path $stagedPackRoot 'builds\steam-goty-1.0'
$stagedDeltaPath = Join-Path $stagedPackBuildRoot 'assets\deltas\Frontend-graphics-options.hgdelta'
$activationBackupRoot = Join-Path $systemTempRoot ('HelenBatmanGraphicsShellBackup-' + [Guid]::NewGuid().ToString('N'))
$stagedPackJsonPath = Join-Path $stagedPackRoot 'pack.json'
$stagedBuildJsonPath = Join-Path $stagedPackBuildRoot 'build.json'
$stagedBindingsJsonPath = Join-Path $stagedPackBuildRoot 'bindings.json'
$stagedCommandsJsonPath = Join-Path $stagedPackBuildRoot 'commands.json'
$stagedFilesJsonPath = Join-Path $stagedPackBuildRoot 'files.json'
$stagedHooksJsonPath = Join-Path $stagedPackBuildRoot 'hooks.json'
Assert-GraphicsPublicationSameVolume -TempRoot $tempRoot -BackupRoot $activationBackupRoot -LivePackRoot $packRoot -LiveTargetPath $stableTargetPath
$primaryFailure = $null
try {
    New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
    Assert-SafeMutationTarget -Path $tempRoot -AllowedDescendantRoots @($systemTempRoot) | Out-Null
    Invoke-RequiredProcess -FilePath 'dotnet' -Arguments @('build', $builderProjectPath, '-c', $Configuration, '--disable-build-servers', '-nr:false', '-p:UseSharedCompilation=false') -FailureMessage 'SubtitleSizeModBuilder build failed.'
    Invoke-RequiredProcess -FilePath 'dotnet' -Arguments @('build', $patcherProjectPath, '-c', $Configuration, '--disable-build-servers', '-nr:false', '-p:UseSharedCompilation=false') -FailureMessage 'BmGameGfxPatcher build failed.'
    Invoke-RequiredProcess -FilePath 'dotnet' -Arguments @('run', '--no-build', '--project', $builderProjectPath, '-c', $Configuration, '--', 'build-main-menu-graphics-shell', '--root', $BuilderRoot, '--output-dir', $prototypeOutputRoot, '--ffdec', $ffdecPath, '--ini', $BatmanUserIniPath) -FailureMessage 'build-main-menu-graphics-shell failed.'
    if (-not (Test-Path -LiteralPath $prototypeGfxPath)) { throw "Shell prototype GFX was not generated: $prototypeGfxPath" }

    $manifest = [ordered]@{
        name = 'MainMenu MainV2 graphics-options shell patch'
        patches = @([ordered]@{
            owner = 'MainMenu'
            exportName = 'MainV2'
            exportType = 'GFxMovieInfo'
            replacementPath = $prototypeGfxPath
            payloadMagic = 'GFX'
        })
    }
    Write-Utf8TextFile -Path $patchManifestPath -Contents ($manifest | ConvertTo-Json -Depth 5)
    Invoke-RequiredProcess -FilePath 'dotnet' -Arguments @('run', '--no-build', '--project', $patcherProjectPath, '-c', $Configuration, '--', 'patch', '--package', $retailBasePath, '--manifest', $patchManifestPath, '--output', $tempTargetPath) -FailureMessage 'Patching the verified retail Frontend.umap failed.'
    if (-not (Test-Path -LiteralPath $tempTargetPath)) { throw "Current-run shell target was not generated: $tempTargetPath" }

    $targetStorage = Get-UnrealPackageStorageInfo -Path $tempTargetPath
    if ($targetStorage.CompressionChunkCount -le 0) { throw 'Current-run shell target is not chunk-compressed.' }
    New-Item -ItemType Directory -Force -Path $stagedPackBuildRoot | Out-Null
    $deltaInfo = & $buildHgdeltaPath -BaseFile $retailBasePath -TargetFile $tempTargetPath -OutputFile $stagedDeltaPath -ChunkSize 65536
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $stagedDeltaPath)) { throw 'Building the graphics shell hgdelta failed.' }
    # Keep the verified current-run target beside the staged pack; this is the only target that may become stable.
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $stagedTargetPath) | Out-Null
    Copy-Item -LiteralPath $tempTargetPath -Destination $stagedTargetPath -Force

    $buildMatch = & $buildMatchPath
    if ($buildMatch.BuildId -cne 'steam-goty-1.0' -or $buildMatch.Executable -cne 'ShippingPC-BmGame.exe' -or [int64]$buildMatch.FileSize -ne 38758728 -or $buildMatch.Sha256 -cne '4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028') { throw 'Steam build match is not the verified retail executable identity.' }
    $files = [ordered]@{
        virtualFiles = @([ordered]@{
            id = 'frontendGraphicsOptionsPackage'
            path = 'BmGame/CookedPC/Maps/Frontend/Frontend.umap'
            mode = 'delta-on-read'
            source = [ordered]@{
                kind = 'delta-file'
                path = 'assets/deltas/Frontend-graphics-options.hgdelta'
                base = [ordered]@{ size = $deltaInfo.BaseSize; sha256 = $deltaInfo.BaseSha256 }
                target = [ordered]@{ size = $deltaInfo.TargetSize; sha256 = $deltaInfo.TargetSha256 }
                chunkSize = $deltaInfo.ChunkSize
            }
        })
    }
    $graphicsAddressMatchValues = [Collections.Generic.List[int]]::new()
    foreach ($protocol in $graphicsProtocol) {
        foreach ($addressValue in @($protocol.Read) + @($protocol.Responses) + @($protocol.Writes) + @($protocol.Acks) + @($protocol.Failure)) {
            $graphicsAddressMatchValues.Add([int]$addressValue)
        }
    }
    foreach ($addressValue in $dynamicCatalogRequests + @($dynamicCatalogFailure) + $resolutionWriteRequests + $resolutionAcknowledgementRequests + @($resolutionFailure)) {
        $graphicsAddressMatchValues.Add([int]$addressValue)
    }
    foreach ($addressValue in $graphicsCommandValues) {
        $graphicsAddressMatchValues.Add([int]$addressValue)
    }
    $graphicsAddressMatchSet = [Collections.Generic.HashSet[int]]::new()
    foreach ($addressValue in $graphicsAddressMatchValues) {
        if (-not $graphicsAddressMatchSet.Add([int]$addressValue)) {
            throw "Batman graphics protocol contains duplicate positive carrier value $addressValue."
        }
    }
    $graphicsAddressMatchValues = @($graphicsAddressMatchSet | Sort-Object | ForEach-Object { [int]$_ })
    $configKeys = @(
        'fullscreen',
        'resolutionWidth',
        'resolutionHeight',
        'resolutionModeIndex',
        'vsync',
        'msaa',
        'detailLevel',
        'bloom',
        'dynamicShadows',
        'motionBlur',
        'distortion',
        'fogVolumes',
        'sphericalHarmonicLighting',
        'ambientOcclusion',
        'physx',
        'stereo',
        'applySignal',
        'rollbackSignal'
    )
    $config = @(
        foreach ($configKey in $configKeys) {
            [ordered]@{
                key = $configKey
                type = 'int'
                defaultValue = if ($configKey -eq 'resolutionModeIndex') { [int]-1 } else { [int]0 }
            }
        }
    )
    $loadGraphicsDraftCommand = [ordered]@{
        id = 'loadBatmanGraphicsDraftIntoConfig'
        name = 'Load Batman Graphics Draft Into Config'
        steps = @(
            [ordered]@{
                kind = 'load-batman-graphics-draft-into-config'
            }
        )
    }
    $applyGraphicsDraftCommand = [ordered]@{
        id = 'applyBatmanGraphicsDraft'
        name = 'Apply Batman Graphics Draft'
        steps = @(
            [ordered]@{
                kind = 'apply-batman-graphics-config'
            },
            [ordered]@{
                kind = 'load-batman-graphics-draft-into-config'
            }
        )
    }
    $syncGraphicsDetailLevelCommand = [ordered]@{
        id = 'syncBatmanGraphicsDetailLevel'
        name = 'Sync Batman Graphics Detail Level'
        steps = @(
            [ordered]@{
                kind = 'sync-batman-graphics-detail-level'
            }
        )
    }
    $setGraphicsResolutionModeCommand = [ordered]@{
        id = 'setBatmanGraphicsResolutionMode'
        name = 'Set Batman Graphics Resolution Mode'
        steps = @(
            [ordered]@{
                kind = 'set-batman-graphics-resolution-mode'
            }
        )
    }
    $commands = [ordered]@{
        commands = @($loadGraphicsDraftCommand, $syncGraphicsDetailLevelCommand, $setGraphicsResolutionModeCommand, $applyGraphicsDraftCommand)
    }
    $finiteSettingObservers = @(
        foreach ($protocol in $graphicsProtocol) {
            $settingMappings = @(
                for ($mappingIndex = 0; $mappingIndex -lt @($protocol.Writes).Count; $mappingIndex++) {
                    [ordered]@{ match = [int]$protocol.Writes[$mappingIndex]; value = [int]$protocol.ConfigValues[$mappingIndex] }
                }
            )
            $settingResponseMappings = @(
                for ($mappingIndex = 0; $mappingIndex -lt @($protocol.ConfigValues).Count; $mappingIndex++) {
                    [ordered]@{ match = [int]$protocol.ConfigValues[$mappingIndex]; value = [int]$protocol.Responses[$mappingIndex] }
                }
            )
            $settingAcknowledgementMappings = @(
                for ($mappingIndex = 0; $mappingIndex -lt @($protocol.Writes).Count; $mappingIndex++) {
                    [ordered]@{ match = [int]$protocol.Writes[$mappingIndex]; value = [int]$protocol.Acks[$mappingIndex] }
                }
            )
            New-GraphicsCarrierObserver -Id $protocol.Id -TargetConfigKey $protocol.Key -AddressMatchValues $graphicsAddressMatchValues -Mappings $settingMappings -ResponseRequestValue ([int]$protocol.Read) -ResponseMappings $settingResponseMappings -AcknowledgementMappings $settingAcknowledgementMappings -FailureResponseValue ([int]$protocol.Failure) -Command $protocol.Command
        }
    )
    $dynamicCatalogObserver = New-GraphicsCarrierObserver -Id 'graphicsObserverDisplayModeCatalog' -AddressMatchValues $graphicsAddressMatchValues -DynamicResponseProvider 'batmanDisplayModes' -DynamicResponseRequests $dynamicCatalogRequests -DynamicResponseMinimumValue 1 -DynamicResponseMaximumValue 32767 -FailureResponseValue $dynamicCatalogFailure
    $resolutionMappings = @(
        for ($mappingIndex = 0; $mappingIndex -lt 98; $mappingIndex++) {
            [ordered]@{ match = [int]$resolutionWriteRequests[$mappingIndex]; value = [int]$resolutionConfigValues[$mappingIndex] }
        }
    )
    $resolutionAcknowledgementMappings = @(
        for ($mappingIndex = 0; $mappingIndex -lt 98; $mappingIndex++) {
            [ordered]@{ match = [int]$resolutionWriteRequests[$mappingIndex]; value = [int]$resolutionAcknowledgementRequests[$mappingIndex] }
        }
    )
    $resolutionObserver = New-GraphicsCarrierObserver -Id 'graphicsObserverResolutionModeIndex' -TargetConfigKey 'resolutionModeIndex' -AddressMatchValues $graphicsAddressMatchValues -Mappings $resolutionMappings -AcknowledgementMappings $resolutionAcknowledgementMappings -FailureResponseValue $resolutionFailure -Command 'setBatmanGraphicsResolutionMode'
    $applyAcknowledgementMappings = @(
        [ordered]@{ match = [int]4990; value = [int]4980 },
        [ordered]@{ match = [int]4991; value = [int]4981 }
    )
    $rollbackAcknowledgementMappings = @(
        [ordered]@{ match = [int]4970; value = [int]4960 },
        [ordered]@{ match = [int]4971; value = [int]4961 }
    )
    $applyObserver = New-GraphicsCarrierObserver -Id 'graphicsObserverApplySignal' -TargetConfigKey 'applySignal' -AddressMatchValues $graphicsAddressMatchValues -Mappings @(
        [ordered]@{ match = [int]4990; value = [int]0 },
        [ordered]@{ match = [int]4991; value = [int]1 }
    ) -AcknowledgementMappings $applyAcknowledgementMappings -FailureResponseValue ([int]4989) -Command 'applyBatmanGraphicsDraft'
    $rollbackObserver = New-GraphicsCarrierObserver -Id 'graphicsObserverRollbackSignal' -TargetConfigKey 'rollbackSignal' -AddressMatchValues $graphicsAddressMatchValues -Mappings @(
        [ordered]@{ match = [int]4970; value = [int]0 },
        [ordered]@{ match = [int]4971; value = [int]1 }
    ) -AcknowledgementMappings $rollbackAcknowledgementMappings -FailureResponseValue ([int]4969) -Command 'loadBatmanGraphicsDraftIntoConfig'
    $hooks = [ordered]@{
        runtimeSlots = @()
        stateObservers = @($finiteSettingObservers[0], $dynamicCatalogObserver, $resolutionObserver) + @($finiteSettingObservers[1..$($finiteSettingObservers.Count - 1)]) + @($applyObserver, $rollbackObserver)
        hooks = @()
    }
    $pack = [ordered]@{
        schemaVersion = 1
        id = 'batman-aa-graphics-options'
        name = 'Batman Graphics Options'
        targets = @([ordered]@{ gameId = 'batman-arkham-asylum'; executables = @($buildMatch.Executable) })
        config = $config
        builds = @($buildMatch.BuildId)
    }
    $build = [ordered]@{
        id = $buildMatch.BuildId
        executable = $buildMatch.Executable
        match = [ordered]@{ fileSize = $buildMatch.FileSize; sha256 = $buildMatch.Sha256 }
        startupCommands = @('loadBatmanGraphicsDraftIntoConfig')
    }
    Write-Utf8TextFile -Path $stagedFilesJsonPath -Contents ($files | ConvertTo-Json -Depth 7)
    Write-Utf8TextFile -Path $stagedPackJsonPath -Contents ($pack | ConvertTo-Json -Depth 5)
    Write-Utf8TextFile -Path $stagedBuildJsonPath -Contents ($build | ConvertTo-Json -Depth 5)
    Write-Utf8TextFile -Path $stagedBindingsJsonPath -Contents (([ordered]@{ bindings = @() }) | ConvertTo-Json -Depth 3)
    Write-Utf8TextFile -Path $stagedCommandsJsonPath -Contents ($commands | ConvertTo-Json -Depth 6)
    Write-Utf8TextFile -Path $stagedHooksJsonPath -Contents ($hooks | ConvertTo-Json -Depth 8)

    # The verifier reopens/export-checks the staged target and reconstructs its delta before activation.
    & $packageVerifierPath -BatmanRoot $BatmanRoot -BuilderRoot $BuilderRoot -Configuration $Configuration -PackRootOverride $stagedPackRoot -TargetPathOverride $stagedTargetPath -StagedPackageValidation
    if (-not $?) { throw 'Staged graphics-options pack verification failed.' }

    $verifyPublication = {
        param($LivePackRootForVerification, $LiveTargetPathForVerification)
        & $packageVerifierPath -BatmanRoot $BatmanRoot -BuilderRoot $BuilderRoot -Configuration $Configuration
        if (-not $?) { throw 'Post-activation graphics-options pack verification failed.' }
    }
    Invoke-AtomicGraphicsPublication -LivePackRoot $packRoot -LiveTargetPath $stableTargetPath -StagedPackRoot $stagedPackRoot -StagedTargetPath $stagedTargetPath -BackupRoot $activationBackupRoot -TempRoot $tempRoot -VerifyPublication $verifyPublication
}
catch {
    $primaryFailure = $_
    throw $primaryFailure
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        try {
            Remove-SafeMutationTarget -Path $tempRoot -AllowedDescendantRoots @($systemTempRoot)
        }
        catch {
            if ($null -ne $primaryFailure) {
                Write-Warning "Graphics-shell staging cleanup failed after the primary failure '$($primaryFailure.Exception.Message)': $($_.Exception.Message)"
            } else {
                throw "Graphics-shell staging cleanup failed: $($_.Exception.Message)"
            }
        }
    }
}

$finalInfo = Get-Item -LiteralPath $stableTargetPath
$finalHash = (Get-FileHash -LiteralPath $stableTargetPath -Algorithm SHA256).Hash
$finalDeltaPath = Join-Path $packRoot 'builds\steam-goty-1.0\assets\deltas\Frontend-graphics-options.hgdelta'
Write-Output 'Rebuilt Batman graphics-options shell outputs with rollback-safe publication:'
Write-Output "  Retail base:     $retailBasePath ($($retailInfo.Length) bytes, $retailHash)"
Write-Output "  Frontend target: $stableTargetPath ($($finalInfo.Length) bytes, $finalHash)"
Write-Output "  Frontend delta:  $finalDeltaPath ($((Get-Item -LiteralPath $finalDeltaPath).Length) bytes, $((Get-FileHash -LiteralPath $finalDeltaPath -Algorithm SHA256).Hash))"
