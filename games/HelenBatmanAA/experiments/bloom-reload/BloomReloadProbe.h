#pragma once
#include <HelenHook/BatmanGraphicsApplyResult.h>
#include <HelenHook/BatmanGraphicsDraftState.h>
#include <HelenHook/FileWriteRoutingService.h>
#include <memory>

namespace helen {
    /** @brief Opt-in, one-shot investigation of session INI to engine cache to live Bloom application. */
    class BloomReloadProbe {
    public:
        /** @brief Retains the initialized route owner before menu publication; no file or engine mutation occurs here. */
        static void Bind(const std::filesystem::path& ini, std::shared_ptr<FileWriteRoutingService> routing);
        /** @brief Called only after the existing probe verifies the executable, idle viewport and owning game thread. */
        static BatmanGraphicsApplyResult Apply(const BatmanGraphicsDraftState& baseline, const BatmanGraphicsDraftState& draft);
    };
}
