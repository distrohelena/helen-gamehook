using System.Text;

namespace BmGameGfxPatcher;

internal static class BinarySearch
{
    public static int FindUniqueAscii(ReadOnlySpan<byte> data, string text)
    {
        byte[] needle = Encoding.ASCII.GetBytes(text);
        int matchIndex = -1;

        for (int index = 0; index <= data.Length - needle.Length; index++)
        {
            if (!data[index..(index + needle.Length)].SequenceEqual(needle))
            {
                continue;
            }

            if (matchIndex >= 0)
            {
                throw new InvalidOperationException($"Payload marker '{text}' is not unique inside the export object.");
            }

            matchIndex = index;
        }

        if (matchIndex < 0)
        {
            throw new InvalidOperationException($"Payload marker '{text}' was not found inside the export object.");
        }

        return matchIndex;
    }
}
