#include <HelenHook/PackRepository.h>
#include <HelenHook/DeltaVirtualFileSource.h>
#include <fstream>
#include <iterator>
#include <windows.h>
#include <iostream>
#include <stdexcept>

/** Parses the experimental package using the same native repository code used by the game. */
int wmain(int argc, wchar_t** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    try {
        if (argc != 3) { throw std::runtime_error("Expected staged package parent and retail base."); }
        const helen::PackRepository repository;
        const auto loaded = repository.LoadForExecutable(argv[1], "ShippingPC-BmGame.exe", 38758728,
            "4dac1f5e2ac6710b7378fdce74601f616f4753e3756cb5fda63c7519cc2eb028");
        if (!loaded.has_value()) { throw std::runtime_error("Native pack parser rejected probe."); }
        if (loaded->Build.Hooks.size() != 1) { throw std::runtime_error("Expected one direct dispatch hook."); }
        const helen::HookDefinition& hook = loaded->Build.Hooks[0];
        if (hook.Id != "directFullscreenProbe" || hook.Blob.Relocations.size() != 2) {
            throw std::runtime_error("Wrong probe hook or relocations.");
        }
        if (hook.Blob.Relocations[0].Source.ExportName != "@HelenProbeDispatch@24") {
            throw std::runtime_error("Wrong native dispatch export.");
        }
        for (const auto& observer : loaded->Build.StateObservers) {
            if (observer.Id == "graphicsObserverFullscreen") { throw std::runtime_error("Old Fullscreen observer still enabled."); }
        }
        std::cout << "NATIVE_PROBE_PACKAGE_PASS\n";
        if (loaded->Build.VirtualFiles.size() != 1) { throw std::runtime_error("Expected one frontend replacement."); }
        const std::filesystem::path packageRoot(argv[1]);
        const helen::PackAssetResolver resolver(loaded->PackDirectory, loaded->BuildDirectory);
        helen::DeltaVirtualFileSource source(resolver, packageRoot / L"verification-cache", argv[2], loaded->Build.VirtualFiles[0]);
        std::ifstream target(packageRoot / L"Frontend-direct-probe.umap", std::ios::binary);
        if (!target) { throw std::runtime_error("Missing current-run frontend target."); }
        const std::vector<char> expected{std::istreambuf_iterator<char>(target), std::istreambuf_iterator<char>()};
        std::vector<char> actual(expected.size());
        std::size_t bytesRead = 0;
        if (!source.Read(0, actual.data(), actual.size(), bytesRead) || bytesRead != expected.size() || actual != expected) {
            throw std::runtime_error("Native delta reconstruction did not reproduce the current-run target.");
        }
        std::cout << "PROBE_DELTA_RECONSTRUCTION_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
