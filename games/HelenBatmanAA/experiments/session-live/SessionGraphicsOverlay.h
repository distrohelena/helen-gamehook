#pragma once
#include "SessionGraphicsDelta.h"
#include <HelenHook/FileWriteRoutingService.h>
#include <string>

namespace helen {
    /** @brief Builds complete updates in a private sibling, then publishes only to the active routed session copy. */
    class SessionGraphicsOverlay {
    private:
        /** @brief Required routing owner; it must outlive this adapter and all Apply calls. */
        FileWriteRoutingService& Routing;
        /** @brief Protected original anchor; this adapter never opens it for writing. */
        std::filesystem::path Original;
    public:
        /** @brief Binds the existing route owner without creating files or selecting stale cache directories. */
        SessionGraphicsOverlay(FileWriteRoutingService& routing, std::filesystem::path original);
        /** @brief Resolves exactly one active redirected-read/write route, rejecting aliasing or invalid policies. */
        std::filesystem::path RequirePath() const;
        /** @brief Stages and independently checks all changed keys before serialized session publication. */
        void Stage(const BatmanGraphicsDraftState& draft, const SessionGraphicsDelta& delta) const;
        /** @brief Reads exact file bytes for integrity/readback tests and prepublication comparisons. */
        static std::string ReadBytes(const std::filesystem::path& path);
    };
}
