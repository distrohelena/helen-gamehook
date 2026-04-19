/*
 * minilzo_wrapper.c - P/Invoke wrapper for MiniLZO decompression
 * Compiles as a DLL for use with C# HelenGameHook builder tools.
 */

#include <windows.h>
#include <stddef.h>
#include "minilzo-2.10/minilzo.h"

/* Work memory for LZO decompression (LZO1X_MEM_DECOMPRESS = 0, no work memory needed for decompression) */
static lzo_bytep work_memory = NULL;

/* Initialize work memory (call once before decompression) */
__declspec(dllexport) int __cdecl minilzo_init(void)
{
    /* LZO1X decompression doesn't need work memory, but we allocate it for safety */
    work_memory = (lzo_bytep)malloc(16384);
    if (work_memory == NULL) {
        return LZO_E_OUT_OF_MEMORY;
    }
    return LZO_E_OK;
}

/* Cleanup work memory */
__declspec(dllexport) void __cdecl minilzo_cleanup(void)
{
    if (work_memory != NULL) {
        free(work_memory);
        work_memory = NULL;
    }
}

/*
 * Decompress LZO1x-1 compressed data.
 * 
 * Parameters:
 *   input       - pointer to compressed data
 *   input_len   - length of compressed data
 *   output      - pointer to output buffer (pre-allocated)
 *   output_len  - pointer to variable that receives actual decompressed length
 *
 * Returns:
 *   LZO_E_OK on success, LZO error code on failure
 */
__declspec(dllexport) int __cdecl minilzo_decompress(
    const lzo_bytep input,
    lzo_uint input_len,
    lzo_bytep output,
    lzo_uintp output_len)
{
    if (work_memory == NULL) {
        return LZO_E_ERROR;
    }
    
    /* Use standard decompress instead of safe version - UE3 may use slightly non-standard LZO */
    return lzo1x_decompress(input, input_len, output, output_len, work_memory);
}

/*
 * Compress data using LZO1x-1.
 * Allocates work memory per call to avoid static variable issues.
 * 
 * Parameters:
 *   input       - pointer to uncompressed data
 *   input_len   - length of uncompressed data
 *   output      - pointer to output buffer (must be at least input_len + input_len/16 + 64 + 3 bytes)
 *   output_len  - pointer to variable that receives actual compressed length
 *
 * Returns:
 *   LZO_E_OK on success, LZO error code on failure
 */
__declspec(dllexport) int __cdecl minilzo_compress(
    const lzo_bytep input,
    lzo_uint input_len,
    lzo_bytep output,
    lzo_uintp output_len)
{
    /* Allocate work memory per call */
    lzo_bytep wrkmem = (lzo_bytep)malloc(LZO1X_1_MEM_COMPRESS);
    if (wrkmem == NULL) {
        return LZO_E_OUT_OF_MEMORY;
    }
    
    int result = lzo1x_1_compress(input, input_len, output, output_len, wrkmem);
    free(wrkmem);
    return result;
}

/* Get required work memory size */
__declspec(dllexport) size_t __cdecl minilzo_work_memory_size(void)
{
    return 16384;
}
