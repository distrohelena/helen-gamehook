#pragma once

#include "BatmanGraphicsExternalInterface.h"
#include <HelenHook/BatmanGraphicsConfigService.h>
#include <HelenHook/BatmanDisplayModeService.h>
#include <HelenHook/FileWriteRoutingService.h>
#include <memory>
#if defined(HELEN_ENABLE_SESSION_LIVE)
#include "SessionGraphicsNative.h"
#include "SessionGraphicsBackend.h"
#endif

namespace helen {
    /** @brief Owns every direct callback dependency together so retirement cannot invalidate an in-flight session. */
    class BatmanGraphicsRuntimeContext {
    private:
        /** @brief Dedicated catalog capture service, never shared with legacy observer scratch state. */
        BatmanDisplayModeService Display;
        /** @brief Existing persistence implementation bound to the required engine INI anchor. */
        BatmanGraphicsConfigService Config;
#if defined(HELEN_ENABLE_SESSION_LIVE)
        /** @brief Pinned native/session route dependency initialized before the live backend and callbacks. */
        SessionGraphicsNative Native;
        /** @brief Session-only applied-state owner; the normal persistent backend remains available outside this candidate. */
        SessionGraphicsBackend Live;
#endif
        /** @brief Serialized session and transaction owner, destroyed before its required services. */
        BatmanGraphicsSessionService Sessions;
        /** @brief Exact-name primitive adapter destroyed before session ownership. */
        BatmanGraphicsExternalInterface Adapter;
    public:
        /** @brief Constructs references in dependency order without reading files or installing hooks. */
        explicit BatmanGraphicsRuntimeContext(const std::filesystem::path& engineIniPath);
        /**
         * @brief Constructs direct callbacks with the same optional route owner used by legacy persistence.
         * @param engineIniPath Absolute or relative path to the Batman user engine INI file.
         * @param routing_service Shared initialized routing service, or null when the active pack has no routes.
         */
        BatmanGraphicsRuntimeContext(const std::filesystem::path& engineIniPath,
            std::shared_ptr<FileWriteRoutingService> routing_service);
        /** @brief Handles an owned call while the caller retains shared ownership of this entire context. */
        void Handle(const char* name, const void* arguments, unsigned count, BatmanGraphicsPrimitiveResult& result);
    };
}
