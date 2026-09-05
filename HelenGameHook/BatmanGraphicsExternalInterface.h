#pragma once

#include "BatmanGraphicsPrimitiveResult.h"
#include <HelenHook/BatmanGraphicsSessionService.h>

namespace helen {
    /** @brief Implements the allowlisted direct graphics primitive protocol without owning engine objects. */
    class BatmanGraphicsExternalInterface {
    private:
        /** @brief Required initialized session service; its owner outlives this adapter and every dispatch. */
        BatmanGraphicsSessionService& Sessions;
    public:
        /** @brief Binds explicit native session ownership before a runtime publishes the dispatch hook. */
        explicit BatmanGraphicsExternalInterface(BatmanGraphicsSessionService& sessions);
        /** @brief Recognizes only the twelve exact versioned operation names without accessing runtime state. */
        static bool Owns(const char* name);
        /**
         * @brief Handles exact V1 graphics names and leaves malformed owned calls undefined; stock names are untouched.
         * @param name Borrowed null-terminated engine method name, or null for an unowned call.
         * @param arguments Engine-converted records, valid for count elements during this call only.
         * @param count Engine argument count, required to match the operation's exact arity.
         * @param result Pre-cleared borrowed movie result; never supplies managed-object storage to this codec.
         * @return True for an owned operation even when rejected, false only when stock dispatch must continue.
         */
        bool TryHandle(const char* name, const void* arguments, unsigned count, BatmanGraphicsPrimitiveResult& result);
        /** @brief Forwards the untouched stock handler, movie, name and arguments using Batman's original x86 thiscall. */
        static void ForwardStock(void* handler, void* movie, const char* name, const void* arguments, unsigned count);
    };
}
