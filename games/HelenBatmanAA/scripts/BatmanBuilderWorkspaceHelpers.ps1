function Resolve-OptionalBuilderRoot {
    param(
        [string]$BatmanRootPath,
        [string]$BuilderRootPath
    )

    if ([string]::IsNullOrWhiteSpace($BuilderRootPath)) {
        return [System.IO.Path]::GetFullPath((Join-Path $BatmanRootPath 'builder'))
    }

    if ([System.IO.Path]::IsPathRooted($BuilderRootPath)) {
        return [System.IO.Path]::GetFullPath($BuilderRootPath)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $BatmanRootPath $BuilderRootPath))
}

function Get-UnrealPackageStorageInfo {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $ResolvedPath = (Resolve-Path $Path).Path
    $Bytes = [System.IO.File]::ReadAllBytes($ResolvedPath)
    $Stream = [System.IO.MemoryStream]::new($Bytes, $false)

    try {
        $Reader = [System.IO.BinaryReader]::new($Stream, [System.Text.Encoding]::ASCII, $false)

        try {
            $Signature = $Reader.ReadUInt32()
            $Version = $Reader.ReadUInt16()
            $Licensee = $Reader.ReadUInt16()
            $PackageSize = $Reader.ReadInt32()
            $FolderLength = $Reader.ReadInt32()
            $Reader.BaseStream.Seek($FolderLength, [System.IO.SeekOrigin]::Current) | Out-Null
            $Flags = $Reader.ReadUInt32()
            $NameCount = $Reader.ReadInt32()
            $NameOffset = $Reader.ReadInt32()
            $ExportCount = $Reader.ReadInt32()
            $ExportOffset = $Reader.ReadInt32()
            $ImportCount = $Reader.ReadInt32()
            $ImportOffset = $Reader.ReadInt32()
            $DependsOffset = $Reader.ReadInt32()
            $Reader.BaseStream.Seek(16, [System.IO.SeekOrigin]::Current) | Out-Null
            $GenerationCount = $Reader.ReadInt32()
            $Reader.BaseStream.Seek($GenerationCount * 8, [System.IO.SeekOrigin]::Current) | Out-Null
            $Reader.BaseStream.Seek(8, [System.IO.SeekOrigin]::Current) | Out-Null
            $CompressionFlags = $Reader.ReadUInt32()
            $CompressionChunkCount = $Reader.ReadInt32()

            return [pscustomobject]@{
                Path = $ResolvedPath
                Signature = $Signature
                Version = $Version
                Licensee = $Licensee
                PackageSize = $PackageSize
                FileLength = $Bytes.Length
                Flags = $Flags
                NameCount = $NameCount
                NameOffset = $NameOffset
                ExportCount = $ExportCount
                ExportOffset = $ExportOffset
                ImportCount = $ImportCount
                ImportOffset = $ImportOffset
                DependsOffset = $DependsOffset
                CompressionFlags = $CompressionFlags
                CompressionChunkCount = $CompressionChunkCount
            }
        }
        finally {
            $Reader.Dispose()
        }
    }
    finally {
        $Stream.Dispose()
    }
}

function Assert-UnrealPackageIsUnpacked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Context
    )

    $PackageInfo = Get-UnrealPackageStorageInfo -Path $Path

    if ($PackageInfo.CompressionChunkCount -ne 0) {
        throw (
            "$Context '$($PackageInfo.Path)' is stored as a chunk-compressed Unreal package " +
            "(compressionFlags=0x{0:X8}, chunkCount={1}). This workflow requires an unpacked/decompressed package at that path." -f
            $PackageInfo.CompressionFlags,
            $PackageInfo.CompressionChunkCount
        )
    }

    return $PackageInfo
}
