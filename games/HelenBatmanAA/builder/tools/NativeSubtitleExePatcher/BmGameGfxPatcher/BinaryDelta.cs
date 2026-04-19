using System;
using System.IO;

namespace BmGameGfxPatcher;

/// <summary>
/// Simple binary delta generator and applier.
/// </summary>
public static class BinaryDelta
{
    /// <summary>
    /// Generates a delta between two files.
    /// </summary>
    public static void GenerateDelta(string sourcePath, string targetPath, string deltaPath)
    {
        byte[] source = File.ReadAllBytes(sourcePath);
        byte[] target = File.ReadAllBytes(targetPath);
        
        Console.WriteLine($"Source: {source.Length} bytes");
        Console.WriteLine($"Target: {target.Length} bytes");
        
        // Find differences
        var changes = new System.Collections.Generic.List<(int offset, byte[] data)>();
        int minLength = Math.Min(source.Length, target.Length);
        
        int changeStart = -1;
        for (int i = 0; i < minLength; i++)
        {
            if (source[i] != target[i])
            {
                if (changeStart == -1) changeStart = i;
            }
            else
            {
                if (changeStart != -1)
                {
                    int changeLength = i - changeStart;
                    byte[] data = new byte[changeLength];
                    Array.Copy(target, changeStart, data, 0, changeLength);
                    changes.Add((changeStart, data));
                    changeStart = -1;
                }
            }
        }
        
        // Handle trailing changes
        if (changeStart != -1)
        {
            int changeLength = minLength - changeStart;
            byte[] data = new byte[changeLength];
            Array.Copy(target, changeStart, data, 0, changeLength);
            changes.Add((changeStart, data));
        }
        
        // Handle appended data
        if (target.Length > source.Length)
        {
            byte[] data = new byte[target.Length - source.Length];
            Array.Copy(target, source.Length, data, 0, data.Length);
            changes.Add((source.Length, data));
        }
        
        Console.WriteLine($"Delta: {changes.Count} changes");
        
        // Write delta file
        using var writer = new BinaryWriter(File.Open(deltaPath, FileMode.Create));
        writer.Write(changes.Count);
        foreach (var (offset, data) in changes)
        {
            writer.Write(offset);
            writer.Write(data.Length);
            writer.Write(data);
        }
        
        Console.WriteLine($"Delta written to {deltaPath}");
    }
    
    /// <summary>
    /// Applies a delta to a source file.
    /// </summary>
    public static byte[] ApplyDelta(byte[] source, string deltaPath)
    {
        using var reader = new BinaryReader(File.OpenRead(deltaPath));
        int changeCount = reader.ReadInt32();
        
        byte[] result = new byte[source.Length];
        Array.Copy(source, result, source.Length);
        
        for (int i = 0; i < changeCount; i++)
        {
            int offset = reader.ReadInt32();
            int length = reader.ReadInt32();
            byte[] data = reader.ReadBytes(length);
            
            // Expand array if needed
            if (offset + length > result.Length)
            {
                Array.Resize(ref result, offset + length);
            }
            
            Array.Copy(data, 0, result, offset, length);
        }
        
        return result;
    }
}
