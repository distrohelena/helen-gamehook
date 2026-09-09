#pragma once
#include <HelenHook/BatmanGraphicsDraftState.h>

namespace helen {
    /** @brief Restricts the throwaway reload probe to one Bloom or DynamicShadows edit, excluding mixed settings. */
    class BloomDraft {
    public:
        /** @brief Returns the requested Boolean only if the supported selected field alone changed; throws before writes otherwise. */
        static int Select(const BatmanGraphicsDraftState& baseline, const BatmanGraphicsDraftState& draft,
            BatmanGraphicsField field = BatmanGraphicsField::Bloom);
    };
}
