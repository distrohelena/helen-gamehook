using System.Runtime.InteropServices;

namespace BmGameGfxPatcher;

/// <summary>
/// Rebuilds a compressed UE3 package using the original retail chunk table as the on-disk layout template.
/// </summary>
internal static class Ue3Compressor
{
    /// <summary>
    /// Stores the expected chunk signature used by Batman's compressed frontend package.
    /// </summary>
    private const uint ChunkSignature = 0x9E2A83C1;

    /// <summary>
    /// Imports the MiniLZO initialization routine.
    /// </summary>
    /// <returns>The native MiniLZO status code.</returns>
    [DllImport("MiniLzoDll.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int minilzo_init();

    /// <summary>
    /// Imports the MiniLZO compression routine.
    /// </summary>
    /// <param name="input">Pointer to the uncompressed block bytes.</param>
    /// <param name="inputLen">Length of the uncompressed block bytes.</param>
    /// <param name="output">Pointer to the destination compressed buffer.</param>
    /// <param name="outputLen">Receives the actual compressed byte count.</param>
    /// <returns>The native MiniLZO status code.</returns>
    [DllImport("MiniLzoDll.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int minilzo_compress(IntPtr input, uint inputLen, IntPtr output, ref uint outputLen);

    /// <summary>
    /// Imports the MiniLZO cleanup routine.
    /// </summary>
    [DllImport("MiniLzoDll.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void minilzo_cleanup();

    /// <summary>
    /// Compresses one logical decompressed package using the retail package as the compression-layout template.
    /// </summary>
    /// <param name="decompressedBytes">The logical decompressed package bytes.</param>
    /// <param name="originalCompressedPackagePath">Path to the original compressed retail package.</param>
    /// <returns>The rebuilt compressed package bytes.</returns>
    public static byte[] Compress(byte[] decompressedBytes, string originalCompressedPackagePath)
    {
        if (decompressedBytes.Length == 0)
        {
            throw new InvalidOperationException("Cannot compress an empty UE3 package.");
        }

        byte[] originalCompressedBytes = File.ReadAllBytes(Path.GetFullPath(originalCompressedPackagePath));
        Ue3CompressionLayout layout = Ue3CompressionLayout.Read(originalCompressedBytes);
        if (!layout.HasCompressedChunks)
        {
            throw new InvalidOperationException("Expected a compressed retail package as the compression template.");
        }

        MiniLzoNative.EnsureLoaded();
        int initializeResult = minilzo_init();
        if (initializeResult != 0)
        {
            throw new InvalidOperationException($"MiniLZO initialization failed with status {initializeResult}.");
        }

        try
        {
            return CompressUsingLayout(decompressedBytes, originalCompressedBytes, layout);
        }
        finally
        {
            minilzo_cleanup();
        }
    }

    /// <summary>
    /// Compresses one logical package using the parsed retail layout metadata.
    /// </summary>
    /// <param name="decompressedBytes">The logical decompressed package bytes.</param>
    /// <param name="originalCompressedBytes">The original compressed package bytes.</param>
    /// <param name="layout">The parsed retail compression layout.</param>
    /// <returns>The rebuilt compressed package bytes.</returns>
    private static byte[] CompressUsingLayout(
        byte[] decompressedBytes,
        byte[] originalCompressedBytes,
        Ue3CompressionLayout layout)
    {
        byte[] resultPrefix = originalCompressedBytes[..layout.PreservedPrefixLength].ToArray();
        var chunkBlobs = new List<ChunkBlob>(layout.Chunks.Count);
        int lastChunkIndex = layout.Chunks.Count - 1;
        int currentChunkOffset = layout.PreservedPrefixLength;
        int totalLength = layout.PreservedPrefixLength;

        for (int chunkIndex = 0; chunkIndex < layout.Chunks.Count; chunkIndex++)
        {
            Ue3CompressionLayout.ChunkTableEntry originalChunk = layout.Chunks[chunkIndex];
            int logicalChunkSize = ResolveLogicalChunkSize(decompressedBytes.Length, layout.Chunks, chunkIndex);
            Ue3CompressionLayout.ChunkHeader originalChunkHeader = layout.ReadChunkHeader(originalCompressedBytes, originalChunk);

            if (originalChunkHeader.BlockSize <= 0)
            {
                throw new InvalidOperationException($"Chunk {chunkIndex} reported invalid block size {originalChunkHeader.BlockSize}.");
            }

            byte[] chunkBlobBytes = BuildChunkBlob(decompressedBytes, originalChunk.UncompressedOffset, logicalChunkSize, originalChunkHeader.BlockSize);
            chunkBlobs.Add(new ChunkBlob(currentChunkOffset, logicalChunkSize, chunkBlobBytes));
            currentChunkOffset = checked(currentChunkOffset + chunkBlobBytes.Length);
            totalLength = checked(totalLength + chunkBlobBytes.Length);

            if (chunkIndex < lastChunkIndex && logicalChunkSize != originalChunk.UncompressedSize)
            {
                throw new InvalidOperationException(
                    $"Only the final chunk may change size after patching. Chunk {chunkIndex} expected {originalChunk.UncompressedSize} bytes but resolved {logicalChunkSize}.");
            }
        }

        byte[] resultBytes = new byte[totalLength];
        resultPrefix.CopyTo(resultBytes, 0);

        for (int chunkIndex = 0; chunkIndex < layout.Chunks.Count; chunkIndex++)
        {
            Ue3CompressionLayout.ChunkTableEntry originalChunk = layout.Chunks[chunkIndex];
            ChunkBlob chunkBlob = chunkBlobs[chunkIndex];
            int chunkTableEntryOffset = checked(layout.ChunkTableOffset + (chunkIndex * 16));

            WriteInt32(resultBytes, chunkTableEntryOffset, originalChunk.UncompressedOffset);
            WriteInt32(resultBytes, chunkTableEntryOffset + 4, chunkBlob.LogicalUncompressedSize);
            WriteInt32(resultBytes, chunkTableEntryOffset + 8, chunkBlob.CompressedOffset);
            WriteInt32(resultBytes, chunkTableEntryOffset + 12, chunkBlob.Bytes.Length);

            chunkBlob.Bytes.CopyTo(resultBytes, chunkBlob.CompressedOffset);
        }

        return resultBytes;
    }

    /// <summary>
    /// Resolves the logical uncompressed size for one chunk in the patched decompressed package.
    /// </summary>
    /// <param name="logicalPackageLength">The full logical package length.</param>
    /// <param name="chunks">The original chunk-table entries.</param>
    /// <param name="chunkIndex">The chunk index being resolved.</param>
    /// <returns>The logical chunk size for that chunk.</returns>
    private static int ResolveLogicalChunkSize(
        int logicalPackageLength,
        IReadOnlyList<Ue3CompressionLayout.ChunkTableEntry> chunks,
        int chunkIndex)
    {
        Ue3CompressionLayout.ChunkTableEntry chunk = chunks[chunkIndex];
        if (chunkIndex < chunks.Count - 1)
        {
            return chunk.UncompressedSize;
        }

        int lastChunkSize = checked(logicalPackageLength - chunk.UncompressedOffset);
        if (lastChunkSize <= 0)
        {
            throw new InvalidOperationException(
                $"Logical package length {logicalPackageLength} does not reach the last chunk offset {chunk.UncompressedOffset}.");
        }

        return lastChunkSize;
    }

    /// <summary>
    /// Builds one compressed chunk blob from a logical decompressed chunk slice.
    /// </summary>
    /// <param name="logicalPackageBytes">The logical decompressed package bytes.</param>
    /// <param name="logicalChunkOffset">The chunk offset in the logical package.</param>
    /// <param name="logicalChunkSize">The chunk size in the logical package.</param>
    /// <param name="blockSize">The chunk block size.</param>
    /// <returns>The serialized compressed chunk blob.</returns>
    private static byte[] BuildChunkBlob(byte[] logicalPackageBytes, int logicalChunkOffset, int logicalChunkSize, int blockSize)
    {
        ValidateSlice(logicalPackageBytes.Length, logicalChunkOffset, logicalChunkSize, $"logical chunk at offset {logicalChunkOffset}");
        int blockCount = (int)Math.Ceiling(logicalChunkSize / (double)blockSize);
        var blocks = new List<CompressedBlock>(blockCount);
        int totalCompressedBlockBytes = 0;

        for (int blockIndex = 0; blockIndex < blockCount; blockIndex++)
        {
            int blockOffset = checked(logicalChunkOffset + (blockIndex * blockSize));
            int blockUncompressedSize = Math.Min(blockSize, logicalChunkSize - (blockIndex * blockSize));
            CompressedBlock block = CompressBlock(logicalPackageBytes, blockOffset, blockUncompressedSize);
            blocks.Add(block);
            totalCompressedBlockBytes = checked(totalCompressedBlockBytes + block.CompressedBytes.Length);
        }

        int chunkBlobLength = checked(16 + (blockCount * 8) + totalCompressedBlockBytes);
        byte[] chunkBlobBytes = new byte[chunkBlobLength];
        WriteUInt32(chunkBlobBytes, 0, ChunkSignature);
        WriteInt32(chunkBlobBytes, 4, blockSize);
        WriteInt32(chunkBlobBytes, 8, totalCompressedBlockBytes);
        WriteInt32(chunkBlobBytes, 12, logicalChunkSize);

        int dataOffset = checked(16 + (blockCount * 8));
        for (int blockIndex = 0; blockIndex < blocks.Count; blockIndex++)
        {
            CompressedBlock block = blocks[blockIndex];
            int headerOffset = checked(16 + (blockIndex * 8));
            WriteInt32(chunkBlobBytes, headerOffset, block.CompressedBytes.Length);
            WriteInt32(chunkBlobBytes, headerOffset + 4, block.UncompressedSize);
            block.CompressedBytes.CopyTo(chunkBlobBytes, dataOffset);
            dataOffset = checked(dataOffset + block.CompressedBytes.Length);
        }

        return chunkBlobBytes;
    }

    /// <summary>
    /// Compresses one logical block with MiniLZO.
    /// </summary>
    /// <param name="logicalPackageBytes">The logical package bytes.</param>
    /// <param name="blockOffset">The block offset.</param>
    /// <param name="blockUncompressedSize">The block size.</param>
    /// <returns>The compressed block description.</returns>
    private static CompressedBlock CompressBlock(byte[] logicalPackageBytes, int blockOffset, int blockUncompressedSize)
    {
        ValidateSlice(logicalPackageBytes.Length, blockOffset, blockUncompressedSize, $"logical block at offset {blockOffset}");

        byte[] blockBytes = new byte[blockUncompressedSize];
        Array.Copy(logicalPackageBytes, blockOffset, blockBytes, 0, blockUncompressedSize);

        int maxCompressedLength = checked(blockUncompressedSize + (blockUncompressedSize / 16) + 64 + 3);
        byte[] compressedBytes = new byte[maxCompressedLength];
        uint actualCompressedLength = 0;

        GCHandle inputHandle = GCHandle.Alloc(blockBytes, GCHandleType.Pinned);
        GCHandle outputHandle = GCHandle.Alloc(compressedBytes, GCHandleType.Pinned);

        try
        {
            int result = minilzo_compress(
                inputHandle.AddrOfPinnedObject(),
                (uint)blockUncompressedSize,
                outputHandle.AddrOfPinnedObject(),
                ref actualCompressedLength);

            if (result != 0)
            {
                throw new InvalidOperationException($"MiniLZO compression failed with status {result}.");
            }
        }
        finally
        {
            inputHandle.Free();
            outputHandle.Free();
        }

        int exactCompressedLength = checked((int)actualCompressedLength);
        byte[] exactCompressedBytes = new byte[exactCompressedLength];
        Array.Copy(compressedBytes, exactCompressedBytes, exactCompressedLength);
        return new CompressedBlock(exactCompressedBytes, blockUncompressedSize);
    }

    /// <summary>
    /// Writes one Int32 into the provided buffer.
    /// </summary>
    /// <param name="buffer">The destination buffer.</param>
    /// <param name="offset">The byte offset.</param>
    /// <param name="value">The value to write.</param>
    private static void WriteInt32(byte[] buffer, int offset, int value)
    {
        ValidateSlice(buffer.Length, offset, sizeof(int), $"Int32 write at {offset}");
        BitConverter.GetBytes(value).CopyTo(buffer, offset);
    }

    /// <summary>
    /// Writes one UInt32 into the provided buffer.
    /// </summary>
    /// <param name="buffer">The destination buffer.</param>
    /// <param name="offset">The byte offset.</param>
    /// <param name="value">The value to write.</param>
    private static void WriteUInt32(byte[] buffer, int offset, uint value)
    {
        ValidateSlice(buffer.Length, offset, sizeof(uint), $"UInt32 write at {offset}");
        BitConverter.GetBytes(value).CopyTo(buffer, offset);
    }

    /// <summary>
    /// Validates that one slice stays inside the provided buffer length.
    /// </summary>
    /// <param name="bufferLength">The buffer length.</param>
    /// <param name="offset">The slice offset.</param>
    /// <param name="length">The slice length.</param>
    /// <param name="description">Human-readable description used in failures.</param>
    private static void ValidateSlice(int bufferLength, int offset, int length, string description)
    {
        if (offset < 0 || length < 0 || offset + length > bufferLength)
        {
            throw new InvalidOperationException($"{description} pointed outside the buffer. offset={offset} length={length} bufferLength={bufferLength}");
        }
    }

    /// <summary>
    /// Stores the serialized bytes for one compressed chunk plus its logical uncompressed size.
    /// </summary>
    private readonly struct ChunkBlob
    {
        /// <summary>
        /// Initializes one compressed chunk blob descriptor.
        /// </summary>
        /// <param name="compressedOffset">Offset of the chunk blob in the final compressed package.</param>
        /// <param name="logicalUncompressedSize">Logical chunk size in the decompressed package.</param>
        /// <param name="bytes">Serialized chunk bytes.</param>
        public ChunkBlob(int compressedOffset, int logicalUncompressedSize, byte[] bytes)
        {
            CompressedOffset = compressedOffset;
            LogicalUncompressedSize = logicalUncompressedSize;
            Bytes = bytes;
        }

        /// <summary>
        /// Stores the chunk offset in the final compressed package.
        /// </summary>
        public int CompressedOffset { get; }

        /// <summary>
        /// Stores the logical uncompressed chunk size.
        /// </summary>
        public int LogicalUncompressedSize { get; }

        /// <summary>
        /// Stores the serialized chunk bytes.
        /// </summary>
        public byte[] Bytes { get; }
    }

    /// <summary>
    /// Stores the compressed bytes for one logical block.
    /// </summary>
    private readonly struct CompressedBlock
    {
        /// <summary>
        /// Initializes one compressed block descriptor.
        /// </summary>
        /// <param name="compressedBytes">Compressed block bytes.</param>
        /// <param name="uncompressedSize">Logical uncompressed block size.</param>
        public CompressedBlock(byte[] compressedBytes, int uncompressedSize)
        {
            CompressedBytes = compressedBytes;
            UncompressedSize = uncompressedSize;
        }

        /// <summary>
        /// Stores the compressed block bytes.
        /// </summary>
        public byte[] CompressedBytes { get; }

        /// <summary>
        /// Stores the logical uncompressed block size.
        /// </summary>
        public int UncompressedSize { get; }
    }
}
