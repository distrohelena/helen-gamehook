#include <HelenHook/Log.h>

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <system_error>
#include <vector>

namespace
{
    /**
     * @brief Shared mutex that serializes log-path updates and append operations.
     */
    std::mutex LogMutex;

    /**
     * @brief Current runtime log path used by the append helpers.
     */
    std::filesystem::path LogPath;

    /**
     * @brief Converts one UTF-16 message into UTF-8 for append-only file output.
     * @param message UTF-16 message text that should be converted.
     * @return UTF-8 encoded message text, or an empty string when conversion fails.
     */
    std::string ToUtf8(std::wstring_view message)
    {
        if (message.empty())
        {
            return {};
        }

        const int required_length = WideCharToMultiByte(
            CP_UTF8,
            0,
            message.data(),
            static_cast<int>(message.size()),
            nullptr,
            0,
            nullptr,
            nullptr);
        if (required_length <= 0)
        {
            return {};
        }

        std::string utf8_text(static_cast<std::size_t>(required_length), '\0');
        const int actual_length = WideCharToMultiByte(
            CP_UTF8,
            0,
            message.data(),
            static_cast<int>(message.size()),
            utf8_text.data(),
            required_length,
            nullptr,
            nullptr);
        if (actual_length != required_length)
        {
            return {};
        }

        return utf8_text;
    }

    /**
     * @brief Ensures the parent directory for the current log file exists before one append operation.
     * @param log_path Current log file path whose parent directory should be created when needed.
     */
    void EnsureParentDirectory(const std::filesystem::path& log_path)
    {
        if (log_path.empty())
        {
            return;
        }

        const std::filesystem::path parent_path = log_path.parent_path();
        if (parent_path.empty())
        {
            return;
        }

        std::error_code error_code;
        std::filesystem::create_directories(parent_path, error_code);
    }

    /**
     * @brief Appends one UTF-16 message plus a newline to the active log file when a path is configured.
     * @param message Message text that should be appended.
     */
    void AppendMessage(std::wstring_view message)
    {
        std::lock_guard<std::mutex> lock(LogMutex);
        if (LogPath.empty())
        {
            return;
        }

        EnsureParentDirectory(LogPath);

        std::ofstream stream(LogPath, std::ios::binary | std::ios::app);
        if (!stream)
        {
            return;
        }

        const std::string utf8_message = ToUtf8(message);
        stream.write(utf8_message.data(), static_cast<std::streamsize>(utf8_message.size()));
        stream.write("\r\n", 2);
    }
}

namespace helen
{
    /**
     * @brief Stores the active log file path used by later Log and Logf calls.
     * @param log_path Absolute or relative log file path that should receive future appended messages.
     */
    void SetLogPath(const std::filesystem::path& log_path)
    {
        std::lock_guard<std::mutex> lock(LogMutex);
        LogPath = log_path;
        EnsureParentDirectory(LogPath);
    }

    /**
     * @brief Returns the active log file path.
     * @return Shared reference to the stored log file path.
     */
    const std::filesystem::path& GetLogPath()
    {
        return LogPath;
    }

    /**
     * @brief Appends one message line to the active log file when logging is configured.
     * @param message Message text that should be appended exactly once.
     */
    void Log(std::wstring_view message)
    {
        AppendMessage(message);
    }

    /**
     * @brief Formats one wide-character message and appends it to the active log file.
     * @param format `printf`-style wide-character format string.
     */
    void Logf(const wchar_t* format, ...)
    {
        if (format == nullptr)
        {
            return;
        }

        va_list arguments;
        va_start(arguments, format);
        const int required_length = _vscwprintf(format, arguments);
        va_end(arguments);
        if (required_length <= 0)
        {
            return;
        }

        std::vector<wchar_t> buffer(static_cast<std::size_t>(required_length) + 1, L'\0');
        va_start(arguments, format);
        const int actual_length = vswprintf_s(buffer.data(), buffer.size(), format, arguments);
        va_end(arguments);
        if (actual_length <= 0)
        {
            return;
        }

        AppendMessage(std::wstring_view(buffer.data(), static_cast<std::size_t>(actual_length)));
    }
}
