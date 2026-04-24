namespace BmGameGfxPatcher;

/// <summary>
/// Parses the chunk-compression metadata stored in one UE3 package header so the retail
/// Batman frontend package can be decompressed and recompressed without inventing a new layout.
/// </summary>
internal sealed class Ue3CompressionLayout
{
    /// <summary>
    /// Stores the expected Unreal package signature.
    /// </summary>
    private const uint PackageSignature = 0x9E2A83C1;

    /// <summary>
    /// Stores the encrypted package signature, which the Batman tools do not support.
    /// </summary>
    private const uint EncryptedSignature = 0xF84CEAB0;

    /// <summary>
    /// Initializes one parsed compression-layout description.
    /// </summary>
    /// <param name="compressionFlagsOffset">Byte offset of the compression-flags field.</param>
    /// <param name="compressionFlags">Compression flags value read from the package header.</param>
    /// <param name="chunkTableOffset">Byte offset of the chunk table.</param>
    /// <param name="chunks">Chunk-table entries in table order.</param>
    private Ue3CompressionLayout(
        int compressionFlagsOffset,
        uint compressionFlags,
        int chunkTableOffset,
        IReadOnlyList<ChunkTableEntry> chunks)
    {
        CompressionFlagsOffset = compressionFlagsOffset;
        CompressionFlags = compressionFlags;
        ChunkTableOffset = chunkTableOffset;
        Chunks = chunks;
    }

    /// <summary>
    /// Stores the byte offset of the compression-flags field.
    /// </summary>
    public int CompressionFlagsOffset { get; }

    /// <summary>
    /// Stores the package compression flags.
    /// </summary>
    public uint CompressionFlags { get; }

    /// <summary>
    /// Stores the byte offset of the chunk table.
    /// </summary>
    public int ChunkTableOffset { get; }

    /// <summary>
    /// Stores the parsed chunk-table entries in table order.
    /// </summary>
    public IReadOnlyList<ChunkTableEntry> Chunks { get; }

    /// <summary>
    /// Reports whether the package contains compressed chunks.
    /// </summary>
    public bool HasCompressedChunks => Chunks.Count > 0;

    /// <summary>
    /// Stores the compressed-file prefix length that must be preserved before the first chunk blob.
    /// </summary>
    public int PreservedPrefixLength => HasCompressedChunks ? Chunks[0].CompressedOffset : 0;

    /// <summary>
    /// Stores the decompressed-file prefix length copied before the first chunk begins.
    /// </summary>
    public int LogicalPrefixLength => HasCompressedChunks ? Chunks[0].UncompressedOffset : 0;

    /// <summary>
    /// Parses one package header and returns the compression layout described by its chunk table.
    /// </summary>
    /// <param name="packageBytes">The raw package bytes.</param>
    /// <returns>The parsed compression layout.</returns>
    public static Ue3CompressionLayout Read(ReadOnlySpan<byte> packageBytes)
    {
        uint signature = ReadUInt32(packageBytes, 0);
        if (signature == EncryptedSignature)
        {
            throw new InvalidOperationException("Encrypted packages are not supported.");
        }

        if (signature != PackageSignature)
        {
            throw new InvalidOperationException("File is not a valid Unreal package.");
        }

        int folderNameLength = ReadInt32(packageBytes, 12);
        int generationCount = ReadInt32(packageBytes, 64 + folderNameLength);
        int compressionFlagsOffset = checked(76 + folderNameLength + (generationCount * 12));
        uint compressionFlags = ReadUInt32(packageBytes, compressionFlagsOffset);
        int chunkCount = ReadInt32(packageBytes, compressionFlagsOffset + 4);
        int chunkTableOffset = checked(compressionFlagsOffset + 8);

        if (chunkCount < 0)
        {
            throw new InvalidOperationException("UE3 package reported a negative compression chunk count.");
        }

        var chunks = new List<ChunkTableEntry>(chunkCount);
        for (int index = 0; index < chunkCount; index++)
        {
            int entryOffset = checked(chunkTableOffset + (index * 16));
            chunks.Add(
                new ChunkTableEntry(
                    ReadInt32(packageBytes, entryOffset),
                    ReadInt32(packageBytes, entryOffset + 4),
                    ReadInt32(packageBytes, entryOffset + 8),
                    ReadInt32(packageBytes, entryOffset + 12)));
        }

        return new Ue3CompressionLayout(compressionFlagsOffset, compressionFlags, chunkTableOffset, chunks);
    }

    /// <summary>
    /// Reads one chunk header using the compressed offset from the matching chunk-table entry.
    /// </summary>
    /// <param name="packageBytes">The raw compressed package bytes.</param>
    /// <param name="chunk">The chunk-table entry to inspect.</param>
    /// <returns>The parsed chunk header.</returns>
    public ChunkHeader ReadChunkHeader(ReadOnlySpan<byte> packageBytes, ChunkTableEntry chunk)
    {
        return new ChunkHeader(
            ReadUInt32(packageBytes, chunk.CompressedOffset),
            ReadInt32(packageBytes, chunk.CompressedOffset + 4),
            ReadInt32(packageBytes, chunk.CompressedOffset + 8),
            ReadInt32(packageBytes, chunk.CompressedOffset + 12));
    }

    /// <summary>
    /// Reads one little-endian Int32 from the provided package bytes.
    /// </summary>
    /// <param name="buffer">The package buffer.</param>
    /// <param name="offset">The byte offset to read.</param>
    /// <returns>The decoded Int32 value.</returns>
    private static int ReadInt32(ReadOnlySpan<byte> buffer, int offset)
    {
        EnsureSlice(buffer, offset, sizeof(int));
        return BitConverter.ToInt32(buffer[offset..(offset + sizeof(int))]);
    }

    /// <summary>
    /// Reads one little-endian UInt32 from the provided package bytes.
    /// </summary>
    /// <param name="buffer">The package buffer.</param>
    /// <param name="offset">The byte offset to read.</param>
    /// <returns>The decoded UInt32 value.</returns>
    private static uint ReadUInt32(ReadOnlySpan<byte> buffer, int offset)
    {
        EnsureSlice(buffer, offset, sizeof(uint));
        return BitConverter.ToUInt32(buffer[offset..(offset + sizeof(uint))]);
    }

    /// <summary>
    /// Validates that one byte slice is fully contained within the provided buffer.
    /// </summary>
    /// <param name="buffer">The buffer being sliced.</param>
    /// <param name="offset">The starting byte offset.</param>
    /// <param name="length">The requested slice length.</param>
    private static void EnsureSlice(ReadOnlySpan<byte> buffer, int offset, int length)
    {
        if (offset < 0 || length < 0 || offset + length > buffer.Length)
        {
            throw new InvalidOperationException($"Compressed package metadata points outside the package. offset={offset} length={length}");
        }
    }

    /// <summary>
    /// Describes one entry from the UE3 compression chunk table.
    /// </summary>
    internal readonly struct ChunkTableEntry
    {
        /// <summary>
        /// Initializes one chunk-table entry.
        /// </summary>
        /// <param name="uncompressedOffset">Offset of this chunk in the logical decompressed package.</param>
        /// <param name="uncompressedSize">Size of this chunk in the logical decompressed package.</param>
        /// <param name="compressedOffset">Offset of this chunk blob in the compressed package.</param>
        /// <param name="compressedSize">Size of this chunk blob in the compressed package.</param>
        public ChunkTableEntry(int uncompressedOffset, int uncompressedSize, int compressedOffset, int compressedSize)
        {
            UncompressedOffset = uncompressedOffset;
            UncompressedSize = uncompressedSize;
            CompressedOffset = compressedOffset;
            CompressedSize = compressedSize;
        }

        /// <summary>
        /// Stores the chunk offset in the logical decompressed package.
        /// </summary>
        public int UncompressedOffset { get; }

        /// <summary>
        /// Stores the chunk size in the logical decompressed package.
        /// </summary>
        public int UncompressedSize { get; }

        /// <summary>
        /// Stores the chunk offset in the compressed package.
        /// </summary>
        public int CompressedOffset { get; }

        /// <summary>
        /// Stores the chunk size in the compressed package.
        /// </summary>
        public int CompressedSize { get; }
    }

    /// <summary>
    /// Describes one UE3 chunk header that precedes the block-header table and block payloads.
    /// </summary>
    internal readonly struct ChunkHeader
    {
        /// <summary>
        /// Initializes one chunk header.
        /// </summary>
        /// <param name="signature">Chunk signature value.</param>
        /// <param name="blockSize">Logical uncompressed block size.</param>
        /// <param name="compressedDataSize">Total compressed bytes across all blocks in the chunk.</param>
        /// <param name="uncompressedSize">Logical uncompressed size of the chunk.</param>
        public ChunkHeader(uint signature, int blockSize, int compressedDataSize, int uncompressedSize)
        {
            Signature = signature;
            BlockSize = blockSize;
            CompressedDataSize = compressedDataSize;
            UncompressedSize = uncompressedSize;
        }

        /// <summary>
        /// Stores the chunk signature.
        /// </summary>
        public uint Signature { get; }

        /// <summary>
        /// Stores the logical uncompressed block size.
        /// </summary>
        public int BlockSize { get; }

        /// <summary>
        /// Stores the total compressed byte count across all chunk blocks.
        /// </summary>
        public int CompressedDataSize { get; }

        /// <summary>
        /// Stores the logical uncompressed chunk size.
        /// </summary>
        public int UncompressedSize { get; }
    }
}
