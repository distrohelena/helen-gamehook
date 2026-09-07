#include <HelenHook/FileWriteRouteDefinition.h>
#include <HelenHook/FileWriteRouteResolver.h>
#include <HelenHook/PackRepository.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    /**
     * @brief Throws when one route declaration assertion is false.
     * @param condition Condition that must be true for the test to continue.
     * @param message Failure message identifying the violated parser contract.
     */
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Writes a UTF-8 manifest fixture to a temporary path.
     * @param path Destination path that should be replaced.
     * @param text Complete manifest payload.
     */
    void WriteAllText(const std::filesystem::path& path, std::string_view text)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::runtime_error("Failed to create route declaration fixture.");
        }

        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream)
        {
            throw std::runtime_error("Failed to write route declaration fixture.");
        }
    }

    /**
     * @brief Creates one split-pack fixture with the supplied build manifest.
     * @param packs_root Temporary repository root.
     * @param build_json Complete build.json payload.
     * @param build_id Build identifier used by the pack and directory.
     */
    void WritePackFixture(
        const std::filesystem::path& packs_root,
        std::string_view build_json,
        std::string_view build_id)
    {
        const std::filesystem::path build_root = packs_root / "route-pack" / "builds" / std::string(build_id);
        std::filesystem::create_directories(build_root);
        WriteAllText(
            build_root.parent_path().parent_path() / "pack.json",
            "{\"id\":\"route-pack\",\"name\":\"Route Pack\",\"targets\":[{\"executables\":[\"RouteGame.exe\"]}],\"builds\":[\"" +
                std::string(build_id) + "\"]}");
        WriteAllText(build_root / "build.json", build_json);
    }
}

/**
 * @brief Verifies route declarations are parsed strictly while old build manifests remain valid.
 */
void RunFileWriteRouteDeclarationTests()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "HelenRuntimeTests" / "FileWriteRouteDeclaration";
    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);
    std::filesystem::create_directories(root);

    const std::string valid_build =
        "{\"id\":\"route-build\",\"executable\":\"RouteGame.exe\",\"match\":{\"fileSize\":1,\"sha256\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"},"
        "\"fileWriteRoutes\":[{\"id\":\"engine-config\",\"root\":\"documents\",\"path\":\"Square Enix/Batman Arkham Asylum GOTY/BmGame/Config/BmEngine.ini\",\"writePolicy\":\"redirect\",\"readPolicy\":\"redirected\",\"lifetime\":\"session\"}]}";
    WritePackFixture(root / "valid", valid_build, "route-build");

    const helen::PackRepository repository;
    const std::optional<helen::LoadedBuildPack> loaded = repository.LoadForExecutable(
        root / "valid",
        "RouteGame.exe",
        1,
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    Expect(loaded.has_value(), "Valid route declaration pack was rejected.");
    Expect(loaded->Build.FileWriteRoutes.size() == 1, "Valid route declaration was not retained.");
    Expect(loaded->Build.FileWriteRoutes.front().Id == "engine-config", "Route id was not parsed.");
    Expect(loaded->Build.FileWriteRoutes.front().Root == "documents", "Route root was not parsed.");
    Expect(loaded->Build.FileWriteRoutes.front().Path.generic_string().find("Square Enix/") == 0, "Route UTF-8 path was not parsed.");
    Expect(loaded->Build.FileWriteRoutes.front().WritePolicy == helen::FileWritePolicy::Redirect, "Route write policy was not parsed.");
    Expect(loaded->Build.FileWriteRoutes.front().ReadPolicy == helen::FileReadPolicy::Redirected, "Route read policy was not parsed.");

    const std::string invalid_build =
        "{\"id\":\"route-build\",\"executable\":\"RouteGame.exe\",\"match\":{\"fileSize\":1,\"sha256\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"},"
        "\"fileWriteRoutes\":[{\"id\":\"engine-config\",\"root\":\"documents\",\"path\":\"BmEngine.ini\",\"writePolicy\":\"redirect\",\"readPolicy\":\"redirected\",\"lifetime\":\"persistent\"}]}";
    WritePackFixture(root / "invalid", invalid_build, "route-build");
    const std::optional<helen::LoadedBuildPack> rejected = repository.LoadForExecutable(
        root / "invalid",
        "RouteGame.exe",
        1,
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    Expect(!rejected.has_value(), "Invalid route lifetime was silently accepted.");
}

/**
 * @brief Verifies logical route roots resolve to injected temporary roots without changing UTF-8 paths.
 */
void RunFileWriteRouteResolverTests()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "HelenRuntimeTests" / "FileWriteRouteResolver";
    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);
    std::filesystem::create_directories(root / "game" / "BmGame" / "Config");
    std::filesystem::create_directories(root / "documents" / "Square Enix" / "Batman Arkham Asylum GOTY" / "BmGame" / "Config");

    const helen::FileWriteRouteDefinition documents_route = {
        "documents-route",
        "documents",
        std::filesystem::path("Square Enix") / "Batman Arkham Asylum GOTY" / "BmGame" / "Config" / "BmEngine.ini",
        helen::FileWritePolicy::Redirect,
        helen::FileReadPolicy::Redirected};
    const helen::FileWriteRouteDefinition game_route = {
        "game-route",
        "game",
        std::filesystem::path("BmGame") / "Config" / "BmEngine.ini",
        helen::FileWritePolicy::Deny,
        helen::FileReadPolicy::Original};
    const helen::FileWriteRouteResolver resolver(root / "game", root / "documents");
    std::vector<helen::FileWriteRoute> resolved_routes;
    std::string failure_reason;
    Expect(resolver.TryResolve({documents_route, game_route}, resolved_routes, failure_reason),
        "Injected route roots were not resolved.");
    Expect(resolved_routes.size() == 2, "Resolver did not retain every declared route.");
    Expect(resolved_routes.front().OriginalPath == (root / "documents" / documents_route.Path).lexically_normal(),
        "Documents route did not resolve to the injected known-folder root.");
    Expect(resolved_routes.back().OriginalPath == (root / "game" / game_route.Path).lexically_normal(),
        "Game route did not resolve to the injected installation root.");

    const helen::FileWriteRouteDefinition escaping_route = {
        "escaping-route",
        "documents",
        std::filesystem::path("..") / "outside.ini",
        helen::FileWritePolicy::Redirect,
        helen::FileReadPolicy::Redirected};
    resolved_routes.clear();
    failure_reason.clear();
    Expect(!resolver.TryResolve({escaping_route}, resolved_routes, failure_reason),
        "Resolver accepted a route escaping its logical root.");
}
