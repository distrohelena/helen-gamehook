param(
    [Parameter(Mandatory = $true)][string]$BaseFile,
    [Parameter(Mandatory = $true)][string]$TargetFile,
    [Parameter(Mandatory = $true)][string]$OutputFile,
    [int]$ChunkSize = 65536
)

$ErrorActionPreference = 'Stop'

function Get-LowerSha256 {
    param(
        [Parameter(Mandatory = $true)][string]$Path
    )

    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    try {
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        try {
            $hashBytes = $sha256.ComputeHash($stream)
        }
        finally {
            $sha256.Dispose()
        }

        return ([System.BitConverter]::ToString($hashBytes)).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $stream.Dispose()
    }
}

function Convert-HexToBytes {
    param(
        [Parameter(Mandatory = $true)][string]$Hex
    )

    if ($Hex.Length -ne 64) {
        throw "Expected a 64-character SHA-256 digest."
    }

    $bytes = New-Object byte[] 32
    for ($index = 0; $index -lt 32; $index++) {
        $bytes[$index] = [Convert]::ToByte($Hex.Substring($index * 2, 2), 16)
    }

    return ,$bytes
}

function Write-UInt32LittleEndian {
    param(
        [Parameter(Mandatory = $true)][System.IO.BinaryWriter]$Writer,
        [Parameter(Mandatory = $true)][UInt32]$Value
    )

    $Writer.Write([System.BitConverter]::GetBytes($Value))
}

function Write-UInt64LittleEndian {
    param(
        [Parameter(Mandatory = $true)][System.IO.BinaryWriter]$Writer,
        [Parameter(Mandatory = $true)][UInt64]$Value
    )

    $Writer.Write([System.BitConverter]::GetBytes($Value))
}

function Write-Sha256Digest {
    param(
        [Parameter(Mandatory = $true)][System.IO.BinaryWriter]$Writer,
        [Parameter(Mandatory = $true)][string]$Digest
    )

    [byte[]]$digestBytes = Convert-HexToBytes -Hex $Digest
    if ($digestBytes.Length -ne 32) {
        throw "Expected a 32-byte SHA-256 digest."
    }

    $Writer.Write($digestBytes)
}

function Read-Chunk {
    param(
        [Parameter(Mandatory = $true)][System.IO.FileStream]$Stream,
        [Parameter(Mandatory = $true)][byte[]]$Buffer,
        [Parameter(Mandatory = $true)][UInt64]$Offset,
        [Parameter(Mandatory = $true)][int]$Length
    )

    if ($Length -eq 0) {
        return 0
    }

    [void]$Stream.Seek([Int64]$Offset, [System.IO.SeekOrigin]::Begin)

    $totalRead = 0
    while ($totalRead -lt $Length) {
        $read = $Stream.Read($Buffer, $totalRead, $Length - $totalRead)
        if ($read -le 0) {
            throw "Failed to read the requested chunk bytes."
        }

        $totalRead += $read
    }

    return $totalRead
}

function Test-ChunkEquality {
    param(
        [Parameter(Mandatory = $true)][byte[]]$BaseBuffer,
        [Parameter(Mandatory = $true)][byte[]]$TargetBuffer,
        [Parameter(Mandatory = $true)][int]$Length
    )

    for ($index = 0; $index -lt $Length; $index++) {
        if ($BaseBuffer[$index] -ne $TargetBuffer[$index]) {
            return $false
        }
    }

    return $true
}

if ($ChunkSize -le 0) {
    throw "ChunkSize must be a positive integer."
}

$basePath = (Resolve-Path $BaseFile).Path
$targetPath = (Resolve-Path $TargetFile).Path
$outputPath = [System.IO.Path]::GetFullPath($OutputFile)
$outputDirectory = Split-Path -Parent $outputPath
if ([string]::IsNullOrWhiteSpace($outputDirectory)) {
    throw "OutputFile must include a writable directory."
}

[System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null

$baseInfo = Get-Item -LiteralPath $basePath
$targetInfo = Get-Item -LiteralPath $targetPath
$baseHash = Get-LowerSha256 -Path $basePath
$targetHash = Get-LowerSha256 -Path $targetPath
$chunkCount = [int][Math]::Ceiling($targetInfo.Length / [double]$ChunkSize)
$chunkEntries = New-Object 'System.Collections.Generic.List[object]'
$payloadPath = $outputPath + '.payload.tmp'

if (Test-Path $payloadPath) {
    Remove-Item -LiteralPath $payloadPath -Force
}

$baseStream = [System.IO.File]::Open($basePath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
$targetStream = [System.IO.File]::Open($targetPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
$payloadStream = [System.IO.File]::Open($payloadPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)

try {
    $baseBuffer = New-Object byte[] $ChunkSize
    $targetBuffer = New-Object byte[] $ChunkSize

    for ($chunkIndex = 0; $chunkIndex -lt $chunkCount; $chunkIndex++) {
        $offset = [uint64]($chunkIndex * $ChunkSize)
        $targetLength = [int][Math]::Min([double]$ChunkSize, [double]($targetInfo.Length - [int64]$offset))
        $baseLength = 0

        [void](Read-Chunk -Stream $targetStream -Buffer $targetBuffer -Offset $offset -Length $targetLength)
        if ($offset -lt [uint64]$baseInfo.Length) {
            $baseLength = [int][Math]::Min([double]$ChunkSize, [double]($baseInfo.Length - [int64]$offset))
            [void](Read-Chunk -Stream $baseStream -Buffer $baseBuffer -Offset $offset -Length $baseLength)
        }

        $sameAsBase = $targetLength -eq $baseLength
        if ($sameAsBase) {
            $sameAsBase = Test-ChunkEquality -BaseBuffer $baseBuffer -TargetBuffer $targetBuffer -Length $targetLength
        }

        if ($sameAsBase) {
            $chunkEntries.Add([pscustomobject]@{
                Kind = [uint32]0
                TargetSize = [uint32]$targetLength
                PayloadOffset = [uint64]0
                PayloadSize = [uint32]0
            }) | Out-Null
        }
        else {
            $payloadOffset = [uint64]$payloadStream.Position
            $payloadStream.Write($targetBuffer, 0, $targetLength)
            $chunkEntries.Add([pscustomobject]@{
                Kind = [uint32]1
                TargetSize = [uint32]$targetLength
                PayloadOffset = $payloadOffset
                PayloadSize = [uint32]$targetLength
            }) | Out-Null
        }
    }
}
finally {
    $baseStream.Dispose()
    $targetStream.Dispose()
}

try {
    $chunkTableOffset = [uint64]116
    $payloadOffset = $chunkTableOffset + ([uint64]$chunkEntries.Count * [uint64]20)

    $outputStream = [System.IO.File]::Open($outputPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    try {
        $writer = New-Object System.IO.BinaryWriter($outputStream)
        try {
            $writer.Write([System.Text.Encoding]::ASCII.GetBytes('HGDL'))
            Write-UInt32LittleEndian -Writer $writer -Value ([uint32]1)
            Write-UInt32LittleEndian -Writer $writer -Value ([uint32]0)
            Write-UInt32LittleEndian -Writer $writer -Value ([uint32]$ChunkSize)
            Write-UInt64LittleEndian -Writer $writer -Value ([uint64]$baseInfo.Length)
            Write-UInt64LittleEndian -Writer $writer -Value ([uint64]$targetInfo.Length)
            Write-Sha256Digest -Writer $writer -Digest $baseHash
            Write-Sha256Digest -Writer $writer -Digest $targetHash
            Write-UInt32LittleEndian -Writer $writer -Value ([uint32]$chunkEntries.Count)
            Write-UInt64LittleEndian -Writer $writer -Value $chunkTableOffset
            Write-UInt64LittleEndian -Writer $writer -Value $payloadOffset

            foreach ($chunkEntry in $chunkEntries) {
                Write-UInt32LittleEndian -Writer $writer -Value $chunkEntry.Kind
                Write-UInt32LittleEndian -Writer $writer -Value $chunkEntry.TargetSize
                Write-UInt64LittleEndian -Writer $writer -Value $chunkEntry.PayloadOffset
                Write-UInt32LittleEndian -Writer $writer -Value $chunkEntry.PayloadSize
            }

            $payloadStream.Position = 0
            $payloadStream.CopyTo($outputStream)
            $writer.Flush()
        }
        finally {
            $writer.Dispose()
        }
    }
    finally {
        $outputStream.Dispose()
    }
}
finally {
    $payloadStream.Dispose()
    if (Test-Path $payloadPath) {
        Remove-Item -LiteralPath $payloadPath -Force
    }
}

[pscustomobject]@{
    BaseSize = [uint64]$baseInfo.Length
    BaseSha256 = $baseHash
    TargetSize = [uint64]$targetInfo.Length
    TargetSha256 = $targetHash
    ChunkSize = [uint32]$ChunkSize
    ChunkCount = [uint32]$chunkEntries.Count
    OutputFile = $outputPath
}
