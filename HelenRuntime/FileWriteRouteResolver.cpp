#include <HelenHook/FileWriteRouteResolver.h>

#include <algorithm>
#include <cctype>
#include <set>

namespace
{
    /**
     * @brief Folds route identifiers and normalized target text for case-insensitive conflict checks.
     * @param value Text that should be folded.
     * @return Lowercase copy of value.
     */
    std::string FoldAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](char character) {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        });
        return value;
    }

    /**
     * @brief Returns whether a declaration path is one safe relative regular-file spelling.
     * @param path Path supplied by a pack definition.
     * @return True when path contains no root, traversal, empty component, or terminal directory.
     */
    bool IsSafeRelativePath(const std::filesystem::path& path)
    {
        if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory() || !path.has_filename())
        {
            return false;
        }

        for (const std::filesystem::path& component : path)
        {
            if (component.empty() || component == "." || component == "..")
            {
                return false;
            }
        }

        return path.generic_string().find(':') == std::string::npos;
    }

    /**
     * @brief Returns whether a normalized candidate stays beneath one normalized root.
     * @param candidate Candidate path produced by root/path lexical composition.
     * @param root Root path that candidate must remain under.
     * @return True when candidate is a strict descendant of root.
     */
    bool IsWithinRoot(const std::filesystem::path& candidate, const std::filesystem::path& root)
    {
        const std::filesystem::path relative = candidate.lexically_relative(root);
        if (relative.empty() || relative.is_absolute())
        {
            return false;
        }

        const auto first = relative.begin();
        return first == relative.end() || *first != "..";
    }
}

namespace helen
{
    FileWriteRouteResolver::FileWriteRouteResolver(
        std::filesystem::path game_root,
        std::filesystem::path documents_root)
        : game_root_(std::move(game_root)),
          documents_root_(std::move(documents_root))
    {
    }

    bool FileWriteRouteResolver::TryResolve(
        const std::vector<FileWriteRouteDefinition>& definitions,
        std::vector<FileWriteRoute>& routes,
        std::string& failure_reason) const
    {
        routes.clear();
        failure_reason.clear();
        const std::filesystem::path normalized_game_root = game_root_.lexically_normal();
        const std::filesystem::path normalized_documents_root = documents_root_.lexically_normal();
        bool requires_game_root = false;
        bool requires_documents_root = false;
        for (const FileWriteRouteDefinition& definition : definitions)
        {
            if (definition.Root == "game")
            {
                requires_game_root = true;
            }
            else if (definition.Root == "documents")
            {
                requires_documents_root = true;
            }
            else
            {
                failure_reason = "Route declaration selected an unsupported root.";
                return false;
            }
        }

        std::error_code error;
        if (requires_game_root && (normalized_game_root.empty() || !normalized_game_root.is_absolute() ||
            !std::filesystem::is_directory(normalized_game_root, error) || error)
            )
        {
            failure_reason = "Game route root is missing or not an absolute directory.";
            return false;
        }

        error.clear();
        if (requires_documents_root && (normalized_documents_root.empty() || !normalized_documents_root.is_absolute() ||
            !std::filesystem::is_directory(normalized_documents_root, error) || error)
            )
        {
            failure_reason = "Documents route root is missing or not an absolute directory.";
            return false;
        }

        std::set<std::string> route_ids;
        std::set<std::string> route_targets;
        for (const FileWriteRouteDefinition& definition : definitions)
        {
            if (definition.Id.empty() || !route_ids.insert(FoldAscii(definition.Id)).second ||
                !IsSafeRelativePath(definition.Path))
            {
                failure_reason = "Route declaration has an empty, duplicate, or unsafe identifier/path.";
                routes.clear();
                return false;
            }

            const std::filesystem::path root = definition.Root == "game"
                ? normalized_game_root
                : definition.Root == "documents"
                    ? normalized_documents_root
                    : std::filesystem::path();
            if (root.empty())
            {
                failure_reason = "Route declaration selected an unsupported root.";
                routes.clear();
                return false;
            }

            const std::filesystem::path original_path = (root / definition.Path).lexically_normal();
            if (!IsWithinRoot(original_path, root))
            {
                failure_reason = "Route declaration escapes its selected root.";
                routes.clear();
                return false;
            }

            const std::string target_key = FoldAscii(original_path.generic_string());
            if (!route_targets.insert(target_key).second)
            {
                failure_reason = "Route declarations target the same resolved file.";
                routes.clear();
                return false;
            }

            FileWriteRoute route;
            route.Id = definition.Id;
            route.OriginalPath = original_path;
            route.WritePolicy = definition.WritePolicy;
            route.ReadPolicy = definition.ReadPolicy;
            routes.push_back(std::move(route));
        }

        return true;
    }
}
