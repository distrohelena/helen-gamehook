#include "NoSaveVsyncRequest.h"
#include <stdexcept>

namespace helen {
    std::optional<D3d9VsyncOverride> NoSaveVsyncRequest::Select(const BatmanGraphicsDraftState& baseline,
        const BatmanGraphicsDraftState& draft) {
        if (baseline.Get(BatmanGraphicsField::Vsync) == draft.Get(BatmanGraphicsField::Vsync)) {
            return std::nullopt;
        }
        for (unsigned index = 2; index < 12; ++index) {
            const BatmanGraphicsField field = static_cast<BatmanGraphicsField>(index);
            if (baseline.Get(field) != draft.Get(field)) {
                throw std::invalid_argument("VSync experiment supports only resolution/fullscreen alongside VSync");
            }
        }
        return draft.Get(BatmanGraphicsField::Vsync) == 1 ? D3d9VsyncOverride::ForceOn : D3d9VsyncOverride::ForceOff;
    }
}
