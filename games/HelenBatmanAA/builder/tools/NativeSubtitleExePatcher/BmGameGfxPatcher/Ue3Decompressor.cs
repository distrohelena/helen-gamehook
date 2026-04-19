using System;
using System.IO;
using System.Runtime.InteropServices;

namespace BmGameGfxPatcher;

/// <summary>
/// UE3 LZO decompressor with correct header parsing for v576.
/// </summary>
public static class Ue3Decompressor
{
    [DllImport("MiniLzoDll.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int minilzo_init();

    [DllImport("MiniLzoDll.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int minilzo_decompress(IntPtr input, uint inputLen, IntPtr output, ref uint outputLen);

    [DllImport("MiniLzoDll.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void minilzo_cleanup();

    public static byte[] DecompressAndFixPackage(string inputPath)
    {
        byte[] data = DecompressPackage(inputPath);
        // Clear compression flags for decompressed file
        // Need to re-parse header to find correct offset
        int folderLen = BitConverter.ToInt32(data, 12);
        int compFlagsOffset = 48 + folderLen;
        BitConverter.GetBytes(0u).CopyTo(data, compFlagsOffset);
        BitConverter.GetBytes(0).CopyTo(data, compFlagsOffset + 4);
        return data;
    }

    public static byte[] DecompressPackage(string inputPath)
    {
        byte[] raw = File.ReadAllBytes(inputPath);
        minilzo_init();
        
        try
        {
            // Parse UE3 v576 header
            int folderLen = BitConverter.ToInt32(raw, 12);
            int genCount = BitConverter.ToInt32(raw, 64 + folderLen);
            // guid=48+folderLen, genCount=64+folderLen, genInfo=68+folderLen (12 bytes each),
            // engineVer=68+folderLen+12*gen+4, cookerVer=engineVer+4,
            // compFlags=cookerVer+4, chunkCount=compFlags+4
            int compFlagsOffset = 76 + folderLen + genCount * 12;
            int chunkCount = BitConverter.ToInt32(raw, compFlagsOffset + 4);
            int chunkTableOffset = compFlagsOffset + 8;
            uint compFlags = BitConverter.ToUInt32(raw, compFlagsOffset);
            
            Console.WriteLine($"[DEBUG] FolderLen={folderLen}, GenCount={genCount}, CompFlags=0x{compFlags:X8}, Chunks={chunkCount}");
            Console.WriteLine($"[DEBUG] CompFlags offset: {compFlagsOffset}, Chunk table: {chunkTableOffset}");
            
            if (chunkCount == 0)
            {
                Console.WriteLine("[DEBUG] No compression");
                return raw;
            }
            
            // Read chunk table
            var chunks = new (int uncompOffset, int uncompSize, int compOffset, int compSize)[chunkCount];
            int maxOffset = 0;
            
            for (int i = 0; i < chunkCount; i++)
            {
                int off = chunkTableOffset + i * 16;
                chunks[i] = (
                    BitConverter.ToInt32(raw, off),
                    BitConverter.ToInt32(raw, off + 4),
                    BitConverter.ToInt32(raw, off + 8),
                    BitConverter.ToInt32(raw, off + 12)
                );
                
                int end = chunks[i].uncompOffset + chunks[i].uncompSize;
                if (end > maxOffset) maxOffset = end;
                
                Console.WriteLine($"[DEBUG] Chunk {i}: uncompOff={chunks[i].uncompOffset}, uncompSize={chunks[i].uncompSize}, compOff=0x{chunks[i].compOffset:X} ({chunks[i].compOffset}), compSize={chunks[i].compSize}");
            }
            
            Console.WriteLine($"[DEBUG] Total uncompressed: {maxOffset} bytes");
            
            // Create output buffer
            byte[] logical = new byte[maxOffset];
            
            // Copy uncompressed header
            int firstOffset = chunks[0].uncompOffset;
            Array.Copy(raw, 0, logical, 0, firstOffset);
            Console.WriteLine($"[DEBUG] Copied {firstOffset} bytes header");
            
            // Decompress each chunk
            for (int i = 0; i < chunkCount; i++)
            {
                var chunk = chunks[i];
                if (chunk.uncompSize == 0) continue;
                
                int hdrOff = chunk.compOffset;
                if (hdrOff + 16 > raw.Length)
                {
                    Console.WriteLine($"[DEBUG] Chunk {i}: header offset 0x{hdrOff:X} out of bounds");
                    continue;
                }
                
                uint tag = BitConverter.ToUInt32(raw, hdrOff);
                if (tag != 0x9E2A83C1)
                {
                    Console.WriteLine($"[DEBUG] Chunk {i}: invalid tag 0x{tag:X8}");
                    continue;
                }
                
                int blockSize = BitConverter.ToInt32(raw, hdrOff + 4);
                int totalComp = BitConverter.ToInt32(raw, hdrOff + 8);
                int totalUncomp = BitConverter.ToInt32(raw, hdrOff + 12);
                
                Console.WriteLine($"[DEBUG] Chunk {i}: blockSz={blockSize}, compSz={totalComp}, uncompSz={totalUncomp}");
                
                int blockCnt = (totalUncomp + blockSize - 1) / blockSize;
                int blocksOff = hdrOff + 16;
                int dataOff = blocksOff;
                int outOff = chunk.uncompOffset;
                int done = 0;
                
                Console.WriteLine($"[DEBUG] Chunk {i}: {blockCnt} blocks, headers at 0x{blocksOff:X}, data at 0x{dataOff:X}");
                
                for (int j = 0; j < blockCnt && done < chunk.uncompSize; j++)
                {
                    if (dataOff + 8 > raw.Length)
                    {
                        Console.WriteLine($"[DEBUG] Chunk {i} Block {j}: dataOff+8={dataOff+8} > raw.Length={raw.Length}");
                        break;
                    }
                    
                    int blkComp = BitConverter.ToInt32(raw, dataOff);
                    int blkUncomp = BitConverter.ToInt32(raw, dataOff + 4);
                    dataOff += 8;
                    
                    if (j == 0) Console.WriteLine($"[DEBUG] Chunk {i} Block 0: blkComp={blkComp}, blkUncomp={blkUncomp}");
                    
                    int thisBlock = Math.Min(blkUncomp, chunk.uncompSize - done);
                    
                    if (blkComp == 0)
                    {
                        int cp = Math.Min(blkUncomp, thisBlock);
                        if (dataOff + cp <= raw.Length && outOff + cp <= logical.Length)
                            Array.Copy(raw, dataOff, logical, outOff, cp);
                        dataOff += blkUncomp;
                        outOff += cp;
                        done += cp;
                        continue;
                    }
                    
                    if (blkComp < 0 || blkUncomp <= 0 || dataOff + blkComp > raw.Length)
                    {
                        Console.WriteLine($"[DEBUG] Chunk {i} Block {j}: bad sizes comp={blkComp} uncomp={blkUncomp}");
                        break;
                    }
                    
                    // Safety check: limit block sizes to reasonable values
                    if (blkComp > 200000 || blkUncomp > 200000)
                    {
                        Console.WriteLine($"[DEBUG] Chunk {i} Block {j}: suspiciously large sizes comp={blkComp} uncomp={blkUncomp}");
                        break;
                    }
                    
                    byte[] compData = new byte[blkComp];
                    Array.Copy(raw, dataOff, compData, 0, blkComp);
                    dataOff += blkComp;
                    
                    byte[] uncompData = new byte[blkUncomp];
                    GCHandle ih = GCHandle.Alloc(compData, GCHandleType.Pinned);
                    GCHandle oh = GCHandle.Alloc(uncompData, GCHandleType.Pinned);
                    
                    try
                    {
                        uint outLen = (uint)blkUncomp;
                        int res = minilzo_decompress(ih.AddrOfPinnedObject(), (uint)blkComp, oh.AddrOfPinnedObject(), ref outLen);
                        
                        if (res == 0)
                        {
                            int cp = Math.Min((int)outLen, thisBlock);
                            if (outOff + cp <= logical.Length)
                                Array.Copy(uncompData, 0, logical, outOff, cp);
                        }
                        else
                        {
                            Console.WriteLine($"[DEBUG] Chunk {i} Block {j}: LZO error {res}");
                            break;
                        }
                    }
                    finally { ih.Free(); oh.Free(); }
                    
                    outOff += thisBlock;
                    done += thisBlock;
                }
                
                Console.WriteLine($"[DEBUG] Chunk {i}: decompressed {done}/{chunk.uncompSize}");
            }
            
            Console.WriteLine($"[DEBUG] Output: {logical.Length} bytes");
            return logical;
        }
        finally { minilzo_cleanup(); }
    }

    public static byte[]? ExtractMainV2Payload(byte[] data)
    {
        for (int i = 0; i < data.Length - 7; i++)
        {
            if (data[i] == 0x47 && data[i+1] == 0x46 && data[i+2] == 0x58)
            {
                if (i + 8 <= data.Length)
                {
                    uint sz = BitConverter.ToUInt32(data, i + 4);
                    if (sz > 100000 && sz < 2000000 && i + sz <= data.Length)
                    {
                        Console.WriteLine($"[DEBUG] Found GFX at {i}, size={sz}");
                        byte[] payload = new byte[sz];
                        Array.Copy(data, i, payload, 0, (int)sz);
                        return payload;
                    }
                }
            }
        }
        return null;
    }
}
