#include "VsyncScalar.h"
#include <stdexcept>

namespace helen {
    VsyncScalar::VsyncScalar(std::uint32_t& field) noexcept : Field(field) {}
    bool VsyncScalar::Read() const {
        if (Field > 1) { throw std::runtime_error("Unsupported VSync representation"); }
        return Field == 1;
    }
    void VsyncScalar::Write(bool enabled) {
        static_cast<void>(Read());
        Field = enabled ? 1u : 0u;
    }
}
