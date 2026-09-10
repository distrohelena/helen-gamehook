#include "SessionSettingsPayload.h"
#include <stdexcept>
#include <limits>
namespace helen {
    std::size_t SessionSettingsPayload::Offset(BatmanGraphicsField field) {
        if (field == BatmanGraphicsField::Physx) { throw std::invalid_argument("PhysX has no FSystemSettings payload binding"); }
        /** @brief Offsets independently extracted from C20130's Boolean and integer tables, relative to owner+4. */
        static constexpr std::array<std::size_t,14> offsets{0x244,0x21C,0x248,0x34,0x24C,0x28,0x3C,0x48,0x60,0x30,0,0x2A8,0x23C,0x240};
        return offsets.at(static_cast<std::size_t>(field));
    }
    std::optional<std::uintptr_t> SessionSettingsPayload::Mirror(BatmanGraphicsField field) {
        switch (field) {
        case BatmanGraphicsField::MotionBlur: return 0x26C0DE8;
        case BatmanGraphicsField::AmbientOcclusion: return 0x26C0DEC;
        case BatmanGraphicsField::DynamicShadows: return 0x26C0DF0;
        case BatmanGraphicsField::FogVolumes: return 0x26C0DF4;
        case BatmanGraphicsField::Msaa: return 0x26C0DF8;
        default: return std::nullopt;
        }
    }
    SessionSettingsPayload::Values SessionSettingsPayload::Build(const Values& before, const BatmanGraphicsDraftState& draft,
        const SessionGraphicsDelta& delta) {
        Values incoming = before;
        for (const BatmanGraphicsField field : delta.Fields()) {
            // The experimental PhysX edit belongs only to Engine.Engine's cache, never FSystemSettings.
            if (field == BatmanGraphicsField::Physx) { continue; }
            incoming.at(Offset(field)/4) = static_cast<std::uint32_t>(SessionGraphicsDelta::Encode(field,draft.Get(field)));
        }
        return incoming;
    }
    int SessionSettingsPayload::Decode(BatmanGraphicsField field, std::uint32_t value) {
        if (value > static_cast<std::uint32_t>((std::numeric_limits<int>::max)())) { throw std::runtime_error("Invalid live graphics integer"); }
        if (field == BatmanGraphicsField::Msaa) {
            switch (value) {
            case 0: case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            case 8: return 3;
            case 16: return 5;
            default: throw std::runtime_error("Unsupported live multisample encoding");
            }
        }
        const int normalized = field == BatmanGraphicsField::SphericalHarmonicLighting ? 1-static_cast<int>(value) : static_cast<int>(value);
        (void)SessionGraphicsDelta::Encode(field,normalized);
        return normalized;
    }
}
