namespace BmGameGfxPatcher;

internal static class FontScaler
{
    public static void ScaleFonts(
        string packagePath,
        string outputPath,
        string owner,
        IReadOnlyList<string> exportNames,
        double scale,
        int startOffset)
    {
        string fullPackagePath = Path.GetFullPath(packagePath);
        string fullOutputPath = Path.GetFullPath(outputPath);

        if (string.Equals(fullPackagePath, fullOutputPath, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("Refusing to patch in place. Write to a separate output path.");
        }

        UnrealPackage package = UnrealPackage.Load(fullPackagePath);
        string? outputDirectory = Path.GetDirectoryName(fullOutputPath);
        if (!string.IsNullOrWhiteSpace(outputDirectory))
        {
            Directory.CreateDirectory(outputDirectory);
        }

        File.Copy(fullPackagePath, fullOutputPath, overwrite: true);
        var expectedHashes = new Dictionary<string, string>(StringComparer.Ordinal);

        using (var stream = new FileStream(fullOutputPath, FileMode.Open, FileAccess.ReadWrite, FileShare.None))
        {
            foreach (string exportName in exportNames)
            {
                ExportEntry exportEntry = package.FindExport(owner, exportName, "Font");
                byte[] patchedObject = ScaleFontObject(package, exportEntry, startOffset, scale);
                stream.Seek(exportEntry.SerialDataOffset, SeekOrigin.Begin);
                stream.Write(patchedObject, 0, patchedObject.Length);
                expectedHashes[exportName] = Hashing.Sha256Hex(patchedObject);
            }
        }

        VerifyOutput(fullOutputPath, owner, exportNames, expectedHashes);
    }

    private static byte[] ScaleFontObject(
        UnrealPackage package,
        ExportEntry exportEntry,
        int startOffset,
        double scale)
    {
        byte[] objectBytes = package.ReadObjectBytes(exportEntry).ToArray();
        IReadOnlyList<SerializedPropertyTag> properties = SerializedPropertyParser.ReadProperties(package, objectBytes, startOffset);
        SerializedPropertyTag? charactersProperty = properties.FirstOrDefault(property =>
            string.Equals(property.Name, "Characters", StringComparison.Ordinal) &&
            string.Equals(property.TypeName, "ArrayProperty", StringComparison.Ordinal));

        if (charactersProperty is null)
        {
            throw new InvalidOperationException($"Font export {exportEntry.ObjectName.Value} is missing the Characters array.");
        }

        int count = ReadInt32(objectBytes, charactersProperty.PayloadOffset);
        int entrySize = checked((charactersProperty.Size - sizeof(int)) / Math.Max(count, 1));

        if (count <= 0 || charactersProperty.Size < sizeof(int) || (charactersProperty.Size - sizeof(int)) % count != 0)
        {
            throw new InvalidOperationException(
                $"Font export {exportEntry.ObjectName.Value} has an unexpected Characters payload size ({charactersProperty.Size}) for count {count}.");
        }

        if (entrySize != 21)
        {
            throw new InvalidOperationException(
                $"Font export {exportEntry.ObjectName.Value} uses unsupported FontCharacter size {entrySize}.");
        }

        int cursor = charactersProperty.PayloadOffset + sizeof(int);

        for (int index = 0; index < count; index++)
        {
            int entryOffset = cursor + (index * entrySize);
            int uSizeOffset = entryOffset + 8;
            int vSizeOffset = entryOffset + 12;
            int verticalOffsetOffset = entryOffset + 17;

            int uSize = ReadInt32(objectBytes, uSizeOffset);
            int vSize = ReadInt32(objectBytes, vSizeOffset);
            int verticalOffset = ReadInt32(objectBytes, verticalOffsetOffset);

            WriteInt32(objectBytes, uSizeOffset, ScaleInt(uSize, scale));
            WriteInt32(objectBytes, vSizeOffset, ScaleInt(vSize, scale));
            WriteInt32(objectBytes, verticalOffsetOffset, ScaleInt(verticalOffset, scale));
        }

        return objectBytes;
    }

    private static void VerifyOutput(
        string outputPath,
        string owner,
        IReadOnlyList<string> exportNames,
        IReadOnlyDictionary<string, string> expectedHashes)
    {
        UnrealPackage outputPackage = UnrealPackage.Load(outputPath);

        foreach (string exportName in exportNames)
        {
            ExportEntry exportEntry = outputPackage.FindExport(owner, exportName, "Font");
            string outputHash = Hashing.Sha256Hex(outputPackage.ReadObjectBytes(exportEntry).Span);

            if (!string.Equals(outputHash, expectedHashes[exportName], StringComparison.Ordinal))
            {
                throw new InvalidOperationException($"Verification failed for font export {exportName}.");
            }
        }
    }

    private static int ScaleInt(int value, double scale)
    {
        if (value == 0)
        {
            return 0;
        }

        return Math.Max(1, checked((int)Math.Round(value * scale, MidpointRounding.AwayFromZero)));
    }

    private static int ReadInt32(byte[] buffer, int offset)
    {
        if (offset < 0 || offset + sizeof(int) > buffer.Length)
        {
            throw new InvalidOperationException($"Cannot read Int32 at offset {offset}.");
        }

        return BitConverter.ToInt32(buffer, offset);
    }

    private static void WriteInt32(byte[] buffer, int offset, int value)
    {
        if (offset < 0 || offset + sizeof(int) > buffer.Length)
        {
            throw new InvalidOperationException($"Cannot write Int32 at offset {offset}.");
        }

        BitConverter.GetBytes(value).CopyTo(buffer, offset);
    }
}
