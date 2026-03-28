#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include <HelenHook/HookBlobDefinition.h>

namespace helen
{
    /**
     * @brief Declares one native hook or patch target for a specific build.
     */
    class HookDefinition
    {
    public:
        /** @brief Stable hook identifier used by tooling and diagnostics. */
        std::string Id;
        /** @brief Module name that contains the target bytes or symbol. */
        std::string ModuleName;
        /** @brief Optional section name that narrows pattern resolution. */
        std::string SectionName;
        /** @brief Optional module-relative RVA that resolves the exact hook target without pattern scanning. */
        std::optional<std::uint32_t> RelativeVirtualAddress;
        /** @brief Byte-pattern expression used to resolve the target location. */
        std::string Pattern;
        /** @brief Expected bytes used to validate the resolved target before patching. */
        std::string ExpectedBytes;
        /** @brief Declarative patch action such as inline-jump-to-pack-blob, write-bytes, or nop-range. */
        std::string Action;
        /** @brief Number of original target bytes that the runtime must overwrite when applying the hook. */
        std::size_t OverwriteLength = 0;
        /** @brief Relative byte offset from the target used to resume execution after the blob finishes. */
        std::size_t ResumeOffsetFromTarget = 0;
        /** @brief Blob-backed native patch payload used by inline-jump-to-pack-blob actions. */
        HookBlobDefinition Blob;
    };
}