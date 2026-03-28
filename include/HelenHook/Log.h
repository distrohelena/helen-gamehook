#pragma once

#include <filesystem>
#include <string_view>

namespace helen
{
    void SetLogPath(const std::filesystem::path& log_path);
    const std::filesystem::path& GetLogPath();
    void Log(std::wstring_view message);
    void Logf(const wchar_t* format, ...);
}
