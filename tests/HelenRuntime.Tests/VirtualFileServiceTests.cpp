#include <HelenHook/FullFileVirtualFileSource.h>
#include <HelenHook/PackScopedVirtualFileRegistration.h>
#include <HelenHook/PackAssetResolver.h>
#include <HelenHook/VirtualFileDefinition.h>
#include <HelenHook/VirtualFileService.h>
#include <HelenHook/VirtualFileSourceKind.h>

#include <Windows.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace
{
    /**
     * @brief Throws when one required boolean condition is false so the shared test harness stops at the first failure.
     * @param condition Boolean condition that must evaluate to true.
     * @param message Failure message reported by the shared test runner.
     */
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Writes one exact binary payload to disk for virtual-file replacement scenarios.
     * @param path Destination file path that should be created or replaced.
     * @param bytes Binary payload written into the file.
     */
    void WriteAllBytes(const std::filesystem::path& path, std::string_view bytes)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::runtime_error("Failed to create a virtual file test asset.");
        }

        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!stream)
        {
            throw std::runtime_error("Failed to write a virtual file test asset.");
        }
    }

    /**
     * @brief Builds one replace-on-read virtual-file definition for the supplied game path and asset path.
     * @param game_path Game-relative path that should be virtualized.
     * @param asset_path Build-relative replacement asset path loaded by the runtime.
     * @return Fully populated virtual-file definition.
     */
    helen::VirtualFileDefinition CreateVirtualFileDefinition(const char* game_path, const char* asset_path)
    {
        helen::VirtualFileDefinition definition;
        definition.Id = "bmgameGameplayPackage";
        definition.GamePath = game_path;
        definition.Mode = "replace-on-read";
        definition.Source.Path = asset_path;
        return definition;
    }

    /**
     * @brief Builds one pack-scoped virtual-file registration for the supplied resolver and declaration.
     * @param pack_id Stable synthetic pack identifier used by diagnostics.
     * @param build_id Stable synthetic build identifier used by diagnostics.
     * @param resolver Pack-local asset resolver that owns the declared source asset.
     * @param definition Virtual-file declaration that should be registered.
     * @return Fully populated pack-scoped registration.
     */
    helen::PackScopedVirtualFileRegistration CreatePackScopedRegistration(
        const char* pack_id,
        const char* build_id,
        const helen::PackAssetResolver& resolver,
        const helen::VirtualFileDefinition& definition)
    {
        return helen::PackScopedVirtualFileRegistration(pack_id, build_id, resolver, definition);
    }

    /**
     * @brief Builds one delta-on-read virtual-file definition for registration validation scenarios.
     * @param game_path Game-relative path that should be virtualized.
     * @param asset_path Build-relative hgdelta asset path declared by the virtual file.
     * @return Fully populated delta-backed virtual-file definition with exact placeholder fingerprints.
     */
    helen::VirtualFileDefinition CreateDeltaVirtualFileDefinition(const char* game_path, const char* asset_path)
    {
        helen::VirtualFileDefinition definition;
        definition.Id = "bmgameGameplayPackageDelta";
        definition.GamePath = game_path;
        definition.Mode = "delta-on-read";
        definition.Source.Kind = helen::VirtualFileSourceKind::DeltaFile;
        definition.Source.Path = asset_path;
        definition.Source.Base.FileSize = 8;
        definition.Source.Base.Sha256 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        definition.Source.Target.FileSize = 8;
        definition.Source.Target.Sha256 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
        definition.Source.ChunkSize = 4;
        return definition;
    }
}

/**
 * @brief Verifies that RAM-backed virtual files register, match by normalized suffix, and expose read, seek, size, and mapping behavior.
 */
void RunVirtualFileServiceTests()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "HelenRuntimeTests" / "VirtualFileService";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    try
    {
        const std::filesystem::path pack_root = root / "pack";
        const std::filesystem::path build_root = pack_root / "builds" / "steam-goty-1.0";
        const std::filesystem::path second_pack_root = root / "pack-b";
        const std::filesystem::path second_build_root = second_pack_root / "builds" / "steam-goty-1.0";
        const std::filesystem::path cache_directory = root / "helengamehook" / "cache";
        const std::filesystem::path asset_path = build_root / "assets" / "packages" / "BmGame-subtitle-signal.u";
        const std::filesystem::path second_asset_path = second_build_root / "assets" / "packages" / "Frontend.umap";
        std::filesystem::create_directories(asset_path.parent_path());
        std::filesystem::create_directories(second_asset_path.parent_path());
        WriteAllBytes(asset_path, "ABCDE");
        WriteAllBytes(second_asset_path, "VWXYZ");

        const helen::PackAssetResolver resolver(pack_root, build_root);
        const helen::PackAssetResolver second_resolver(second_pack_root, second_build_root);
        helen::VirtualFileService service(cache_directory);

        const helen::VirtualFileDefinition definition = CreateVirtualFileDefinition(
            "BmGame/CookedPC/BmGame.u",
            "assets/packages/BmGame-subtitle-signal.u");
        const helen::PackScopedVirtualFileRegistration registration = CreatePackScopedRegistration(
            "pack",
            "steam-goty-1.0",
            resolver,
            definition);
        Expect(service.RegisterVirtualFile(registration), "Expected the virtual gameplay package to register.");
        Expect(!service.RegisterVirtualFile(registration), "Duplicate virtual file registration unexpectedly succeeded.");

        helen::VirtualFileDefinition invalid_definition = definition;
        invalid_definition.GamePath = "../escape.u";
        const helen::PackScopedVirtualFileRegistration invalid_registration = CreatePackScopedRegistration(
            "pack",
            "steam-goty-1.0",
            resolver,
            invalid_definition);
        Expect(!service.RegisterVirtualFile(invalid_registration), "Escaping virtual file path unexpectedly registered.");

        const helen::VirtualFileDefinition missing_delta_definition = CreateDeltaVirtualFileDefinition(
            "BmGame/CookedPC/BmGame-Delta-Missing.u",
            "assets/deltas/Missing.hgdelta");
        const helen::PackScopedVirtualFileRegistration missing_delta_registration = CreatePackScopedRegistration(
            "pack",
            "steam-goty-1.0",
            resolver,
            missing_delta_definition);
        Expect(!service.RegisterVirtualFile(missing_delta_registration), "Missing delta asset unexpectedly registered as a virtual file.");

        const std::filesystem::path malformed_delta_path = build_root / "assets" / "deltas" / "Malformed.hgdelta";
        std::filesystem::create_directories(malformed_delta_path.parent_path());
        WriteAllBytes(malformed_delta_path, "BAD");
        const helen::VirtualFileDefinition malformed_delta_definition = CreateDeltaVirtualFileDefinition(
            "BmGame/CookedPC/BmGame-Delta-Malformed.u",
            "assets/deltas/Malformed.hgdelta");
        const helen::PackScopedVirtualFileRegistration malformed_delta_registration = CreatePackScopedRegistration(
            "pack",
            "steam-goty-1.0",
            resolver,
            malformed_delta_definition);
        Expect(!service.RegisterVirtualFile(malformed_delta_registration), "Malformed delta asset unexpectedly registered as a virtual file.");

        const helen::VirtualFileDefinition second_definition = CreateVirtualFileDefinition(
            "BmGame/CookedPC/Maps/Frontend/Frontend.umap",
            "assets/packages/Frontend.umap");
        const helen::PackScopedVirtualFileRegistration second_registration = CreatePackScopedRegistration(
            "pack-b",
            "steam-goty-1.0",
            second_resolver,
            second_definition);
        Expect(service.RegisterVirtualFile(second_registration), "Expected the second pack-owned virtual file to register.");

        const std::optional<HANDLE> missing_handle = service.OpenVirtualFile("BmGame/CookedPC/Missing.u");
        Expect(!missing_handle.has_value(), "Missing virtual file path unexpectedly opened.");

        const std::optional<HANDLE> handle = service.OpenVirtualFile(R"(C:\Games\Batman Arkham Asylum GOTY\BmGame\CookedPC\BmGame.u)");
        Expect(handle.has_value(), "Expected the absolute gameplay package path to match the registered suffix.");
        Expect(service.IsVirtualHandle(*handle), "Expected the returned gameplay package handle to be virtual.");

        const std::optional<HANDLE> second_handle = service.OpenVirtualFile("BmGame/CookedPC/Maps/Frontend/Frontend.umap");
        Expect(second_handle.has_value(), "Expected the second pack-owned virtual file to open.");

        std::array<char, 6> second_pack_read{};
        DWORD second_pack_bytes_read = 0;
        Expect(service.Read(*second_handle, second_pack_read.data(), 5, &second_pack_bytes_read), "Expected the second pack-owned virtual file read to succeed.");
        Expect(second_pack_bytes_read == 5, "Second pack-owned virtual file read byte count mismatch.");
        Expect(std::string_view(second_pack_read.data(), 5) == "VWXYZ", "Second pack-owned virtual file payload mismatch.");

        LARGE_INTEGER size{};
        Expect(service.GetSize(*handle, &size), "Expected GetSize to succeed for the virtual gameplay package.");
        Expect(size.QuadPart == 5, "Virtual gameplay package size mismatch.");

        std::array<char, 4> first_read{};
        DWORD bytes_read = 0;
        Expect(service.Read(*handle, first_read.data(), 3, &bytes_read), "Expected the first virtual file read to succeed.");
        Expect(bytes_read == 3, "First virtual file read byte count mismatch.");
        Expect(std::string_view(first_read.data(), 3) == "ABC", "First virtual file read payload mismatch.");

        LARGE_INTEGER seek_distance{};
        seek_distance.QuadPart = 1;
        LARGE_INTEGER new_position{};
        Expect(service.Seek(*handle, seek_distance, FILE_BEGIN, &new_position), "Expected FILE_BEGIN seek to succeed.");
        Expect(new_position.QuadPart == 1, "FILE_BEGIN seek position mismatch.");

        std::array<char, 5> second_read{};
        bytes_read = 0;
        Expect(service.Read(*handle, second_read.data(), 4, &bytes_read), "Expected the second virtual file read to succeed.");
        Expect(bytes_read == 4, "Second virtual file read byte count mismatch.");
        Expect(std::string_view(second_read.data(), 4) == "BCDE", "Second virtual file read payload mismatch.");

        const std::optional<HANDLE> mapping_handle = service.CreateFileMapping(*handle, PAGE_READONLY, 0, 0);
        Expect(mapping_handle.has_value(), "Expected CreateFileMapping to succeed for the virtual gameplay package.");
        void* const mapping_view = MapViewOfFile(*mapping_handle, FILE_MAP_READ, 0, 0, 5);
        Expect(mapping_view != nullptr, "Expected MapViewOfFile to succeed for the virtual gameplay package mapping.");
        Expect(std::string_view(static_cast<const char*>(mapping_view), 5) == "ABCDE", "Mapped virtual file payload mismatch.");
        UnmapViewOfFile(mapping_view);
        CloseHandle(*mapping_handle);

        helen::FullFileVirtualFileSource full_file_source(std::vector<std::uint8_t>{ 'A', 'B', 'C', 'D', 'E' });
        Expect(full_file_source.GetSize() == 5, "Full-file source size mismatch.");

        std::array<char, 5> source_bytes{};
        std::size_t source_bytes_read = 0;
        Expect(full_file_source.Read(0, source_bytes.data(), source_bytes.size(), source_bytes_read), "Expected the full-file source read to succeed.");
        Expect(source_bytes_read == 5, "Full-file source read byte count mismatch.");
        Expect(std::string_view(source_bytes.data(), 5) == "ABCDE", "Full-file source read payload mismatch.");

        const std::optional<HANDLE> source_mapping_handle = full_file_source.CreateFileMapping(PAGE_READONLY, 0, 0);
        Expect(source_mapping_handle.has_value(), "Expected CreateFileMapping to succeed for the full-file source.");
        void* const source_mapping_view = MapViewOfFile(*source_mapping_handle, FILE_MAP_READ, 0, 0, 5);
        Expect(source_mapping_view != nullptr, "Expected MapViewOfFile to succeed for the full-file source mapping.");
        Expect(std::string_view(static_cast<const char*>(source_mapping_view), 5) == "ABCDE", "Mapped full-file source payload mismatch.");
        UnmapViewOfFile(source_mapping_view);
        CloseHandle(*source_mapping_handle);

        Expect(service.Close(*handle), "Expected Close to succeed for the virtual gameplay package.");
        Expect(!service.IsVirtualHandle(*handle), "Closed gameplay package handle is still tracked as virtual.");
        Expect(!service.Close(*handle), "Closing the same virtual gameplay package handle twice unexpectedly succeeded.");
        Expect(service.Close(*second_handle), "Expected Close to succeed for the second pack-owned virtual file.");
    }
    catch (...)
    {
        std::filesystem::remove_all(root);
        throw;
    }

    std::filesystem::remove_all(root);
}
