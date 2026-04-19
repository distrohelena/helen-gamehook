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

if (-not (Test-Path -LiteralPath $PackJsonPath)) {
    throw "Batman subtitle pack manifest not found: $PackJsonPath"
}

$PackJson = Get-Content -LiteralPath $PackJsonPath -Raw | ConvertFrom-Json

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

Write-Output 'PASS'
