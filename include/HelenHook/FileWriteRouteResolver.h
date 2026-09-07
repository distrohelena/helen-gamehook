#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <HelenHook/FileWriteRoute.h>
#include <HelenHook/FileWriteRouteDefinition.h>

namespace helen
{
    /**
     * @brief Resolves pack-declared logical file roots into exact absolute runtime routes.
     *
     * The resolver accepts explicit roots so parser and runtime tests can use isolated temporary
     * fixtures. Production startup supplies the installation root and Windows Documents known
     * folder; no pack declaration can select an arbitrary output directory.
     */
    class FileWriteRouteResolver
    {
    public:
        /**
         * @brief Binds route resolution to the game installation and Documents roots.
         * @param game_root Absolute installation root used by `game` declarations.
         * @param documents_root Absolute known-folder root used by `documents` declarations.
         */
        FileWriteRouteResolver(std::filesystem::path game_root, std::filesystem::path documents_root);

        /**
         * @brief Resolves and validates all route declarations atomically.
         * @param definitions Pack declarations to resolve in declaration order.
         * @param routes Receives resolved routes only when every declaration succeeds.
         * @param failure_reason Receives a readable reason when resolution fails.
         * @return True when every logical root and relative path is safe and resolvable.
         */
        bool TryResolve(
            const std::vector<FileWriteRouteDefinition>& definitions,
            std::vector<FileWriteRoute>& routes,
            std::string& failure_reason) const;

    private:
        /** @brief Absolute installation root used by game declarations. */
        std::filesystem::path game_root_;

        /** @brief Absolute Documents known-folder root used by documents declarations. */
        std::filesystem::path documents_root_;
    };
}
