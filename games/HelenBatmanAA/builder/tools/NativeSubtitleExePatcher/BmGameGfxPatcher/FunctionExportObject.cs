namespace BmGameGfxPatcher;

internal static class FunctionExportObject
{
    private const int ScriptSizeFieldOffset = 40;
    private const int ScriptDataOffset = 44;

    public static FunctionScriptLayout ReadScriptLayout(ReadOnlySpan<byte> objectBytes)
    {
        if (objectBytes.Length < ScriptDataOffset)
        {
            throw new InvalidOperationException("Function export object is too small to contain a script header.");
        }

        int scriptSize = BitConverter.ToInt32(objectBytes[ScriptSizeFieldOffset..(ScriptSizeFieldOffset + sizeof(int))]);
        if (scriptSize < 0 || ScriptDataOffset + scriptSize > objectBytes.Length)
        {
            throw new InvalidOperationException("Function export script size points outside the export object.");
        }

        return new FunctionScriptLayout(ScriptDataOffset, scriptSize, objectBytes[ScriptDataOffset..(ScriptDataOffset + scriptSize)].ToArray());
    }
}

internal sealed record FunctionScriptLayout(int ScriptOffset, int ScriptSize, byte[] ScriptBytes);
