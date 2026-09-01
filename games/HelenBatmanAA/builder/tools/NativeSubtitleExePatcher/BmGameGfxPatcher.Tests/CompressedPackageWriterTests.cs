using System.Reflection;
using Xunit;

namespace BmGameGfxPatcher.Tests;

/// <summary>
/// Verifies that rebuilt Batman frontend packages retain the retail LZO block layout and do not
/// regress to a literal-only representation that is technically decodable but unsuitable for UE3.
/// </summary>
public sealed class CompressedPackageWriterTests
{
    /// <summary>
    /// Stores the physical prefix length used by the compact synthetic UE3 package fixture.
    /// </summary>
    private const int PrefixLength = 128;

    /// <summary>
    /// Stores the logical payload length used to make literal-only compression regressions obvious.
    /// </summary>
    private const int LogicalPayloadLength = 1024 * 1024;

    /// <summary>
    /// Stores the retail UE3 logical block size used by Batman's frontend package.
    /// </summary>
    private const int RetailBlockSize = 131072;

    /// <summary>
    /// Stores the retail UE3 package signature.
    /// </summary>
    private const uint PackageSignature = 0x9E2A83C1;

    /// <summary>
    /// Stores the retail UE3 chunk signature.
    /// </summary>
    private const uint ChunkSignature = 0x9E2A83C1;

    /// <summary>
    /// Stores the retail package LZO compression flag.
    /// </summary>
    private const uint LzoCompressionFlags = 0x00000002;

    /// <summary>
    /// Rebuilds one deterministic compressed fixture and proves that its LZO stream remains compact,
    /// retains the source block size, and round-trips through the production decoder.
    /// </summary>
    [Fact]
    public void WritePackage_UsesRetailBlockSizeAndCompactNativeLzoPayload()
    {
        byte[] sourcePrefix = BuildPackagePrefix();
        byte[] logicalBytes = BuildLogicalBytes(sourcePrefix);
        string fixturePath = CreateTemporaryPath("source.umap");
        string outputPath = CreateTemporaryPath("rebuilt.umap");

        try
        {
            File.WriteAllBytes(fixturePath, BuildSourcePhysicalPackage(sourcePrefix));
            UnrealPackage package = CreatePackage(fixturePath, sourcePrefix, logicalBytes);

            CompressedPackageWriter.WritePackage(package, logicalBytes, outputPath);

            byte[] rebuiltBytes = File.ReadAllBytes(outputPath);
            int chunkOffset = BitConverter.ToInt32(rebuiltBytes, 92);
            int rebuiltBlockSize = BitConverter.ToInt32(rebuiltBytes, chunkOffset + 4);
            byte[] roundTrippedBytes = Ue3Decompressor.DecompressPackage(rebuiltBytes);

            Assert.Equal(RetailBlockSize, rebuiltBlockSize);
            Assert.True(
                rebuiltBytes.Length < logicalBytes.Length / 2,
                $"Retail-compatible LZO output was unexpectedly large: {rebuiltBytes.Length} bytes for {logicalBytes.Length} logical bytes.");
            byte[] expectedRoundTrippedBytes = logicalBytes.ToArray();
            Buffer.BlockCopy(rebuiltBytes, 76, expectedRoundTrippedBytes, 76, 24);
            Assert.Equal(expectedRoundTrippedBytes, roundTrippedBytes);
        }
        finally
        {
            DeleteTemporaryFile(fixturePath);
            DeleteTemporaryFile(outputPath);
        }
    }

    /// <summary>
    /// Rebuilds the same fixture twice and proves that native compression output is deterministic.
    /// </summary>
    [Fact]
    public void WritePackage_ProducesDeterministicBytes()
    {
        byte[] sourcePrefix = BuildPackagePrefix();
        byte[] logicalBytes = BuildLogicalBytes(sourcePrefix);
        string fixturePath = CreateTemporaryPath("source.umap");
        string firstOutputPath = CreateTemporaryPath("first.umap");
        string secondOutputPath = CreateTemporaryPath("second.umap");

        try
        {
            File.WriteAllBytes(fixturePath, BuildSourcePhysicalPackage(sourcePrefix));
            UnrealPackage package = CreatePackage(fixturePath, sourcePrefix, logicalBytes);

            CompressedPackageWriter.WritePackage(package, logicalBytes, firstOutputPath);
            CompressedPackageWriter.WritePackage(package, logicalBytes, secondOutputPath);

            Assert.Equal(File.ReadAllBytes(firstOutputPath), File.ReadAllBytes(secondOutputPath));
        }
        finally
        {
            DeleteTemporaryFile(fixturePath);
            DeleteTemporaryFile(firstOutputPath);
            DeleteTemporaryFile(secondOutputPath);
        }
    }

    /// <summary>
    /// Creates a private UnrealPackage instance around the synthetic compression metadata used by
    /// the writer tests without requiring a full export/name table fixture.
    /// </summary>
    /// <param name="fixturePath">Physical package path exposed by the package instance.</param>
    /// <param name="sourcePrefix">Physical package prefix before the first compressed chunk.</param>
    /// <param name="logicalBytes">Logical bytes exposed by the package instance.</param>
    /// <returns>A package object containing the synthetic retail compression layout.</returns>
    private static UnrealPackage CreatePackage(string fixturePath, byte[] sourcePrefix, byte[] logicalBytes)
    {
        var chunks = new List<CompressionChunkRecord>
        {
            new(PrefixLength, 16, PrefixLength, LogicalPayloadLength)
        };
        var logicalImage = new LogicalPackageImage(logicalBytes, chunks, usesCompressedStorage: true);
        var header = new PackageHeader(
            PackageSignature,
            Version: 684,
            Licensee: 0,
            PackageSize: checked(sourcePrefix.Length + LogicalPayloadLength),
            Flags: 0,
            NameTableCount: 0,
            NameTableOffset: 0,
            ExportTableCount: 0,
            ExportTableOffset: 0,
            ImportTableCount: 0,
            ImportTableOffset: 0,
            DependsTableOffset: 0,
            CompressionFlags: LzoCompressionFlags,
            CompressionFlagsFieldOffset: 76,
            CompressionChunkCount: 1,
            CompressionChunkTableOffset: 84,
            HeaderSize: PrefixLength);

        ConstructorInfo? constructor = typeof(UnrealPackage).GetConstructor(
            BindingFlags.Instance | BindingFlags.NonPublic,
            binder: null,
            [
                typeof(string),
                typeof(byte[]),
                typeof(LogicalPackageImage),
                typeof(PackageHeader),
                typeof(IReadOnlyList<string>),
                typeof(IReadOnlyList<ImportEntry>),
                typeof(IReadOnlyList<ExportEntry>)
            ],
            modifiers: null);
        Assert.NotNull(constructor);

        object? package = constructor!.Invoke(
        [
            fixturePath,
            BuildSourcePhysicalPackage(sourcePrefix),
            logicalImage,
            header,
            Array.Empty<string>(),
            Array.Empty<ImportEntry>(),
            Array.Empty<ExportEntry>()
        ]);
        return Assert.IsType<UnrealPackage>(package);
    }

    /// <summary>
    /// Builds the logical package bytes with a highly compressible, deterministic payload.
    /// </summary>
    /// <param name="sourcePrefix">Logical prefix bytes copied from the synthetic source header.</param>
    /// <returns>The complete logical package byte array.</returns>
    private static byte[] BuildLogicalBytes(byte[] sourcePrefix)
    {
        byte[] logicalBytes = new byte[checked(PrefixLength + LogicalPayloadLength)];
        sourcePrefix.CopyTo(logicalBytes, 0);
        for (int index = PrefixLength; index < logicalBytes.Length; index++)
        {
            logicalBytes[index] = (byte)('A' + ((index - PrefixLength) % 3));
        }

        return logicalBytes;
    }

    /// <summary>
    /// Builds a minimal physical source package whose chunk header supplies the retail block size.
    /// </summary>
    /// <param name="sourcePrefix">Physical prefix bytes containing the package and chunk tables.</param>
    /// <returns>A synthetic physical package byte array.</returns>
    private static byte[] BuildSourcePhysicalPackage(byte[] sourcePrefix)
    {
        byte[] physicalBytes = new byte[checked(PrefixLength + 16)];
        sourcePrefix.CopyTo(physicalBytes, 0);
        WriteUInt32(physicalBytes, PrefixLength, ChunkSignature);
        WriteInt32(physicalBytes, PrefixLength + 4, RetailBlockSize);
        WriteInt32(physicalBytes, PrefixLength + 8, 0);
        WriteInt32(physicalBytes, PrefixLength + 12, LogicalPayloadLength);
        return physicalBytes;
    }

    /// <summary>
    /// Builds a minimal package header and one chunk-table entry in the source prefix.
    /// </summary>
    /// <returns>The initialized physical package prefix.</returns>
    private static byte[] BuildPackagePrefix()
    {
        byte[] prefix = new byte[PrefixLength];
        WriteUInt32(prefix, 0, PackageSignature);
        WriteInt32(prefix, 12, 0);
        WriteInt32(prefix, 64, 0);
        WriteUInt32(prefix, 76, LzoCompressionFlags);
        WriteInt32(prefix, 80, 1);
        WriteInt32(prefix, 84, PrefixLength);
        WriteInt32(prefix, 88, LogicalPayloadLength);
        WriteInt32(prefix, 92, PrefixLength);
        WriteInt32(prefix, 96, 16);
        return prefix;
    }

    /// <summary>
    /// Allocates a unique temporary file path without creating the file.
    /// </summary>
    /// <param name="fileName">Leaf file name for the temporary path.</param>
    /// <returns>An absolute temporary file path.</returns>
    private static string CreateTemporaryPath(string fileName)
    {
        string directory = Path.Combine(Path.GetTempPath(), "BmGameGfxPatcherTests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(directory);
        return Path.Combine(directory, fileName);
    }

    /// <summary>
    /// Removes a temporary test file and its empty parent directory when present.
    /// </summary>
    /// <param name="filePath">Temporary file path to remove.</param>
    private static void DeleteTemporaryFile(string filePath)
    {
        if (File.Exists(filePath))
        {
            File.Delete(filePath);
        }

        string? directory = Path.GetDirectoryName(filePath);
        if (directory is not null && Directory.Exists(directory) && Directory.GetFileSystemEntries(directory).Length == 0)
        {
            Directory.Delete(directory);
        }
    }

    /// <summary>
    /// Writes a little-endian Int32 into a byte buffer.
    /// </summary>
    /// <param name="buffer">Destination byte buffer.</param>
    /// <param name="offset">Write offset.</param>
    /// <param name="value">Value to write.</param>
    private static void WriteInt32(byte[] buffer, int offset, int value)
    {
        BitConverter.GetBytes(value).CopyTo(buffer, offset);
    }

    /// <summary>
    /// Writes a little-endian UInt32 into a byte buffer.
    /// </summary>
    /// <param name="buffer">Destination byte buffer.</param>
    /// <param name="offset">Write offset.</param>
    /// <param name="value">Value to write.</param>
    private static void WriteUInt32(byte[] buffer, int offset, uint value)
    {
        BitConverter.GetBytes(value).CopyTo(buffer, offset);
    }
}
