#include "BloomDraft.h"
#include <stdexcept>
namespace helen {
    int BloomDraft::Select(const BatmanGraphicsDraftState& baseline, const BatmanGraphicsDraftState& draft, BatmanGraphicsField requestedField) {
        if (requestedField != BatmanGraphicsField::Bloom && requestedField != BatmanGraphicsField::DynamicShadows) {
            throw std::invalid_argument("Reload probe supports only Bloom or DynamicShadows");
        }
        for (unsigned index = 0; index < 14; ++index) {
            if (index == static_cast<unsigned>(requestedField)) { continue; }
            const BatmanGraphicsField field = static_cast<BatmanGraphicsField>(index);
            if (baseline.Get(field) != draft.Get(field)) { throw std::invalid_argument("Reload probe requires a single-setting edit"); }
        }
        const int selected = draft.Get(requestedField);
        if (selected == baseline.Get(requestedField)) { throw std::invalid_argument("Reload probe requires a changed value"); }
        return selected;
    }
}
