#pragma once
#include <filesystem>

namespace helen {
    /** @brief Owns one private INI staging file until routing consumes it; cleanup never touches originals. */
    class SessionStagingFile {
    private:
        /** @brief Unique sibling path allocated inside the current session directory. */
        std::filesystem::path Path;
    public:
        /** @brief Allocates and copies an existing session file, throwing on any incomplete initialization. */
        explicit SessionStagingFile(const std::filesystem::path& source);
        /** @brief Deletes only the owned staging path; successful publication has already consumed it. */
        ~SessionStagingFile() noexcept;
        SessionStagingFile(const SessionStagingFile&) = delete;
        SessionStagingFile& operator=(const SessionStagingFile&) = delete;
        /** @brief Returns the private staging filename for encoding and verified publication. */
        const std::filesystem::path& GetPath() const noexcept;
    };
}
