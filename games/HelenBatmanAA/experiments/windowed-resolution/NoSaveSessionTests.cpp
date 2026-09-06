#include <HelenHook/BatmanGraphicsSessionService.h>
#include <HelenHook/Log.h>
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
    /** @brief Report test failures through the console rather than assertion dialogs. */
    void Expect(bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    }
    /** @brief Read exact fixture bytes to detect any publication or encoding changes. */
    std::string Bytes(const std::filesystem::path& path) {
        std::ifstream stream(path, std::ios::binary);
        Expect(stream.good(), "Cannot read fixture");
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }
}

/** @brief Exercise the linked real session Commit and prove this experimental build never publishes its draft. */
int wmain(int argc, wchar_t** argv) {
    SetErrorMode(0x8003);
    try {
        Expect(argc == 2, "Pass a fresh test fixture directory");
        const std::filesystem::path root(argv[1]);
        Expect(std::filesystem::create_directory(root), "Test root must not already exist");
        const std::filesystem::path user = root / L"UserEngine.ini";
        const std::filesystem::path engine = root / L"BmEngine.ini";
        const std::string initial = "[SystemSettings]\nFullscreen=False\nResX=1280\nResY=720\nUseVsync=False\nMaxMultisamples=0\n"
            "Bloom=True\nDynamicShadows=True\nMotionBlur=False\nDistortion=False\nFogVolumes=False\n"
            "DisableSphericalHarmonicLights=False\nAmbientOcclusion=False\nStereo=False\n[Engine.Engine]\nPhysXLevel=0\n";
        for (const std::filesystem::path& path : {user, engine}) {
            std::ofstream stream(path, std::ios::binary);
            stream.exceptions(std::ios::badbit | std::ios::failbit);
            stream << initial;
        }
        helen::SetLogPath(root / L"probe.log");
        helen::BatmanDisplayModeService display;
        helen::BatmanGraphicsConfigService config(engine, display);
        helen::BatmanGraphicsSessionService sessions(config, display);
        const auto session = sessions.Open();
        Expect(session.has_value(), "Open failed");
        Expect(sessions.Get(*session, helen::BatmanGraphicsField::CanApply) == 1, "Fixture baseline invalid");
        Expect(sessions.EndRead(*session), "EndRead failed");
        const auto transaction = sessions.BeginApply(*session);
        Expect(transaction.has_value(), "BeginApply failed");
        Expect(sessions.SetField(*session, *transaction, helen::BatmanGraphicsField::Vsync, 1), "Staging failed");
        const auto result = sessions.Commit(*session, *transaction);
        Expect(result.has_value() && result->Outcome == helen::BatmanGraphicsApplyOutcome::NotApplied, "Probe must never claim saved success");
        Expect(Bytes(user) == initial && Bytes(engine) == initial, "No-save Commit changed INI bytes");
        Expect(Bytes(root/L"probe.log").find("resolution-no-save") != std::string::npos, "Probe implementation was not linked");
        Expect(!sessions.Commit(*session, *transaction).has_value(), "Consumed transaction executed twice");
        std::cout << "NO_SAVE_SESSION_PASS: real Commit bypasses writer; foreign executable rejected; both INIs unchanged\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
