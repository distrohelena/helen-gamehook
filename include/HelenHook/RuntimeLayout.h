#pragma once

#include <filesystem>

namespace helen
{
    /**
     * @brief Describes the directory layout derived from the runtime module location.
     */
    class RuntimeLayout
    {
    public:
        /**
         * @brief Gets the game installation root inferred from the runtime module path.
         */
        std::filesystem::path GameRoot;

        /**
         * @brief Gets the helengamehook working root beneath the game installation.
         */
        std::filesystem::path HelenRoot;

        /**
         * @brief Gets the directory used to store patch pack data.
         */
        std::filesystem::path PacksDirectory;

        /**
         * @brief Gets the directory used to store configuration files.
         */
        std::filesystem::path ConfigDirectory;

        /**
         * @brief Gets the directory used to store runtime logs.
         */
        std::filesystem::path LogsDirectory;

        /**
         * @brief Gets the directory used to store extracted or generated assets.
         */
        std::filesystem::path AssetsDirectory;

        /**
         * @brief Gets the directory used for cache data.
         */
        std::filesystem::path CacheDirectory;

        /**
         * @brief Derives the runtime layout from the absolute path to the loaded runtime module.
         * @param runtime_module_path The full path to the runtime module that was loaded by the game.
         * @return A layout object with all standard runtime directories populated.
         */
        static RuntimeLayout FromRuntimeModulePath(const std::filesystem::path& runtime_module_path);
    };
}