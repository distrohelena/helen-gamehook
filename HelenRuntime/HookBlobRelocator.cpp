#include <HelenHook/HookBlobRelocator.h>

#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <windows.h>

namespace
{
    /**
     * @brief Size in bytes of every relocation encoding supported by the current runtime.
     */
    constexpr std::size_t EncodedRelocationSize = sizeof(std::uint32_t);

    /**
     * @brief Adds one unsigned offset to one base address while rejecting wraparound.
     * @param base Base address that should receive the supplied offset.
     * @param offset Byte offset that should be added to the base address.
     * @param result Receives the computed address when the addition does not overflow.
     * @return True when the address addition is valid for the current pointer width; otherwise false.
     */
    bool TryAddAddress(std::uintptr_t base, std::size_t offset, std::uintptr_t& result)
    {
        if (offset > (std::numeric_limits<std::uintptr_t>::max)() - base)
        {
            return false;
        }

        result = base + offset;
        return true;
    }

    /**
     * @brief Returns whether one relocation patch site can hold a supported encoded relocation value.
     * @param blob_bytes Mutable blob payload that will receive relocation writes.
     * @param relocation Relocation metadata whose patch-site offset should be validated.
     * @return True when the relocation has enough remaining bytes for one 32-bit encoded value; otherwise false.
     */
    bool CanWriteRelocation(
        const std::vector<std::uint8_t>& blob_bytes,
        const helen::HookBlobRelocationDefinition& relocation)
    {
        return relocation.Offset <= blob_bytes.size() && blob_bytes.size() - relocation.Offset >= EncodedRelocationSize;
    }

    /**
     * @brief Converts one UTF-8 module name into UTF-16 so Win32 loader queries can resolve it.
     * @param text UTF-8 module name declared by one relocation source.
     * @return Converted UTF-16 string when the text is valid and non-empty; otherwise no value.
     */
    std::optional<std::wstring> TryConvertUtf8ToWide(std::string_view text)
    {
        if (text.empty())
        {
            return std::nullopt;
        }

        const int required_length = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0);
        if (required_length <= 0)
        {
            return std::nullopt;
        }

        std::wstring wide_text(static_cast<std::size_t>(required_length), L'\0');
        const int actual_length = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            wide_text.data(),
            required_length);
        if (actual_length != required_length)
        {
            return std::nullopt;
        }

        return wide_text;
    }

    /**
     * @brief Resolves one module-export relocation source into the exported function address.
     * @param relocation Relocation metadata whose module-export source should be resolved.
     * @param absolute_value Receives the exported function address when resolution succeeds.
     * @return True when the module is loaded and the requested export exists; otherwise false.
     */
    bool TryResolveModuleExportAddress(
        const helen::HookBlobRelocationDefinition& relocation,
        std::uintptr_t& absolute_value)
    {
        const std::optional<std::wstring> wide_module_name = TryConvertUtf8ToWide(relocation.Source.ModuleName);
        if (!wide_module_name.has_value())
        {
            return false;
        }

        const HMODULE module_handle = GetModuleHandleW(wide_module_name->c_str());
        if (module_handle == nullptr)
        {
            return false;
        }

        const FARPROC export_address = GetProcAddress(module_handle, relocation.Source.ExportName.c_str());
        if (export_address == nullptr)
        {
            return false;
        }

        absolute_value = reinterpret_cast<std::uintptr_t>(export_address);
        return true;
    }

    /**
     * @brief Resolves one relocation source into an absolute target address.
     * @param relocation Relocation metadata whose source operand should be resolved.
     * @param hook Hook metadata that supplies the resume offset for hook-resume relocations.
     * @param hook_target Absolute address of the patched hook target.
     * @param blob_base Absolute execution address of the mutable blob payload.
     * @param runtime_values Runtime slot storage used to resolve runtime-slot relocations.
     * @param absolute_value Receives the resolved absolute address on success.
     * @return True when the source kind is supported and its operand resolves successfully; otherwise false.
     */
    bool TryResolveSourceAddress(
        const helen::HookBlobRelocationDefinition& relocation,
        const helen::HookDefinition& hook,
        std::uintptr_t hook_target,
        std::uintptr_t blob_base,
        const helen::RuntimeValueStore& runtime_values,
        std::uintptr_t& absolute_value)
    {
        if (relocation.Source.Kind == "runtime-slot")
        {
            const std::optional<const void*> slot_address = runtime_values.TryGetAddress(relocation.Source.Slot);
            if (!slot_address.has_value())
            {
                return false;
            }

            absolute_value = reinterpret_cast<std::uintptr_t>(*slot_address);
            return true;
        }

        if (relocation.Source.Kind == "hook-target")
        {
            absolute_value = hook_target;
            return true;
        }

        if (relocation.Source.Kind == "hook-resume")
        {
            return TryAddAddress(hook_target, hook.ResumeOffsetFromTarget, absolute_value);
        }

        if (relocation.Source.Kind == "module-export")
        {
            return TryResolveModuleExportAddress(relocation, absolute_value);
        }

        if (relocation.Source.Kind == "blob-offsetof")
        {
            if (!relocation.Source.BlobOffset.has_value())
            {
                return false;
            }

            return TryAddAddress(blob_base, *relocation.Source.BlobOffset, absolute_value);
        }

        return false;
    }

    /**
     * @brief Builds one human-readable label for the relocation source that resolved successfully.
     * @param relocation Relocation metadata whose source should be summarized for debug output.
     * @return Human-readable label such as a slot id, export name, or blob offset.
     */
    std::string BuildSourceLabel(const helen::HookBlobRelocationDefinition& relocation)
    {
        if (relocation.Source.Kind == "runtime-slot")
        {
            return relocation.Source.Slot;
        }

        if (relocation.Source.Kind == "module-export")
        {
            return relocation.Source.ModuleName + "!" + relocation.Source.ExportName;
        }

        if (relocation.Source.Kind == "blob-offsetof")
        {
            if (!relocation.Source.BlobOffset.has_value())
            {
                return {};
            }

            return std::to_string(*relocation.Source.BlobOffset);
        }

        return {};
    }

    /**
     * @brief Encodes one resolved absolute relocation value using the requested relocation encoding.
     * @param relocation Relocation metadata that declares the output encoding and patch-site offset.
     * @param absolute_value Absolute target address resolved from the relocation source.
     * @param blob_base Absolute execution address of the mutable blob payload.
     * @param encoded_value Receives the 32-bit encoded relocation value on success.
     * @return True when the encoding is supported and the resulting value fits the encoded width; otherwise false.
     */
    bool TryEncodeRelocationValue(
        const helen::HookBlobRelocationDefinition& relocation,
        std::uintptr_t absolute_value,
        std::uintptr_t blob_base,
        std::uint32_t& encoded_value)
    {
        if (relocation.Encoding == "abs32")
        {
            if (absolute_value > (std::numeric_limits<std::uint32_t>::max)())
            {
                return false;
            }

            encoded_value = static_cast<std::uint32_t>(absolute_value);
            return true;
        }

        if (relocation.Encoding == "rel32")
        {
            std::uintptr_t patch_address = 0;
            if (!TryAddAddress(blob_base, relocation.Offset, patch_address))
            {
                return false;
            }

            std::uintptr_t next_instruction_address = 0;
            if (!TryAddAddress(patch_address, EncodedRelocationSize, next_instruction_address))
            {
                return false;
            }

            const std::int64_t displacement = static_cast<std::int64_t>(absolute_value) - static_cast<std::int64_t>(next_instruction_address);
            if (displacement < static_cast<std::int64_t>((std::numeric_limits<std::int32_t>::min)()) ||
                displacement > static_cast<std::int64_t>((std::numeric_limits<std::int32_t>::max)()))
            {
                return false;
            }

            encoded_value = static_cast<std::uint32_t>(static_cast<std::int32_t>(displacement));
            return true;
        }

        return false;
    }
}

namespace helen
{
    bool HookBlobRelocator::ApplyRelocations(
        std::vector<std::uint8_t>& blob_bytes,
        const HookDefinition& hook,
        std::uintptr_t hook_target,
        std::uintptr_t blob_base,
        const RuntimeValueStore& runtime_values,
        std::vector<AppliedRelocationView>* applied_relocations) const
    {
        if (applied_relocations != nullptr)
        {
            applied_relocations->clear();
            applied_relocations->reserve(hook.Blob.Relocations.size());
        }

        for (const HookBlobRelocationDefinition& relocation : hook.Blob.Relocations)
        {
            if (!CanWriteRelocation(blob_bytes, relocation))
            {
                return false;
            }

            std::uintptr_t absolute_value = 0;
            if (!TryResolveSourceAddress(relocation, hook, hook_target, blob_base, runtime_values, absolute_value))
            {
                return false;
            }

            std::uint32_t encoded_value = 0;
            if (!TryEncodeRelocationValue(relocation, absolute_value, blob_base, encoded_value))
            {
                return false;
            }

            std::memcpy(blob_bytes.data() + relocation.Offset, &encoded_value, EncodedRelocationSize);

            if (applied_relocations != nullptr)
            {
                AppliedRelocationView applied_relocation;
                applied_relocation.Offset = relocation.Offset;
                applied_relocation.Encoding = relocation.Encoding;
                applied_relocation.SourceKind = relocation.Source.Kind;
                applied_relocation.SourceLabel = BuildSourceLabel(relocation);
                applied_relocation.ResolvedAddress = absolute_value;
                applied_relocations->push_back(applied_relocation);
            }
        }

        return true;
    }
}