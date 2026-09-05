#pragma once

#include <filesystem>

namespace helen {
    /** @brief Publishes a fully owned direct graphics context before installing its dispatch hook; duplicate binding fails. */
    void InitializeBatmanGraphicsRuntime(const std::filesystem::path& engineIniPath);
    /** @brief Retires the published context after hook removal; in-flight calls retain their complete dependency graph. */
    void ResetBatmanGraphicsRuntime();
}

/** @brief Bridges the verified x86 ABI; explicit throwing specification preserves stock exceptions under MSVC /EHsc. */
extern "C" __declspec(dllexport) void __fastcall HelenGraphicsDispatch(
    void* handler, void* unusedEdx, void* movie, const char* name, const void* arguments, unsigned count) noexcept(false);
