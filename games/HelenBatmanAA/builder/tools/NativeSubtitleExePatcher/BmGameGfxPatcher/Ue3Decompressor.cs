using System.Runtime.InteropServices;

namespace BmGameGfxPatcher;

/// <summary>
/// Decompresses chunk-compressed UE3 packages into the logical package bytes that the
/// existing Unreal export-table tooling can parse.
/// </summary>
internal static class Ue3Decompressor
{
    /// <summary>
    /// Stores the expected chunk signature used by Batman's compressed frontend package.
    /// </summary>
    private const uint ChunkSignature = 0x9E2A83C1;

    [DllImport("MiniLzoDll.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int minilzo_init();

    [DllImport("MiniLzoDll.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int minilzo_decompress(IntPtr input, uint inputLen, IntPtr output, ref uint outputLen);

    [DllImport("MiniLzoDll.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void minilzo_cleanup();

    public static byte[] DecompressAndFixPackage(string inputPath)
    {
        return DecompressAndFixPackage(File.ReadAllBytes(Path.GetFullPath(inputPath)));
    }

    public static byte[] DecompressAndFixPackage(byte[] rawPackageBytes)
    {
        byte[] decompressedBytes = DecompressPackage(rawPackageBytes);
        Ue3CompressionLayout layout = Ue3CompressionLayout.Read(rawPackageBytes);

        if (layout.HasCompressedChunks)
        {
            BitConverter.GetBytes(0u).CopyTo(decompressedBytes, layout.CompressionFlagsOffset);
            BitConverter.GetBytes(0).CopyTo(decompressedBytes, layout.CompressionFlagsOffset + 4);
        }

        return decompressedBytes;
    }

    public static byte[] DecompressPackage(string inputPath)
    {
        return DecompressPackage(File.ReadAllBytes(Path.GetFullPath(inputPath)));
    }

    public static byte[] DecompressPackage(byte[] rawPackageBytes)
    {
        Ue3CompressionLayout layout = Ue3CompressionLayout.Read(rawPackageBytes);
        if (!layout.HasCompressedChunks)
        {
            return rawPackageBytes.ToArray();
        }

        MiniLzoNative.EnsureLoaded();
        int initializeResult = minilzo_init();
        if (initializeResult != 0)
        {
            throw new InvalidOperationException($"MiniLZO initialization failed with status {initializeResult}.");
        }

        try
        {
            int logicalLength = GetLogicalPackageLength(layout);
            byte[] logicalBytes = new byte[logicalLength];
            rawPackageBytes.AsSpan(0, layout.LogicalPrefixLength).CopyTo(logicalBytes);

            for (int chunkIndex = 0; chunkIndex < layout.Chunks.Count; chunkIndex++)
            {
                Ue3CompressionLayout.ChunkTableEntry chunk = layout.Chunks[chunkIndex];
                Ue3CompressionLayout.ChunkHeader chunkHeader = layout.ReadChunkHeader(rawPackageBytes, chunk);
                ValidateChunkHeader(chunkIndex, chunk, chunkHeader);
                DecompressChunk(rawPackageBytes, logicalBytes, chunkIndex, chunk, chunkHeader);
            }

            return logicalBytes;
        }
        finally
        {
            minilzo_cleanup();
        }
    }

    private static int GetLogicalPackageLength(Ue3CompressionLayout layout)
    {
        Ue3CompressionLayout.ChunkTableEntry lastChunk = layout.Chunks[^1];
        return checked(lastChunk.UncompressedOffset + lastChunk.UncompressedSize);
    }

    private static void ValidateChunkHeader(
        int chunkIndex,
        Ue3CompressionLayout.ChunkTableEntry chunk,
        Ue3CompressionLayout.ChunkHeader chunkHeader)
    {
        if (chunkHeader.Signature != ChunkSignature)
        {
            throw new InvalidOperationException($"Chunk {chunkIndex} had invalid signature 0x{chunkHeader.Signature:X8}.");
        }

        if (chunkHeader.BlockSize <= 0)
        {
            throw new InvalidOperationException($"Chunk {chunkIndex} reported invalid block size {chunkHeader.BlockSize}.");
        }

        if (chunkHeader.UncompressedSize != chunk.UncompressedSize)
        {
            throw new InvalidOperationException(
                $"Chunk {chunkIndex} header uncompressed size {chunkHeader.UncompressedSize} did not match the table value {chunk.UncompressedSize}.");
        }

        if (chunk.CompressedSize < 16)
        {
            throw new InvalidOperationException($"Chunk {chunkIndex} compressed size {chunk.CompressedSize} was too small.");
        }
    }

    private static void DecompressChunk(
        byte[] rawPackageBytes,
        byte[] logicalBytes,
        int chunkIndex,
        Ue3CompressionLayout.ChunkTableEntry chunk,
        Ue3CompressionLayout.ChunkHeader chunkHeader)
    {
        int blockCount = GetBlockCount(chunkHeader.UncompressedSize, chunkHeader.BlockSize);
        int blockHeadersOffset = checked(chunk.CompressedOffset + 16);
        int compressedDataOffset = checked(blockHeadersOffset + (blockCount * 8));
        int currentDataOffset = compressedDataOffset;
        int currentOutputOffset = chunk.UncompressedOffset;

        for (int blockIndex = 0; blockIndex < blockCount; blockIndex++)
        {
            int blockHeaderOffset = checked(blockHeadersOffset + (blockIndex * 8));
            int compressedSize = ReadInt32(rawPackageBytes, blockHeaderOffset);
            int uncompressedSize = ReadInt32(rawPackageBytes, blockHeaderOffset + 4);
            ValidateBlockHeader(chunkIndex, blockIndex, compressedSize, uncompressedSize, chunkHeader.BlockSize);

            if (compressedSize == 0)
            {
                CopyStoredBlock(rawPackageBytes, logicalBytes, chunkIndex, blockIndex, currentDataOffset, currentOutputOffset, uncompressedSize);
            }
            else
            {
                DecompressStoredBlock(rawPackageBytes, logicalBytes, chunkIndex, blockIndex, currentDataOffset, currentOutputOffset, compressedSize, uncompressedSize);
            }

            currentDataOffset = checked(currentDataOffset + compressedSize);
            currentOutputOffset = checked(currentOutputOffset + uncompressedSize);
        }

        if (currentOutputOffset != chunk.UncompressedOffset + chunk.UncompressedSize)
        {
            throw new InvalidOperationException($"Chunk {chunkIndex} decompressed to {currentOutputOffset - chunk.UncompressedOffset} bytes instead of {chunk.UncompressedSize}.");
        }
    }

    private static int GetBlockCount(int chunkUncompressedSize, int blockSize)
    {
        return (int)Math.Ceiling(chunkUncompressedSize / (double)blockSize);
    }

    private static void ValidateBlockHeader(int chunkIndex, int blockIndex, int compressedSize, int uncompressedSize, int blockSize)
    {
        if (compressedSize < 0 || uncompressedSize <= 0 || uncompressedSize > blockSize)
        {
            throw new InvalidOperationException(
                $"Chunk {chunkIndex} block {blockIndex} had invalid sizes comp={compressedSize} uncomp={uncompressedSize} blockSize={blockSize}.");
        }
    }

    private static void CopyStoredBlock(
        byte[] rawPackageBytes,
        byte[] logicalBytes,
        int chunkIndex,
        int blockIndex,
        int dataOffset,
        int outputOffset,
        int length)
    {
        ValidateSlice(rawPackageBytes.Length, dataOffset, length, $"chunk {chunkIndex} block {blockIndex} source");
        ValidateSlice(logicalBytes.Length, outputOffset, length, $"chunk {chunkIndex} block {blockIndex} destination");
        Array.Copy(rawPackageBytes, dataOffset, logicalBytes, outputOffset, length);
    }

    private static void DecompressStoredBlock(
        byte[] rawPackageBytes,
        byte[] logicalBytes,
        int chunkIndex,
        int blockIndex,
        int dataOffset,
        int outputOffset,
        int compressedSize,
        int uncompressedSize)
    {
        ValidateSlice(rawPackageBytes.Length, dataOffset, compressedSize, $"chunk {chunkIndex} block {blockIndex} compressed source");
        ValidateSlice(logicalBytes.Length, outputOffset, uncompressedSize, $"chunk {chunkIndex} block {blockIndex} logical destination");

        byte[] compressedBlock = new byte[compressedSize];
        byte[] uncompressedBlock = new byte[uncompressedSize];
        Array.Copy(rawPackageBytes, dataOffset, compressedBlock, 0, compressedSize);

        GCHandle inputHandle = GCHandle.Alloc(compressedBlock, GCHandleType.Pinned);
        GCHandle outputHandle = GCHandle.Alloc(uncompressedBlock, GCHandleType.Pinned);

        try
        {
            uint actualOutputLength = (uint)uncompressedSize;
            int result = minilzo_decompress(
                inputHandle.AddrOfPinnedObject(),
                (uint)compressedSize,
                outputHandle.AddrOfPinnedObject(),
                ref actualOutputLength);

            if (result != 0)
            {
                throw new InvalidOperationException($"MiniLZO decompression failed for chunk {chunkIndex} block {blockIndex} with status {result}.");
            }

            if (actualOutputLength != uncompressedSize)
            {
                throw new InvalidOperationException(
                    $"MiniLZO decompressed chunk {chunkIndex} block {blockIndex} to {actualOutputLength} bytes instead of {uncompressedSize}.");
            }

            Array.Copy(uncompressedBlock, 0, logicalBytes, outputOffset, uncompressedSize);
        }
        finally
        {
            inputHandle.Free();
            outputHandle.Free();
        }
    }

    private static int ReadInt32(byte[] buffer, int offset)
    {
        ValidateSlice(buffer.Length, offset, sizeof(int), $"Int32 read at {offset}");
        return BitConverter.ToInt32(buffer, offset);
    }

    private static void ValidateSlice(int bufferLength, int offset, int length, string description)
    {
        if (offset < 0 || length < 0 || offset + length > bufferLength)
        {
            throw new InvalidOperationException($"{description} pointed outside the buffer. offset={offset} length={length} bufferLength={bufferLength}");
        }
    }

    public static byte[]? ExtractMainV2Payload(byte[] data)
    {
        for (int i = 0; i < data.Length - 7; i++)
        {
            if (data[i] == 0x47 && data[i+1] == 0x46 && data[i+2] == 0x58)
            {
                if (i + 8 <= data.Length)
                {
                    uint sz = BitConverter.ToUInt32(data, i + 4);
                    if (sz > 100000 && sz < 2000000 && i + sz <= data.Length)
                    {
                        Console.WriteLine($"[DEBUG] Found GFX at {i}, size={sz}");
                        byte[] payload = new byte[sz];
                        Array.Copy(data, i, payload, 0, (int)sz);
                        return payload;
                    }
                }
            }
        }
        return null;
    }
}
