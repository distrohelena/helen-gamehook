#include "SessionGraphicsOverlay.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

/** @brief Throws console-readable failures without graphical assertions. */
void Expect(bool condition, const char* message) { if (!condition) { throw std::runtime_error(message); } }
/** @brief Writes exact test input bytes, preserving explicit ANSI or UTF-16 encoding. */
void Write(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream stream(path,std::ios::binary);
    stream.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    stream.close();
    Expect(!stream.fail(), "Fixture write failed");
}
/** @brief Constructs a UTF-16LE ASCII fixture without depending on production INI encoding helpers. */
std::string Wide(const std::string& text) {
    std::string bytes("\xFF\xFE",2);
    for (const char character : text) { bytes.push_back(character); bytes.push_back('\0'); }
    return bytes;
}
/** @brief Exercises real routed files, whole-delta staging, repeat updates and prepublication failures. */
int main(int argc, char** argv) {
    try {
        using namespace helen;
        Expect(argc==2,"Fixture directory required");
        const std::filesystem::path root(argv[1]);
        const std::string initial = "; unrelated comment\r\n[SystemSettings]\r\nBloom=False\r\nDynamicShadows=False\r\n"
            "DisableSphericalHarmonicLights=False\r\nMaxMultisamples=1\r\nUnrelated=keep me\r\n";
        const std::string expected = "; unrelated comment\r\n[SystemSettings]\r\nBloom=True\r\nDynamicShadows=True\r\n"
            "DisableSphericalHarmonicLights=True\r\nMaxMultisamples=8\r\nUnrelated=keep me\r\n";
        for (const bool unicode : {false,true}) {
            const std::filesystem::path directory = root / (unicode ? "unicode" : "ansi");
            std::filesystem::create_directories(directory);
            const std::filesystem::path original = directory / "BmEngine.ini";
            const std::string originalBytes = unicode ? Wide(initial) : initial;
            Write(original, originalBytes);
            FileWriteRoutingService routing(directory / "cache",directory);
            DWORD error = ERROR_SUCCESS;
            Expect(routing.Initialize({{"engine",original,FileWritePolicy::Redirect,FileReadPolicy::Redirected}},error),"Route failed");
            SessionGraphicsOverlay overlay(routing,original);
            const auto baseline = BatmanGraphicsDraftState::TryCreate({0,1,0,0,0,0,0,0,1,0,1,0,1920,1080});
            Expect(baseline.has_value(),"Fixture draft invalid");
            BatmanGraphicsDraftState draft = *baseline;
            Expect(draft.TrySet(BatmanGraphicsField::Bloom,1) && draft.TrySet(BatmanGraphicsField::DynamicShadows,1) &&
                draft.TrySet(BatmanGraphicsField::SphericalHarmonicLighting,0) && draft.TrySet(BatmanGraphicsField::Msaa,3),"Fixture edits failed");
            SessionGraphicsDelta::Capabilities capabilities;
            capabilities.fill(true);
            const SessionGraphicsDelta delta(*baseline,draft,capabilities);
            overlay.Stage(draft,delta);
            Expect(SessionGraphicsOverlay::ReadBytes(overlay.RequirePath()) == (unicode ? Wide(expected) : expected),"Mixed staged bytes differ");
            Expect(SessionGraphicsOverlay::ReadBytes(original)==originalBytes,"Original changed");
            const std::string applied = SessionGraphicsOverlay::ReadBytes(overlay.RequirePath());
            const std::filesystem::path stale = overlay.RequirePath().parent_path()/"stale-stage.tmp";
            Write(stale,originalBytes);
            Expect(!routing.PublishSessionReplacement(original,originalBytes,stale) &&
                SessionGraphicsOverlay::ReadBytes(overlay.RequirePath())==applied && std::filesystem::exists(stale),
                "Stale staged update overwrote newer session bytes");
            HANDLE reader = routing.Open(original,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
            Expect(reader != INVALID_HANDLE_VALUE,"Tracked fixture open failed");
            bool blocked = false;
            try { overlay.Stage(*baseline,SessionGraphicsDelta(draft,*baseline,capabilities)); }
            catch (const std::exception&) { blocked = true; }
            Expect(routing.Close(reader)!=FALSE,"Tracked close failed");
            Expect(blocked && SessionGraphicsOverlay::ReadBytes(overlay.RequirePath())==applied,"Open-handle publication not refused cleanly");
            BatmanGraphicsDraftState missing = draft;
            Expect(missing.TrySet(BatmanGraphicsField::FogVolumes,1),"Missing fixture failed");
            bool missingRejected = false;
            try { overlay.Stage(missing,SessionGraphicsDelta(draft,missing,capabilities)); }
            catch (const std::exception&) { missingRejected = true; }
            Expect(missingRejected && SessionGraphicsOverlay::ReadBytes(overlay.RequirePath())==applied,"Missing-key edit partially published");
            overlay.Stage(*baseline,SessionGraphicsDelta(draft,*baseline,capabilities));
            Expect(SessionGraphicsOverlay::ReadBytes(overlay.RequirePath())==originalBytes,"Reverse staging retained old edits");
            bool invalidRoute = false;
            try { (void)SessionGraphicsOverlay(routing,overlay.RequirePath()).RequirePath(); }
            catch (const std::exception&) { invalidRoute = true; }
            Expect(invalidRoute,"Overlay accepted as protected original");
            Expect(SessionGraphicsOverlay::ReadBytes(original)==originalBytes,"Failure/reverse changed original");
        }
        std::cout << "SESSION_OVERLAY_PASS\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
