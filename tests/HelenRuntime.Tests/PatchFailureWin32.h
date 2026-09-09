#pragma once
#include <windows.h>

/** @brief Test-only protection boundary forwarding all uninjected calls to Windows. */
BOOL WINAPI PatchTestVirtualProtect(LPVOID address, SIZE_T size, DWORD protection, PDWORD previous);
/** @brief Test-only cache boundary forwarding all uninjected calls to Windows. */
BOOL WINAPI PatchTestFlushInstructionCache(HANDLE process, LPCVOID address, SIZE_T size);
/** @brief Test-only executable-allocation release boundary. */
BOOL WINAPI PatchTestVirtualFree(LPVOID address, SIZE_T size, DWORD type);
#define VirtualProtect PatchTestVirtualProtect
#define FlushInstructionCache PatchTestFlushInstructionCache
#define VirtualFree PatchTestVirtualFree
