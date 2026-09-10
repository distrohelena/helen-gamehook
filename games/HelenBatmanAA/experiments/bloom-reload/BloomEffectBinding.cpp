#include "BloomEffectBinding.h"
#include <stdexcept>

namespace helen {
    BloomEffectBinding BloomEffectBinding::For(BatmanGraphicsField field) {
        if (field == BatmanGraphicsField::Bloom) { return BloomEffectBinding(0x34, std::nullopt); }
        if (field == BatmanGraphicsField::DynamicShadows) { return BloomEffectBinding(0x24C, 0x26C0DF0); }
        throw std::invalid_argument("Unsupported effect binding");
    }
}
