#pragma once

#include "BatmanGraphicsExternalInterface.h"
#include <HelenHook/BatmanGraphicsConfigService.h>
#include <HelenHook/BatmanDisplayModeService.h>

namespace helen {
    /** @brief Owns every direct callback dependency together so retirement cannot invalidate an in-flight session. */
    class BatmanGraphicsRuntimeContext {
    private:
        /** @brief Dedicated catalog capture service, never shared with legacy observer scratch state. */
        BatmanDisplayModeService Display;
        /** @brief Existing persistence implementation bound to the required engine INI anchor. */
        BatmanGraphicsConfigService Config;
        /** @brief Serialized session and transaction owner, destroyed before its required services. */
        BatmanGraphicsSessionService Sessions;
        /** @brief Exact-name primitive adapter destroyed before session ownership. */
        BatmanGraphicsExternalInterface Adapter;
    public:
        /** @brief Constructs references in dependency order without reading files or installing hooks. */
        explicit BatmanGraphicsRuntimeContext(const std::filesystem::path& engineIniPath);
        /** @brief Handles an owned call while the caller retains shared ownership of this entire context. */
        void Handle(const char* name, const void* arguments, unsigned count, BatmanGraphicsPrimitiveResult& result);
    };
}
