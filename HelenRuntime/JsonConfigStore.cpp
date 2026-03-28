#include <HelenHook/JsonConfigStore.h>

#include <HelenHook/JsonParser.h>
#include <HelenHook/JsonValue.h>

#include <array>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>
#include <windows.h>

namespace
{
    /**
     * @brief Reads the full contents of a text file into memory.
     * @param path Filesystem path to read.
     * @return Complete file contents as a UTF-8-compatible byte string.
     */
    std::string ReadAllText(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            throw std::runtime_error("Failed to open config file for reading.");
        }

        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    /**
     * @brief Writes one full text payload to a file path.
     * @param path Filesystem path that should receive the serialized config.
     * @param text Serialized JSON text to write.
     */
    void WriteAllText(const std::filesystem::path& path, std::string_view text)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::runtime_error("Failed to open config file for writing.");
        }

        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream)
        {
            throw std::runtime_error("Failed to write config file.");
        }
    }

    /**
     * @brief Creates one temporary file path in the destination directory for atomic replacement.
     * @param directory Existing directory that will contain both the temp file and final config file.
     * @return Temporary file path created by the operating system.
     */
    std::filesystem::path CreateTemporaryFilePath(const std::filesystem::path& directory)
    {
        std::array<wchar_t, MAX_PATH> buffer{};
        const UINT result = GetTempFileNameW(directory.c_str(), L"hhc", 0, buffer.data());
        if (result == 0)
        {
            throw std::runtime_error("Failed to allocate a temporary config file path.");
        }

        return std::filesystem::path(buffer.data());
    }

    /**
     * @brief Removes a temporary file without masking the original failure path.
     * @param path Temporary file path that should be deleted if it still exists.
     */
    void RemoveTemporaryFile(const std::filesystem::path& path) noexcept
    {
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    /**
     * @brief Escapes a string for JSON object key output.
     * @param text Raw string that should be serialized as a JSON string token.
     * @return Escaped JSON string contents without the surrounding quotes.
     */
    std::string EscapeJsonString(std::string_view text)
    {
        std::string escaped;
        escaped.reserve(text.size());

        for (const char character : text)
        {
            switch (character)
            {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\b':
                escaped += "\\b";
                break;
            case '\f':
                escaped += "\\f";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped.push_back(character);
                break;
            }
        }

        return escaped;
    }

    /**
     * @brief Converts a parsed JSON numeric value into a checked 32-bit integer.
     * @param value Parsed JSON value that must contain an integral number.
     * @param key Config key being loaded for error reporting.
     * @return Integer value represented by the JSON number.
     */
    int ParseIntValue(const helen::JsonValue& value, std::string_view key)
    {
        const std::optional<double> number = value.AsNumber();
        if (!number.has_value())
        {
            throw std::runtime_error("Config value is not numeric.");
        }

        double integral_part = 0.0;
        const double fractional_part = std::modf(*number, &integral_part);
        if (fractional_part != 0.0)
        {
            throw std::runtime_error("Config value is not an integer.");
        }

        if (integral_part < static_cast<double>(std::numeric_limits<int>::min()) ||
            integral_part > static_cast<double>(std::numeric_limits<int>::max()))
        {
            throw std::runtime_error("Config value is outside the supported integer range.");
        }

        (void)key;
        return static_cast<int>(integral_part);
    }

    /**
     * @brief Serializes the current integer config map as a flat JSON object.
     * @param int_values Current config entries that should be written to disk.
     * @return Complete JSON document text with trailing newline.
     */
    std::string SerializeConfig(const std::map<std::string, int>& int_values)
    {
        std::ostringstream stream;
        stream << "{\n";

        bool first = true;
        for (const auto& [key, value] : int_values)
        {
            if (!first)
            {
                stream << ",\n";
            }

            first = false;
            stream << "  \"" << EscapeJsonString(key) << "\": " << value;
        }

        stream << "\n}\n";
        return stream.str();
    }
}

namespace helen
{
    JsonConfigStore::JsonConfigStore(std::filesystem::path path)
        : path_(std::move(path))
    {
        Load();
    }

    int JsonConfigStore::GetInt(const std::string& key, int default_value) const
    {
        const auto found = int_values_.find(key);
        return found != int_values_.end() ? found->second : default_value;
    }

    void JsonConfigStore::SetInt(const std::string& key, int value)
    {
        int_values_[key] = value;
    }

    void JsonConfigStore::Save() const
    {
        const std::filesystem::path directory = path_.has_parent_path()
            ? path_.parent_path()
            : std::filesystem::current_path();
        if (path_.has_parent_path())
        {
            std::filesystem::create_directories(directory);
        }

        const std::string serializedConfig = SerializeConfig(int_values_);
        const std::filesystem::path temporaryPath = CreateTemporaryFilePath(directory);
        try
        {
            WriteAllText(temporaryPath, serializedConfig);
        }
        catch (...)
        {
            RemoveTemporaryFile(temporaryPath);
            throw;
        }

        if (!MoveFileExW(temporaryPath.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            RemoveTemporaryFile(temporaryPath);
            throw std::runtime_error("Failed to replace the config file.");
        }
    }

    void JsonConfigStore::Load()
    {
        int_values_.clear();
        if (!std::filesystem::exists(path_))
        {
            return;
        }

        const std::string text = ReadAllText(path_);
        const std::optional<JsonValue> parsed = JsonParser::Parse(text);
        if (!parsed.has_value())
        {
            throw std::runtime_error("Config file does not contain valid JSON.");
        }

        const JsonValue::Object* object = parsed->AsObject();
        if (object == nullptr)
        {
            throw std::runtime_error("Config file root must be a JSON object.");
        }

        for (const auto& [key, value] : *object)
        {
            int_values_.emplace(key, ParseIntValue(value, key));
        }
    }
}
