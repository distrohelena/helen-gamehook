using System.Text;

namespace BmGameGfxPatcher;

internal sealed class UnrealPackage
{
    private const uint PackageSignature = 0x9E2A83C1;
    private const uint EncryptedSignature = 0xF84CEAB0;

    private UnrealPackage(
        string fullPath,
        byte[] bytes,
        PackageHeader header,
        IReadOnlyList<string> names,
        IReadOnlyList<ImportEntry> imports,
        IReadOnlyList<ExportEntry> exports)
    {
        FullPath = fullPath;
        Bytes = bytes;
        Header = header;
        Names = names;
        Imports = imports;
        Exports = exports;
    }

    public string FullPath { get; }

    public byte[] Bytes { get; }

    public PackageHeader Header { get; }

    public IReadOnlyList<string> Names { get; }

    public IReadOnlyList<ImportEntry> Imports { get; }

    public IReadOnlyList<ExportEntry> Exports { get; }

    public static UnrealPackage Load(string packagePath)
    {
        string fullPath = Path.GetFullPath(packagePath);
        byte[] bytes = File.ReadAllBytes(fullPath);
        using var stream = new MemoryStream(bytes, writable: false);
        using var reader = new BinaryReader(stream, Encoding.ASCII, leaveOpen: false);

        PackageHeader header = ReadHeader(reader);

        if (header.Signature == EncryptedSignature)
        {
            throw new InvalidOperationException("Encrypted packages are not supported.");
        }

        if (header.Signature != PackageSignature)
        {
            throw new InvalidOperationException("File is not a valid Unreal package.");
        }

        List<string> names = ReadNames(reader, header);
        List<ImportEntry> imports = ReadImports(reader, header, names);
        List<ExportEntry> exports = ReadExports(reader, header, names);

        var package = new UnrealPackage(fullPath, bytes, header, names, imports, exports);

        foreach (ImportEntry importEntry in package.Imports)
        {
            importEntry.ResolveNames(package.ResolveObjectName);
        }

        foreach (ExportEntry exportEntry in package.Exports)
        {
            exportEntry.ResolveNames(package.ResolveObjectName);
        }

        return package;
    }

    public ReadOnlyMemory<byte> ReadObjectBytes(ExportEntry exportEntry)
    {
        ValidateSlice(exportEntry.SerialDataOffset, exportEntry.SerialDataSize);
        return Bytes.AsMemory(exportEntry.SerialDataOffset, exportEntry.SerialDataSize);
    }

    public string? ResolveObjectName(int reference)
    {
        if (reference == 0)
        {
            return null;
        }

        if (reference < 0)
        {
            int index = -reference - 1;
            return index >= 0 && index < Imports.Count ? Imports[index].ObjectName.Value : null;
        }

        int exportIndex = reference - 1;
        return exportIndex >= 0 && exportIndex < Exports.Count ? Exports[exportIndex].ObjectName.Value : null;
    }

    public ReferenceInfo? ResolveReferenceInfo(int reference)
    {
        if (reference == 0)
        {
            return null;
        }

        if (reference < 0)
        {
            int index = -reference - 1;
            if (index < 0 || index >= Imports.Count)
            {
                return null;
            }

            ImportEntry importEntry = Imports[index];
            return new ReferenceInfo("Import", importEntry.TableIndex, importEntry.TypeName.Value, importEntry.OwnerName, importEntry.ObjectName.Value);
        }

        int exportIndex = reference - 1;
        if (exportIndex < 0 || exportIndex >= Exports.Count)
        {
            return null;
        }

        ExportEntry exportEntry = Exports[exportIndex];
        return new ReferenceInfo("Export", exportEntry.TableIndex, exportEntry.TypeName, exportEntry.OwnerName, exportEntry.ObjectName.Value);
    }

    public IReadOnlyList<ReferenceOccurrence> ScanResolvedInt32References(ReadOnlySpan<byte> data)
    {
        var results = new List<ReferenceOccurrence>();

        for (int offset = 0; offset <= data.Length - sizeof(int); offset++)
        {
            int reference = BitConverter.ToInt32(data[offset..(offset + sizeof(int))]);
            ReferenceInfo? referenceInfo = ResolveReferenceInfo(reference);

            if (referenceInfo is null)
            {
                continue;
            }

            results.Add(new ReferenceOccurrence(offset, reference, referenceInfo));
        }

        return results;
    }

    public ExportEntry FindExport(string owner, string exportName, string exportType)
    {
        ExportEntry? match = Exports.SingleOrDefault(exportEntry =>
            OwnerMatches(exportEntry.OwnerName, owner) &&
            string.Equals(exportEntry.ObjectName.Value, exportName, StringComparison.Ordinal) &&
            string.Equals(exportEntry.TypeName, exportType, StringComparison.Ordinal));

        if (match is null)
        {
            throw new InvalidOperationException(
                $"Could not find export '{exportType}:{owner}.{exportName}' inside {FullPath}.");
        }

        return match;
    }

    private static bool OwnerMatches(string? exportOwnerName, string requestedOwner)
    {
        if (string.IsNullOrEmpty(requestedOwner))
        {
            return string.IsNullOrWhiteSpace(exportOwnerName);
        }

        return string.Equals(exportOwnerName, requestedOwner, StringComparison.Ordinal);
    }

    private static PackageHeader ReadHeader(BinaryReader reader)
    {
        uint signature = reader.ReadUInt32();
        ushort version = reader.ReadUInt16();
        ushort licensee = reader.ReadUInt16();
        int packageSize = reader.ReadInt32();
        _ = ReadDomainString(reader);
        uint flags = reader.ReadUInt32();
        int nameCount = reader.ReadInt32();
        int nameOffset = reader.ReadInt32();
        int exportCount = reader.ReadInt32();
        int exportOffset = reader.ReadInt32();
        int importCount = reader.ReadInt32();
        int importOffset = reader.ReadInt32();
        int dependsOffset = reader.ReadInt32();
        reader.BaseStream.Seek(16, SeekOrigin.Current);
        int generationCount = reader.ReadInt32();
        reader.BaseStream.Seek(generationCount * 8L, SeekOrigin.Current);
        reader.BaseStream.Seek(8, SeekOrigin.Current);
        uint compressionFlags = reader.ReadUInt32();
        int compressionChunkCount = reader.ReadInt32();

        if (compressionChunkCount > 0)
        {
            reader.BaseStream.Seek(compressionChunkCount * 16L, SeekOrigin.Current);
        }

        reader.BaseStream.Seek(8, SeekOrigin.Current);

        return new PackageHeader(
            signature,
            version,
            licensee,
            packageSize,
            flags,
            nameCount,
            nameOffset,
            exportCount,
            exportOffset,
            importCount,
            importOffset,
            dependsOffset,
            compressionFlags);
    }

    private static List<string> ReadNames(BinaryReader reader, PackageHeader header)
    {
        reader.BaseStream.Seek(header.NameTableOffset, SeekOrigin.Begin);
        var names = new List<string>(header.NameTableCount);

        for (int index = 0; index < header.NameTableCount; index++)
        {
            names.Add(ReadDomainString(reader));
            reader.BaseStream.Seek(8, SeekOrigin.Current);
        }

        return names;
    }

    private static List<ImportEntry> ReadImports(BinaryReader reader, PackageHeader header, IReadOnlyList<string> names)
    {
        reader.BaseStream.Seek(header.ImportTableOffset, SeekOrigin.Begin);
        var imports = new List<ImportEntry>(header.ImportTableCount);

        for (int index = 0; index < header.ImportTableCount; index++)
        {
            NameReference packageName = ReadNameReference(reader, names);
            NameReference typeName = ReadNameReference(reader, names);
            int ownerReference = reader.ReadInt32();
            NameReference objectName = ReadNameReference(reader, names);

            imports.Add(new ImportEntry(-(index + 1), packageName, typeName, ownerReference, objectName));
        }

        return imports;
    }

    private static List<ExportEntry> ReadExports(BinaryReader reader, PackageHeader header, IReadOnlyList<string> names)
    {
        reader.BaseStream.Seek(header.ExportTableOffset, SeekOrigin.Begin);
        var exports = new List<ExportEntry>(header.ExportTableCount);

        for (int index = 0; index < header.ExportTableCount; index++)
        {
            int entryOffset = checked((int)reader.BaseStream.Position);
            int typeReference = reader.ReadInt32();
            int parentReference = reader.ReadInt32();
            int ownerReference = reader.ReadInt32();
            NameReference objectName = ReadNameReference(reader, names);
            int archetypeReference = reader.ReadInt32();
            uint flagsHigh = reader.ReadUInt32();
            uint flagsLow = reader.ReadUInt32();
            int serialDataSize = reader.ReadInt32();
            int serialDataOffset = reader.ReadInt32();
            uint exportFlags = reader.ReadUInt32();
            int netObjectCount = reader.ReadInt32();
            reader.BaseStream.Seek(16, SeekOrigin.Current);
            uint unknown1 = reader.ReadUInt32();
            reader.BaseStream.Seek(sizeof(uint) * (long)netObjectCount, SeekOrigin.Current);

            exports.Add(
                new ExportEntry(
                    index + 1,
                    entryOffset,
                    typeReference,
                    parentReference,
                    ownerReference,
                    objectName,
                    archetypeReference,
                    flagsHigh,
                    flagsLow,
                    serialDataSize,
                    serialDataOffset,
                    exportFlags,
                    netObjectCount,
                    unknown1));
        }

        return exports;
    }

    private static NameReference ReadNameReference(BinaryReader reader, IReadOnlyList<string> names)
    {
        int index = reader.ReadInt32();
        int numeric = reader.ReadInt32();

        if (index < 0 || index >= names.Count)
        {
            throw new InvalidOperationException($"Name index {index} is out of range.");
        }

        string baseName = names[index];
        string resolved = numeric > 0 ? $"{baseName}_{numeric - 1}" : baseName;

        return new NameReference(index, numeric, resolved);
    }

    private static string ReadDomainString(BinaryReader reader)
    {
        int size = reader.ReadInt32();

        if (size == 0)
        {
            return string.Empty;
        }

        if (size < 0)
        {
            int byteCount = checked(-size * 2);
            byte[] raw = reader.ReadBytes(byteCount);
            return Encoding.Unicode.GetString(raw).TrimEnd('\0');
        }

        byte[] ascii = reader.ReadBytes(size);
        return Encoding.ASCII.GetString(ascii, 0, Math.Max(0, size - 1));
    }

    private void ValidateSlice(int offset, int length)
    {
        if (offset < 0 || length < 0 || offset + length > Bytes.Length)
        {
            throw new InvalidOperationException($"Export object points outside the package. offset={offset} length={length}");
        }
    }
}

internal sealed record PackageHeader(
    uint Signature,
    ushort Version,
    ushort Licensee,
    int PackageSize,
    uint Flags,
    int NameTableCount,
    int NameTableOffset,
    int ExportTableCount,
    int ExportTableOffset,
    int ImportTableCount,
    int ImportTableOffset,
    int DependsTableOffset,
    uint CompressionFlags);

internal sealed record NameReference(int Index, int Numeric, string Value);

internal sealed record ImportEntry(
    int TableIndex,
    NameReference PackageName,
    NameReference TypeName,
    int OwnerReference,
    NameReference ObjectName)
{
    public string? OwnerName { get; private set; }

    public void ResolveNames(Func<int, string?> resolveObjectName)
    {
        OwnerName = resolveObjectName(OwnerReference);
    }
}

internal sealed class ExportEntry
{
    public ExportEntry(
        int tableIndex,
        int entryOffset,
        int typeReference,
        int parentReference,
        int ownerReference,
        NameReference objectName,
        int archetypeReference,
        uint flagsHigh,
        uint flagsLow,
        int serialDataSize,
        int serialDataOffset,
        uint exportFlags,
        int netObjectCount,
        uint unknown1)
    {
        TableIndex = tableIndex;
        EntryOffset = entryOffset;
        TypeReference = typeReference;
        ParentReference = parentReference;
        OwnerReference = ownerReference;
        ObjectName = objectName;
        ArchetypeReference = archetypeReference;
        FlagsHigh = flagsHigh;
        FlagsLow = flagsLow;
        SerialDataSize = serialDataSize;
        SerialDataOffset = serialDataOffset;
        ExportFlags = exportFlags;
        NetObjectCount = netObjectCount;
        Unknown1 = unknown1;
    }

    public int TableIndex { get; }

    public int EntryOffset { get; }

    public int TypeReference { get; }

    public int ParentReference { get; }

    public int OwnerReference { get; }

    public NameReference ObjectName { get; }

    public int ArchetypeReference { get; }

    public uint FlagsHigh { get; }

    public uint FlagsLow { get; }

    public int SerialDataSize { get; private set; }

    public int SerialDataOffset { get; private set; }

    public uint ExportFlags { get; }

    public int NetObjectCount { get; }

    public uint Unknown1 { get; }

    public int SerialDataSizeFieldOffset => EntryOffset + 32;

    public int SerialDataOffsetFieldOffset => EntryOffset + 36;

    public string? TypeName { get; private set; }

    public string? OwnerName { get; private set; }

    public void ResolveNames(Func<int, string?> resolveObjectName)
    {
        TypeName = resolveObjectName(TypeReference);
        OwnerName = resolveObjectName(OwnerReference);
    }
}

internal sealed record ReferenceInfo(
    string Kind,
    int TableIndex,
    string? TypeName,
    string? OwnerName,
    string ObjectName);

internal sealed record ReferenceOccurrence(
    int Offset,
    int Reference,
    ReferenceInfo ReferenceInfo);
