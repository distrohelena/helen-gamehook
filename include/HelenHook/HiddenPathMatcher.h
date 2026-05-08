#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace helen
{
    /**
     * @brief Normalizes game file paths and tests whether they belong to the active hidden-path set.
     *
     * The matcher accepts both absolute paths rooted under the game installation and relative game paths.
     * Comparisons are case-insensitive and slash-insensitive after normalization.
     */
    class HiddenPathMatcher
    {
    public:
        /**
         * @brief Creates one hidden-path matcher bound to a concrete game installation root.
         * @param game_root Absolute game installation root used to relativize incoming absolute paths.
         * @param hidden_paths Canonical relative paths that should be treated as hidden.
         */
        HiddenPathMatcher(std::filesystem::path game_root, std::vector<std::string> hidden_paths);

        /**
         * @brief Returns whether one candidate path should be treated as hidden.
         * @param candidate_path Runtime file path that may be relative or absolute.
         * @return True when the normalized path matches one declared hidden path exactly.
         */
        bool ShouldHidePath(const std::filesystem::path& candidate_path) const;

        /**
         * @brief Normalizes one candidate path into a folded relative game path.
         * @param candidate_path Runtime file path that may be relative or absolute.
         * @return Lowercased slash-normalized relative game path used for comparisons.
         */
        std::wstring NormalizeToRelativeGamePath(const std::filesystem::path& candidate_path) const;

    private:
        /** @brief Canonical absolute game installation root used to relativize incoming absolute paths. */
        std::filesystem::path game_root_;

        /** @brief Lowercased slash-normalized hidden paths stored as relative wide strings. */
        std::vector<std::wstring> hidden_paths_;
    };
}
