#pragma once
#include "SessionGraphicsEngine.h"
#include "SessionGraphicsOverlay.h"
#include "SessionNativeObservation.h"
#include <HelenHook/BatmanGraphicsConfigService.h>

namespace helen {
    /** @brief Binds the session coordinator to the pinned Batman runtime and existing D3D9 refresh bridge. */
    class SessionGraphicsNative final : public SessionGraphicsEngine {
    private:
        /** @brief Legacy read-only configuration source used only for startup-owned PhysX observation. */
        BatmanGraphicsConfigService& Config;
        /** @brief Required shared route owner, retained for the entire graphics context lifetime. */
        std::shared_ptr<FileWriteRoutingService> Routing;
        /** @brief Original path used strictly as a protected route identity and integrity observation. */
        std::filesystem::path Ini;
        /** @brief Whole-delta private staging adapter; never invokes the persistent config writer. */
        SessionGraphicsOverlay Overlay;
        /** @brief Preflight exists only between a validated request and its completed verification. */
        std::optional<SessionNativeObservation> Before;
        /** @brief Exact original bytes captured before any mutation and checked after native application. */
        std::string OriginalBytes;
        /** @brief Validated logical engine cache key captured during preflight. */
        std::wstring Filename;
        /** @brief Rejects missing routing during dependency construction, before callbacks are published. */
        static FileWriteRoutingService& RequireRouting(const std::shared_ptr<FileWriteRoutingService>& routing);
    public:
        /** @brief Establishes required ownership without reading engine state before startup has completed. */
        SessionGraphicsNative(BatmanGraphicsConfigService& config, std::shared_ptr<FileWriteRoutingService> routing,
            std::filesystem::path ini);
        /** @brief Captures actual display/live settings and engine-owned PhysX level; loaded content may still reflect startup. */
        BatmanGraphicsSnapshot Capture() const override;
        /** @brief Enables renderer/display paths and experimental PhysX engine-field/cache edits; stereo remains unsupported. */
        SessionGraphicsDelta::Capabilities Capabilities() const noexcept override;
        /** @brief Validates bindings, route identity, directional-lightmap preservation and complete target support. */
        void Preflight(const BatmanGraphicsDraftState& baseline, const BatmanGraphicsDraftState& draft,
            const SessionGraphicsDelta& delta) override;
        /** @brief Publishes the complete delta only into this session's authoritative routed copy. */
        void Stage(const BatmanGraphicsDraftState& draft, const SessionGraphicsDelta& delta) override;
        /** @brief Reloads cache, runs stock Apply(save=false), then performs at most one explicit display refresh. */
        void Apply(const BatmanGraphicsDraftState& draft, const SessionGraphicsDelta& delta) override;
        /** @brief Requires selected cache/live/render/device values and unrelated payload/original preservation. */
        void Verify(const BatmanGraphicsDraftState& draft, const SessionGraphicsDelta& delta) override;
    };
}
