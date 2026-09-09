#pragma once
#include <filesystem>
#include <string>

namespace helen {
    /** @brief Validates the captured engine-config cache key against Batman's live user root and protected file. */
    class BloomIniBinding {
    public:
        /** @brief Refuses unsupported logical names or a root mapping that does not identify the routed original INI. */
        static void Require(const std::wstring& logicalName, const std::filesystem::path& userRoot, const std::filesystem::path& original);
    };
}
