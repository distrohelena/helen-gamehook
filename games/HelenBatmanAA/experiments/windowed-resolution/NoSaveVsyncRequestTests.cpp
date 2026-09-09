#include "NoSaveVsyncRequest.h"
#include <windows.h>
#include <iostream>
#include <stdexcept>

namespace {
    /** @brief Fails an observable request contract through console output, never a dialog. */
    void Expect(bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    }

    /** @brief Detects lost or inverted menu selection and unintended policy writes from ordinary display edits. */
    void CheckSelection() {
        const auto initial = helen::BatmanGraphicsDraftState::TryCreate({0,0,0,0,0,0,0,0,0,0,0,0,1280,720});
        Expect(initial.has_value(), "Invalid fixture baseline");
        const helen::BatmanGraphicsDraftState baseline = *initial;
        helen::BatmanGraphicsDraftState draft = baseline;
        Expect(draft.TrySetResolution(1920, 1080), "Cannot stage resolution");
        Expect(!helen::NoSaveVsyncRequest::Select(baseline, draft).has_value(), "Display-only edit changed VSync policy");
        Expect(draft.TrySet(helen::BatmanGraphicsField::Vsync, 1), "Cannot stage VSync");
        Expect(helen::NoSaveVsyncRequest::Select(baseline, draft) == helen::D3d9VsyncOverride::ForceOn,
            "Menu VSync on was not selected before display apply");
        helen::BatmanGraphicsDraftState enabled = baseline;
        Expect(enabled.TrySet(helen::BatmanGraphicsField::Vsync, 1), "Cannot create enabled baseline");
        Expect(draft.TrySet(helen::BatmanGraphicsField::Vsync, 0), "Cannot stage VSync off");
        Expect(helen::NoSaveVsyncRequest::Select(enabled, draft) == helen::D3d9VsyncOverride::ForceOff,
            "Menu VSync off was not selected");
        draft = enabled;
        Expect(draft.TrySet(helen::BatmanGraphicsField::Fullscreen, 1), "Cannot stage fullscreen");
        Expect(helen::NoSaveVsyncRequest::Select(baseline, draft) == helen::D3d9VsyncOverride::ForceOn,
            "Same-size fullscreen plus VSync edit was rejected");
        Expect(helen::NoSaveVsyncRequest::Select(baseline, enabled) == helen::D3d9VsyncOverride::ForceOn,
            "VSync-only on edit rejected");
        Expect(helen::NoSaveVsyncRequest::Select(enabled, baseline) == helen::D3d9VsyncOverride::ForceOff,
            "VSync-only off edit rejected");
        bool refused = false;
        for (unsigned index = 2; index < 12; ++index) {
            draft = enabled;
            Expect(draft.TrySetResolution(1920,1080), "Cannot stage mixed resolution");
            Expect(draft.TrySet(static_cast<helen::BatmanGraphicsField>(index), 1), "Cannot stage unrelated field");
            refused = false;
            try { (void)helen::NoSaveVsyncRequest::Select(baseline, draft); }
            catch (const std::invalid_argument&) { refused = true; }
            Expect(refused, "Unsupported mixed graphics edit was partially accepted");
        }
    }
}

/** @brief Runs menu-to-policy selection tests without Batman, files or a graphics device. */
int main() {
    SetErrorMode(0x8003);
    try { CheckSelection(); std::cout << "NO_SAVE_VSYNC_REQUEST_PASS\n"; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
