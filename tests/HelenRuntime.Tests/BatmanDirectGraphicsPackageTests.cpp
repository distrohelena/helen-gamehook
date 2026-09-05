#include <HelenHook/PackRepository.h>
#include <HelenHook/DeltaVirtualFileSource.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <windows.h>

namespace {
    /** @brief Fails the console verifier immediately when a shipping contract is violated. */
    void Require(bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    }
}

/** @brief Parses the real candidate, checks direct-only hook ownership and reconstructs its exact frontend bytes. */
int wmain(int argc, wchar_t** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    try {
        Require(argc == 5, "Expected candidate pack parent, retail base, current target and candidate DLL.");
        const helen::PackRepository repository;
        const std::optional<helen::LoadedBuildPack> loaded = repository.LoadForExecutable(argv[1], "ShippingPC-BmGame.exe", 38758728,
            "4dac1f5e2ac6710b7378fdce74601f616f4753e3756cb5fda63c7519cc2eb028");
        Require(loaded.has_value(), "Native pack parser rejected candidate.");
        Require(loaded->Pack.Id == "batman-aa-graphics-options", "Unexpected candidate pack.");
        const std::set<std::filesystem::path> expected_files{
            "pack.json", "builds/steam-goty-1.0/build.json", "builds/steam-goty-1.0/bindings.json",
            "builds/steam-goty-1.0/commands.json", "builds/steam-goty-1.0/hooks.json", "builds/steam-goty-1.0/files.json",
            "builds/steam-goty-1.0/assets/native/direct-dispatch.bin", "builds/steam-goty-1.0/assets/deltas/Frontend-graphics-options.hgdelta"
        };
        std::set<std::filesystem::path> actual_files;
        for (const std::filesystem::directory_entry& file : std::filesystem::recursive_directory_iterator(loaded->PackDirectory)) {
            if (file.is_regular_file()) { actual_files.insert(file.path().lexically_relative(loaded->PackDirectory)); }
        }
        Require(actual_files == expected_files, "Candidate contains missing or unexpected pack files.");
        Require(loaded->Pack.Features.empty() && loaded->Build.ExternalBindings.empty() && loaded->Build.MissingPaths.empty() &&
            loaded->Build.TextureReplacements.empty(), "Graphics candidate contains unrelated behavior declarations.");
        Require(loaded->Pack.ConfigEntries.empty() && loaded->Build.Commands.empty() && loaded->Build.StartupCommandIds.empty(),
            "Graphics candidate still owns legacy config or command state.");
        Require(loaded->Build.StateObservers.empty() && loaded->Build.RuntimeSlots.empty(), "Graphics candidate still installs carrier observers or slots.");
        Require(loaded->Build.Hooks.size() == 1, "Expected exactly one production dispatch hook.");
        const helen::HookDefinition& hook = loaded->Build.Hooks[0];
        Require(hook.Id == "directGraphicsDispatch" && hook.ModuleName == "ShippingPC-BmGame.exe" &&
            hook.RelativeVirtualAddress == 0x015FD9D0u && hook.ExpectedBytes == "8B118B5204" && hook.Pattern.empty(), "Dispatch executable contract mismatch.");
        Require(hook.Action == "inline-jump-to-pack-blob" && hook.OverwriteLength == 5 && hook.ResumeOffsetFromTarget == 5 &&
            hook.Blob.EntryOffset == 0u && hook.Blob.Relocations.size() == 2, "Dispatch overwrite or resume contract mismatch.");
        const helen::HookBlobRelocationDefinition& entry = hook.Blob.Relocations[0];
        const helen::HookBlobRelocationDefinition& resume = hook.Blob.Relocations[1];
        Require(entry.Offset == 1 && entry.Encoding == "abs32" && entry.Source.Kind == "module-export" &&
            entry.Source.ModuleName == "HelenGameHook.dll" && entry.Source.ExportName == "@HelenGraphicsDispatch@24", "Dispatch export relocation mismatch.");
        Require(resume.Offset == 6 && resume.Encoding == "rel32" && resume.Source.Kind == "hook-resume", "Dispatch resume relocation mismatch.");
        const std::filesystem::path blob_path = loaded->BuildDirectory / hook.Blob.AssetPath;
        std::ifstream blob(blob_path, std::ios::binary);
        const std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(blob), std::istreambuf_iterator<char>()};
        Require(bytes == std::vector<unsigned char>({0xBA,0,0,0,0,0xE9,0,0,0,0}), "Dispatch blob bytes mismatch.");
        const HMODULE module = LoadLibraryExW(argv[4], nullptr, DONT_RESOLVE_DLL_REFERENCES);
        Require(module != nullptr, "Cannot inspect candidate DLL exports.");
        const bool export_present = GetProcAddress(module, "@HelenGraphicsDispatch@24") != nullptr;
        const bool probe_present = GetProcAddress(module, "@HelenProbeDispatch@24") != nullptr;
        FreeLibrary(module);
        Require(export_present && !probe_present, "Candidate DLL lacks production dispatch or contains the experimental probe.");
        Require(loaded->Build.VirtualFiles.size() == 1, "Expected exactly one frontend replacement.");
        const helen::PackAssetResolver resolver(loaded->PackDirectory, loaded->BuildDirectory);
        helen::DeltaVirtualFileSource source(resolver, std::filesystem::path(argv[1]) / L"verification-cache", argv[2], loaded->Build.VirtualFiles[0]);
        std::ifstream target(argv[3], std::ios::binary);
        Require(static_cast<bool>(target), "Missing current-run frontend target.");
        const std::vector<char> expected{std::istreambuf_iterator<char>(target), std::istreambuf_iterator<char>()};
        std::vector<char> actual(expected.size());
        std::size_t bytes_read = 0;
        Require(source.Read(0, actual.data(), actual.size(), bytes_read) && bytes_read == expected.size() && actual == expected,
            "Native delta reconstruction differs from the current-run target.");
        std::cout << "BATMAN_DIRECT_PACKAGE_PASS\nBATMAN_DIRECT_DELTA_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
