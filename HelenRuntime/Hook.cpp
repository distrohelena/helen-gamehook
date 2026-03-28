#include <HelenHook/Hook.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <limits>
#include <vector>

#include <winnt.h>

namespace
{
    /**
     * @brief Size of one x86 near jump instruction used by the inline hook helper.
     */
    constexpr std::size_t NearJumpSize = 5;

    /**
     * @brief Compares two ASCII strings case-insensitively.
     * @param left First string to compare.
     * @param right Second string to compare.
     * @return True when the strings are equal ignoring ASCII case; otherwise false.
     */
    bool EqualsAsciiIgnoreCase(std::string_view left, std::string_view right)
    {
        if (left.size() != right.size())
        {
            return false;
        }

        for (std::size_t index = 0; index < left.size(); ++index)
        {
            const unsigned char left_character = static_cast<unsigned char>(left[index]);
            const unsigned char right_character = static_cast<unsigned char>(right[index]);
            if (std::tolower(left_character) != std::tolower(right_character))
            {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Returns the PE import descriptor array for one loaded module when it has imports.
     * @param module Loaded module that should be inspected.
     * @return Pointer to the first import descriptor when imports exist; otherwise nullptr.
     */
    const IMAGE_IMPORT_DESCRIPTOR* QueryImportDescriptors(const helen::ModuleView& module)
    {
        const auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(module.base_address);
        if (dos_header == nullptr || dos_header->e_magic != IMAGE_DOS_SIGNATURE)
        {
            return nullptr;
        }

        const auto* nt_headers =
            reinterpret_cast<const IMAGE_NT_HEADERS32*>(module.base_address + static_cast<std::uintptr_t>(dos_header->e_lfanew));
        if (nt_headers == nullptr || nt_headers->Signature != IMAGE_NT_SIGNATURE)
        {
            return nullptr;
        }

        const IMAGE_DATA_DIRECTORY& import_directory =
            nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (import_directory.VirtualAddress == 0 || import_directory.Size == 0)
        {
            return nullptr;
        }

        return reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(module.base_address + import_directory.VirtualAddress);
    }

    /**
     * @brief Writes one x86 near jump at the supplied address.
     * @param patch_address Address that should receive the near jump opcode and displacement.
     * @param target_address Absolute jump destination.
     * @param bytes Receives the encoded near jump bytes on success.
     * @return True when the signed relative displacement fits inside a 32-bit near jump; otherwise false.
     */
    bool TryBuildNearJump(std::uintptr_t patch_address, std::uintptr_t target_address, std::array<std::uint8_t, NearJumpSize>& bytes)
    {
        const std::int64_t displacement =
            static_cast<std::int64_t>(target_address) - static_cast<std::int64_t>(patch_address + NearJumpSize);
        if (displacement < static_cast<std::int64_t>((std::numeric_limits<std::int32_t>::min)()) ||
            displacement > static_cast<std::int64_t>((std::numeric_limits<std::int32_t>::max)()))
        {
            return false;
        }

        bytes[0] = 0xE9;
        const std::int32_t encoded_displacement = static_cast<std::int32_t>(displacement);
        std::memcpy(bytes.data() + 1, &encoded_displacement, sizeof(encoded_displacement));
        return true;
    }
}

namespace helen
{
    /**
     * @brief Locates one imported function slot inside the module import address table.
     * @param module Loaded module whose imports should be examined.
     * @param imported_dll Imported DLL name that owns the requested symbol.
     * @param imported_name Imported function name to resolve.
     * @return Address of the writable IAT slot when the import exists; otherwise nullptr.
     */
    void** FindImportAddress(const ModuleView& module, std::string_view imported_dll, std::string_view imported_name)
    {
        const IMAGE_IMPORT_DESCRIPTOR* descriptor = QueryImportDescriptors(module);
        if (descriptor == nullptr)
        {
            return nullptr;
        }

        for (; descriptor->Name != 0; ++descriptor)
        {
            const auto* dll_name = reinterpret_cast<const char*>(module.base_address + descriptor->Name);
            if (dll_name == nullptr || !EqualsAsciiIgnoreCase(dll_name, imported_dll))
            {
                continue;
            }

            auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA32*>(module.base_address + descriptor->FirstThunk);
            auto* original_thunk = descriptor->OriginalFirstThunk != 0
                ? reinterpret_cast<IMAGE_THUNK_DATA32*>(module.base_address + descriptor->OriginalFirstThunk)
                : thunk;

            while (thunk != nullptr && original_thunk != nullptr && original_thunk->u1.AddressOfData != 0)
            {
                if ((original_thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32) == 0)
                {
                    const auto* import_by_name =
                        reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(module.base_address + original_thunk->u1.AddressOfData);
                    if (import_by_name != nullptr && std::string_view(reinterpret_cast<const char*>(import_by_name->Name)) == imported_name)
                    {
                        return reinterpret_cast<void**>(&thunk->u1.Function);
                    }
                }

                ++thunk;
                ++original_thunk;
            }
        }

        return nullptr;
    }

    /**
     * @brief Removes the installed inline detour and releases the trampoline allocation.
     */
    InlineHook::~InlineHook()
    {
        Remove();
    }

    /**
     * @brief Installs one x86 near-jump detour and preserves the overwritten bytes in a trampoline.
     * @param target Address of the function bytes that should be overwritten.
     * @param detour Address that should receive execution after patching.
     * @param patch_size Number of original bytes that the installer may overwrite.
     * @return True when the hook installs successfully; otherwise false.
     */
    bool InlineHook::Install(void* target, void* detour, std::size_t patch_size)
    {
        if (target == nullptr || detour == nullptr || IsInstalled())
        {
            return false;
        }

        if (patch_size < NearJumpSize || patch_size > sizeof(original_bytes_))
        {
            return false;
        }

        void* const trampoline = AllocateExecutable(patch_size + NearJumpSize);
        if (trampoline == nullptr)
        {
            return false;
        }

        std::memcpy(original_bytes_, target, patch_size);

        std::vector<std::uint8_t> trampoline_bytes(patch_size + NearJumpSize, 0x90);
        std::memcpy(trampoline_bytes.data(), target, patch_size);

        std::array<std::uint8_t, NearJumpSize> trampoline_jump_bytes{};
        if (!TryBuildNearJump(
                reinterpret_cast<std::uintptr_t>(trampoline) + patch_size,
                reinterpret_cast<std::uintptr_t>(target) + patch_size,
                trampoline_jump_bytes))
        {
            VirtualFree(trampoline, 0, MEM_RELEASE);
            return false;
        }

        std::memcpy(trampoline_bytes.data() + patch_size, trampoline_jump_bytes.data(), trampoline_jump_bytes.size());
        if (!WriteMemory(trampoline, trampoline_bytes.data(), trampoline_bytes.size()))
        {
            VirtualFree(trampoline, 0, MEM_RELEASE);
            return false;
        }

        std::vector<std::uint8_t> patch_bytes(patch_size, 0x90);
        std::array<std::uint8_t, NearJumpSize> patch_jump_bytes{};
        if (!TryBuildNearJump(
                reinterpret_cast<std::uintptr_t>(target),
                reinterpret_cast<std::uintptr_t>(detour),
                patch_jump_bytes))
        {
            VirtualFree(trampoline, 0, MEM_RELEASE);
            return false;
        }

        std::memcpy(patch_bytes.data(), patch_jump_bytes.data(), patch_jump_bytes.size());
        if (!WriteMemory(target, patch_bytes.data(), patch_bytes.size()))
        {
            VirtualFree(trampoline, 0, MEM_RELEASE);
            return false;
        }

        target_ = target;
        detour_ = detour;
        trampoline_ = trampoline;
        patch_size_ = patch_size;
        return true;
    }

    /**
     * @brief Restores the original target bytes and frees the trampoline allocation when a hook is installed.
     */
    void InlineHook::Remove()
    {
        if (!IsInstalled())
        {
            return;
        }

        WriteMemory(target_, original_bytes_, patch_size_);
        VirtualFree(trampoline_, 0, MEM_RELEASE);

        target_ = nullptr;
        detour_ = nullptr;
        trampoline_ = nullptr;
        patch_size_ = 0;
        std::fill(std::begin(original_bytes_), std::end(original_bytes_), 0);
    }

    /**
     * @brief Returns whether this instance currently owns an installed inline hook.
     * @return True when a trampoline and patched target are active; otherwise false.
     */
    bool InlineHook::IsInstalled() const noexcept
    {
        return target_ != nullptr && trampoline_ != nullptr && patch_size_ != 0;
    }

    /**
     * @brief Restores the original import address table slot when one is currently patched.
     */
    IatHook::~IatHook()
    {
        Remove();
    }

    /**
     * @brief Installs one import-address-table hook inside the supplied module.
     * @param module Loaded module whose imported function slot should be replaced.
     * @param imported_dll Imported DLL name that owns the function to replace.
     * @param imported_name Imported function name to replace.
     * @param replacement Replacement function pointer written into the IAT slot.
     * @return True when the IAT slot resolves and the replacement is written successfully; otherwise false.
     */
    bool IatHook::Install(const ModuleView& module, std::string_view imported_dll, std::string_view imported_name, void* replacement)
    {
        if (replacement == nullptr || IsInstalled())
        {
            return false;
        }

        void** const slot = FindImportAddress(module, imported_dll, imported_name);
        if (slot == nullptr)
        {
            return false;
        }

        original_ = *slot;
        if (!WriteMemory(slot, &replacement, sizeof(replacement)))
        {
            original_ = nullptr;
            return false;
        }

        slot_ = slot;
        return true;
    }

    /**
     * @brief Restores the original imported function pointer when one is currently patched.
     */
    void IatHook::Remove()
    {
        if (!IsInstalled())
        {
            return;
        }

        WriteMemory(slot_, &original_, sizeof(original_));
        slot_ = nullptr;
        original_ = nullptr;
    }

    /**
     * @brief Returns whether this instance currently owns an installed IAT patch.
     * @return True when the original slot pointer is stored and the replacement is active; otherwise false.
     */
    bool IatHook::IsInstalled() const noexcept
    {
        return slot_ != nullptr;
    }
}
