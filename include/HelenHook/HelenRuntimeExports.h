#pragma once

#include <Windows.h>

/**
 * @brief Initializes the Helen runtime services for the current game process.
 *
 * The runtime uses this entry point to resolve the active `helengamehook` layout, load the
 * matching pack, install declared hooks, and activate runtime services. The function is
 * idempotent and returns success when initialization already completed earlier in the process.
 *
 * @return TRUE when the Helen runtime is ready for use; otherwise FALSE.
 */
extern "C" __declspec(dllimport) BOOL __stdcall HelenInitialize();

/**
 * @brief Requests Helen runtime shutdown for the current process.
 *
 * The current runtime implementation keeps process-lifetime hooks active after successful
 * initialization, so shutdown is intentionally a no-op beyond recording the request.
 */
extern "C" __declspec(dllimport) void __stdcall HelenShutdown();
