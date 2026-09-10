#include "SessionSettingsPayload.h"
#include <iostream>
#include <stdexcept>

/** @brief Reports console failures without assertion dialogs. */
void Expect(bool condition, const char* message) { if (!condition) { throw std::runtime_error(message); } }
/** @brief Uses hand-checked loader offsets to catch collateral writes and incorrect inverse/normalized encodings. */
int main() {
    try {
        using namespace helen;
        const auto initial = BatmanGraphicsDraftState::TryCreate({0,1,0,0,0,0,0,0,1,0,1,0,1920,1080});
        Expect(initial.has_value(),"Invalid fixture");
        BatmanGraphicsDraftState draft = *initial;
        Expect(draft.TrySet(BatmanGraphicsField::Bloom,1) && draft.TrySet(BatmanGraphicsField::DynamicShadows,1) &&
            draft.TrySet(BatmanGraphicsField::MotionBlur,1) && draft.TrySet(BatmanGraphicsField::Distortion,1) &&
            draft.TrySet(BatmanGraphicsField::FogVolumes,1) && draft.TrySet(BatmanGraphicsField::SphericalHarmonicLighting,0) &&
            draft.TrySet(BatmanGraphicsField::AmbientOcclusion,1) && draft.TrySet(BatmanGraphicsField::Msaa,3),"Draft failed");
        SessionGraphicsDelta::Capabilities capabilities;
        capabilities.fill(true);
        capabilities[10]=false;
        capabilities[11]=false;
        SessionSettingsPayload::Values before;
        before.fill(0x1234ABCD);
        const auto after = SessionSettingsPayload::Build(before,draft,SessionGraphicsDelta(*initial,draft,capabilities));
        SessionSettingsPayload::Values expected = before;
        expected[0x34/4]=1;
        expected[0x24C/4]=1;
        expected[0x28/4]=1;
        expected[0x3C/4]=1;
        expected[0x48/4]=1;
        expected[0x60/4]=1;
        expected[0x30/4]=1;
        expected[0x248/4]=8;
        Expect(after==expected,"Mixed payload modified wrong or unrelated fields");
        Expect(!SessionSettingsPayload::Mirror(BatmanGraphicsField::Bloom).has_value(),"Bloom falsely uses AO renderer mirror");
        Expect(SessionSettingsPayload::Mirror(BatmanGraphicsField::Msaa)==std::uintptr_t{0x26C0DF8},"MSAA render binding wrong");
        Expect(SessionSettingsPayload::Decode(BatmanGraphicsField::Msaa,16)==5 &&
            SessionSettingsPayload::Decode(BatmanGraphicsField::Msaa,0)==0 &&
            SessionSettingsPayload::Decode(BatmanGraphicsField::SphericalHarmonicLighting,1)==0,"Live decode wrong");
        bool rejected = false;
        try { (void)SessionSettingsPayload::Offset(BatmanGraphicsField::Physx); }
        catch (const std::invalid_argument&) { rejected = true; }
        Expect(rejected,"PhysX invented a settings payload binding");
        capabilities[10] = true;
        BatmanGraphicsDraftState physicsDraft = *initial;
        Expect(physicsDraft.TrySet(BatmanGraphicsField::Physx,2),"Experimental PhysX draft failed");
        Expect(SessionSettingsPayload::Build(before,physicsDraft,SessionGraphicsDelta(*initial,physicsDraft,capabilities)) == before,
            "Cache-only PhysX experiment modified the renderer payload");
        Expect(draft.TrySet(BatmanGraphicsField::Physx,0),"Mixed PhysX draft failed");
        Expect(SessionSettingsPayload::Build(before,draft,SessionGraphicsDelta(*initial,draft,capabilities)) == expected,
            "Experimental PhysX disrupted ordinary renderer edits");
        std::cout << "SESSION_PAYLOAD_PASS\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
