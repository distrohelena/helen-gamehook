param(
    [string]$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$BuilderRoot
)

$ErrorActionPreference = 'Stop'

function Convert-BytesToLowerHex {
    param(
        [Parameter(Mandatory = $true)]
        [byte[]]$Bytes
    )

    return ([System.BitConverter]::ToString($Bytes)).Replace('-', '').ToLowerInvariant()
}

function Read-HgdeltaHeader {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    try {
        $reader = New-Object System.IO.BinaryReader($stream)
        try {
            $magic = [System.Text.Encoding]::ASCII.GetString($reader.ReadBytes(4))
            $majorVersion = $reader.ReadUInt32()
            $minorVersion = $reader.ReadUInt32()
            $chunkSize = $reader.ReadUInt32()
            $baseSize = $reader.ReadUInt64()
            $targetSize = $reader.ReadUInt64()
            $baseSha256 = Convert-BytesToLowerHex -Bytes ($reader.ReadBytes(32))
            $targetSha256 = Convert-BytesToLowerHex -Bytes ($reader.ReadBytes(32))
            $chunkCount = $reader.ReadUInt32()
            $chunkTableOffset = $reader.ReadUInt64()
            $payloadOffset = $reader.ReadUInt64()

            return [pscustomobject]@{
                Magic = $magic
                MajorVersion = $majorVersion
                MinorVersion = $minorVersion
                ChunkSize = $chunkSize
                BaseSize = $baseSize
                TargetSize = $targetSize
                BaseSha256 = $baseSha256
                TargetSha256 = $targetSha256
                ChunkCount = $chunkCount
                ChunkTableOffset = $chunkTableOffset
                PayloadOffset = $payloadOffset
                FileSize = [uint64]$stream.Length
            }
        }
        finally {
            $reader.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
}

$BuilderRoot = if ([string]::IsNullOrWhiteSpace($BuilderRoot)) {
    Join-Path $BatmanRoot 'builder'
} elseif ([System.IO.Path]::IsPathRooted($BuilderRoot)) {
    $BuilderRoot
} else {
    Join-Path $BatmanRoot $BuilderRoot
}
$BuilderRoot = [System.IO.Path]::GetFullPath($BuilderRoot)

$PackRoot = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-subtitles'
$PackBuildRoot = Join-Path $PackRoot 'builds\steam-goty-1.0'
$PackJsonPath = Join-Path $PackRoot 'pack.json'
$FilesJsonPath = Join-Path $PackBuildRoot 'files.json'
$GameplayDeltaPath = Join-Path $PackBuildRoot 'assets\deltas\BmGame-subtitle-signal.hgdelta'
$ExpectedVirtualFiles = @(
    @{
        Id = 'bmgameGameplayPackage'
        Path = 'BmGame/CookedPC/BmGame.u'
        Mode = 'delta-on-read'
        Kind = 'delta-file'
        DeltaPath = 'assets/deltas/BmGame-subtitle-signal.hgdelta'
        DeltaFilePath = $GameplayDeltaPath
        DeltaFileHash = '74b4d535883f4b0dccfd93c87df476f2e11516b708dd5130f74bf59c595c2107'
        BaseSize = 59857525
        BaseSha256 = '4306148e7627ec2c0de4144fd6ab45521b3b7e090d1028a0b685cadafafb89e6'
        TargetSize = 106217949
        TargetSha256 = '3e83e7001b9bc806c20fdff77865f0af1aedad6079032d7b43952a93db8db7bd'
        ChunkSize = 65536
        ChunkTableOffset = 116
    }
)

if (-not (Test-Path $FilesJsonPath)) {
    throw "Batman gameplay package manifest not found: $FilesJsonPath"
}

if (-not (Test-Path -LiteralPath $PackJsonPath)) {
    throw "Batman subtitle pack manifest not found: $PackJsonPath"
}

foreach ($ExpectedVirtualFile in $ExpectedVirtualFiles) {
    foreach ($RequiredPath in @($ExpectedVirtualFile.DeltaFilePath)) {
        if (-not (Test-Path -LiteralPath $RequiredPath)) {
            throw "Batman pack verification input not found: $RequiredPath"
        }
    }
}

$FilesManifest = Get-Content -LiteralPath $FilesJsonPath -Raw | ConvertFrom-Json
$PackManifest = Get-Content -LiteralPath $PackJsonPath -Raw | ConvertFrom-Json
$VirtualFiles = @($FilesManifest.virtualFiles)
if ($VirtualFiles.Count -ne $ExpectedVirtualFiles.Count) {
    throw "Batman gameplay package manifest expected exactly $($ExpectedVirtualFiles.Count) virtual files, found $($VirtualFiles.Count)."
}

foreach ($ExpectedVirtualFile in $ExpectedVirtualFiles) {
    $VirtualFile = @($VirtualFiles | Where-Object { $_.id -eq $ExpectedVirtualFile.Id })[0]
    if ($null -eq $VirtualFile) {
        throw "Batman gameplay package manifest is missing virtual file id $($ExpectedVirtualFile.Id)."
    }

    if ($VirtualFile.path -ne $ExpectedVirtualFile.Path) {
        throw "Batman gameplay package virtual file path mismatch for $($ExpectedVirtualFile.Id). Expected $($ExpectedVirtualFile.Path) but found $($VirtualFile.path)."
    }

    if ($VirtualFile.mode -ne $ExpectedVirtualFile.Mode) {
        throw "Batman gameplay package virtual file mode mismatch for $($ExpectedVirtualFile.Id). Expected $($ExpectedVirtualFile.Mode) but found $($VirtualFile.mode)."
    }

    if ($VirtualFile.source.kind -ne $ExpectedVirtualFile.Kind) {
        throw "Batman gameplay package source kind mismatch for $($ExpectedVirtualFile.Id). Expected $($ExpectedVirtualFile.Kind) but found $($VirtualFile.source.kind)."
    }

    if ($VirtualFile.source.path -ne $ExpectedVirtualFile.DeltaPath) {
        throw "Batman gameplay package delta path mismatch for $($ExpectedVirtualFile.Id). Expected $($ExpectedVirtualFile.DeltaPath) but found $($VirtualFile.source.path)."
    }

    $ExpectedBaseSize = $ExpectedVirtualFile.BaseSize
    $ExpectedBaseSha256 = $ExpectedVirtualFile.BaseSha256
    $ExpectedTargetSize = $ExpectedVirtualFile.TargetSize
    $ExpectedTargetSha256 = $ExpectedVirtualFile.TargetSha256
    $ExpectedChunkCount = [uint32][Math]::Ceiling($ExpectedTargetSize / [double]$ExpectedVirtualFile.ChunkSize)
    $ExpectedPayloadOffset = [uint64]($ExpectedVirtualFile.ChunkTableOffset + ($ExpectedChunkCount * 20))

    if ([int64]$VirtualFile.source.base.size -ne $ExpectedBaseSize) {
        throw "Batman gameplay package base size mismatch for $($ExpectedVirtualFile.Id). Expected $ExpectedBaseSize but found $($VirtualFile.source.base.size)."
    }

    if (-not [string]::Equals($VirtualFile.source.base.sha256, $ExpectedBaseSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Batman gameplay package base hash mismatch for $($ExpectedVirtualFile.Id). Expected $ExpectedBaseSha256 but found $($VirtualFile.source.base.sha256)."
    }

    if ([int64]$VirtualFile.source.target.size -ne $ExpectedTargetSize) {
        throw "Batman gameplay package target size mismatch for $($ExpectedVirtualFile.Id). Expected $ExpectedTargetSize but found $($VirtualFile.source.target.size)."
    }

    if (-not [string]::Equals($VirtualFile.source.target.sha256, $ExpectedTargetSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Batman gameplay package target hash mismatch for $($ExpectedVirtualFile.Id). Expected $ExpectedTargetSha256 but found $($VirtualFile.source.target.sha256)."
    }

    if ([int64]$VirtualFile.source.chunkSize -ne $ExpectedVirtualFile.ChunkSize) {
        throw "Batman gameplay package chunk size mismatch for $($ExpectedVirtualFile.Id). Expected $($ExpectedVirtualFile.ChunkSize) but found $($VirtualFile.source.chunkSize)."
    }

    $ActualDeltaHash = (Get-FileHash -LiteralPath $ExpectedVirtualFile.DeltaFilePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if (-not [string]::Equals($ActualDeltaHash, $ExpectedVirtualFile.DeltaFileHash, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Batman gameplay package delta file hash mismatch for $($ExpectedVirtualFile.Id). Expected $($ExpectedVirtualFile.DeltaFileHash) but found $ActualDeltaHash."
    }

    $DeltaHeader = Read-HgdeltaHeader -Path $ExpectedVirtualFile.DeltaFilePath
    if ($DeltaHeader.Magic -ne 'HGDL') {
        throw "Batman gameplay package delta magic mismatch for $($ExpectedVirtualFile.Id). Expected HGDL but found $($DeltaHeader.Magic)."
    }

    if ($DeltaHeader.MajorVersion -ne 1 -or $DeltaHeader.MinorVersion -ne 0) {
        throw "Batman gameplay package delta version mismatch for $($ExpectedVirtualFile.Id). Expected 1.0 but found $($DeltaHeader.MajorVersion).$($DeltaHeader.MinorVersion)."
    }

    if ([uint32]$DeltaHeader.ChunkSize -ne [uint32]$ExpectedVirtualFile.ChunkSize) {
        throw "Batman gameplay package delta header chunk size mismatch for $($ExpectedVirtualFile.Id). Expected $($ExpectedVirtualFile.ChunkSize) but found $($DeltaHeader.ChunkSize)."
    }

    if ([uint64]$DeltaHeader.BaseSize -ne [uint64]$ExpectedBaseSize) {
        throw "Batman gameplay package delta header base size mismatch for $($ExpectedVirtualFile.Id). Expected $ExpectedBaseSize but found $($DeltaHeader.BaseSize)."
    }

    if (-not [string]::Equals($DeltaHeader.BaseSha256, $ExpectedBaseSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Batman gameplay package delta header base hash mismatch for $($ExpectedVirtualFile.Id). Expected $ExpectedBaseSha256 but found $($DeltaHeader.BaseSha256)."
    }

    if ([uint64]$DeltaHeader.TargetSize -ne [uint64]$ExpectedTargetSize) {
        throw "Batman gameplay package delta header target size mismatch for $($ExpectedVirtualFile.Id). Expected $ExpectedTargetSize but found $($DeltaHeader.TargetSize)."
    }

    if (-not [string]::Equals($DeltaHeader.TargetSha256, $ExpectedTargetSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Batman gameplay package delta header target hash mismatch for $($ExpectedVirtualFile.Id). Expected $ExpectedTargetSha256 but found $($DeltaHeader.TargetSha256)."
    }

    if ([uint32]$DeltaHeader.ChunkCount -ne $ExpectedChunkCount) {
        throw "Batman gameplay package delta header chunk count mismatch for $($ExpectedVirtualFile.Id). Expected $ExpectedChunkCount but found $($DeltaHeader.ChunkCount)."
    }

    if ([uint64]$DeltaHeader.ChunkTableOffset -ne [uint64]$ExpectedVirtualFile.ChunkTableOffset) {
        throw "Batman gameplay package delta header chunk table offset mismatch for $($ExpectedVirtualFile.Id). Expected $($ExpectedVirtualFile.ChunkTableOffset) but found $($DeltaHeader.ChunkTableOffset)."
    }

    if ([uint64]$DeltaHeader.PayloadOffset -ne $ExpectedPayloadOffset) {
        throw "Batman gameplay package delta header payload offset mismatch for $($ExpectedVirtualFile.Id). Expected $ExpectedPayloadOffset but found $($DeltaHeader.PayloadOffset)."
    }

    if ([uint64]$DeltaHeader.PayloadOffset -gt [uint64]$DeltaHeader.FileSize) {
        throw "Batman gameplay package delta header payload offset exceeds file length for $($ExpectedVirtualFile.Id)."
    }

    if ([int64]$VirtualFile.source.base.size -ne [int64]$DeltaHeader.BaseSize) {
        throw "Batman gameplay package manifest/header base size mismatch for $($ExpectedVirtualFile.Id)."
    }

    if (-not [string]::Equals($VirtualFile.source.base.sha256, $DeltaHeader.BaseSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Batman gameplay package manifest/header base hash mismatch for $($ExpectedVirtualFile.Id)."
    }

    if ([int64]$VirtualFile.source.target.size -ne [int64]$DeltaHeader.TargetSize) {
        throw "Batman gameplay package manifest/header target size mismatch for $($ExpectedVirtualFile.Id)."
    }

    if (-not [string]::Equals($VirtualFile.source.target.sha256, $DeltaHeader.TargetSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Batman gameplay package manifest/header target hash mismatch for $($ExpectedVirtualFile.Id)."
    }

    if ([int64]$VirtualFile.source.chunkSize -ne [int64]$DeltaHeader.ChunkSize) {
        throw "Batman gameplay package manifest/header chunk size mismatch for $($ExpectedVirtualFile.Id)."
    }
}

$ExpectedIniFiles = @(
    @{
        id = 'user-engine'
        root = 'documents'
        path = 'Square Enix/Batman Arkham Asylum GOTY/BmGame/Config/BmEngine.ini'
    }
)

$ExpectedIniStores = @(
    @{
        id = 'batmanFrontendUi'
        files = @('user-engine')
        keys = @('subtitleSize')
    }
)

$ExpectedIniKeys = @(
    @{
        id = 'subtitleSize'
        file = 'user-engine'
        section = 'Engine.HUD'
        key = 'ConsoleFontSize'
        type = 'choice-map'
        writable = $true
        ownership = 'hijacked-native-key'
        valueMap = @(
            @{ match = 0; encodedValue = 5 },
            @{ match = 1; encodedValue = 6 },
            @{ match = 2; encodedValue = 7 }
        )
    }
)

$CommandsJsonPath = Join-Path $PackBuildRoot 'commands.json'
$BindingsJsonPath = Join-Path $PackBuildRoot 'bindings.json'
$HooksJsonPath = Join-Path $PackBuildRoot 'hooks.json'
$ExpectedSubtitleScaleMappings = @(
    @{ match = 0; value = 1.0 },
    @{ match = 1; value = 1.5 },
    @{ match = 2; value = 2.0 }
)
$ExpectedBindings = @(
    @{
        ExternalName = 'Helen_GetInt'
        Mode = 'get-int'
        ConfigKey = 'ui.subtitleSize'
        Command = $null
    },
    @{
        ExternalName = 'Helen_SetInt'
        Mode = 'set-int'
        ConfigKey = 'ui.subtitleSize'
        Command = $null
    },
    @{
        ExternalName = 'Helen_RunCommand'
        Mode = 'run-command'
        ConfigKey = $null
        Command = 'applySubtitleSize'
    }
)

if (@($PackManifest.iniFiles).Count -ne $ExpectedIniFiles.Count) {
    throw "Batman subtitle pack expected exactly $($ExpectedIniFiles.Count) iniFiles entry, found $(@($PackManifest.iniFiles).Count)."
}

if (@($PackManifest.iniStores).Count -ne $ExpectedIniStores.Count) {
    throw "Batman subtitle pack expected exactly $($ExpectedIniStores.Count) iniStores entry, found $(@($PackManifest.iniStores).Count)."
}

if (@($PackManifest.iniKeys).Count -ne $ExpectedIniKeys.Count) {
    throw "Batman subtitle pack expected exactly $($ExpectedIniKeys.Count) iniKeys entry, found $(@($PackManifest.iniKeys).Count)."
}

$ActualIniFile = @($PackManifest.iniFiles)[0]
if ($ActualIniFile.id -ne $ExpectedIniFiles[0].id -or
    $ActualIniFile.root -ne $ExpectedIniFiles[0].root -or
    $ActualIniFile.path -ne $ExpectedIniFiles[0].path) {
    throw 'Batman subtitle pack iniFiles declaration did not match the expected batmanFrontendUi source file.'
}

$ActualIniStore = @($PackManifest.iniStores)[0]
if ($ActualIniStore.id -ne $ExpectedIniStores[0].id -or
    (@($ActualIniStore.files) -join ',') -ne (@($ExpectedIniStores[0].files) -join ',') -or
    (@($ActualIniStore.keys) -join ',') -ne (@($ExpectedIniStores[0].keys) -join ',')) {
    throw 'Batman subtitle pack iniStores declaration did not match the expected batmanFrontendUi store.'
}

$ActualIniKey = @($PackManifest.iniKeys)[0]
if ($ActualIniKey.id -ne $ExpectedIniKeys[0].id -or
    $ActualIniKey.file -ne $ExpectedIniKeys[0].file -or
    $ActualIniKey.section -ne $ExpectedIniKeys[0].section -or
    $ActualIniKey.key -ne $ExpectedIniKeys[0].key -or
    $ActualIniKey.type -ne $ExpectedIniKeys[0].type -or
    $ActualIniKey.writable -ne $ExpectedIniKeys[0].writable -or
    $ActualIniKey.ownership -ne $ExpectedIniKeys[0].ownership) {
    throw 'Batman subtitle pack iniKeys declaration did not match the expected hijacked native key.'
}

$ActualIniValueMap = @($ActualIniKey.valueMap)
if ($ActualIniValueMap.Count -ne $ExpectedIniKeys[0].valueMap.Count) {
    throw 'Batman subtitle pack iniKeys value map count did not match the expected choice-map.'
}

for ($Index = 0; $Index -lt $ExpectedIniKeys[0].valueMap.Count; $Index++) {
    if ($ActualIniValueMap[$Index].match -ne $ExpectedIniKeys[0].valueMap[$Index].match -or
        $ActualIniValueMap[$Index].encodedValue -ne $ExpectedIniKeys[0].valueMap[$Index].encodedValue) {
        throw "Batman subtitle pack iniKeys value map mismatch at index $Index."
    }
}

if (-not (Test-Path -LiteralPath $CommandsJsonPath)) {
    throw "Batman gameplay command manifest not found: $CommandsJsonPath"
}

if (-not (Test-Path -LiteralPath $BindingsJsonPath)) {
    throw "Batman gameplay bindings manifest not found: $BindingsJsonPath"
}

if (-not (Test-Path -LiteralPath $HooksJsonPath)) {
    throw "Batman gameplay hooks manifest not found: $HooksJsonPath"
}

$CommandsManifest = Get-Content -LiteralPath $CommandsJsonPath -Raw | ConvertFrom-Json
$BindingsManifest = Get-Content -LiteralPath $BindingsJsonPath -Raw | ConvertFrom-Json
$HooksManifest = Get-Content -LiteralPath $HooksJsonPath -Raw | ConvertFrom-Json
$ApplySubtitleCommand = @($CommandsManifest.commands | Where-Object { $_.id -eq 'applySubtitleSize' })[0]
if ($null -eq $ApplySubtitleCommand) {
    throw "Batman gameplay command manifest did not declare applySubtitleSize."
}

$MapIntToDoubleStep = @($ApplySubtitleCommand.steps | Where-Object { $_.kind -eq 'map-int-to-double' })[0]
if ($null -eq $MapIntToDoubleStep) {
    throw "Batman gameplay applySubtitleSize command did not declare a map-int-to-double step."
}

$ActualMappings = @($MapIntToDoubleStep.mappings)
if ($ActualMappings.Count -ne $ExpectedSubtitleScaleMappings.Count) {
    throw "Batman gameplay subtitle scale mapping count mismatch. Expected $($ExpectedSubtitleScaleMappings.Count) but found $($ActualMappings.Count)."
}

for ($Index = 0; $Index -lt $ExpectedSubtitleScaleMappings.Count; $Index++) {
    if ($ActualMappings[$Index].match -ne $ExpectedSubtitleScaleMappings[$Index].match) {
        throw "Batman gameplay subtitle scale mapping match mismatch at index $Index."
    }

    if ([double]$ActualMappings[$Index].value -ne [double]$ExpectedSubtitleScaleMappings[$Index].value) {
        throw "Batman gameplay subtitle scale mapping value mismatch at index $Index."
    }
}

$ActualBindings = @($BindingsManifest.bindings)
if ($ActualBindings.Count -ne $ExpectedBindings.Count) {
    throw "Batman gameplay bindings count mismatch. Expected $($ExpectedBindings.Count) but found $($ActualBindings.Count)."
}

foreach ($ExpectedBinding in $ExpectedBindings) {
    $MatchingBinding = @(
        $ActualBindings |
        Where-Object {
            $_.externalName -eq $ExpectedBinding.ExternalName -and
            $_.mode -eq $ExpectedBinding.Mode -and
            (
                $ExpectedBinding.ConfigKey -eq $null -or
                $_.configKey -eq $ExpectedBinding.ConfigKey
            ) -and
            (
                $ExpectedBinding.Command -eq $null -or
                $_.command -eq $ExpectedBinding.Command
            )
        }
    )

    if ($MatchingBinding.Count -ne 1) {
        throw "Batman gameplay bindings manifest is missing the expected $($ExpectedBinding.ExternalName) / $($ExpectedBinding.Mode) subtitle binding."
    }
}

$SubtitleObserver = @($HooksManifest.stateObservers | Where-Object { $_.id -eq 'subtitleUiStateObserver' })[0]
if ($null -ne $SubtitleObserver) {
    throw 'Batman gameplay hooks manifest must not depend on subtitleUiStateObserver for live subtitle changes.'
}

if (@($HooksManifest.stateObservers).Count -ne 0) {
    throw 'Batman gameplay subtitle pack should not ship stateObservers after the direct signal-hook migration.'
}

$SubtitleScaleHook = @($HooksManifest.hooks | Where-Object { $_.id -eq 'subtitleTextScaleHook' })[0]
if ($null -ne $SubtitleScaleHook) {
    throw 'Batman gameplay hooks manifest must not ship subtitleTextScaleHook after the direct signal-hook migration.'
}

if (@($HooksManifest.hooks).Count -ne 0) {
    throw 'Batman gameplay subtitle pack should not ship runtime inline hooks after the direct signal-hook migration.'
}

Write-Output 'PASS'
