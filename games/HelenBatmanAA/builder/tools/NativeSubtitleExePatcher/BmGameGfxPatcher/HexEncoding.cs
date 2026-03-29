namespace BmGameGfxPatcher;

internal static class HexEncoding
{
    public static byte[] ParseHex(string text)
    {
        if (string.IsNullOrWhiteSpace(text))
        {
            return [];
        }

        string normalized = text
            .Replace("0x", string.Empty, StringComparison.OrdinalIgnoreCase)
            .Replace(" ", string.Empty, StringComparison.Ordinal)
            .Replace("-", string.Empty, StringComparison.Ordinal)
            .Replace("_", string.Empty, StringComparison.Ordinal);

        if (normalized.Length % 2 != 0)
        {
            throw new InvalidOperationException($"Hex value '{text}' has an odd number of digits.");
        }

        return Convert.FromHexString(normalized);
    }
}
