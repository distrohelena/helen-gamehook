#include <HelenHook/PackAssetResolver.h>

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
     * @brief Writes one exact UTF-8 text payload to disk for resolver path scenarios that need an existing file.
     * @param path Destination file path that should be created or replaced.
     * @param text Exact text content written into the file.
     */
    void WriteAllText(const std::filesystem::path& path, std::string_view text)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::runtime_error("Failed to create a pack asset resolver test file.");
        }

        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream)
        {
            throw std::runtime_error("Failed to write a pack asset resolver test file.");
        }
    }
}

/**
 * @brief Verifies that asset resolution stays inside the active pack root while still allowing build-local and shared assets.
 */
void RunPackAssetResolverTests()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "HelenRuntimeTests" / "PackAssetResolver";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    try
    {
        const std::filesystem::path pack_root = root / "pack";
        const std::filesystem::path build_root = pack_root / "builds" / "steam-goty-1.0";
        const std::filesystem::path build_asset = build_root / "assets" / "native" / "hook.bin";
        const std::filesystem::path shared_asset = pack_root / "shared" / "subtitle.bin";
        const std::filesystem::path outside_directory = root / "outside";

        std::filesystem::create_directories(build_asset.parent_path());
        std::filesystem::create_directories(shared_asset.parent_path());
        std::filesystem::create_directories(outside_directory);
        WriteAllText(build_asset, "hook");
        WriteAllText(shared_asset, "shared");

        const helen::PackAssetResolver resolver(pack_root, build_root);

        const std::optional<std::filesystem::path> build_local_asset = resolver.Resolve("assets/native/hook.bin");
        Expect(build_local_asset.has_value(), "Build-local asset path did not resolve.");
        Expect(std::filesystem::equivalent(*build_local_asset, build_asset), "Build-local asset path resolved to the wrong location.");

        const std::optional<std::filesystem::path> shared_pack_asset = resolver.Resolve("../../shared/subtitle.bin");
        Expect(shared_pack_asset.has_value(), "Shared pack asset path did not resolve.");
        Expect(std::filesystem::equivalent(*shared_pack_asset, shared_asset), "Shared pack asset path resolved to the wrong location.");

        Expect(!resolver.Resolve("").has_value(), "Empty asset path unexpectedly resolved.");
        Expect(!resolver.Resolve("../../../outside/escape.bin").has_value(), "Escaping asset path unexpectedly resolved outside the pack root.");
        Expect(!resolver.Resolve(R"(C:\outside\escape.bin)").has_value(), "Absolute asset path unexpectedly resolved.");

        bool constructor_threw = false;
        try
        {
            const helen::PackAssetResolver invalid_resolver(pack_root, outside_directory);
            static_cast<void>(invalid_resolver);
        }
        catch (const std::invalid_argument&)
        {
            constructor_threw = true;
        }

        Expect(constructor_threw, "Resolver unexpectedly accepted a build root outside the pack root.");
    }
    catch (...)
    {
        std::filesystem::remove_all(root);
        throw;
    }

    std::filesystem::remove_all(root);
}
