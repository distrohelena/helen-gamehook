Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class MiniLZO
{
    [DllImport("MiniLzoDll.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int minilzo_init();

    [DllImport("MiniLzoDll.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int minilzo_compress(IntPtr input, uint inputLen, IntPtr output, ref uint outputLen);

    [DllImport("MiniLzoDll.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void minilzo_cleanup();
}
"@

# Set environment to find the DLL
$env:PATH = "C:\dev\helenhook\games\HelenBatmanAA\builder\tools\NativeSubtitleExePatcher\BmGameGfxPatcher\bin\Release\net8.0;" + $env:PATH

$inputPath = 'C:\dev\helenhook\games\HelenBatmanAA\builder\generated\graphics-options-experiment\Frontend-final.upk'
$outputPath = 'C:\dev\helenhook\games\HelenBatmanAA\builder\generated\graphics-options-experiment\Frontend-compressed.upk'

$decompressed = [System.IO.File]::ReadAllBytes($inputPath)
Write-Output "Input (decompressed): $($decompressed.Length) bytes"

# UE3 LZO compression parameters
$blockSize = 131072  # 128KB blocks
$blockCount = [Math]::Ceiling($decompressed.Length / $blockSize)

Write-Output "Block size: $blockSize, Block count: $blockCount"

# Initialize LZO
[MiniLZO]::minilzo_init() | Out-Null

# Compress each block
$compressedBlocks = @()
$totalCompressedSize = 0

for ($i = 0; $i -lt $blockCount; $i++) {
    $offset = $i * $blockSize
    $uncompSize = [Math]::Min($blockSize, $decompressed.Length - $offset)
    
    $blockData = New-Object byte[] $uncompSize
    [Array]::Copy($decompressed, $offset, $blockData, 0, $uncompSize)
    
    # Allocate output buffer (max size for LZO1x-1: input + input/16 + 64 + 3)
    $maxOutSize = $uncompSize + ($uncompSize / 16) + 64 + 3
    $outBuffer = New-Object byte[] $maxOutSize
    
    $outLen = [uint32]0
    
    # Compress
    $pinIn = [System.Runtime.InteropServices.GCHandle]::Alloc($blockData, 'Pinned')
    $pinOut = [System.Runtime.InteropServices.GCHandle]::Alloc($outBuffer, 'Pinned')
    
    try {
        $result = [MiniLZO]::minilzo_compress($pinIn.AddrOfPinnedObject(), $uncompSize, $pinOut.AddrOfPinnedObject(), [ref]$outLen)
        if ($result -ne 0) {
            Write-Error "LZO compression failed for block $i with error code $result"
            exit 1
        }
    } finally {
        $pinIn.Free()
        $pinOut.Free()
    }
    
    # Copy compressed data
    $compressedBlock = New-Object byte[] $outLen
    [Array]::Copy($outBuffer, $compressedBlock, $outLen)
    $compressedBlocks += $compressedBlock
    $totalCompressedSize += $outLen
    
    $ratio = [Math]::Round($outLen/$uncompSize*100,1)
    Write-Output ("Block {0}: {1} -> {2} bytes ({3}%)" -f $i, $uncompSize, $outLen, $ratio)
}

$uncompSize = $decompressed.Length

Write-Output "Total compressed data size: $totalCompressedSize bytes"

# Create output file with UE3 chunk format
$writer = New-Object System.IO.BinaryWriter([System.IO.File]::OpenWrite($outputPath))

# Write chunk header (16 bytes)
$writer.Write([uint32]0x9E2A83C1)  # Tag
$writer.Write([uint32]$blockSize)   # BlockSize
$writer.Write([uint32]$totalCompressedSize)  # CompressedSize
$writer.Write([uint32]$uncompSize)  # UncompressedSize

Write-Output "Chunk header written: tag=0x9E2A83C1, blockSize=$blockSize, compSize=$totalCompressedSize, uncompSize=$uncompSize"

# Write block headers
foreach ($block in $compressedBlocks) {
    $writer.Write([uint32]$block.Length)  # BlockCompressedSize
    $writer.Write([uint32]$blockSize)     # BlockUncompressedSize (use full block size for all except possibly last)
}

# Write compressed data
foreach ($block in $compressedBlocks) {
    $writer.Write($block)
}

# We also need to update the UE3 package header to reflect compression
# Re-open to modify header
$writer.Close()

# Read back and update header
$compressed = [System.IO.File]::ReadAllBytes($outputPath)

# The original decompressed file had correct UE3 header with offsets
# We need to preserve those offsets but update PackageSize
# PackageSize is at offset 8
[System.BitConverter]::GetBytes($compressed.Length).CopyTo($compressed, 8)

# Set compression flags (method 2 = LZO)
# CompressionFlags at offset 89: 0x00020000 (method 2 in high byte)
[System.BitConverter]::GetBytes([uint32]0x00020000).CopyTo($compressed, 89)
# CompressionChunkCount at offset 93: 1 chunk
[System.BitConverter]::GetBytes([int32]1).CopyTo($compressed, 93)

[System.IO.File]::WriteAllBytes($outputPath, $compressed)

[MiniLZO]::minilzo_cleanup()

Write-Output "Compressed Frontend saved: $($compressed.Length) bytes"
Write-Output "Compression ratio: $([Math]::Round($compressed.Length/$uncompSize*100,1))%"
