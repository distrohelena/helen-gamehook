namespace BmGameGfxPatcher;

/// <summary>
/// Stores one logical-to-physical compression chunk mapping from a retail Unreal package.
/// </summary>
internal sealed class CompressionChunkRecord
{
    /// <summary>
    /// Initializes one immutable compression chunk record.
    /// </summary>
    /// <param name="compressedOffset">Physical file offset of the serialized compressed chunk.</param>
    /// <param name="compressedSize">Serialized byte length of the compressed chunk, including chunk and block headers.</param>
    /// <param name="uncompressedOffset">Logical package offset where the decompressed chunk bytes begin.</param>
    /// <param name="uncompressedSize">Logical byte length produced after decompressing the chunk.</param>
    public CompressionChunkRecord(int compressedOffset, int compressedSize, int uncompressedOffset, int uncompressedSize)
    {
        CompressedOffset = compressedOffset;
        CompressedSize = compressedSize;
        UncompressedOffset = uncompressedOffset;
        UncompressedSize = uncompressedSize;
    }

    /// <summary>
    /// Gets the physical file offset of the serialized compressed chunk.
    /// </summary>
    public int CompressedOffset { get; }

    /// <summary>
    /// Gets the serialized byte length of the compressed chunk, including the chunk header and block headers.
    /// </summary>
    public int CompressedSize { get; }

    /// <summary>
    /// Gets the logical package offset where the decompressed bytes belong.
    /// </summary>
    public int UncompressedOffset { get; }

    /// <summary>
    /// Gets the logical byte length produced after decompressing the chunk.
    /// </summary>
    public int UncompressedSize { get; }
}
