#include "SessionStagingFile.h"
#include <windows.h>
#include <stdexcept>

namespace helen {
    SessionStagingFile::SessionStagingFile(const std::filesystem::path& source) {
        wchar_t temporary[MAX_PATH];
        if (!GetTempFileNameW(source.parent_path().c_str(),L"slv",0,temporary)) {
            throw std::runtime_error("Private session staging allocation failed");
        }
        Path = temporary;
        if (!CopyFileW(source.c_str(),Path.c_str(),FALSE)) {
            const DWORD error = GetLastError();
            DeleteFileW(Path.c_str());
            throw std::runtime_error("Private session staging copy failed: " + std::to_string(error));
        }
    }
    SessionStagingFile::~SessionStagingFile() noexcept {
        std::error_code error;
        std::filesystem::remove(Path,error);
    }
    const std::filesystem::path& SessionStagingFile::GetPath() const noexcept { return Path; }
}
