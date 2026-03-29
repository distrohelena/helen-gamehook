param(
    [string]$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'

$PackBuildRoot = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-subtitles\builds\steam-goty-1.0'
$FilesJsonPath = Join-Path $PackBuildRoot 'files.json'
$GameplayDeltaPath = Join-Path $PackBuildRoot 'assets\deltas\BmGame-subtitle-signal.hgdelta'

$ExpectedVirtualFileId = 'bmgameGameplayPackage'
$ExpectedVirtualFilePath = 'BmGame/CookedPC/BmGame.u'
$ExpectedVirtualFileMode = 'delta-on-read'
$ExpectedVirtualFileKind = 'delta-file'
$ExpectedDeltaPath = 'assets/deltas/BmGame-subtitle-signal.hgdelta'
$ExpectedBaseSize = 100365345
$ExpectedBaseSha256 = '621a5c8d99c9f7c7283531d05a4a6d56bdf15ad93ede0d5bf2f5d3e45117ff36'
$ExpectedTargetSize = 101117004
$ExpectedTargetSha256 = 'b241625d604de97f7b7fda53ad6df2346992d8ef2b9be66fd45d395839080329'
$ExpectedChunkSize = 65536
$ExpectedGameplayDeltaHash = '2A4988D7BC655C8779C0B3718F60226119AFEC386AB56C22F14CBFC2454FC3C1'

if (-not (Test-Path $FilesJsonPath)) {
    throw "Batman gameplay package manifest not found: $FilesJsonPath"
}

if (-not (Test-Path $GameplayDeltaPath)) {
    throw "Batman gameplay delta not found: $GameplayDeltaPath"
}

$FilesManifest = Get-Content -LiteralPath $FilesJsonPath -Raw | ConvertFrom-Json
$VirtualFiles = @($FilesManifest.virtualFiles)
if ($VirtualFiles.Count -ne 1) {
    throw "Batman gameplay package manifest expected exactly one virtual file, found $($VirtualFiles.Count)."
}

$GameplayFile = $VirtualFiles[0]
if ($GameplayFile.id -ne $ExpectedVirtualFileId) {
    throw "Batman gameplay package virtual file id mismatch. Expected $ExpectedVirtualFileId but found $($GameplayFile.id)."
}

if ($GameplayFile.path -ne $ExpectedVirtualFilePath) {
    throw "Batman gameplay package virtual file path mismatch. Expected $ExpectedVirtualFilePath but found $($GameplayFile.path)."
}

if ($GameplayFile.mode -ne $ExpectedVirtualFileMode) {
    throw "Batman gameplay package virtual file mode mismatch. Expected $ExpectedVirtualFileMode but found $($GameplayFile.mode)."
}

if ($GameplayFile.source.kind -ne $ExpectedVirtualFileKind) {
    throw "Batman gameplay package source kind mismatch. Expected $ExpectedVirtualFileKind but found $($GameplayFile.source.kind)."
}

if ($GameplayFile.source.path -ne $ExpectedDeltaPath) {
    throw "Batman gameplay package delta path mismatch. Expected $ExpectedDeltaPath but found $($GameplayFile.source.path)."
}

if ([int64]$GameplayFile.source.base.size -ne $ExpectedBaseSize) {
    throw "Batman gameplay package base size mismatch. Expected $ExpectedBaseSize but found $($GameplayFile.source.base.size)."
}

if (-not [string]::Equals($GameplayFile.source.base.sha256, $ExpectedBaseSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Batman gameplay package base hash mismatch. Expected $ExpectedBaseSha256 but found $($GameplayFile.source.base.sha256)."
}

if ([int64]$GameplayFile.source.target.size -ne $ExpectedTargetSize) {
    throw "Batman gameplay package target size mismatch. Expected $ExpectedTargetSize but found $($GameplayFile.source.target.size)."
}

if (-not [string]::Equals($GameplayFile.source.target.sha256, $ExpectedTargetSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Batman gameplay package target hash mismatch. Expected $ExpectedTargetSha256 but found $($GameplayFile.source.target.sha256)."
}

if ([int64]$GameplayFile.source.chunkSize -ne $ExpectedChunkSize) {
    throw "Batman gameplay package chunk size mismatch. Expected $ExpectedChunkSize but found $($GameplayFile.source.chunkSize)."
}

$ActualGameplayDeltaHash = (Get-FileHash -LiteralPath $GameplayDeltaPath -Algorithm SHA256).Hash
if (-not [string]::Equals($ActualGameplayDeltaHash, $ExpectedGameplayDeltaHash, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Batman gameplay delta hash mismatch. Expected $ExpectedGameplayDeltaHash but found $ActualGameplayDeltaHash."
}

Write-Output 'PASS'
