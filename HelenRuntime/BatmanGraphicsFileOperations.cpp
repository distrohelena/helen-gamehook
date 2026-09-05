#include <HelenHook/BatmanGraphicsFileOperations.h>
#include <HelenHook/Log.h>
#include <windows.h>

namespace helen {
    BatmanGraphicsFileOperations& BatmanGraphicsFileOperations::Native() {
        /** @brief Stateless Win32 operations shared by production config services for their whole lifetime. */
        static BatmanGraphicsFileOperations operations;
        return operations;
    }

    bool BatmanGraphicsFileOperations::Replace(
        const std::filesystem::path& target, const std::filesystem::path& staged, unsigned long& error) const {
        const bool succeeded = ReplaceFileW(target.c_str(), staged.c_str(), nullptr, 0, nullptr, nullptr) != FALSE;
        error = succeeded ? ERROR_SUCCESS : GetLastError();
        return succeeded;
    }

    bool BatmanGraphicsFileOperations::Restore(
        const std::filesystem::path& target, const std::filesystem::path& candidate, unsigned long& error) const {
        const bool succeeded = MoveFileExW(candidate.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
        error = succeeded ? ERROR_SUCCESS : GetLastError();
        return succeeded;
    }

    bool BatmanGraphicsFileOperations::Remove(const std::filesystem::path& path) const {
        if (DeleteFileW(path.c_str()) != FALSE) {
            return true;
        }
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) {
            return true;
        }
        Logf(L"[graphics] Unable to clean owned transaction path=%ls error=%lu.", path.c_str(), error);
        return false;
    }
}
