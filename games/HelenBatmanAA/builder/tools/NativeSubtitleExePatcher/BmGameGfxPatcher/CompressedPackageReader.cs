using System.IO.Compression;

namespace BmGameGfxPatcher;

/// <summary>
/// Reconstructs the logical byte image for physically compressed retail Unreal packages.
/// </summary>
internal static class CompressedPackageReader
{
    private const uint ChunkHeaderSignature = 0x9E2A83C1;

    /// <summary>
    /// Builds the logical package image for one physical package file.
    /// </summary>
    /// <param name="physicalBytes">Full physical package bytes as stored on disk.</param>
    /// <param name="header">Parsed package header metadata from the physical header prefix.</param>
    /// <returns>Logical package bytes and validated chunk metadata.</returns>
    public static LogicalPackageImage BuildLogicalImage(byte[] physicalBytes, PackageHeader header)
    {
        ArgumentNullException.ThrowIfNull(physicalBytes);

        if (header.CompressionChunkCount < 0)
        {
            throw new InvalidOperationException($"Compression chunk count cannot be negative. value={header.CompressionChunkCount}");
        }

        if (header.CompressionChunkCount <= 0)
        {
            if (header.CompressionFlags != 0)
            {
                throw new InvalidOperationException(
                    $"Package declares compression flags but has no top-level compression chunk table. flags=0x{header.CompressionFlags:X8} chunkCount={header.CompressionChunkCount}");
            }

            return new LogicalPackageImage(physicalBytes, Array.Empty<CompressionChunkRecord>(), false);
        }

        IReadOnlyList<CompressionChunkRecord> compressionChunks = ReadCompressionChunks(physicalBytes, header);
        int logicalLength = checked((int)compressionChunks.Max(chunk => (long)chunk.UncompressedOffset + chunk.UncompressedSize));
        byte[] logicalBytes = new byte[logicalLength];
        int headerPrefixLength = compressionChunks[0].UncompressedOffset;

        ValidateSlice(0, headerPrefixLength, physicalBytes.Length, "Logical header prefix");
        Buffer.BlockCopy(physicalBytes, 0, logicalBytes, 0, headerPrefixLength);

        foreach (CompressionChunkRecord compressionChunk in compressionChunks)
        {
            byte[] chunkBytes = InflateChunkBytes(physicalBytes, compressionChunk, header.CompressionFlags);
            Buffer.BlockCopy(chunkBytes, 0, logicalBytes, compressionChunk.UncompressedOffset, chunkBytes.Length);
        }

        return new LogicalPackageImage(logicalBytes, compressionChunks, true);
    }

    /// <summary>
    /// Reads and validates the top-level retail compression chunk table from the package header trailer.
    /// </summary>
    /// <param name="physicalBytes">Full physical package bytes as stored on disk.</param>
    /// <param name="header">Parsed package header metadata that points to the chunk table.</param>
    /// <returns>Validated logical-to-physical chunk mappings.</returns>
    public static IReadOnlyList<CompressionChunkRecord> ReadCompressionChunks(byte[] physicalBytes, PackageHeader header)
    {
        ArgumentNullException.ThrowIfNull(physicalBytes);

        if (header.CompressionChunkCount < 0)
        {
            throw new InvalidOperationException($"Compression chunk count cannot be negative. value={header.CompressionChunkCount}");
        }

        int tableByteLength = checked(header.CompressionChunkCount * 16);
        ValidateSlice(header.CompressionChunkTableOffset, tableByteLength, physicalBytes.Length, "Compression chunk table");

        var chunks = new List<CompressionChunkRecord>(header.CompressionChunkCount);

        for (int index = 0; index < header.CompressionChunkCount; index++)
        {
            int recordOffset = checked(header.CompressionChunkTableOffset + (index * 16));
            int uncompressedOffset = ReadInt32(physicalBytes, recordOffset);
            int uncompressedSize = ReadInt32(physicalBytes, recordOffset + 4);
            int compressedOffset = ReadInt32(physicalBytes, recordOffset + 8);
            int compressedSize = ReadInt32(physicalBytes, recordOffset + 12);

            if (uncompressedOffset < 0 || uncompressedSize <= 0)
            {
                throw new InvalidOperationException(
                    $"Compression chunk {index} has an invalid logical slice. offset={uncompressedOffset} size={uncompressedSize}");
            }

            if (compressedOffset < 0 || compressedSize <= 0)
            {
                throw new InvalidOperationException(
                    $"Compression chunk {index} has an invalid physical slice. offset={compressedOffset} size={compressedSize}");
            }

            ValidateSlice(compressedOffset, compressedSize, physicalBytes.Length, $"Compression chunk {index}");
            chunks.Add(new CompressionChunkRecord(compressedOffset, compressedSize, uncompressedOffset, uncompressedSize));
        }

        return chunks;
    }

    /// <summary>
    /// Inflates one retail chunk from physical storage into the exact logical byte span declared by the top-level chunk table.
    /// </summary>
    /// <param name="physicalBytes">Full physical package bytes as stored on disk.</param>
    /// <param name="compressionChunk">Logical-to-physical chunk mapping.</param>
    /// <returns>Exact decompressed logical bytes for the chunk.</returns>
    public static byte[] InflateChunkBytes(byte[] physicalBytes, CompressionChunkRecord compressionChunk, uint compressionFlags)
    {
        ValidateSlice(compressionChunk.CompressedOffset, compressionChunk.CompressedSize, physicalBytes.Length, "Compressed chunk");
        uint signature = ReadUInt32(physicalBytes, compressionChunk.CompressedOffset);

        if (signature != ChunkHeaderSignature)
        {
            throw new InvalidOperationException(
                $"Compressed chunk header signature mismatch at {compressionChunk.CompressedOffset}. Found 0x{signature:X8}.");
        }

        int blockSize = ReadInt32(physicalBytes, compressionChunk.CompressedOffset + 4);
        int totalCompressedPayloadSize = ReadInt32(physicalBytes, compressionChunk.CompressedOffset + 8);
        int totalUncompressedSize = ReadInt32(physicalBytes, compressionChunk.CompressedOffset + 12);

        if (blockSize <= 0 || totalCompressedPayloadSize <= 0 || totalUncompressedSize <= 0)
        {
            throw new InvalidOperationException(
                $"Compressed chunk header at {compressionChunk.CompressedOffset} contains invalid sizes.");
        }

        if (totalUncompressedSize != compressionChunk.UncompressedSize)
        {
            throw new InvalidOperationException(
                $"Compressed chunk logical size mismatch at {compressionChunk.CompressedOffset}. Table={compressionChunk.UncompressedSize} Header={totalUncompressedSize}");
        }

        int blockCount = checked((totalUncompressedSize + blockSize - 1) / blockSize);
        int blockTableOffset = checked(compressionChunk.CompressedOffset + 16);
        int blockTableLength = checked(blockCount * 8);
        int payloadOffset = checked(blockTableOffset + blockTableLength);
        int accumulatedCompressedSize = 0;
        int accumulatedUncompressedSize = 0;
        int physicalPayloadOffset = payloadOffset;
        byte[] chunkBytes = new byte[totalUncompressedSize];
        int logicalOffset = 0;

        ValidateSlice(blockTableOffset, blockTableLength, physicalBytes.Length, "Compressed chunk block table");

        for (int blockIndex = 0; blockIndex < blockCount; blockIndex++)
        {
            int blockHeaderOffset = checked(blockTableOffset + (blockIndex * 8));
            int blockCompressedSize = ReadInt32(physicalBytes, blockHeaderOffset);
            int blockUncompressedSize = ReadInt32(physicalBytes, blockHeaderOffset + 4);

            if (blockCompressedSize <= 0 || blockUncompressedSize <= 0)
            {
                throw new InvalidOperationException(
                    $"Compressed chunk block {blockIndex} has an invalid size. compressed={blockCompressedSize} uncompressed={blockUncompressedSize}");
            }

            accumulatedCompressedSize = checked(accumulatedCompressedSize + blockCompressedSize);
            accumulatedUncompressedSize = checked(accumulatedUncompressedSize + blockUncompressedSize);
            ValidateSlice(physicalPayloadOffset, blockCompressedSize, physicalBytes.Length, $"Compressed chunk block {blockIndex}");

            ReadOnlySpan<byte> compressedBlockBytes = physicalBytes.AsSpan(physicalPayloadOffset, blockCompressedSize);
            byte[] blockBytes = InflateBlockBytes(compressedBlockBytes, blockUncompressedSize, compressionFlags);
            Buffer.BlockCopy(blockBytes, 0, chunkBytes, logicalOffset, blockBytes.Length);
            logicalOffset = checked(logicalOffset + blockBytes.Length);
            physicalPayloadOffset = checked(physicalPayloadOffset + blockCompressedSize);
        }

        if (accumulatedCompressedSize != totalCompressedPayloadSize)
        {
            throw new InvalidOperationException(
                $"Compressed chunk payload size mismatch at {compressionChunk.CompressedOffset}. Blocks={accumulatedCompressedSize} Header={totalCompressedPayloadSize}");
        }

        if (accumulatedUncompressedSize != totalUncompressedSize)
        {
            throw new InvalidOperationException(
                $"Compressed chunk decompressed size mismatch at {compressionChunk.CompressedOffset}. Blocks={accumulatedUncompressedSize} Header={totalUncompressedSize}");
        }

        int serializedChunkSize = checked(16 + blockTableLength + totalCompressedPayloadSize);
        if (serializedChunkSize != compressionChunk.CompressedSize)
        {
            throw new InvalidOperationException(
                $"Compressed chunk serialized size mismatch at {compressionChunk.CompressedOffset}. Table={compressionChunk.CompressedSize} Calculated={serializedChunkSize}");
        }

        return chunkBytes;
    }

    /// <summary>
    /// Inflates one compressed block using the Unreal codec stored in retail Batman packages.
    /// </summary>
    /// <param name="compressedBlockBytes">Serialized compressed block bytes.</param>
    /// <param name="expectedLength">Exact logical length expected after decompression.</param>
    /// <returns>Exact decompressed block bytes.</returns>
    internal static byte[] InflateBlockBytes(ReadOnlySpan<byte> compressedBlockBytes, int expectedLength, uint compressionFlags)
    {
        return compressionFlags switch
        {
            0x1 => InflateZlibBlockBytes(compressedBlockBytes, expectedLength),
            0x2 => InflateLzoBlockBytes(compressedBlockBytes, expectedLength),
            _ => throw new InvalidOperationException($"Unsupported package compression flags 0x{compressionFlags:X8}.")
        };
    }

    /// <summary>
    /// Inflates one zlib-compressed block and validates the resulting byte count.
    /// </summary>
    /// <param name="compressedBlockBytes">Serialized zlib block bytes.</param>
    /// <param name="expectedLength">Exact logical length expected after decompression.</param>
    /// <returns>Exact decompressed block bytes.</returns>
    private static byte[] InflateZlibBlockBytes(ReadOnlySpan<byte> compressedBlockBytes, int expectedLength)
    {
        using var sourceStream = new MemoryStream(compressedBlockBytes.ToArray(), writable: false);
        using var zlibStream = new ZLibStream(sourceStream, CompressionMode.Decompress, leaveOpen: false);
        using var destinationStream = new MemoryStream();
        zlibStream.CopyTo(destinationStream);
        byte[] blockBytes = destinationStream.ToArray();

        if (blockBytes.Length != expectedLength)
        {
            throw new InvalidOperationException(
                $"Zlib block decompressed to {blockBytes.Length} bytes, expected {expectedLength}.");
        }

        return blockBytes;
    }

    /// <summary>
    /// Inflates one headerless LZO block and validates the resulting byte count.
    /// </summary>
    /// <param name="compressedBlockBytes">Serialized LZO block bytes.</param>
    /// <param name="expectedLength">Exact logical length expected after decompression.</param>
    /// <returns>Exact decompressed block bytes.</returns>
    private static byte[] InflateLzoBlockBytes(ReadOnlySpan<byte> compressedBlockBytes, int expectedLength)
    {
        byte[] blockBytes = new byte[expectedLength];
        int inputOffset = 0;
        int outputOffset = 0;
        int plainBytes = 0;
        int flag = ReadByte(compressedBlockBytes, ref inputOffset);

        if (flag > 17)
        {
            int literalLength = flag - 17;
            CopyLiteralBytes(compressedBlockBytes, ref inputOffset, blockBytes, ref outputOffset, literalLength);
            flag = ReadByte(compressedBlockBytes, ref inputOffset);
        }

        while (true)
        {
            int flagCode = flag >> 4;
            int copyLength;
            int distance;

            if (flagCode == 0)
            {
                if (plainBytes == 0)
                {
                    copyLength = 3 + flag;
                    if (copyLength == 3)
                    {
                        copyLength = 18 + ReadExtendedLength(compressedBlockBytes, ref inputOffset);
                    }

                    plainBytes = 4;
                    CopyLiteralBytes(compressedBlockBytes, ref inputOffset, blockBytes, ref outputOffset, copyLength);
                    flag = ReadByte(compressedBlockBytes, ref inputOffset);
                    continue;
                }

                distance = ReadByte(compressedBlockBytes, ref inputOffset);
                distance = plainBytes <= 3
                    ? (distance << 2) + (flag >> 2) + 1
                    : (distance << 2) + (flag >> 2) + 2049;
                copyLength = plainBytes <= 3 ? 2 : 3;
            }
            else if (flagCode == 1)
            {
                copyLength = 2 + (flag & 0x7);
                if (copyLength == 2)
                {
                    copyLength = 9 + ReadExtendedLength(compressedBlockBytes, ref inputOffset);
                }

                distance = 16384 + ((flag & 0x8) << 11);
                flag = ReadByte(compressedBlockBytes, ref inputOffset);
                distance |= (ReadByte(compressedBlockBytes, ref inputOffset) << 6) | (flag >> 2);

                if (distance == 16384)
                {
                    if (outputOffset != expectedLength)
                    {
                        throw new InvalidOperationException(
                            $"LZO block terminated after {outputOffset} bytes, expected {expectedLength}.");
                    }

                    return blockBytes;
                }
            }
            else if (flagCode <= 3)
            {
                copyLength = 2 + (flag & 0x1F);
                if (copyLength == 2)
                {
                    copyLength = 33 + ReadExtendedLength(compressedBlockBytes, ref inputOffset);
                }

                flag = ReadByte(compressedBlockBytes, ref inputOffset);
                distance = (ReadByte(compressedBlockBytes, ref inputOffset) << 6) | (flag >> 2);
                distance += 1;
            }
            else if (flagCode <= 7)
            {
                copyLength = 3 + ((flag >> 5) & 0x1);
                distance = ReadByte(compressedBlockBytes, ref inputOffset);
                distance = (distance << 3) + ((flag >> 2) & 0x7) + 1;
            }
            else
            {
                copyLength = 5 + ((flag >> 5) & 0x3);
                distance = ReadByte(compressedBlockBytes, ref inputOffset);
                distance = (distance << 3) + ((flag & 0x1C) >> 2) + 1;
            }

            plainBytes = flag & 0x3;
            CopyBackReference(blockBytes, ref outputOffset, distance, copyLength);
            CopyLiteralBytes(compressedBlockBytes, ref inputOffset, blockBytes, ref outputOffset, plainBytes);
            flag = ReadByte(compressedBlockBytes, ref inputOffset);
        }
    }

    /// <summary>
    /// Copies one literal byte span from the compressed input into the decompressed output.
    /// </summary>
    /// <param name="source">Compressed block bytes.</param>
    /// <param name="sourceOffset">Current compressed input offset, advanced by the copied length.</param>
    /// <param name="destination">Destination output buffer.</param>
    /// <param name="destinationOffset">Current output offset, advanced by the copied length.</param>
    /// <param name="length">Literal byte count to copy.</param>
    private static void CopyLiteralBytes(
        ReadOnlySpan<byte> source,
        ref int sourceOffset,
        byte[] destination,
        ref int destinationOffset,
        int length)
    {
        ValidateSlice(sourceOffset, length, source.Length, "Compressed literal");
        ValidateSlice(destinationOffset, length, destination.Length, "Decompressed literal");
        source.Slice(sourceOffset, length).CopyTo(destination.AsSpan(destinationOffset, length));
        sourceOffset += length;
        destinationOffset += length;
    }

    /// <summary>
    /// Copies one back-reference from already decompressed output bytes, including overlapping copies.
    /// </summary>
    /// <param name="destination">Destination output buffer.</param>
    /// <param name="destinationOffset">Current output offset, advanced by the copied length.</param>
    /// <param name="distance">Backward distance from the current output cursor.</param>
    /// <param name="length">Number of bytes to copy from the back-reference source.</param>
    private static void CopyBackReference(byte[] destination, ref int destinationOffset, int distance, int length)
    {
        if (distance <= 0)
        {
            throw new InvalidOperationException($"LZO back-reference distance must be positive. distance={distance}");
        }

        int copySourceOffset = destinationOffset - distance;
        if (copySourceOffset < 0)
        {
            throw new InvalidOperationException(
                $"LZO back-reference points before the start of the output buffer. offset={destinationOffset} distance={distance}");
        }

        ValidateSlice(destinationOffset, length, destination.Length, "LZO back-reference destination");

        for (int index = 0; index < length; index++)
        {
            destination[destinationOffset++] = destination[copySourceOffset + index];
        }
    }

    /// <summary>
    /// Reads one LZO extended length value.
    /// </summary>
    /// <param name="source">Compressed block bytes.</param>
    /// <param name="sourceOffset">Current compressed input offset, advanced past the encoded length.</param>
    /// <returns>Decoded extended length contribution.</returns>
    private static int ReadExtendedLength(ReadOnlySpan<byte> source, ref int sourceOffset)
    {
        int length = 0;

        while (true)
        {
            int value = ReadByte(source, ref sourceOffset);
            if (value != 0)
            {
                return length + value;
            }

            length += byte.MaxValue;
        }
    }

    /// <summary>
    /// Reads one byte from the provided span and advances the caller's cursor.
    /// </summary>
    /// <param name="source">Source span.</param>
    /// <param name="offset">Current cursor, advanced by one byte.</param>
    /// <returns>Read byte value as an integer.</returns>
    private static int ReadByte(ReadOnlySpan<byte> source, ref int offset)
    {
        ValidateSlice(offset, 1, source.Length, "Compressed byte");
        return source[offset++];
    }

    /// <summary>
    /// Reads one 32-bit signed little-endian integer from the provided byte array.
    /// </summary>
    /// <param name="bytes">Source byte array.</param>
    /// <param name="offset">Byte offset of the 32-bit value.</param>
    /// <returns>Parsed signed 32-bit integer.</returns>
    private static int ReadInt32(byte[] bytes, int offset)
    {
        ValidateSlice(offset, sizeof(int), bytes.Length, "Int32");
        return BitConverter.ToInt32(bytes, offset);
    }

    /// <summary>
    /// Reads one 32-bit unsigned little-endian integer from the provided byte array.
    /// </summary>
    /// <param name="bytes">Source byte array.</param>
    /// <param name="offset">Byte offset of the 32-bit value.</param>
    /// <returns>Parsed unsigned 32-bit integer.</returns>
    private static uint ReadUInt32(byte[] bytes, int offset)
    {
        ValidateSlice(offset, sizeof(uint), bytes.Length, "UInt32");
        return BitConverter.ToUInt32(bytes, offset);
    }

    /// <summary>
    /// Validates that one offset/length pair stays inside the provided buffer length.
    /// </summary>
    /// <param name="offset">Slice start offset.</param>
    /// <param name="length">Slice byte length.</param>
    /// <param name="bufferLength">Total buffer length.</param>
    /// <param name="label">Human-readable slice label used in failures.</param>
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
