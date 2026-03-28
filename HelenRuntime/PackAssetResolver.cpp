#include <HelenHook/PackAssetResolver.h>

#include <stdexcept>
#include <system_error>

namespace
{
    /**
     * @brief Returns whether one normalized path is equal to or nested beneath one normalized root path.
     * @param candidate Normalized candidate path that should stay inside the supplied root.
     * @param root Normalized root path that acts as the containment boundary.
     * @return True when every component of the root matches the leading components of the candidate path.
     */
    bool IsWithinRoot(const std::filesystem::path& candidate, const std::filesystem::path& root)
    {
        auto candidate_iterator = candidate.begin();
        auto root_iterator = root.begin();
        for (; root_iterator != root.end(); ++root_iterator, ++candidate_iterator)
        {
            if (candidate_iterator == candidate.end() || *candidate_iterator != *root_iterator)
            {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Normalizes one required existing directory through filesystem-aware canonicalization.
     * @param directory_path Directory path that must exist before the runtime can resolve assets beneath it.
     * @param empty_message Validation message reported when the supplied path is empty.
     * @param missing_message Validation message reported when the supplied directory does not exist.
     * @param directory_message Validation message reported when the supplied path exists but is not a directory.
     * @return Filesystem-aware normalized directory path.
     * @throws std::invalid_argument Thrown when the directory is empty, missing, not a directory, or cannot be normalized.
     */
    std::filesystem::path NormalizeExistingDirectory(
        const std::filesystem::path& directory_path,
        const char* empty_message,
        const char* missing_message,
        const char* directory_message)
    {
        if (directory_path.empty())
        {
            throw std::invalid_argument(empty_message);
        }

        std::error_code error_code;
        if (!std::filesystem::exists(directory_path, error_code))
        {
            throw std::invalid_argument(missing_message);
        }

        if (error_code)
        {
            throw std::invalid_argument(missing_message);
        }

        if (!std::filesystem::is_directory(directory_path, error_code))
        {
            throw std::invalid_argument(directory_message);
        }

        if (error_code)
        {
            throw std::invalid_argument(directory_message);
        }

        const std::filesystem::path normalized_directory = std::filesystem::weakly_canonical(directory_path, error_code);
        if (error_code || normalized_directory.empty())
        {
            throw std::invalid_argument(directory_message);
        }

        return normalized_directory;
    }

    /**
     * @brief Resolves one candidate asset path relative to one existing build root using filesystem-aware normalization.
     * @param build_root Existing normalized build root that acts as the resolution base.
     * @param relative_path Relative asset path declared by the selected pack build.
     * @return Filesystem-aware normalized candidate path when resolution succeeds; otherwise no value.
     */
    std::optional<std::filesystem::path> TryResolveCandidatePath(
        const std::filesystem::path& build_root,
        const std::filesystem::path& relative_path)
    {
        std::error_code error_code;
        const std::filesystem::path resolved_path = std::filesystem::weakly_canonical(build_root / relative_path, error_code);
        if (error_code || resolved_path.empty())
        {
            return std::nullopt;
        }

        return resolved_path;
    }
}

namespace helen
{
    PackAssetResolver::PackAssetResolver(const std::filesystem::path& pack_root, const std::filesystem::path& build_root)
        : pack_root_(NormalizeExistingDirectory(
              pack_root,
              "Pack asset resolver requires a non-empty pack root.",
              "Pack asset resolver requires an existing pack root directory.",
              "Pack asset resolver requires the pack root to be a directory.")),
          build_root_(NormalizeExistingDirectory(
              build_root,
              "Pack asset resolver requires a non-empty build root.",
              "Pack asset resolver requires an existing build root directory.",
              "Pack asset resolver requires the build root to be a directory."))
    {
        if (!IsWithinRoot(build_root_, pack_root_))
        {
            throw std::invalid_argument("Pack asset resolver requires the build root to stay inside the pack root.");
        }
    }

    std::optional<std::filesystem::path> PackAssetResolver::Resolve(const std::filesystem::path& relative_path) const
    {
        if (relative_path.empty() || relative_path.has_root_name() || relative_path.has_root_directory())
        {
            return std::nullopt;
        }

        const std::filesystem::path normalized_relative_path = relative_path.lexically_normal();
        if (normalized_relative_path.empty() || normalized_relative_path == std::filesystem::path("."))
        {
            return std::nullopt;
        }

        const std::optional<std::filesystem::path> resolved_path = TryResolveCandidatePath(build_root_, normalized_relative_path);
        if (!resolved_path.has_value() || !IsWithinRoot(*resolved_path, pack_root_))
        {
            return std::nullopt;
        }

        return resolved_path;
    }
}