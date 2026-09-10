#include "SessionGraphicsDelta.h"
#include <stdexcept>
#include <string>
namespace helen {
    SessionGraphicsDelta::SessionGraphicsDelta(const BatmanGraphicsDraftState& baseline, const BatmanGraphicsDraftState& draft,
        const Capabilities& capabilities) {
        for (std::size_t index = 0; index < capabilities.size(); ++index) {
            const BatmanGraphicsField field = static_cast<BatmanGraphicsField>(index);
            if (baseline.Get(field) != draft.Get(field)) {
                if (!capabilities[index]) {
                    throw std::invalid_argument(std::string(Key(field)) + " has no verified live application path; session-only edits are not saved for restart");
                }
                Changed.push_back(field);
            }
        }
    }
    const std::vector<BatmanGraphicsField>& SessionGraphicsDelta::Fields() const noexcept { return Changed; }
    int SessionGraphicsDelta::Encode(BatmanGraphicsField field, int normalized) {
        (void)Key(field);
        if (field == BatmanGraphicsField::Msaa) {
            switch (normalized) {
            case 0: return 1;
            case 1: return 2;
            case 2: return 4;
            case 3: return 8;
            case 5: return 16;
            default: throw std::invalid_argument("Invalid normalized MSAA");
            }
        } else if (field == BatmanGraphicsField::PersistedWidth || field == BatmanGraphicsField::PersistedHeight) {
            if (normalized <= 0) { throw std::invalid_argument("Nonpositive resolution"); }
        } else if (field == BatmanGraphicsField::Physx) {
            if (normalized < 0 || normalized > 2) { throw std::invalid_argument("Invalid PhysX level"); }
        } else if (normalized != 0 && normalized != 1) {
            throw std::invalid_argument("Non-Boolean graphics setting");
        }
        return field == BatmanGraphicsField::SphericalHarmonicLighting ? 1 - normalized : normalized;
    }
    const char* SessionGraphicsDelta::Key(BatmanGraphicsField field) {
        /** @brief Pinned configuration names in the public protocol's editable-field order. */
        static constexpr std::array<const char*,14> keys{"Fullscreen","UseVsync","MaxMultisamples","Bloom",
            "DynamicShadows","MotionBlur","Distortion","FogVolumes","DisableSphericalHarmonicLights",
            "AmbientOcclusion","PhysXLevel","Stereo","ResX","ResY"};
        return keys.at(static_cast<std::size_t>(field));
    }
    const char* SessionGraphicsDelta::Section(BatmanGraphicsField field) {
        (void)Key(field);
        return field == BatmanGraphicsField::Physx ? "Engine.Engine" : "SystemSettings";
    }
}
