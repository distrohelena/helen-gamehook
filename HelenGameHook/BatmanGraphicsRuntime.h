#pragma once

#include <filesystem>
#include <memory>

namespace helen {
    class FileWriteRoutingService;
}

namespace helen {
    /** @brief Publishes a fully owned direct graphics context before installing its dispatch hook; duplicate binding fails. */
    void InitializeBatmanGraphicsRuntime(const std::filesystem::path& engineIniPath);
    /**
     * @brief Publishes a direct graphics runtime whose config service shares the legacy route owner.
     * @param engineIniPath Absolute or relative path to the Batman user engine INI file.
     * @param routing_service Shared initialized route owner, or null when routing is absent.
     */
    void InitializeBatmanGraphicsRuntime(const std::filesystem::path& engineIniPath,
        std::shared_ptr<FileWriteRoutingService> routing_service);
    /** @brief Retires the published context after hook removal; in-flight calls retain their complete dependency graph. */
    void ResetBatmanGraphicsRuntime();
}

/** @brief Bridges the verified x86 ABI; explicit throwing specification preserves stock exceptions under MSVC /EHsc. */
extern "C" __declspec(dllexport) void __fastcall HelenGraphicsDispatch(
    void* handler, void* unusedEdx, void* movie, const char* name, const void* arguments, unsigned count) noexcept(false);
