#include "SessionGraphicsDelta.h"
#include <iostream>
#include <stdexcept>
#include <string>

/** @brief Reports fixture failures through the console rather than assertion dialogs. */
void Expect(bool condition, const char* message) { if (!condition) { throw std::runtime_error(message); } }
/** @brief Checks mixed delta completeness, inverted lighting, sparse MSAA codes, and fail-before-write capabilities. */
int main() {
    try {
        using namespace helen;
        const auto baseline = BatmanGraphicsDraftState::TryCreate({0,1,0,0,0,0,0,0,1,0,1,0,1920,1080});
        Expect(baseline.has_value(), "Invalid fixture");
        BatmanGraphicsDraftState draft = *baseline;
        Expect(draft.TrySet(BatmanGraphicsField::Bloom,1) && draft.TrySet(BatmanGraphicsField::DynamicShadows,1) &&
            draft.TrySet(BatmanGraphicsField::SphericalHarmonicLighting,0), "Mixed draft failed");
        SessionGraphicsDelta::Capabilities capabilities;
        capabilities.fill(true);
        capabilities[10] = false;
        capabilities[11] = false;
        const SessionGraphicsDelta mixed(*baseline,draft,capabilities);
        Expect(mixed.Fields() == std::vector<BatmanGraphicsField>{BatmanGraphicsField::Bloom,
            BatmanGraphicsField::DynamicShadows,BatmanGraphicsField::SphericalHarmonicLighting}, "Mixed delta lost edits");
        Expect(SessionGraphicsDelta(*baseline,*baseline,capabilities).Fields().empty(), "No-op produced work");
        Expect(SessionGraphicsDelta::Encode(BatmanGraphicsField::SphericalHarmonicLighting,0) == 1 &&
            SessionGraphicsDelta::Encode(BatmanGraphicsField::SphericalHarmonicLighting,1) == 0, "Lighting inversion lost");
        const int normalized[] = {0,1,2,3,5};
        const int samples[] = {1,2,4,8,16};
        for (unsigned index=0; index<5; ++index) {
            Expect(SessionGraphicsDelta::Encode(BatmanGraphicsField::Msaa,normalized[index]) == samples[index], "MSAA encoding incorrect");
        }
        bool invalidRejected = false;
        try { (void)SessionGraphicsDelta::Encode(BatmanGraphicsField::Msaa,4); }
        catch (const std::invalid_argument&) { invalidRejected = true; }
        Expect(invalidRejected, "Invalid MSAA display index accepted");
        for (const BatmanGraphicsField field : {BatmanGraphicsField::Physx,BatmanGraphicsField::Stereo}) {
            BatmanGraphicsDraftState unsupported = draft;
            Expect(unsupported.TrySet(field, field == BatmanGraphicsField::Physx ? 2 : 1), "Unsupported fixture failed");
            bool rejected = false;
            try { (void)SessionGraphicsDelta(*baseline,unsupported,capabilities); }
            catch (const std::invalid_argument& error) {
                rejected = std::string(error.what()).find(SessionGraphicsDelta::Key(field)) != std::string::npos;
            }
            Expect(rejected, "Mixed unsupported edit accepted or lacks specific reason");
        }
        Expect(std::string(SessionGraphicsDelta::Section(BatmanGraphicsField::Physx)) == "Engine.Engine", "Wrong PhysX section");
        Expect(std::string(SessionGraphicsDelta::Key(BatmanGraphicsField::Bloom)) == "Bloom", "Wrong Bloom key");
        std::cout << "SESSION_DELTA_PASS\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
