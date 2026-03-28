#include <HelenHook/BuildHookInstaller.h>
#include <HelenHook/DebugHookCallbacks.h>
#include <HelenHook/Pattern.h>

#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    /**
     * @brief Declarative action name for blob-backed inline detours supported by the runtime installer.
     */
    constexpr std::string_view InlineJumpToPackBlobAction = "inline-jump-to-pack-blob";

    /**
     * @brief Converts one hexadecimal digit into its numeric value.
     * @param character ASCII hexadecimal digit to decode.
     * @return Numeric nibble value when the character is hexadecimal; otherwise no value.
     */
    std::optional<std::uint8_t> TryParseHexDigit(char character)
    {
        if (character >= '0' && character <= '9')
        {
            return static_cast<std::uint8_t>(character - '0');
        }

        const char upper_character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
        if (upper_character >= 'A' && upper_character <= 'F')
        {
            return static_cast<std::uint8_t>(10 + (upper_character - 'A'));
        }

        return std::nullopt;
    }

    /**
     * @brief Parses compact or whitespace-delimited hexadecimal bytes used by expected-bytes validation.
     * @param text Expected-bytes text declared by pack metadata.
     * @return Parsed byte vector when every non-whitespace character is valid hexadecimal and the digit count is even; otherwise no value.
     */
    std::optional<std::vector<std::uint8_t>> TryParseExpectedBytes(std::string_view text)
    {
        std::vector<std::uint8_t> bytes;
        std::optional<std::uint8_t> high_nibble;

        for (const char character : text)
        {
            if (std::isspace(static_cast<unsigned char>(character)))
            {
                continue;
            }

            const std::optional<std::uint8_t> nibble = TryParseHexDigit(character);
            if (!nibble.has_value())
            {
                return std::nullopt;
            }

            if (!high_nibble.has_value())
            {
                high_nibble = *nibble;
                continue;
            }

            bytes.push_back(static_cast<std::uint8_t>((*high_nibble << 4) | *nibble));
            high_nibble.reset();
        }

        if (high_nibble.has_value())
        {
            return std::nullopt;
        }

        return bytes;
    }

    /**
     * @brief Converts one UTF-8 module name from pack metadata into UTF-16 for Win32 loader queries.
     * @param text Module filename declared by one hook definition.
     * @return UTF-16 text when conversion succeeds; otherwise no value.
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
     * @brief Resolves one declarative hook module name into a loaded module view.
     * @param module_name Module filename declared by one hook definition.
     * @return Loaded module view when the named module is present in the process; otherwise no value.
     */
    std::optional<helen::ModuleView> TryResolveModule(std::string_view module_name)
    {
        const std::optional<std::wstring> wide_module_name = TryConvertUtf8ToWide(module_name);
        if (!wide_module_name.has_value())
        {
            return std::nullopt;
        }

        const HMODULE module_handle = GetModuleHandleW(wide_module_name->c_str());
        if (module_handle == nullptr)
        {
            return std::nullopt;
        }

        return helen::QueryModule(module_handle);
    }

    /**
     * @brief Returns the section name that should bound one hook pattern search.
     * @param hook Hook metadata that may declare an explicit section override.
     * @return Requested section name or the default `.text` section when the hook left it empty.
     */
    std::string_view GetSearchSectionName(const helen::HookDefinition& hook)
    {
        if (!hook.SectionName.empty())
        {
            return hook.SectionName;
        }

        return ".text";
    }

    /**
     * @brief Creates one validation range that covers the full loaded image of one module.
     * @param module Loaded module whose in-memory image bounds should be returned.
     * @return Synthetic section view spanning the full module image.
     */
    helen::SectionView CreateModuleImageView(const helen::ModuleView& module)
    {
        helen::SectionView image_view;
        image_view.name = "<module>";
        image_view.address = module.base_address;
        image_view.size = module.image_size;
        return image_view;
    }

    /**
     * @brief Resolves one exact module RVA target and returns the range that should bound expected-byte validation.
     * @param hook Hook metadata that declares an exact module RVA target.
     * @param module Loaded module that should contain the requested RVA.
     * @param target_address Receives the absolute target address when the RVA resolves successfully.
     * @param validation_range Receives either the declared containing section or the full module image.
     * @return True when the RVA resolves inside the requested range; otherwise false.
     */
    bool TryResolveTargetAddressFromRva(
        const helen::HookDefinition& hook,
        const helen::ModuleView& module,
        std::uintptr_t& target_address,
        helen::SectionView& validation_range)
    {
        if (!hook.RelativeVirtualAddress.has_value())
        {
            return false;
        }

        const std::uintptr_t relative_virtual_address = *hook.RelativeVirtualAddress;
        if (relative_virtual_address > (std::numeric_limits<std::uintptr_t>::max)() - module.base_address)
        {
            return false;
        }

        target_address = module.base_address + relative_virtual_address;
        validation_range = CreateModuleImageView(module);

        if (!hook.SectionName.empty())
        {
            const std::optional<helen::SectionView> resolved_section = helen::FindSection(module, hook.SectionName);
            if (!resolved_section.has_value())
            {
                return false;
            }

            if (target_address < resolved_section->address)
            {
                return false;
            }

            const std::size_t offset_within_section = target_address - resolved_section->address;
            if (offset_within_section >= resolved_section->size)
            {
                return false;
            }

            validation_range = *resolved_section;
        }

        return true;
    }

    /**
     * @brief Resolves one hook pattern inside the requested section or the default `.text` section and returns the scanned section bounds.
     * @param hook Hook metadata that declares the target pattern and optional section.
     * @param module Loaded module that should be searched.
     * @param target_address Receives the absolute target address when the pattern resolves successfully.
     * @param search_section Receives the section that bounded the pattern search.
     * @return True when the target address and section bounds resolve successfully; otherwise false.
     */
    bool TryResolveTargetAddress(
        const helen::HookDefinition& hook,
        const helen::ModuleView& module,
        std::uintptr_t& target_address,
        helen::SectionView& validation_range)
    {
        if (hook.RelativeVirtualAddress.has_value())
        {
            return TryResolveTargetAddressFromRva(hook, module, target_address, validation_range);
        }

        const std::string_view section_name = GetSearchSectionName(hook);
        const std::optional<helen::SectionView> resolved_section = helen::FindSection(module, section_name);
        if (!resolved_section.has_value())
        {
            return false;
        }

        const std::optional<std::uintptr_t> resolved_target_address = helen::FindPattern(module, hook.Pattern, section_name);
        if (!resolved_target_address.has_value())
        {
            return false;
        }

        target_address = *resolved_target_address;
        validation_range = *resolved_section;
        return true;
    }

    /**
     * @brief Validates the resolved hook target bytes against the declarative expected-bytes text when it is provided.
     * @param hook Hook metadata that may declare expected bytes.
     * @param target_address Absolute resolved target address.
     * @param search_section Section that bounded the target pattern search and therefore bounds valid expected-byte reads.
     * @return True when expected bytes are absent or match exactly; otherwise false.
     */
    bool ValidateExpectedBytes(
        const helen::HookDefinition& hook,
        std::uintptr_t target_address,
        const helen::SectionView& search_section)
    {
        if (hook.ExpectedBytes.empty())
        {
            return true;
        }

        const std::optional<std::vector<std::uint8_t>> expected_bytes = TryParseExpectedBytes(hook.ExpectedBytes);
        if (!expected_bytes.has_value() || expected_bytes->empty())
        {
            return false;
        }

        if (target_address < search_section.address)
        {
            return false;
        }

        const std::size_t offset_within_section = target_address - search_section.address;
        if (offset_within_section > search_section.size || search_section.size - offset_within_section < expected_bytes->size())
        {
            return false;
        }

        return std::memcmp(
            reinterpret_cast<const void*>(target_address),
            expected_bytes->data(),
            expected_bytes->size()) == 0;
    }

    /**
     * @brief Loads one blob asset file from disk into memory.
     * @param path Normalized filesystem path to the blob asset.
     * @param bytes Receives the complete file contents on success.
     * @return True when the file opens and the full payload is read; otherwise false.
     */
    bool TryReadBlobBytes(const std::filesystem::path& path, std::vector<std::uint8_t>& bytes)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            return false;
        }

        stream.seekg(0, std::ios::end);
        const std::streamoff file_size = stream.tellg();
        if (file_size < 0)
        {
            return false;
        }

        if (file_size == 0)
        {
            bytes.clear();
            return true;
        }

        if (file_size > (std::numeric_limits<std::streamsize>::max)())
        {
            return false;
        }

        stream.seekg(0, std::ios::beg);
        bytes.assign(static_cast<std::size_t>(file_size), 0);
        const std::streamsize requested_bytes = static_cast<std::streamsize>(bytes.size());
        stream.read(reinterpret_cast<char*>(bytes.data()), requested_bytes);

        return stream.gcount() == requested_bytes && !stream.bad();
    }

    /**
     * @brief Writes one 32-bit unsigned integer into a mutable byte buffer at the supplied offset.
     * @param buffer Mutable byte buffer that should receive the encoded integer.
     * @param offset Byte offset where the integer should be written.
     * @param value Value that should be encoded in little-endian order.
     */
    void WriteUInt32(std::vector<std::uint8_t>& buffer, std::size_t offset, std::uint32_t value)
    {
        std::memcpy(buffer.data() + offset, &value, sizeof(value));
    }

    /**
     * @brief Encodes one relative 32-bit branch operand from the next instruction address to the target address.
     * @param next_instruction_address Absolute address of the instruction immediately following the operand.
     * @param target_address Absolute branch target address.
     * @param encoded_displacement Receives the signed 32-bit displacement on success.
     * @return True when the displacement fits the 32-bit relative branch range; otherwise false.
     */
    bool TryEncodeRelative32(
        std::uintptr_t next_instruction_address,
        std::uintptr_t target_address,
        std::int32_t& encoded_displacement)
    {
        const std::int64_t displacement =
            static_cast<std::int64_t>(target_address) - static_cast<std::int64_t>(next_instruction_address);
        if (displacement < static_cast<std::int64_t>((std::numeric_limits<std::int32_t>::min)()) ||
            displacement > static_cast<std::int64_t>((std::numeric_limits<std::int32_t>::max)()))
        {
            return false;
        }

        encoded_displacement = static_cast<std::int32_t>(displacement);
        return true;
    }

    /**
     * @brief Builds one small x86 entry probe that records a hook hit before jumping into the relocated blob payload.
     * @param hook_id_address Absolute address of the null-terminated hook id string stored in the same allocation.
     * @param callback_address Absolute address of the runtime hook-hit callback.
     * @param probe_address Absolute execution address where the probe bytes will be placed.
     * @param blob_entry_address Absolute execution address where the relocated blob should run after the probe.
     * @param probe_bytes Receives the complete probe byte sequence on success.
     * @return True when both the callback call and blob jump fit the x86 relative branch range; otherwise false.
     */
    bool TryBuildHookEntryProbe(
        std::uintptr_t hook_id_address,
        std::uintptr_t callback_address,
        std::uintptr_t probe_address,
        std::uintptr_t blob_entry_address,
        std::vector<std::uint8_t>& probe_bytes)
    {
        probe_bytes = {
            0x9C,
            0x60,
            0x68, 0x00, 0x00, 0x00, 0x00,
            0xE8, 0x00, 0x00, 0x00, 0x00,
            0x61,
            0x9D,
            0xE9, 0x00, 0x00, 0x00, 0x00
        };

        if (hook_id_address > static_cast<std::uintptr_t>((std::numeric_limits<std::uint32_t>::max)()))
        {
            return false;
        }

        WriteUInt32(probe_bytes, 3, static_cast<std::uint32_t>(hook_id_address));

        std::int32_t callback_displacement = 0;
        if (!TryEncodeRelative32(probe_address + 12, callback_address, callback_displacement))
        {
            return false;
        }

        WriteUInt32(probe_bytes, 8, static_cast<std::uint32_t>(callback_displacement));

        std::int32_t blob_displacement = 0;
        if (!TryEncodeRelative32(probe_address + probe_bytes.size(), blob_entry_address, blob_displacement))
        {
            return false;
        }

        WriteUInt32(probe_bytes, 15, static_cast<std::uint32_t>(blob_displacement));
        return true;
    }

    /**
     * @brief Adds one unsigned byte count to one base address while rejecting wraparound.
     * @param base_address Absolute base address that should receive the supplied byte count.
     * @param byte_count Byte count that should be added to the base address.
     * @param result Receives the summed address when no overflow occurs.
     * @return True when the address addition stays within the current pointer width; otherwise false.
     */
    bool TryAddAddress(std::uintptr_t base_address, std::size_t byte_count, std::uintptr_t& result)
    {
        if (byte_count > (std::numeric_limits<std::uintptr_t>::max)() - base_address)
        {
            return false;
        }

        result = base_address + byte_count;
        return true;
    }
}

namespace helen
{
    BuildHookInstaller::BuildHookInstaller(const PackAssetResolver& asset_resolver)
        : asset_resolver_(asset_resolver)
    {
    }

    BuildHookInstaller::~BuildHookInstaller()
    {
        Remove();
    }

    bool BuildHookInstaller::Install(const std::vector<HookDefinition>& hooks, const RuntimeValueStore& runtime_values)
    {
        if (!installed_hooks_.empty() || !executable_blobs_.empty() || !installed_hook_views_.empty())
        {
            return false;
        }

        for (const HookDefinition& hook : hooks)
        {
            if (hook.Action != InlineJumpToPackBlobAction)
            {
                Remove();
                return false;
            }

            const std::optional<ModuleView> module = TryResolveModule(hook.ModuleName);
            if (!module.has_value())
            {
                Remove();
                return false;
            }

            std::uintptr_t target_address = 0;
            SectionView search_section;
            if (!TryResolveTargetAddress(hook, *module, target_address, search_section))
            {
                Remove();
                return false;
            }

            if (!ValidateExpectedBytes(hook, target_address, search_section))
            {
                Remove();
                return false;
            }

            const std::optional<std::filesystem::path> blob_path = asset_resolver_.Resolve(hook.Blob.AssetPath);
            if (!blob_path.has_value())
            {
                Remove();
                return false;
            }

            std::vector<std::uint8_t> blob_bytes;
            if (!TryReadBlobBytes(*blob_path, blob_bytes))
            {
                Remove();
                return false;
            }

            if (hook.Blob.EntryOffset >= blob_bytes.size())
            {
                Remove();
                return false;
            }

            const std::string hook_id = hook.Id.empty() ? std::string("<unnamed-hook>") : hook.Id;
            const std::size_t hook_id_bytes = hook_id.size() + 1;
            std::vector<std::uint8_t> entry_probe_bytes;

            const std::size_t probe_size = 19;
            const std::size_t allocation_size = probe_size + blob_bytes.size() + hook_id_bytes;
            void* const executable_blob = AllocateExecutable(allocation_size);
            if (executable_blob == nullptr)
            {
                Remove();
                return false;
            }

            const std::uintptr_t probe_address = reinterpret_cast<std::uintptr_t>(executable_blob);
            std::uintptr_t blob_address = 0;
            if (!TryAddAddress(probe_address, probe_size, blob_address))
            {
                VirtualFree(executable_blob, 0, MEM_RELEASE);
                Remove();
                return false;
            }

            std::uintptr_t blob_entry_address = 0;
            if (!TryAddAddress(blob_address, hook.Blob.EntryOffset, blob_entry_address))
            {
                VirtualFree(executable_blob, 0, MEM_RELEASE);
                Remove();
                return false;
            }

            std::uintptr_t hook_id_address = 0;
            if (!TryAddAddress(blob_address, blob_bytes.size(), hook_id_address))
            {
                VirtualFree(executable_blob, 0, MEM_RELEASE);
                Remove();
                return false;
            }

            std::vector<HookBlobRelocator::AppliedRelocationView> applied_relocations;
            if (!relocator_.ApplyRelocations(
                    blob_bytes,
                    hook,
                    target_address,
                    blob_address,
                    runtime_values,
                    &applied_relocations))
            {
                VirtualFree(executable_blob, 0, MEM_RELEASE);
                Remove();
                return false;
            }

            if (!TryBuildHookEntryProbe(
                    hook_id_address,
                    reinterpret_cast<std::uintptr_t>(&RecordRuntimeHookHit),
                    probe_address,
                    blob_entry_address,
                    entry_probe_bytes))
            {
                VirtualFree(executable_blob, 0, MEM_RELEASE);
                Remove();
                return false;
            }

            std::vector<std::uint8_t> allocation_bytes;
            allocation_bytes.reserve(allocation_size);
            allocation_bytes.insert(allocation_bytes.end(), entry_probe_bytes.begin(), entry_probe_bytes.end());
            allocation_bytes.insert(allocation_bytes.end(), blob_bytes.begin(), blob_bytes.end());
            allocation_bytes.insert(allocation_bytes.end(), hook_id.begin(), hook_id.end());
            allocation_bytes.push_back('\0');

            if (!WriteMemory(executable_blob, allocation_bytes.data(), allocation_bytes.size()))
            {
                VirtualFree(executable_blob, 0, MEM_RELEASE);
                Remove();
                return false;
            }

            std::unique_ptr<InlineHook> inline_hook = std::make_unique<InlineHook>();
            if (!inline_hook->Install(
                    reinterpret_cast<void*>(target_address),
                    reinterpret_cast<void*>(probe_address),
                    hook.OverwriteLength))
            {
                VirtualFree(executable_blob, 0, MEM_RELEASE);
                Remove();
                return false;
            }

            InstalledHookDebugView installed_hook_view;
            installed_hook_view.Id = hook.Id;
            installed_hook_view.TargetAddress = target_address;
            installed_hook_view.BlobAddress = blob_address;
            installed_hook_view.EntryAddress = probe_address;
            installed_hook_view.ResumeAddress = target_address + hook.ResumeOffsetFromTarget;
            installed_hook_view.Relocations = std::move(applied_relocations);

            executable_blobs_.push_back(executable_blob);
            installed_hooks_.push_back(std::move(inline_hook));
            installed_hook_views_.push_back(std::move(installed_hook_view));
        }

        return true;
    }

    void BuildHookInstaller::Remove()
    {
        for (auto hook_iterator = installed_hooks_.rbegin(); hook_iterator != installed_hooks_.rend(); ++hook_iterator)
        {
            (*hook_iterator)->Remove();
        }

        installed_hooks_.clear();
        installed_hook_views_.clear();

        for (auto blob_iterator = executable_blobs_.rbegin(); blob_iterator != executable_blobs_.rend(); ++blob_iterator)
        {
            if (*blob_iterator != nullptr)
            {
                VirtualFree(*blob_iterator, 0, MEM_RELEASE);
            }
        }

        executable_blobs_.clear();
    }

    const std::vector<BuildHookInstaller::InstalledHookDebugView>& BuildHookInstaller::GetInstalledHooks() const noexcept
    {
        return installed_hook_views_;
    }
}