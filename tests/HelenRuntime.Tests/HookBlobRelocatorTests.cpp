#include <HelenHook/HookBlobRelocationDefinition.h>
#include <HelenHook/HookBlobRelocationSourceDefinition.h>
#include <HelenHook/HookBlobRelocator.h>
#include <HelenHook/HookDefinition.h>
#include <HelenHook/RuntimeSlotDefinition.h>
#include <HelenHook/RuntimeValueStore.h>

#include <Windows.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <vector>

namespace
{
    /**
     * @brief Throws when one required boolean condition is false so the shared test harness stops at the first failure.
     * @param condition Boolean condition that must evaluate to true.
     * @param message Failure message reported by the shared test runner.
     */
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Reads one encoded 32-bit relocation value from a mutable blob byte buffer.
     * @param blob_bytes Mutable blob byte buffer that already received relocation writes.
     * @param offset Byte offset that contains the encoded 32-bit value.
     * @return Little-endian 32-bit value read from the blob.
     */
    std::uint32_t ReadUInt32(const std::vector<std::uint8_t>& blob_bytes, std::size_t offset)
    {
        std::uint32_t value = 0;
        std::memcpy(&value, blob_bytes.data() + offset, sizeof(value));
        return value;
    }

    /**
     * @brief Builds one float32 runtime slot used by the relocation tests.
     * @return Runtime slot definition for the live subtitle scale slot.
     */
    helen::RuntimeSlotDefinition CreateSubtitleScaleSlot()
    {
        helen::RuntimeSlotDefinition definition;
        definition.Id = "subtitle.scale";
        definition.Type = "float32";
        definition.InitialValue = 1.5;
        return definition;
    }

    /**
     * @brief Builds one relocation definition with the supplied encoding and source kind.
     * @param offset Byte offset inside the blob where the encoded relocation should be written.
     * @param encoding Relocation encoding such as abs32 or rel32.
     * @param source_kind Relocation source kind such as runtime-slot or hook-target.
     * @return Fully populated relocation definition.
     */
    helen::HookBlobRelocationDefinition CreateRelocation(
        std::size_t offset,
        const char* encoding,
        const char* source_kind)
    {
        helen::HookBlobRelocationDefinition relocation;
        relocation.Offset = offset;
        relocation.Encoding = encoding;
        relocation.Source.Kind = source_kind;
        return relocation;
    }

    /**
     * @brief Builds one hook definition that carries the supplied relocation table.
     * @param relocations Relocation table applied by the hook blob relocator.
     * @return Hook definition configured for relocation-only unit tests.
     */
    helen::HookDefinition CreateHook(const std::vector<helen::HookBlobRelocationDefinition>& relocations)
    {
        helen::HookDefinition hook;
        hook.Id = "subtitleHook";
        hook.ResumeOffsetFromTarget = 9;
        hook.Blob.Relocations = relocations;
        return hook;
    }
}

/**
 * @brief Verifies that blob relocations resolve runtime slots, hook addresses, blob offsets, and module exports using the declared encodings.
 */
void RunHookBlobRelocatorTests()
{
    helen::RuntimeValueStore runtime_values;
    Expect(runtime_values.RegisterSlot(CreateSubtitleScaleSlot()), "Failed to register the runtime slot used by the relocation tests.");

    helen::HookBlobRelocationDefinition runtime_slot_relocation = CreateRelocation(0, "abs32", "runtime-slot");
    runtime_slot_relocation.Source.Slot = "subtitle.scale";

    helen::HookBlobRelocationDefinition hook_target_relocation = CreateRelocation(4, "abs32", "hook-target");

    helen::HookBlobRelocationDefinition hook_resume_relocation = CreateRelocation(8, "rel32", "hook-resume");

    helen::HookBlobRelocationDefinition blob_offset_relocation = CreateRelocation(12, "abs32", "blob-offsetof");
    blob_offset_relocation.Source.BlobOffset = 20;

    helen::HookBlobRelocationDefinition export_relocation = CreateRelocation(16, "rel32", "module-export");
    export_relocation.Source.ModuleName = "kernel32.dll";
    export_relocation.Source.ExportName = "GetTickCount";

    const helen::HookDefinition hook = CreateHook({
        runtime_slot_relocation,
        hook_target_relocation,
        hook_resume_relocation,
        blob_offset_relocation,
        export_relocation
    });

    std::vector<std::uint8_t> blob_bytes(20, 0x00);
    std::vector<helen::HookBlobRelocator::AppliedRelocationView> applied_relocations;

    const std::uintptr_t hook_target = 0x10001000;
    const std::uintptr_t blob_base = 0x20002000;
    helen::HookBlobRelocator relocator;
    Expect(relocator.ApplyRelocations(blob_bytes, hook, hook_target, blob_base, runtime_values, &applied_relocations), "Expected the relocation table to apply successfully.");

    const std::optional<const void*> runtime_slot_address = runtime_values.TryGetAddress("subtitle.scale");
    Expect(runtime_slot_address.has_value(), "Runtime slot address unexpectedly disappeared during relocation.");
    Expect(ReadUInt32(blob_bytes, 0) == static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(*runtime_slot_address)), "Runtime-slot relocation encoded the wrong absolute address.");
    Expect(ReadUInt32(blob_bytes, 4) == static_cast<std::uint32_t>(hook_target), "Hook-target relocation encoded the wrong absolute address.");

    const std::uintptr_t hook_resume = hook_target + hook.ResumeOffsetFromTarget;
    const std::int32_t expected_hook_resume_displacement =
        static_cast<std::int32_t>(static_cast<std::int64_t>(hook_resume) - static_cast<std::int64_t>(blob_base + 8 + sizeof(std::int32_t)));
    Expect(ReadUInt32(blob_bytes, 8) == static_cast<std::uint32_t>(expected_hook_resume_displacement), "Hook-resume relocation encoded the wrong relative displacement.");

    Expect(ReadUInt32(blob_bytes, 12) == static_cast<std::uint32_t>(blob_base + 20), "Blob offsetof relocation encoded the wrong absolute address.");

    const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    Expect(kernel32 != nullptr, "Expected kernel32.dll to be loaded for the module-export relocation test.");
    const FARPROC get_tick_count = GetProcAddress(kernel32, "GetTickCount");
    Expect(get_tick_count != nullptr, "Expected GetTickCount to resolve for the module-export relocation test.");
    const std::int32_t expected_export_displacement =
        static_cast<std::int32_t>(
            static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(get_tick_count)) -
            static_cast<std::int64_t>(blob_base + 16 + sizeof(std::int32_t)));
    Expect(ReadUInt32(blob_bytes, 16) == static_cast<std::uint32_t>(expected_export_displacement), "Module-export relocation encoded the wrong relative displacement.");

    Expect(applied_relocations.size() == 5, "Applied relocation debug view count mismatch.");
    Expect(applied_relocations[0].SourceKind == "runtime-slot", "Applied relocation debug source kind mismatch for the runtime slot.");
    Expect(applied_relocations[0].SourceLabel == "subtitle.scale", "Applied relocation debug label mismatch for the runtime slot.");
    Expect(applied_relocations[4].SourceKind == "module-export", "Applied relocation debug source kind mismatch for the module export.");
    Expect(applied_relocations[4].SourceLabel == "kernel32.dll!GetTickCount", "Applied relocation debug label mismatch for the module export.");

    const helen::HookDefinition invalid_offset_hook = CreateHook({ CreateRelocation(17, "abs32", "hook-target") });
    std::vector<std::uint8_t> invalid_offset_blob(20, 0x00);
    Expect(!relocator.ApplyRelocations(invalid_offset_blob, invalid_offset_hook, hook_target, blob_base, runtime_values), "Out-of-range relocation offset unexpectedly succeeded.");

    const helen::HookDefinition invalid_source_hook = CreateHook({ CreateRelocation(0, "abs32", "unsupported-source") });
    std::vector<std::uint8_t> invalid_source_blob(4, 0x00);
    Expect(!relocator.ApplyRelocations(invalid_source_blob, invalid_source_hook, hook_target, blob_base, runtime_values), "Unsupported relocation source unexpectedly succeeded.");

    const helen::HookDefinition invalid_encoding_hook = CreateHook({ CreateRelocation(0, "unsupported-encoding", "hook-target") });
    std::vector<std::uint8_t> invalid_encoding_blob(4, 0x00);
    Expect(!relocator.ApplyRelocations(invalid_encoding_blob, invalid_encoding_hook, hook_target, blob_base, runtime_values), "Unsupported relocation encoding unexpectedly succeeded.");
}
