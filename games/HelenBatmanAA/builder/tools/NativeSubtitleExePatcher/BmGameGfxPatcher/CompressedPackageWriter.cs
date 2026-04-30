using System.IO.Compression;

namespace BmGameGfxPatcher;

/// <summary>
/// Rebuilds a physical retail-compressed Unreal package from patched logical bytes by preserving
/// the physical header prefix, rewriting the top-level chunk table, and serializing chunk blocks
/// with the same codec declared by the source package.
/// </summary>
internal static class CompressedPackageWriter
{
    /// <summary>
    /// Chunk header signature used by Batman retail compression streams.
    /// </summary>
    private const uint ChunkHeaderSignature = 0x9E2A83C1;

    /// <summary>
    /// Unreal package compression flag value for retail Batman LZO chunk blocks.
    /// </summary>
    private const uint LzoCompressionFlags = 0x00000002;

    /// <summary>
    /// Unreal package compression flag value for zlib chunk blocks.
    /// </summary>
    private const uint ZlibCompressionFlags = 0x00000001;

    /// <summary>
    /// Serialized byte length of one top-level compression chunk table record.
    /// </summary>
    private const int CompressionChunkRecordSize = 16;

    /// <summary>
    /// Maximum literal count representable by the decoder's initial literal token (<c>flag - 17</c>).
    /// </summary>
    private const int MaximumLiteralOnlyBlockSize = 238;

    /// <summary>
    /// LZO end-marker token prefix used by the existing reader's decoder implementation.
    /// </summary>
    private const byte LzoEndMarkerToken = 0x11;

    /// <summary>
    /// Writes one rebuilt compressed package file using patched logical bytes and original chunk layout.
    /// </summary>
    /// <param name="package">Loaded source package metadata and original physical bytes.</param>
    /// <param name="patchedLogicalBytes">Patched logical package bytes with updated export table entries.</param>
    /// <param name="outputPath">Destination path for the rebuilt compressed package.</param>
    public static void WritePackage(UnrealPackage package, byte[] patchedLogicalBytes, string outputPath)
    {
        ArgumentNullException.ThrowIfNull(package);
        ArgumentNullException.ThrowIfNull(patchedLogicalBytes);
        ArgumentNullException.ThrowIfNull(outputPath);

        if (!package.LogicalImage.UsesCompressedStorage)
        {
            throw new InvalidOperationException("Compressed package writer requires a compressed source package.");
        }

        if (package.Header.CompressionFlags == 0)
        {
            throw new InvalidOperationException(
                "Compressed package writer requires a source package that declares chunk compression.");
        }

        IReadOnlyList<CompressionChunkRecord> sourceChunks = package.LogicalImage.CompressionChunks;
        if (sourceChunks.Count <= 0)
        {
            throw new InvalidOperationException("Compressed package writer requires at least one source compression chunk.");
        }

        CompressionChunkRecord firstChunk = sourceChunks[0];
        int headerPrefixLength = firstChunk.CompressedOffset;
        ValidateSlice(0, headerPrefixLength, package.PhysicalBytes.Length, "Physical header prefix");
        byte[] headerPrefixBytes = package.PhysicalBytes.AsSpan(0, headerPrefixLength).ToArray();
        int tableLength = checked(sourceChunks.Count * CompressionChunkRecordSize);
        ValidateSlice(package.Header.CompressionChunkTableOffset, tableLength, headerPrefixBytes.Length, "Compression chunk table");
        WriteInt32(headerPrefixBytes, package.Header.CompressionFlagsFieldOffset, unchecked((int)package.Header.CompressionFlags));

        int nextCompressedOffset = headerPrefixLength;
        var rebuiltChunkRecords = new List<CompressionChunkRecord>(sourceChunks.Count);
        var rebuiltChunkPayloads = new List<byte[]>(sourceChunks.Count);

        for (int index = 0; index < sourceChunks.Count; index++)
        {
            CompressionChunkRecord sourceChunk = sourceChunks[index];
            int chunkUncompressedSize = ResolveChunkUncompressedSize(sourceChunks, index, patchedLogicalBytes.Length);
            ValidateSlice(sourceChunk.UncompressedOffset, chunkUncompressedSize, patchedLogicalBytes.Length, $"Logical chunk {index}");

            int sourceBlockSize = ReadChunkBlockSize(package.PhysicalBytes, sourceChunk, index);
            int blockSize = package.Header.CompressionFlags == LzoCompressionFlags
                ? Math.Min(sourceBlockSize, MaximumLiteralOnlyBlockSize)
                : sourceBlockSize;
            ReadOnlySpan<byte> logicalChunkBytes = patchedLogicalBytes.AsSpan(sourceChunk.UncompressedOffset, chunkUncompressedSize);
            byte[] compressedChunkBytes = BuildCompressedChunk(logicalChunkBytes, blockSize, package.Header.CompressionFlags);

            rebuiltChunkRecords.Add(
                new CompressionChunkRecord(
                    nextCompressedOffset,
                    compressedChunkBytes.Length,
                    sourceChunk.UncompressedOffset,
                    chunkUncompressedSize));
            rebuiltChunkPayloads.Add(compressedChunkBytes);
            nextCompressedOffset = checked(nextCompressedOffset + compressedChunkBytes.Length);
        }

        RewriteChunkTable(headerPrefixBytes, package.Header.CompressionChunkTableOffset, rebuiltChunkRecords);
        WriteOutputFile(outputPath, headerPrefixBytes, rebuiltChunkPayloads);
    }

    /// <summary>
    /// Resolves the logical uncompressed size for a rebuilt chunk while preserving source chunk count.
    /// </summary>
    /// <param name="sourceChunks">Source top-level chunk records from the input package.</param>
    /// <param name="chunkIndex">Current chunk index being rebuilt.</param>
    /// <param name="logicalLength">Patched logical package byte length.</param>
    /// <returns>Logical uncompressed chunk size for the rebuilt chunk record.</returns>
    private static int ResolveChunkUncompressedSize(
        IReadOnlyList<CompressionChunkRecord> sourceChunks,
        int chunkIndex,
        int logicalLength)
    {
        CompressionChunkRecord sourceChunk = sourceChunks[chunkIndex];
        if (chunkIndex < sourceChunks.Count - 1)
        {
            return sourceChunk.UncompressedSize;
        }

        int originalLogicalEnd = checked(sourceChunk.UncompressedOffset + sourceChunk.UncompressedSize);
        if (logicalLength < originalLogicalEnd)
        {
            throw new InvalidOperationException(
                $"Patched logical package shrank below the original final chunk range. logicalLength={logicalLength} originalEnd={originalLogicalEnd}");
        }

        return checked(logicalLength - sourceChunk.UncompressedOffset);
    }

    /// <summary>
    /// Reads one original chunk block size from the source physical chunk header.
    /// </summary>
    /// <param name="physicalBytes">Original package physical bytes.</param>
    /// <param name="sourceChunk">Source chunk record that points to the serialized chunk header.</param>
    /// <param name="chunkIndex">Chunk index used for validation failures.</param>
    /// <returns>Per-chunk block size declared by the source chunk stream.</returns>
    private static int ReadChunkBlockSize(byte[] physicalBytes, CompressionChunkRecord sourceChunk, int chunkIndex)
    {
        ValidateSlice(sourceChunk.CompressedOffset, sourceChunk.CompressedSize, physicalBytes.Length, $"Compressed chunk {chunkIndex}");
        uint signature = ReadUInt32(physicalBytes, sourceChunk.CompressedOffset);

        if (signature != ChunkHeaderSignature)
        {
            throw new InvalidOperationException(
                $"Compressed chunk {chunkIndex} has an invalid chunk signature 0x{signature:X8}.");
        }

        int blockSize = ReadInt32(physicalBytes, sourceChunk.CompressedOffset + 4);
        if (blockSize <= 0)
        {
            throw new InvalidOperationException($"Compressed chunk {chunkIndex} has an invalid block size {blockSize}.");
        }

        return blockSize;
    }

    /// <summary>
    /// Builds one serialized retail chunk stream for one logical chunk byte span.
    /// </summary>
    /// <param name="logicalChunkBytes">Logical uncompressed bytes for one chunk record.</param>
    /// <param name="blockSize">Chunk block size written into the serialized chunk header.</param>
    /// <param name="compressionFlags">Compression codec flag written into the package header for all blocks.</param>
    /// <returns>Serialized chunk bytes including chunk and block headers plus compressed payloads.</returns>
    private static byte[] BuildCompressedChunk(ReadOnlySpan<byte> logicalChunkBytes, int blockSize, uint compressionFlags)
    {
        if (logicalChunkBytes.Length <= 0)
        {
            throw new InvalidOperationException("Cannot build a compressed chunk with zero logical length.");
        }

        if (blockSize <= 0)
        {
            throw new InvalidOperationException($"Compressed chunk block size must be positive. value={blockSize}");
        }

        int blockCount = checked((logicalChunkBytes.Length + blockSize - 1) / blockSize);
        var compressedBlocks = new List<byte[]>(blockCount);
        var uncompressedBlockSizes = new List<int>(blockCount);
        int totalCompressedPayloadSize = 0;

        for (int blockIndex = 0; blockIndex < blockCount; blockIndex++)
        {
            int blockOffset = checked(blockIndex * blockSize);
            int blockUncompressedSize = Math.Min(blockSize, logicalChunkBytes.Length - blockOffset);
            ReadOnlySpan<byte> uncompressedBlockBytes = logicalChunkBytes.Slice(blockOffset, blockUncompressedSize);
            byte[] compressedBlockBytes = CompressBlock(uncompressedBlockBytes, compressionFlags);

            compressedBlocks.Add(compressedBlockBytes);
            uncompressedBlockSizes.Add(blockUncompressedSize);
            totalCompressedPayloadSize = checked(totalCompressedPayloadSize + compressedBlockBytes.Length);
        }

        using var chunkStream = new MemoryStream();
        using (var writer = new BinaryWriter(chunkStream, System.Text.Encoding.ASCII, leaveOpen: true))
        {
            writer.Write(ChunkHeaderSignature);
            writer.Write(blockSize);
            writer.Write(totalCompressedPayloadSize);
            writer.Write(logicalChunkBytes.Length);

            for (int blockIndex = 0; blockIndex < blockCount; blockIndex++)
            {
                writer.Write(compressedBlocks[blockIndex].Length);
                writer.Write(uncompressedBlockSizes[blockIndex]);
            }

            for (int blockIndex = 0; blockIndex < blockCount; blockIndex++)
            {
                writer.Write(compressedBlocks[blockIndex]);
            }
        }

        return chunkStream.ToArray();
    }

    /// <summary>
    /// Compresses one uncompressed block into a verified payload for the requested Unreal codec.
    /// </summary>
    /// <param name="uncompressedBlockBytes">Uncompressed block bytes to compress.</param>
    /// <param name="compressionFlags">Requested Unreal package compression codec.</param>
    /// <returns>Verified compressed payload accepted by the in-repo package decoder.</returns>
    private static byte[] CompressBlock(ReadOnlySpan<byte> uncompressedBlockBytes, uint compressionFlags)
    {
        if (uncompressedBlockBytes.Length <= 0)
        {
            throw new InvalidOperationException(
                $"Compressed block length must be positive. value={uncompressedBlockBytes.Length}");
        }

        byte[] compressedBlockBytes = compressionFlags switch
        {
            LzoCompressionFlags => CompressLiteralOnlyLzoBlock(uncompressedBlockBytes),
            ZlibCompressionFlags => CompressZlibBlock(uncompressedBlockBytes),
            _ => throw new InvalidOperationException(
                $"Compressed package writer does not support output compression flags 0x{compressionFlags:X8}.")
        };

        byte[] roundTrippedBytes = CompressedPackageReader.InflateBlockBytes(
            compressedBlockBytes,
            uncompressedBlockBytes.Length,
            compressionFlags);
        if (!roundTrippedBytes.AsSpan().SequenceEqual(uncompressedBlockBytes))
        {
            throw new InvalidOperationException(
                $"Compressed block round-trip verification failed for codec 0x{compressionFlags:X8} and block length {uncompressedBlockBytes.Length}.");
        }

        return compressedBlockBytes;
    }

    /// <summary>
    /// Compresses one block into a literal-only headerless LZO payload accepted by the in-repo
    /// retail package decoder.
    /// </summary>
    /// <param name="uncompressedBlockBytes">Uncompressed source bytes to encode.</param>
    /// <returns>Serialized LZO payload bytes for one block.</returns>
    private static byte[] CompressLiteralOnlyLzoBlock(ReadOnlySpan<byte> uncompressedBlockBytes)
    {
        if (uncompressedBlockBytes.Length <= 0 || uncompressedBlockBytes.Length > MaximumLiteralOnlyBlockSize)
        {
            throw new InvalidOperationException(
                $"Literal-only LZO block length must be between 1 and {MaximumLiteralOnlyBlockSize} bytes. value={uncompressedBlockBytes.Length}");
        }

        byte[] compressedBlockBytes = new byte[checked(uncompressedBlockBytes.Length + 4)];
        compressedBlockBytes[0] = (byte)(uncompressedBlockBytes.Length + 17);
        uncompressedBlockBytes.CopyTo(compressedBlockBytes.AsSpan(1, uncompressedBlockBytes.Length));
        int endMarkerOffset = uncompressedBlockBytes.Length + 1;
        compressedBlockBytes[endMarkerOffset] = LzoEndMarkerToken;
        compressedBlockBytes[endMarkerOffset + 1] = 0;
        compressedBlockBytes[endMarkerOffset + 2] = 0;
        return compressedBlockBytes;
    }

    /// <summary>
    /// Compresses one block into a zlib payload that matches the in-repo Unreal package decoder.
    /// </summary>
    /// <param name="uncompressedBlockBytes">Uncompressed source bytes to encode.</param>
    /// <returns>Serialized zlib payload bytes for one block.</returns>
    private static byte[] CompressZlibBlock(ReadOnlySpan<byte> uncompressedBlockBytes)
    {
        using var compressedBlockStream = new MemoryStream();
        using (var zlibStream = new ZLibStream(compressedBlockStream, CompressionLevel.SmallestSize, leaveOpen: true))
        {
            zlibStream.Write(uncompressedBlockBytes);
        }
        byte[] compressedBlockBytes = compressedBlockStream.ToArray();
        if (compressedBlockBytes.Length <= 0)
        {
            throw new InvalidOperationException("Zlib compression produced an empty block payload.");
        }

        return compressedBlockBytes;
    }

    /// <summary>
    /// Rewrites all top-level chunk table records in the preserved package header prefix.
    /// </summary>
    /// <param name="headerPrefixBytes">Preserved physical header prefix bytes to mutate in place.</param>
    /// <param name="tableOffset">Header offset of the top-level chunk table.</param>
    /// <param name="chunkRecords">Rebuilt chunk records in file order.</param>
    private static void RewriteChunkTable(
        byte[] headerPrefixBytes,
        int tableOffset,
        IReadOnlyList<CompressionChunkRecord> chunkRecords)
    {
        for (int chunkIndex = 0; chunkIndex < chunkRecords.Count; chunkIndex++)
        {
            CompressionChunkRecord chunkRecord = chunkRecords[chunkIndex];
            int recordOffset = checked(tableOffset + (chunkIndex * CompressionChunkRecordSize));

            WriteInt32(headerPrefixBytes, recordOffset, chunkRecord.UncompressedOffset);
            WriteInt32(headerPrefixBytes, recordOffset + 4, chunkRecord.UncompressedSize);
            WriteInt32(headerPrefixBytes, recordOffset + 8, chunkRecord.CompressedOffset);
            WriteInt32(headerPrefixBytes, recordOffset + 12, chunkRecord.CompressedSize);
        }
    }

    /// <summary>
    /// Writes the final rebuilt compressed package bytes to disk.
    /// </summary>
    /// <param name="outputPath">Destination package path.</param>
    /// <param name="headerPrefixBytes">Serialized header prefix bytes.</param>
    /// <param name="chunkPayloads">Serialized per-chunk payload streams in file order.</param>
    private static void WriteOutputFile(string outputPath, byte[] headerPrefixBytes, IReadOnlyList<byte[]> chunkPayloads)
    {
        string outputDirectory = Path.GetDirectoryName(outputPath) ?? Directory.GetCurrentDirectory();
        Directory.CreateDirectory(outputDirectory);

        using var outputStream = new FileStream(outputPath, FileMode.Create, FileAccess.Write, FileShare.None);
        outputStream.Write(headerPrefixBytes, 0, headerPrefixBytes.Length);

        for (int chunkIndex = 0; chunkIndex < chunkPayloads.Count; chunkIndex++)
        {
            byte[] chunkPayload = chunkPayloads[chunkIndex];
            outputStream.Write(chunkPayload, 0, chunkPayload.Length);
        }
    }

    /// <summary>
    /// Writes one 32-bit signed integer into a byte array at the specified offset.
    /// </summary>
    /// <param name="buffer">Destination byte array.</param>
    /// <param name="offset">Target byte offset.</param>
    /// <param name="value">Signed value to write.</param>
    private static void WriteInt32(byte[] buffer, int offset, int value)
    {
        ValidateSlice(offset, sizeof(int), buffer.Length, "Int32 write");
        BitConverter.GetBytes(value).CopyTo(buffer, offset);
    }

    /// <summary>
    /// Reads one 32-bit signed integer from a byte array at the specified offset.
    /// </summary>
    /// <param name="buffer">Source byte array.</param>
    /// <param name="offset">Source byte offset.</param>
    /// <returns>Signed 32-bit integer value.</returns>
    private static int ReadInt32(byte[] buffer, int offset)
    {
        ValidateSlice(offset, sizeof(int), buffer.Length, "Int32 read");
        return BitConverter.ToInt32(buffer, offset);
    }

    /// <summary>
    /// Reads one 32-bit unsigned integer from a byte array at the specified offset.
    /// </summary>
    /// <param name="buffer">Source byte array.</param>
    /// <param name="offset">Source byte offset.</param>
    /// <returns>Unsigned 32-bit integer value.</returns>
    private static uint ReadUInt32(byte[] buffer, int offset)
    {
        ValidateSlice(offset, sizeof(uint), buffer.Length, "UInt32 read");
        return BitConverter.ToUInt32(buffer, offset);
    }

    /// <summary>
    /// Validates that a slice offset and length remain inside a buffer length.
    /// </summary>
    /// <param name="offset">Slice start offset.</param>
    /// <param name="length">Slice byte length.</param>
    /// <param name="bufferLength">Total buffer length.</param>
    /// <param name="label">Validation label for failure messages.</param>
    private static void ValidateSlice(int offset, int length, int bufferLength, string label)
    {
        long endOffset = (long)offset + length;

        if (offset < 0 || length < 0 || endOffset > bufferLength)
        {
            throw new InvalidOperationException(
                $"{label} points outside the buffer. offset={offset} length={length} bufferLength={bufferLength}");
        }
    }
}
