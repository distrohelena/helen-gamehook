param(
    [string]$BatmanRoot
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}

$PackRoot = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-subtitles'
$PackJsonPath = Join-Path $PackRoot 'pack.json'
$RebuildScriptPath = Join-Path $BatmanRoot 'scripts\Rebuild-BatmanPack.ps1'
$DeployScriptPath = Join-Path $BatmanRoot 'scripts\Deploy-Batman.ps1'
$ProgramSourcePath = Join-Path $BatmanRoot 'builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\Program.cs'
$AssetBuilderSourcePath = Join-Path $BatmanRoot 'builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeAssetBuilder.cs'

if (-not (Test-Path -LiteralPath $PackJsonPath)) {
    throw "Batman subtitle pack manifest not found: $PackJsonPath"
}

if (-not (Test-Path -LiteralPath $RebuildScriptPath)) {
    throw "Batman subtitle pack rebuild script not found: $RebuildScriptPath"
}

if (-not (Test-Path -LiteralPath $DeployScriptPath)) {
    throw "Batman subtitle pack deploy script not found: $DeployScriptPath"
}

if (-not (Test-Path -LiteralPath $ProgramSourcePath)) {
    throw "Batman subtitle pack builder program source not found: $ProgramSourcePath"
}

if (-not (Test-Path -LiteralPath $AssetBuilderSourcePath)) {
    throw "Batman subtitle pack asset builder source not found: $AssetBuilderSourcePath"
}

$PackJson = Get-Content -LiteralPath $PackJsonPath -Raw | ConvertFrom-Json
$RebuildScriptText = Get-Content -LiteralPath $RebuildScriptPath -Raw
$DeployScriptText = Get-Content -LiteralPath $DeployScriptPath -Raw
$ProgramSourceText = Get-Content -LiteralPath $ProgramSourcePath -Raw
$AssetBuilderSourceText = Get-Content -LiteralPath $AssetBuilderSourcePath -Raw

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

function Assert-ExpectedIniFile {
    param(
        [Parameter(Mandatory = $true)]
        $ActualEntry,
        [Parameter(Mandatory = $true)]
        $ExpectedEntry
    )

    foreach ($Key in @('id', 'root', 'path')) {
        if ($ActualEntry.$Key -ne $ExpectedEntry.$Key) {
            throw "INI file metadata mismatch for key '$Key'. Expected '$($ExpectedEntry.$Key)' but found '$($ActualEntry.$Key)'."
        }
    }
}

function Assert-ExpectedIniStore {
    param(
        [Parameter(Mandatory = $true)]
        $ActualEntry,
        [Parameter(Mandatory = $true)]
        $ExpectedEntry
    )

    if ($ActualEntry.id -ne $ExpectedEntry.id) {
        throw "INI store id mismatch. Expected '$($ExpectedEntry.id)' but found '$($ActualEntry.id)'."
    }

    if (@($ActualEntry.files).Count -ne @($ExpectedEntry.files).Count -or @($ActualEntry.keys).Count -ne @($ExpectedEntry.keys).Count) {
        throw "INI store collection sizes do not match for '$($ExpectedEntry.id)'."
    }

    if ((@($ActualEntry.files) -join ',') -ne (@($ExpectedEntry.files) -join ',')) {
        throw "INI store files mismatch for '$($ExpectedEntry.id)'."
    }

    if ((@($ActualEntry.keys) -join ',') -ne (@($ExpectedEntry.keys) -join ',')) {
        throw "INI store keys mismatch for '$($ExpectedEntry.id)'."
    }
}

function Assert-ExpectedIniKey {
    param(
        [Parameter(Mandatory = $true)]
        $ActualEntry,
        [Parameter(Mandatory = $true)]
        $ExpectedEntry
    )

    foreach ($Key in @('id', 'file', 'section', 'key', 'type', 'ownership')) {
        if ($ActualEntry.$Key -ne $ExpectedEntry.$Key) {
            throw "INI key metadata mismatch for '$($ExpectedEntry.id)' field '$Key'. Expected '$($ExpectedEntry.$Key)' but found '$($ActualEntry.$Key)'."
        }
    }

    if ($ActualEntry.writable -ne $ExpectedEntry.writable) {
        throw "INI key writable flag mismatch for '$($ExpectedEntry.id)'."
    }

    $ActualValueMap = @($ActualEntry.valueMap)
    $ExpectedValueMap = @($ExpectedEntry.valueMap)
    if ($ActualValueMap.Count -ne $ExpectedValueMap.Count) {
        throw "INI key value map length mismatch for '$($ExpectedEntry.id)'."
    }

    for ($Index = 0; $Index -lt $ExpectedValueMap.Count; $Index++) {
        if ($ActualValueMap[$Index].match -ne $ExpectedValueMap[$Index].match) {
            throw "INI key value map match mismatch at index $Index for '$($ExpectedEntry.id)'."
        }

        if ($ActualValueMap[$Index].encodedValue -ne $ExpectedValueMap[$Index].encodedValue) {
            throw "INI key value map encoded value mismatch at index $Index for '$($ExpectedEntry.id)'."
        }
    }
}

$ActualIniFiles = @($PackJson.iniFiles)
$ActualIniStores = @($PackJson.iniStores)
$ActualIniKeys = @($PackJson.iniKeys)

if ($ActualIniFiles.Count -ne $ExpectedIniFiles.Count) {
    throw "Batman subtitle pack expected exactly $($ExpectedIniFiles.Count) iniFiles entry, found $($ActualIniFiles.Count)."
}

if ($ActualIniStores.Count -ne $ExpectedIniStores.Count) {
    throw "Batman subtitle pack expected exactly $($ExpectedIniStores.Count) iniStores entry, found $($ActualIniStores.Count)."
}

if ($ActualIniKeys.Count -ne $ExpectedIniKeys.Count) {
    throw "Batman subtitle pack expected exactly $($ExpectedIniKeys.Count) iniKeys entry, found $($ActualIniKeys.Count)."
}

Assert-ExpectedIniFile -ActualEntry $ActualIniFiles[0] -ExpectedEntry $ExpectedIniFiles[0]
Assert-ExpectedIniStore -ActualEntry $ActualIniStores[0] -ExpectedEntry $ExpectedIniStores[0]
Assert-ExpectedIniKey -ActualEntry $ActualIniKeys[0] -ExpectedEntry $ExpectedIniKeys[0]

if ($RebuildScriptText.Contains('$FrontendPackBuildVersion')) {
    throw 'Batman subtitle pack rebuild still defines a frontend build version label.'
}

if ($RebuildScriptText.Contains('--build-version $FrontendPackBuildVersion')) {
    throw 'Batman subtitle pack rebuild still forwards a frontend build version label into build-main-menu-audio.'
}

if ($RebuildScriptText.Contains('build-main-menu-audio')) {
    throw 'Batman subtitle pack rebuild still invokes the frontend main-menu audio build.'
}

if ($RebuildScriptText.Contains("id = 'frontendMapPackage'") -or $RebuildScriptText.Contains('BmGame/CookedPC/Maps/Frontend/Frontend.umap') -or $RebuildScriptText.Contains('generated\main-menu-audio\Frontend-main-menu-subtitle-size.umap')) {
    throw 'Batman subtitle pack rebuild still includes frontend package patching.'
}

if ($DeployScriptText.Contains("Id = 'frontendMapPackage'") -or $DeployScriptText.Contains('SourceFrontendDeltaPath') -or $DeployScriptText.Contains('BmGame/CookedPC/Maps/Frontend/Frontend.umap')) {
    throw 'Batman subtitle pack deploy script still expects a frontend subtitle payload.'
}

if (-not $ProgramSourceText.Contains('string buildVersion = options.GetValue("--build-version")?.Trim() ?? string.Empty;')) {
    throw 'Batman subtitle pack audio build does not treat the frontend build version as an explicit opt-in value.'
}

if (-not $AssetBuilderSourceText.Contains('if (!string.IsNullOrWhiteSpace(buildVersion))')) {
    throw 'Batman subtitle pack asset builder does not guard the frontend version-label patch behind an explicit build-version check.'
}

Write-Output 'PASS'
