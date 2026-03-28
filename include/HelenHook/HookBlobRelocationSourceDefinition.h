#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace helen
{
    /**
     * @brief Declares the source operand used to resolve one native hook blob relocation.
     */
    class HookBlobRelocationSourceDefinition
    {
    public:
        /** @brief Relocation source kind such as runtime-slot. */
        std::string Kind;
        /** @brief Runtime slot identifier referenced by the relocation when the source kind requires one. */
        std::string Slot;
        /** @brief Loaded module name referenced by module-export relocations. */
        std::string ModuleName;
        /** @brief Exported symbol name referenced by module-export relocations. */
        std::string ExportName;
        /** @brief Blob-local byte offset referenced by blob-offsetof relocations. */
        std::optional<std::size_t> BlobOffset;
    };
}