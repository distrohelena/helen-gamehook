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

function Assert-HgdeltaVirtualFileContract {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Context,
        [Parameter(Mandatory = $true)]
        [psobject]$VirtualFile,
        [Parameter(Mandatory = $true)]
        [string]$ExpectedId,
        [Parameter(Mandatory = $true)]
        [string]$ExpectedPath,
        [Parameter(Mandatory = $true)]
        [string]$ExpectedMode,
        [Parameter(Mandatory = $true)]
        [string]$ExpectedKind,
        [Parameter(Mandatory = $true)]
        [string]$ExpectedDeltaRelativePath,
        [Parameter(Mandatory = $true)]
        [string]$BasePath,
        [Parameter(Mandatory = $true)]
        [string]$TargetPath,
        [Parameter(Mandatory = $true)]
        [string]$DeltaFilePath,
        [uint32]$ChunkSize = 65536,
        [uint64]$ChunkTableOffset = 116
    )

    foreach ($RequiredPath in @($BasePath, $TargetPath, $DeltaFilePath)) {
        if (-not (Test-Path -LiteralPath $RequiredPath)) {
            throw "$Context input not found: $RequiredPath"
        }
    }

    if ($VirtualFile.id -ne $ExpectedId) {
        throw "$Context virtual file id mismatch. Expected $ExpectedId but found $($VirtualFile.id)."
    }

    if ($VirtualFile.path -ne $ExpectedPath) {
        throw "$Context virtual file path mismatch. Expected $ExpectedPath but found $($VirtualFile.path)."
    }

    if ($VirtualFile.mode -ne $ExpectedMode) {
        throw "$Context virtual file mode mismatch. Expected $ExpectedMode but found $($VirtualFile.mode)."
    }

    if ($VirtualFile.source.kind -ne $ExpectedKind) {
        throw "$Context source kind mismatch. Expected $ExpectedKind but found $($VirtualFile.source.kind)."
    }

    if ($VirtualFile.source.path -ne $ExpectedDeltaRelativePath) {
        throw "$Context delta path mismatch. Expected $ExpectedDeltaRelativePath but found $($VirtualFile.source.path)."
    }

    $expectedBaseSize = (Get-Item -LiteralPath $BasePath).Length
    $expectedBaseSha256 = (Get-FileHash -LiteralPath $BasePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $expectedTargetSize = (Get-Item -LiteralPath $TargetPath).Length
    $expectedTargetSha256 = (Get-FileHash -LiteralPath $TargetPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $expectedChunkCount = [uint32][Math]::Ceiling($expectedTargetSize / [double]$ChunkSize)
    $expectedPayloadOffset = [uint64]($ChunkTableOffset + ($expectedChunkCount * 20))

    if ([int64]$VirtualFile.source.base.size -ne $expectedBaseSize) {
        throw "$Context base size mismatch. Expected $expectedBaseSize but found $($VirtualFile.source.base.size)."
    }

    if (-not [string]::Equals($VirtualFile.source.base.sha256, $expectedBaseSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Context base hash mismatch. Expected $expectedBaseSha256 but found $($VirtualFile.source.base.sha256)."
    }

    if ([int64]$VirtualFile.source.target.size -ne $expectedTargetSize) {
        throw "$Context target size mismatch. Expected $expectedTargetSize but found $($VirtualFile.source.target.size)."
    }

    if (-not [string]::Equals($VirtualFile.source.target.sha256, $expectedTargetSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Context target hash mismatch. Expected $expectedTargetSha256 but found $($VirtualFile.source.target.sha256)."
    }

    if ([int64]$VirtualFile.source.chunkSize -ne $ChunkSize) {
        throw "$Context chunk size mismatch. Expected $ChunkSize but found $($VirtualFile.source.chunkSize)."
    }

    $deltaHeader = Read-HgdeltaHeader -Path $DeltaFilePath

    if ($deltaHeader.Magic -ne 'HGDL') {
        throw "$Context delta magic mismatch. Expected HGDL but found $($deltaHeader.Magic)."
    }

    if ($deltaHeader.MajorVersion -ne 1 -or $deltaHeader.MinorVersion -ne 0) {
        throw "$Context delta version mismatch. Expected 1.0 but found $($deltaHeader.MajorVersion).$($deltaHeader.MinorVersion)."
    }

    if ([uint32]$deltaHeader.ChunkSize -ne $ChunkSize) {
        throw "$Context delta header chunk size mismatch. Expected $ChunkSize but found $($deltaHeader.ChunkSize)."
    }

    if ([uint64]$deltaHeader.BaseSize -ne [uint64]$expectedBaseSize) {
        throw "$Context delta header base size mismatch. Expected $expectedBaseSize but found $($deltaHeader.BaseSize)."
    }

    if (-not [string]::Equals($deltaHeader.BaseSha256, $expectedBaseSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Context delta header base hash mismatch. Expected $expectedBaseSha256 but found $($deltaHeader.BaseSha256)."
    }

    if ([uint64]$deltaHeader.TargetSize -ne [uint64]$expectedTargetSize) {
        throw "$Context delta header target size mismatch. Expected $expectedTargetSize but found $($deltaHeader.TargetSize)."
    }

    if (-not [string]::Equals($deltaHeader.TargetSha256, $expectedTargetSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Context delta header target hash mismatch. Expected $expectedTargetSha256 but found $($deltaHeader.TargetSha256)."
    }

    if ([uint32]$deltaHeader.ChunkCount -ne $expectedChunkCount) {
        throw "$Context delta header chunk count mismatch. Expected $expectedChunkCount but found $($deltaHeader.ChunkCount)."
    }

    if ([uint64]$deltaHeader.ChunkTableOffset -ne $ChunkTableOffset) {
        throw "$Context delta header chunk table offset mismatch. Expected $ChunkTableOffset but found $($deltaHeader.ChunkTableOffset)."
    }

    if ([uint64]$deltaHeader.PayloadOffset -ne $expectedPayloadOffset) {
        throw "$Context delta header payload offset mismatch. Expected $expectedPayloadOffset but found $($deltaHeader.PayloadOffset)."
    }

    if ([uint64]$deltaHeader.PayloadOffset -gt [uint64]$deltaHeader.FileSize) {
        throw "$Context delta payload offset exceeds file length."
    }

    if ([int64]$VirtualFile.source.base.size -ne [int64]$deltaHeader.BaseSize) {
        throw "$Context manifest/header base size mismatch."
    }

    if (-not [string]::Equals($VirtualFile.source.base.sha256, $deltaHeader.BaseSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Context manifest/header base hash mismatch."
    }

    if ([int64]$VirtualFile.source.target.size -ne [int64]$deltaHeader.TargetSize) {
        throw "$Context manifest/header target size mismatch."
    }

    if (-not [string]::Equals($VirtualFile.source.target.sha256, $deltaHeader.TargetSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Context manifest/header target hash mismatch."
    }

    if ([int64]$VirtualFile.source.chunkSize -ne [int64]$deltaHeader.ChunkSize) {
        throw "$Context manifest/header chunk size mismatch."
    }
}
