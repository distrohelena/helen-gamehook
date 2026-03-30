using System.Security.Cryptography;

namespace BmGameGfxPatcher;

internal static class Hashing
{
    public static string Sha256Hex(ReadOnlySpan<byte> data)
    {
        return Convert.ToHexString(SHA256.HashData(data)).ToLowerInvariant();
    }
}
