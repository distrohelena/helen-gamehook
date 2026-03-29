#pragma once

#include <filesystem>
#include <optional>

namespace helen
{
    /**
     * @brief Resolves build-scoped asset paths while preventing access outside the active pack root.
     */
    class PackAssetResolver
    {
    public:
        /**
         * @brief Creates an asset resolver for one active pack directory and its selected build directory.
         * @param pack_root Top-level pack directory that acts as the safety boundary for resolved assets.
         * @param build_root Active build directory used as the base for build-local asset paths.
         * @throws std::invalid_argument Thrown when either root is empty, missing, not a directory, cannot be normalized with filesystem-aware resolution, or when the build root does not stay inside the pack root.
         */
        PackAssetResolver(const std::filesystem::path& pack_root, const std::filesystem::path& build_root);

        /**
         * @brief Resolves one build-local or shared asset path when the normalized result stays inside the active pack root.
         * @param relative_path Asset path declared by build metadata, relative to the active build directory.
         * @return Normalized resolved path when the path is relative and remains inside the pack root; otherwise no value.
         */
        std::optional<std::filesystem::path> Resolve(const std::filesystem::path& relative_path) const;

        /**
         * @brief Returns the normalized top-level pack directory that bounds all resolved asset paths.
         * @return Normalized pack root path used by this resolver.
         */
        const std::filesystem::path& GetPackRoot() const noexcept;

        /**
         * @brief Returns the normalized active build directory used to resolve build-local asset paths.
         * @return Normalized build root path used by this resolver.
         */
        const std::filesystem::path& GetBuildRoot() const noexcept;

    private:
        /** @brief Normalized top-level pack directory used as the containment boundary for asset resolution. */
        std::filesystem::path pack_root_;

        /** @brief Normalized active build directory used as the base for build-local asset resolution. */
        std::filesystem::path build_root_;
    };
}
