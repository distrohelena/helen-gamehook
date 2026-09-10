#include "BloomEffectBinding.h"
#include <array>
#include <iostream>
#include <stdexcept>

/** @brief Verifies that applying Bloom changes its extracted field, not the adjacent ambient-occlusion value. */
int main() {
    try {
        std::array<std::uint32_t,0xAB> payload{};
        payload[0x30/4] = 1;
        const helen::BloomEffectBinding bloom = helen::BloomEffectBinding::For(helen::BatmanGraphicsField::Bloom);
        payload.at(bloom.DataOffset/4) = 1;
        if (payload[0x34/4] != 1) { throw std::runtime_error("Bloom binding did not target the loader's Bloom field"); }
        payload.at(bloom.DataOffset/4) = 0;
        if (payload[0x30/4] != 1) { throw std::runtime_error("Bloom binding changed AmbientOcclusion"); }
        if (bloom.RenderMirror.has_value()) { throw std::runtime_error("Bloom incorrectly claims an independent renderer mirror"); }
        const helen::BloomEffectBinding shadows = helen::BloomEffectBinding::For(helen::BatmanGraphicsField::DynamicShadows);
        payload.at(shadows.DataOffset/4) = 1;
        if (payload[0x24C/4] != 1 || shadows.RenderMirror != std::uintptr_t{0x26C0DF0}) { throw std::runtime_error("DynamicShadows binding regressed"); }
        bool refused = false;
        try { (void)helen::BloomEffectBinding::For(helen::BatmanGraphicsField::Physx); }
        catch (const std::invalid_argument&) { refused = true; }
        if (!refused) { throw std::runtime_error("Unsupported binding accepted"); }
        std::cout << "EFFECT_BINDING_PASS\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
