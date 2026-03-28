#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <HelenHook/HookDefinition.h>
#include <HelenHook/RuntimeValueStore.h>

namespace helen
{
    /**
     * @brief Applies declared relocations to one mutable native hook blob before installation.
     */
    class HookBlobRelocator
    {
    public:
        /**
         * @brief Captures one resolved relocation written into an installed hook blob for debug reporting.
         */
        class AppliedRelocationView
        {
        public:
            /** @brief Byte offset inside the blob where the relocation value was encoded. */
            std::size_t Offset = 0;
            /** @brief Encoded relocation format such as `abs32` or `rel32`. */
            std::string Encoding;
            /** @brief Declarative relocation source kind such as `runtime-slot` or `module-export`. */
            std::string SourceKind;
            /** @brief Human-readable relocation source detail such as a slot id or export name. */
            std::string SourceLabel;
            /** @brief Absolute address resolved from the relocation source before encoding. */
            std::uintptr_t ResolvedAddress = 0;
        };

        /**
         * @brief Applies every declared relocation to a mutable blob byte buffer.
         * @param blob_bytes Mutable blob payload that should receive the encoded relocation values.
         * @param hook Hook metadata that declares the relocation table and resume offset.
         * @param hook_target Absolute address of the hook target being patched.
         * @param blob_base Absolute address where the mutable blob bytes will execute.
         * @param runtime_values Declared runtime slot storage used to resolve runtime-slot relocations.
         * @param applied_relocations Receives one debug view per applied relocation when provided.
         * @return True when every relocation kind, encoding, and patch-site write is valid; otherwise false.
         */
        bool ApplyRelocations(
            std::vector<std::uint8_t>& blob_bytes,
            const HookDefinition& hook,
            std::uintptr_t hook_target,
            std::uintptr_t blob_base,
            const RuntimeValueStore& runtime_values,
            std::vector<AppliedRelocationView>* applied_relocations = nullptr) const;
    };
}