#include "BloomDraft.h"
#include <windows.h>
#include <iostream>
#include <stdexcept>

namespace {
    /** @brief Reports request contract failures without GUI assertions. */
    void Expect(bool condition, const char* message) { if (!condition) { throw std::runtime_error(message); } }
}
/** @brief Verifies selected polarity and rejects all unsupported mixed menu edits. */
int main() {
    SetErrorMode(0x8003);
    try {
        const auto initial = helen::BatmanGraphicsDraftState::TryCreate({0,0,0,0,0,0,0,0,0,0,0,0,1280,720});
        Expect(initial.has_value(), "Invalid fixture");
        const helen::BatmanGraphicsDraftState baseline = *initial;
        helen::BatmanGraphicsDraftState enabled = baseline;
        Expect(enabled.TrySet(helen::BatmanGraphicsField::Bloom, 1), "Cannot stage Bloom");
        Expect(helen::BloomDraft::Select(baseline, enabled) == 1, "Bloom on selection lost");
        Expect(helen::BloomDraft::Select(enabled, baseline) == 0, "Bloom off selection lost");
        bool refused = false;
        try { (void)helen::BloomDraft::Select(baseline, baseline); }
        catch (const std::invalid_argument&) { refused = true; }
        Expect(refused, "Unedited request accepted");
        for (unsigned index = 0; index < 12; ++index) {
            if (index == 3) { continue; }
            helen::BatmanGraphicsDraftState mixed = enabled;
            Expect(mixed.TrySet(static_cast<helen::BatmanGraphicsField>(index), 1), "Invalid mixed fixture");
            refused = false;
            try { (void)helen::BloomDraft::Select(baseline, mixed); }
            catch (const std::invalid_argument&) { refused = true; }
            Expect(refused, "Mixed graphics edit accepted");
        }
        Expect(enabled.TrySetResolution(1920,1080), "Cannot stage resolution");
        refused = false;
        try { (void)helen::BloomDraft::Select(baseline, enabled); }
        catch (const std::invalid_argument&) { refused = true; }
        Expect(refused, "Mixed resolution edit accepted");
        helen::BatmanGraphicsDraftState shadows = baseline;
        Expect(shadows.TrySet(helen::BatmanGraphicsField::DynamicShadows, 1), "Cannot stage shadows");
        Expect(helen::BloomDraft::Select(baseline, shadows, helen::BatmanGraphicsField::DynamicShadows) == 1, "Shadows on selection lost");
        Expect(helen::BloomDraft::Select(shadows, baseline, helen::BatmanGraphicsField::DynamicShadows) == 0, "Shadows off selection lost");
        for (unsigned index = 0; index < 12; ++index) {
            if (index == 4) { continue; }
            helen::BatmanGraphicsDraftState mixed = shadows;
            Expect(mixed.TrySet(static_cast<helen::BatmanGraphicsField>(index), 1), "Invalid mixed shadows fixture");
            refused = false;
            try { (void)helen::BloomDraft::Select(baseline, mixed, helen::BatmanGraphicsField::DynamicShadows); }
            catch (const std::invalid_argument&) { refused = true; }
            Expect(refused, "Mixed shadows edit accepted");
        }
        refused = false;
        try { (void)helen::BloomDraft::Select(baseline, shadows, helen::BatmanGraphicsField::MotionBlur); }
        catch (const std::invalid_argument&) { refused = true; }
        Expect(refused, "Unsupported field accepted");
        std::cout << "BLOOM_AND_SHADOWS_DRAFT_PASS\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
