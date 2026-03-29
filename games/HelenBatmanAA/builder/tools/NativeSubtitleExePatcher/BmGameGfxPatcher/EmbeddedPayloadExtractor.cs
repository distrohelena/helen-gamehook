namespace BmGameGfxPatcher;

/// <summary>
/// Extracts one embedded binary payload from an Unreal export object and writes it to disk.
/// </summary>
internal static class EmbeddedPayloadExtractor
{
    /// <summary>
    /// Reads one export object from a package, locates the requested embedded payload by marker, and writes that payload to an output file.
    /// </summary>
    /// <param name="packagePath">Path to the unpacked Unreal package that contains the export object.</param>
    /// <param name="owner">Owner name used to identify the export.</param>
    /// <param name="exportName">Object name used to identify the export.</param>
    /// <param name="exportType">Export type used to identify the export.</param>
    /// <param name="payloadMagic">ASCII marker that identifies the embedded payload inside the export object.</param>
    /// <param name="outputPath">Destination file path that receives the extracted payload bytes.</param>
    public static void WritePayload(
        string packagePath,
        string owner,
        string exportName,
        string exportType,
        string payloadMagic,
        string outputPath)
    {
        UnrealPackage package = UnrealPackage.Load(packagePath);
        ExportEntry exportEntry = package.FindExport(owner, exportName, exportType);
        ReadOnlyMemory<byte> objectBytes = package.ReadObjectBytes(exportEntry);
        byte[] payloadBytes = ExtractPayloadBytes(objectBytes.Span, payloadMagic);
        string fullOutputPath = Path.GetFullPath(outputPath);
        string? outputDirectory = Path.GetDirectoryName(fullOutputPath);

        if (!string.IsNullOrWhiteSpace(outputDirectory))
        {
            Directory.CreateDirectory(outputDirectory);
        }

        File.WriteAllBytes(fullOutputPath, payloadBytes);
    }

    /// <summary>
    /// Extracts one payload slice from an export object using the embedded payload marker and the payload length stored in the payload header.
    /// </summary>
    /// <param name="objectBytes">Full serialized export object bytes.</param>
    /// <param name="payloadMagic">ASCII marker that identifies the embedded payload inside the export object.</param>
    /// <returns>Exact embedded payload bytes starting at the marker and ending at the stored payload length.</returns>
    public static byte[] ExtractPayloadBytes(ReadOnlySpan<byte> objectBytes, string payloadMagic)
    {
        if (string.IsNullOrWhiteSpace(payloadMagic))
        {
            throw new InvalidOperationException("Payload extraction requires a non-empty payload marker.");
        }

        int payloadOffset = BinarySearch.FindUniqueAscii(objectBytes, payloadMagic);
        int payloadLength = ReadInt32(objectBytes, payloadOffset + 4);

        if (payloadLength <= 0)
        {
            throw new InvalidOperationException($"Embedded payload length must be positive, found {payloadLength}.");
        }

        if (payloadOffset + payloadLength > objectBytes.Length)
        {
            throw new InvalidOperationException("Embedded payload length points outside the export object.");
        }

        return objectBytes.Slice(payloadOffset, payloadLength).ToArray();
    }

    /// <summary>
    /// Reads one 32-bit little-endian integer from the provided byte span.
    /// </summary>
    /// <param name="buffer">Byte span that contains the requested value.</param>
    /// <param name="offset">Offset of the 32-bit value relative to the span start.</param>
    /// <returns>Parsed 32-bit integer value.</returns>
    private static int ReadInt32(ReadOnlySpan<byte> buffer, int offset)
    {
        if (offset < 0 || offset + sizeof(int) > buffer.Length)
        {
            throw new InvalidOperationException($"Cannot read Int32 at offset {offset}.");
        }

        return BitConverter.ToInt32(buffer[offset..(offset + sizeof(int))]);
    }
}
