#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <windows.h>

namespace helen
{
    /**
     * @brief Captures the loaded module metadata needed by the runtime hook and memory helpers.
     */
    struct ModuleView
    {
        /**
         * @brief The Win32 module handle returned by the loader.
         */
        HMODULE handle{};

        /**
         * @brief The full filesystem path to the loaded module.
         */
        std::filesystem::path path;

        /**
         * @brief The leaf filename of the loaded module path.
         */
        std::wstring name;

        /**
         * @brief The module base address used for PE header and RVA calculations.
         */
        std::uintptr_t base_address{};

        /**
         * @brief The in-memory image size reported by the PE headers.
         */
        std::size_t image_size{};
    };

    /**
     * @brief Describes a named PE section resolved from a loaded module.
     */
    struct SectionView
    {
        /**
         * @brief The section name as stored in the PE header.
         */
        std::string name;

        /**
         * @brief The virtual address of the section within process memory.
         */
        std::uintptr_t address{};

        /**
         * @brief The size of the section in memory.
         */
        std::size_t size{};
    };

    /**
     * @brief Queries the loader for module metadata and translates it into a ModuleView.
     * @param module The module handle to inspect.
     * @return A populated module view when the handle is valid; otherwise std::nullopt.
     */
    std::optional<ModuleView> QueryModule(HMODULE module);

    /**
     * @brief Queries the current process main module.
     * @return A populated module view for the main executable when available; otherwise std::nullopt.
     */
    std::optional<ModuleView> QueryMainModule();

    /**
     * @brief Locates a named section within a module.
     * @param module The module to inspect.
     * @param section_name The exact section name to locate.
     * @return A populated section view when the section exists; otherwise std::nullopt.
     */
    std::optional<SectionView> FindSection(const ModuleView& module, std::string_view section_name);

    /**
     * @brief Copies bytes into writable process memory after temporarily enabling write access.
     * @param address The destination memory address.
     * @param data The bytes to copy.
     * @param size The number of bytes to write.
     * @return True when the write succeeds; otherwise false.
     */
    bool WriteMemory(void* address, const void* data, std::size_t size);

    /**
     * @brief Fills a writable memory range with a repeated byte value after temporarily enabling write access.
     * @param address The start of the writable memory range.
     * @param value The byte value to write into the range.
     * @param size The number of bytes to write.
     * @return True when the range was filled successfully; otherwise false.
     */
    bool FillMemoryBytes(void* address, std::uint8_t value, std::size_t size);

    /**
     * @brief Changes the protection of a memory range while preserving the previous protection value.
     * @param address The start of the memory range.
     * @param size The number of bytes covered by the protection change.
     * @param new_protect The new protection flags to apply.
     * @param old_protect Receives the previous protection flags when the call succeeds.
     * @return True when the protection change succeeds; otherwise false.
     */
    bool ProtectMemory(void* address, std::size_t size, DWORD new_protect, DWORD& old_protect);

    /**
     * @brief Allocates executable memory for trampolines and other runtime-generated code.
     * @param size The number of bytes to allocate.
     * @return A pointer to executable memory when allocation succeeds; otherwise nullptr.
     */
    void* AllocateExecutable(std::size_t size);
}