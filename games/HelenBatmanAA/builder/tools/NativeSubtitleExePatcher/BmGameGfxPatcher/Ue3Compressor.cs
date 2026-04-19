using System;
using System.IO;
using System.Runtime.InteropServices;

namespace BmGameGfxPatcher;

/// <summary>
/// Compresses a decompressed UE3 package replicating the original 14-chunk structure.
/// </summary>
public static class Ue3Compressor
{
    [DllImport("MiniLzoDll.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int minilzo_init();

    [DllImport("MiniLzoDll.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int minilzo_compress(IntPtr input, uint inputLen, IntPtr output, ref uint outputLen);

    [DllImport("MiniLzoDll.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void minilzo_cleanup();

    public static byte[] Compress(byte[] decompressedData, string originalPath)
    {
        minilzo_init();
        
        try
        {
            byte[] original = File.ReadAllBytes(originalPath);
            const int blockSize = 131072;
            const int headerSize = 137;
            const int originalChunkCount = 14;
            
            // Step 1: Compress all data into blocks
            int totalDataSize = decompressedData.Length;
            int blockCount = (int)Math.Ceiling(totalDataSize / (double)blockSize);
            
            Console.WriteLine($"[DEBUG] Compressing {totalDataSize} bytes in {blockCount} blocks");
            
            byte[][] compressedBlocks = new byte[blockCount][];
            uint totalCompressedSize = 0;
            
            for (int i = 0; i < blockCount; i++)
            {
                int offset = i * blockSize;
                int uncompSize = Math.Min(blockSize, totalDataSize - offset);
                
                byte[] blockData = new byte[uncompSize];
                Array.Copy(decompressedData, offset, blockData, 0, uncompSize);
                
                int maxOutSize = uncompSize + uncompSize / 16 + 64 + 3;
                byte[] outBuffer = new byte[maxOutSize];
                uint outLen = 0;
                
                GCHandle inHandle = GCHandle.Alloc(blockData, GCHandleType.Pinned);
                GCHandle outHandle = GCHandle.Alloc(outBuffer, GCHandleType.Pinned);
                
                try
                {
                    int compressResult = minilzo_compress(inHandle.AddrOfPinnedObject(), (uint)uncompSize, outHandle.AddrOfPinnedObject(), ref outLen);
                    if (compressResult != 0) throw new InvalidOperationException($"LZO failed block {i}: {compressResult}");
                }
                finally { inHandle.Free(); outHandle.Free(); }
                
                compressedBlocks[i] = new byte[outLen];
                Array.Copy(outBuffer, compressedBlocks[i], outLen);
                totalCompressedSize += outLen;
                
                Console.WriteLine($"[DEBUG] Block {i}: {uncompSize} -> {outLen} bytes");
            }
            
            // Step 2: Calculate file layout
            // header(137) + chunkTable(14*16=224) + 14*chunkHeader(14*16=224) + blockHeaders + compressedData
            int chunkTableOffset = 97;
            int chunkHeadersBase = headerSize + originalChunkCount * 16;
            int blockHeadersOffset = chunkHeadersBase + originalChunkCount * 16;
            int blockHeadersSize = blockCount * 8;
            int dataOffset = blockHeadersOffset + blockHeadersSize;
            int totalFileSize = dataOffset + (int)totalCompressedSize;
            
            Console.WriteLine($"[DEBUG] Layout:");
            Console.WriteLine($"[DEBUG]   Header: 0-{headerSize-1} ({headerSize} bytes)");
            Console.WriteLine($"[DEBUG]   Chunk table: 0x{chunkTableOffset:X}-{chunkTableOffset + originalChunkCount * 16 - 1}");
            Console.WriteLine($"[DEBUG]   Chunk headers: 0x{chunkHeadersBase:X}-{chunkHeadersBase + originalChunkCount * 16 - 1}");
            Console.WriteLine($"[DEBUG]   Block headers: 0x{blockHeadersOffset:X}-{blockHeadersOffset + blockHeadersSize - 1}");
            Console.WriteLine($"[DEBUG]   Data: 0x{dataOffset:X}-{dataOffset + totalCompressedSize - 1}");
            Console.WriteLine($"[DEBUG]   Total: {totalFileSize} bytes");
            
            using var ms = new MemoryStream(totalFileSize);
            using var writer = new BinaryWriter(ms);
            
            // Copy original header up to chunk count
            writer.Write(original, 0, 93);
            
            // Chunk count = 14
            writer.Write(originalChunkCount);
            
            // Chunk table: 14 entries
            // Chunk 0: our data
            writer.Write(14); // uncompOffset
            writer.Write(totalDataSize); // uncompSize
            writer.Write(chunkHeadersBase); // compOffset (chunk header location)
            writer.Write((uint)totalCompressedSize); // compSize
            
            // Chunks 1-13: empty
            for (int i = 1; i < originalChunkCount; i++)
            {
                writer.Write(0); // uncompOffset
                writer.Write(0); // uncompSize
                writer.Write(chunkHeadersBase + i * 16); // compOffset (chunk header location)
                writer.Write(0); // compSize
            }
            
            // 14 chunk headers
            for (int i = 0; i < originalChunkCount; i++)
            {
                ms.Position = chunkHeadersBase + i * 16;
                if (i == 0)
                {
                    writer.Write(0x9E2A83C1u); // Tag
                    writer.Write((uint)blockSize); // BlockSize
                    writer.Write(totalCompressedSize); // CompressedSize
                    writer.Write((uint)totalDataSize); // UncompressedSize
                }
                else
                {
                    // Empty chunk headers
                    writer.Write(0x9E2A83C1u);
                    writer.Write(0u); // BlockSize
                    writer.Write(0u); // CompressedSize
                    writer.Write(0u); // UncompressedSize
                }
            }
            
            // Block headers
            ms.Position = blockHeadersOffset;
            foreach (var block in compressedBlocks)
            {
                writer.Write((uint)block.Length);
                writer.Write((uint)blockSize);
            }
            
            // Compressed data
            ms.Position = dataOffset;
            foreach (var block in compressedBlocks)
            {
                writer.Write(block);
            }
            
            byte[] result = ms.ToArray();
            Console.WriteLine($"[DEBUG] Final: {result.Length} bytes");
            
            return result;
        }
        finally
        {
            minilzo_cleanup();
        }
    }
}
