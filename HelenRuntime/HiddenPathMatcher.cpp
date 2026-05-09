#include <HelenHook/HiddenPathMatcher.h>

#include <algorithm>
#include <cwctype>
#include <stdexcept>

namespace
{
    /**
     * @brief Rewrites one filesystem path string into slash-normalized lowercase comparison form.
     * @param value Path text that should be normalized.
     * @return Lowercased path text with forward slashes and no leading slash.
     */
    std::wstring FoldPath(std::wstring value)
    {
        std::replace(value.begin(), value.end(), L'\\', L'/');
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });

        while (!value.empty() && value.front() == L'/')
        {
            value.erase(value.begin());
        }

        return value;
    }
}

namespace helen
{
    HiddenPathMatcher::HiddenPathMatcher(std::filesystem::path game_root, std::vector<std::string> hidden_paths)
        : game_root_(std::filesystem::weakly_canonical(std::move(game_root)))
    {
        if (game_root_.empty())
        {
            throw std::invalid_argument("HiddenPathMatcher requires a non-empty game root.");
        }

        for (const std::string& hidden_path : hidden_paths)
        {
            hidden_paths_.push_back(FoldPath(std::filesystem::path(hidden_path).wstring()));
        }
    }

    bool HiddenPathMatcher::ShouldHidePath(const std::filesystem::path& candidate_path) const
    {
        const std::wstring normalized = NormalizeToRelativeGamePath(candidate_path);
        return std::find(hidden_paths_.begin(), hidden_paths_.end(), normalized) != hidden_paths_.end();
    }

    std::wstring HiddenPathMatcher::NormalizeToRelativeGamePath(const std::filesystem::path& candidate_path) const
    {
        std::filesystem::path normalized_candidate = candidate_path;
        if (normalized_candidate.is_absolute())
        {
            normalized_candidate = normalized_candidate.lexically_relative(game_root_);
        }

        return FoldPath(normalized_candidate.wstring());
    }
}
