namespace BmGameGfxPatcher;

/// <summary>
/// Represents the logical byte image that Unreal readers should consume, regardless of whether the source package is physically compressed.
/// </summary>
internal sealed class LogicalPackageImage
{
    /// <summary>
    /// Initializes one logical package image snapshot.
    /// </summary>
    /// <param name="bytes">Logical package bytes used by the parser and export readers.</param>
    /// <param name="compressionChunks">Compression chunk mappings used to materialize the logical bytes.</param>
    /// <param name="usesCompressedStorage">True when the logical bytes were reconstructed from compressed physical storage.</param>
    public LogicalPackageImage(byte[] bytes, IReadOnlyList<CompressionChunkRecord> compressionChunks, bool usesCompressedStorage)
    {
        Bytes = bytes;
        CompressionChunks = compressionChunks;
        UsesCompressedStorage = usesCompressedStorage;
    }

    /// <summary>
    /// Gets the logical package bytes that Unreal table and object readers should consume.
    /// </summary>
    public byte[] Bytes { get; }

    /// <summary>
    /// Gets the validated compression chunk mappings used to materialize the logical byte image.
    /// </summary>
    public IReadOnlyList<CompressionChunkRecord> CompressionChunks { get; }

    /// <summary>
    /// Gets a value indicating whether the source package used compressed physical storage.
    /// </summary>
    public bool UsesCompressedStorage { get; }
}
