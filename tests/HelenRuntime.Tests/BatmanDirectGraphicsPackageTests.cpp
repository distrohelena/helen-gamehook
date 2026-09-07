#include <HelenHook/PackRepository.h>
#include <HelenHook/DeltaVirtualFileSource.h>
#include "BcryptAlgorithmHandle.h"
#include "BcryptHashHandle.h"
#include <bcrypt.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

namespace {
    /** @brief Fails the console verifier immediately when a shipping contract is violated. */
    void Require(bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    }

    /** @brief Reads a regular file into memory for the native candidate hash check. */
    std::vector<unsigned char> ReadFileBytes(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        Require(static_cast<bool>(file), "Cannot open candidate DLL for hashing.");
        return std::vector<unsigned char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    /** @brief Converts a native digest to the lowercase hexadecimal form used by build provenance. */
    std::string ToLowerHex(const std::vector<unsigned char>& digest) {
        static constexpr char digits[] = "0123456789abcdef";
        std::string result;
        result.reserve(digest.size() * 2);
        for (const unsigned char value : digest) {
            result.push_back(digits[value >> 4]);
            result.push_back(digits[value & 0x0f]);
        }
        return result;
    }

    /** @brief Converts the verifier's ASCII digest to the wide command-line representation. */
    std::wstring ToWideAscii(const std::string& value) {
        std::wstring result;
        result.reserve(value.size());
        for (const char character : value) {
            result.push_back(static_cast<wchar_t>(static_cast<unsigned char>(character)));
        }
        return result;
    }

    /** @brief Computes a candidate DLL SHA-256 digest through Windows CNG. */
    std::string Sha256File(const std::filesystem::path& path) {
        const std::vector<unsigned char> bytes = ReadFileBytes(path);
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        DWORD object_length = 0;
        DWORD result_length = 0;
        DWORD hash_length = 0;
        std::vector<unsigned char> object;
        std::vector<unsigned char> digest;
        NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        Require(status == 0, "Cannot open SHA-256 provider.");
        BcryptAlgorithmHandle algorithm_guard(algorithm);
        status = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_length), sizeof(object_length), &result_length, 0);
        Require(status == 0, "Cannot read SHA-256 object length.");
        status = BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hash_length), sizeof(hash_length), &result_length, 0);
        Require(status == 0, "Cannot read SHA-256 digest length.");
        object.resize(object_length);
        digest.resize(hash_length);
        status = BCryptCreateHash(algorithm, &hash, object.data(), object_length, nullptr, 0, 0);
        Require(status == 0, "Cannot create SHA-256 hash.");
        BcryptHashHandle hash_guard(hash);
        status = BCryptHashData(hash, const_cast<PUCHAR>(bytes.data()), static_cast<ULONG>(bytes.size()), 0);
        Require(status == 0, "Cannot hash candidate DLL.");
        status = BCryptFinishHash(hash, digest.data(), hash_length, 0);
        Require(status == 0, "Cannot finish candidate DLL hash.");
        return ToLowerHex(digest);
    }
}

/** @brief Parses the real candidate, checks direct-only hook ownership and reconstructs its exact frontend bytes. */
int wmain(int argc, wchar_t** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    try {
        Require(argc == 7, "Expected candidate pack parent, retail base, current target, candidate DLL, expected hash and route mode.");
        const std::wstring expected_hash(argv[5]);
        Require(ToWideAscii(Sha256File(argv[4])) == expected_hash, "Candidate DLL hash differs from the fresh-build provenance hash.");
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
        const std::wstring route_mode = argv[6];
        if (route_mode == L"none") {
            Require(loaded->Build.FileWriteRoutes.empty(), "No-route candidate unexpectedly declares file-write routes.");
        }
        else if (route_mode == L"engine-config") {
            Require(loaded->Build.FileWriteRoutes.size() == 1, "Routing candidate must declare exactly one file-write route.");
            const helen::FileWriteRouteDefinition& route = loaded->Build.FileWriteRoutes.front();
            Require(route.Id == "engine-config" && route.Root == "documents" &&
                route.Path.generic_string() == "Square Enix/Batman Arkham Asylum GOTY/BmGame/Config/BmEngine.ini" &&
                route.WritePolicy == helen::FileWritePolicy::Redirect &&
                route.ReadPolicy == helen::FileReadPolicy::Redirected,
                "Routing candidate route fields do not match the approved engine-config declaration.");
        }
        else {
            Require(false, "Unknown candidate route mode.");
        }
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
