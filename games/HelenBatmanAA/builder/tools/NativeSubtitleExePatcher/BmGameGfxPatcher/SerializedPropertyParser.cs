using System.Globalization;
using System.Text;

namespace BmGameGfxPatcher;

internal sealed record SerializedPropertyTag(
    int TagOffset,
    string Name,
    int NameIndex,
    int NameNumber,
    string TypeName,
    int TypeIndex,
    int TypeNumber,
    int Size,
    int ArrayIndex,
    int PayloadOffset,
    string? StructTypeName);

internal static class SerializedPropertyParser
{
    public static IReadOnlyList<SerializedPropertyTag> ReadProperties(
        UnrealPackage package,
        ReadOnlySpan<byte> objectBytes,
        int startOffset)
    {
        if (startOffset < 0 || startOffset > objectBytes.Length)
        {
            throw new InvalidOperationException($"Property start offset {startOffset} is outside the object.");
        }

        var properties = new List<SerializedPropertyTag>();
        int offset = startOffset;

        while (offset + 8 <= objectBytes.Length)
        {
            int tagOffset = offset;
            int nameIndex = ReadInt32(objectBytes, offset);
            int nameNumber = ReadInt32(objectBytes, offset + 4);
            offset += 8;

            string name = ResolveName(package, nameIndex);
            if (string.Equals(name, "None", StringComparison.Ordinal))
            {
                break;
            }

            int typeIndex = ReadInt32(objectBytes, offset);
            int typeNumber = ReadInt32(objectBytes, offset + 4);
            offset += 8;

            string typeName = ResolveName(package, typeIndex);
            int size = ReadInt32(objectBytes, offset);
            offset += 4;
            int arrayIndex = ReadInt32(objectBytes, offset);
            offset += 4;

            string? structTypeName = null;

            if (string.Equals(typeName, "StructProperty", StringComparison.Ordinal))
            {
                int structTypeIndex = ReadInt32(objectBytes, offset);
                _ = ReadInt32(objectBytes, offset + 4);
                offset += 8;
                structTypeName = ResolveName(package, structTypeIndex);
            }

            if (string.Equals(typeName, "BoolProperty", StringComparison.Ordinal))
            {
                if (offset >= objectBytes.Length)
                {
                    throw new InvalidOperationException($"BoolProperty '{name}' is truncated.");
                }

                offset += 1;
            }

            int payloadOffset = offset;
            if (payloadOffset < 0 || payloadOffset + size > objectBytes.Length)
            {
                throw new InvalidOperationException(
                    $"Property '{name}' payload points outside the object. Offset={payloadOffset}, size={size}, object={objectBytes.Length}.");
            }

            properties.Add(
                new SerializedPropertyTag(
                    tagOffset,
                    name,
                    nameIndex,
                    nameNumber,
                    typeName,
                    typeIndex,
                    typeNumber,
                    size,
                    arrayIndex,
                    payloadOffset,
                    structTypeName));

            offset = payloadOffset + size;
        }

        return properties;
    }

    public static string FormatValuePreview(
        UnrealPackage package,
        ReadOnlySpan<byte> objectBytes,
        SerializedPropertyTag property)
    {
        ReadOnlySpan<byte> payload = objectBytes.Slice(property.PayloadOffset, property.Size);

        return property.TypeName switch
        {
            "IntProperty" when property.Size == 4
                => $"  value={ReadInt32(payload, 0)}",
            "FloatProperty" when property.Size == 4
                => $"  value={BitConverter.ToSingle(payload):0.###}",
            "StrProperty"
                => $"  value=\"{ReadSerializedString(payload)}\"",
            "NameProperty" when property.Size >= 8
                => $"  value={ResolveName(package, ReadInt32(payload, 0))}",
            "ObjectProperty" or "ClassProperty" when property.Size == 4
                => $"  value={FormatReference(package, ReadInt32(payload, 0))}",
            "ArrayProperty" when property.Size >= 4
                => $"  count={ReadInt32(payload, 0)}",
            "MapProperty" when property.Size >= 4
                => $"  count={ReadInt32(payload, 0)}",
            "StructProperty"
                => $"  struct={property.StructTypeName ?? "?"}",
            "BoolProperty"
                => string.Empty,
            _ => string.Empty
        };
    }

    private static string ResolveName(UnrealPackage package, int nameIndex)
    {
        if (nameIndex < 0 || nameIndex >= package.Names.Count)
        {
            throw new InvalidOperationException($"Name index {nameIndex} is outside the package name table.");
        }

        return package.Names[nameIndex];
    }

    private static string ReadSerializedString(ReadOnlySpan<byte> payload)
    {
        if (payload.Length < 4)
        {
            return "<truncated>";
        }

        int length = ReadInt32(payload, 0);

        if (length == 0)
        {
            return string.Empty;
        }

        if (length < 0)
        {
            int chars = -length;
            int byteCount = chars * 2;

            if (payload.Length < 4 + byteCount)
            {
                return "<truncated>";
            }

            return Encoding.Unicode.GetString(payload[4..(4 + byteCount - 2)]);
        }

        if (payload.Length < 4 + length)
        {
            return "<truncated>";
        }

        return Encoding.ASCII.GetString(payload[4..(4 + length - 1)]);
    }

    private static string FormatReference(UnrealPackage package, int reference)
    {
        if (reference == 0)
        {
            return "0";
        }

        ReferenceInfo? referenceInfo = package.ResolveReferenceInfo(reference);
        if (referenceInfo is null)
        {
            return reference.ToString(CultureInfo.InvariantCulture);
        }

        return $"{reference} ({referenceInfo.Kind}:{ValueOrDash(referenceInfo.OwnerName)}.{referenceInfo.ObjectName})";
    }

    private static string ValueOrDash(string? value)
    {
        return string.IsNullOrWhiteSpace(value) ? "-" : value;
    }

    private static int ReadInt32(ReadOnlySpan<byte> buffer, int offset)
    {
        if (offset < 0 || offset + sizeof(int) > buffer.Length)
        {
            throw new InvalidOperationException($"Cannot read Int32 at offset {offset}.");
        }

        return BitConverter.ToInt32(buffer[offset..(offset + sizeof(int))]);
    }
}
