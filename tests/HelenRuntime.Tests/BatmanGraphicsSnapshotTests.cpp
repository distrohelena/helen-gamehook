#include <HelenHook/BatmanGraphicsConfigService.h>
#include <HelenHook/BatmanDisplayModeService.h>

#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
    /** @brief Fails Release tests through the console exception handler instead of an assertion dialog. */
    void Expect(bool condition, const char* message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    /** @brief Writes only a test-owned fixture with explicit encoding, preserving production parser behavior. */
    void WriteFixture(const std::filesystem::path& path, const std::string& text, bool utf16) {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.exceptions(std::ios::badbit | std::ios::failbit);
        if (utf16) {
            stream.put('\xFF');
            stream.put('\xFE');
            for (char character : text) {
                stream.put(character);
                stream.put('\0');
            }
        } else {
            stream << text;
        }
    }

    /** @brief Reads exact fixture bytes so a capture that rewrites encodings or settings is caught. */
    std::string ReadBytes(const std::filesystem::path& path) {
        std::ifstream stream(path, std::ios::binary);
        Expect(stream.good(), "Unable to read test fixture.");
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    /** @brief Produces independently specified launcher data for partial-read and normalization cases. */
    std::string Fixture(const std::string& vsync, const std::string& msaa, const std::string& physx, const std::string& height) {
        return "[SystemSettings]\nFullscreen=False\nResX=1600\nResY=" + height +
            "\n" + vsync + "\nMaxMultisamples=" + msaa +
            "\nBloom=True\nDynamicShadows=True\nMotionBlur=False\nDistortion=False\nFogVolumes=False\n"
            "DisableSphericalHarmonicLights=False\nAmbientOcclusion=False\nStereo=False\n"
            "[Engine.Engine]\nPhysXLevel=" + physx + "\n";
    }
}

/** @brief Exercises real INI capture; invalid fields must not erase valid siblings or fabricate defaults. */
void RunBatmanGraphicsSnapshotTests() {
    using helen::BatmanGraphicsField;
    const std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("BatmanGraphicsSnapshotTests-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    const std::filesystem::path user = root / "UserEngine.ini";
    const std::filesystem::path engine = root / "BmEngine.ini";
    helen::BatmanDisplayModeService display;
    helen::BatmanGraphicsConfigService service(engine, display);
    WriteFixture(engine, "generated-file-must-stay-untouched", false);
    for (bool utf16 : {false, true}) {
        WriteFixture(user, Fixture("UseVsync=False", "16", "2", "900"), utf16);
        const std::string before = ReadBytes(user);
        const helen::BatmanGraphicsSnapshot snapshot = service.CaptureReadSnapshot();
        Expect(snapshot.Get(BatmanGraphicsField::Fullscreen) == 0, "Snapshot lost valid Windowed value.");
        Expect(snapshot.Get(BatmanGraphicsField::Msaa) == 5, "16x MSAA did not normalize to five.");
        Expect(snapshot.Get(BatmanGraphicsField::SphericalHarmonicLighting) == 1, "Disable lighting flag was not inverted.");
        Expect(snapshot.Get(BatmanGraphicsField::PersistedWidth) == 1600 &&
            snapshot.Get(BatmanGraphicsField::PersistedHeight) == 900, "Configured pair changed during capture.");
        Expect(snapshot.IsComplete(), "Valid fixture produced an incomplete snapshot.");
        std::optional<helen::BatmanGraphicsDraftState> draft = snapshot.TryCreateDraft();
        Expect(draft.has_value(), "Valid snapshot could not create an independent draft.");
        Expect(draft->Get(BatmanGraphicsField::Msaa) == 5, "Draft lost normalized MSAA.");
        Expect(draft->TrySet(BatmanGraphicsField::Vsync, 1), "Valid draft edit rejected.");
        Expect(!draft->TrySet(BatmanGraphicsField::Vsync, 2) && draft->Get(BatmanGraphicsField::Vsync) == 1,
            "Rejected boolean edit mutated draft.");
        Expect(!draft->TrySet(BatmanGraphicsField::Msaa, 4) && draft->Get(BatmanGraphicsField::Msaa) == 5,
            "Display index four accepted as a normalized MSAA value.");
        Expect(!draft->TrySet(BatmanGraphicsField::PersistedWidth, 800), "Single dimension edit accepted.");
        Expect(!draft->TrySetResolution(800, 0) && draft->Get(BatmanGraphicsField::PersistedWidth) == 1600,
            "Invalid pair partially modified draft.");
        Expect(draft->TrySetResolution(1280, 720) && draft->Get(BatmanGraphicsField::PersistedHeight) == 720,
            "Valid dimension pair edit failed.");
        Expect(snapshot.Get(BatmanGraphicsField::Vsync) == 0, "Draft mutated captured baseline.");
        Expect(ReadBytes(user) == before, "Read capture rewrote launcher INI.");
        WriteFixture(user, Fixture("", "3", "9", "-1"), utf16);
        const helen::BatmanGraphicsSnapshot partial = service.CaptureReadSnapshot();
        Expect(!partial.Get(BatmanGraphicsField::Vsync), "Missing VSync became a default value.");
        Expect(!partial.Get(BatmanGraphicsField::Msaa), "Unsupported MSAA became a default value.");
        Expect(!partial.Get(BatmanGraphicsField::Physx), "Unsupported PhysX became a valid state.");
        Expect(!partial.Get(BatmanGraphicsField::PersistedWidth) && !partial.Get(BatmanGraphicsField::PersistedHeight),
            "Invalid dimension did not invalidate the entire pair.");
        Expect(partial.Get(BatmanGraphicsField::Fullscreen) == 0 && partial.Get(BatmanGraphicsField::Bloom) == 1,
            "One invalid field erased valid siblings.");
        Expect(!partial.IsComplete(), "Partial snapshot claimed to support a complete draft.");
        Expect(!partial.TryCreateDraft(), "Partial snapshot fabricated a complete draft.");
        Expect(snapshot.Get(BatmanGraphicsField::Msaa) == 5, "Getter reread changed INI instead of its snapshot.");
        Expect(!snapshot.Get(BatmanGraphicsField::DesktopWidth) &&
            !snapshot.Get(static_cast<BatmanGraphicsField>(999)), "Snapshot fabricated session projections or invalid fields.");
    }
    Expect(ReadBytes(engine) == "generated-file-must-stay-untouched", "Capture modified generated INI.");
    std::filesystem::remove(user);
    const helen::BatmanGraphicsSnapshot absent = service.CaptureReadSnapshot();
    Expect(!absent.IsComplete() && !absent.Get(BatmanGraphicsField::Fullscreen), "Absent file reused cached values.");
    std::filesystem::remove(engine);
    std::filesystem::remove(root);
}
