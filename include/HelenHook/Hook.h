#pragma once

#include <HelenHook/InlineHook.h>
#include <HelenHook/IatHook.h>

namespace helen
{
    /** @brief Finds a named PE import slot; returns null when the module does not import it. */
    void** FindImportAddress(const ModuleView& module, std::string_view imported_dll, std::string_view imported_name);
}
