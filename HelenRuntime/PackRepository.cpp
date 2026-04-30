#include <HelenHook/PackRepository.h>

#include <HelenHook/BuildDefinition.h>
#include <HelenHook/CommandMapEntryDefinition.h>
#include <HelenHook/ExternalBindingDefinition.h>
#include <HelenHook/FeatureDefinition.h>
#include <HelenHook/HookBlobDefinition.h>
#include <HelenHook/HookBlobRelocationDefinition.h>
#include <HelenHook/HookBlobRelocationSourceDefinition.h>
#include <HelenHook/HookDefinition.h>
#include <HelenHook/JsonParser.h>
#include <HelenHook/Log.h>
#include <HelenHook/MemoryStateObserverCheckDefinition.h>
#include <HelenHook/RuntimeSlotDefinition.h>
#include <HelenHook/VirtualFileHashDefinition.h>
#include <HelenHook/VirtualFileSourceDefinition.h>
#include <HelenHook/VirtualFileSourceKind.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>

namespace
{
    /**
     * @brief Returns whether two ASCII strings match case-insensitively.
     * @param left First string to compare.
     * @param right Second string to compare.
     * @return True when the strings match ignoring ASCII case; otherwise false.
     */
    bool EqualsAsciiIgnoreCase(std::string_view left, std::string_view right)
    {
        if (left.size() != right.size())
        {
            return false;
        }

        for (std::size_t index = 0; index < left.size(); ++index)
        {
            const unsigned char left_character = static_cast<unsigned char>(left[index]);
            const unsigned char right_character = static_cast<unsigned char>(right[index]);
            if (std::tolower(left_character) != std::tolower(right_character))
            {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Converts one ASCII string into lowercase.
     * @param text ASCII text that should be normalized.
     * @return Lowercase copy of the supplied text.
     */
    std::string ToLowerAscii(std::string text)
    {
        for (char& character : text)
        {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }

        return text;
    }

    /**
     * @brief Reads one complete UTF-8 JSON file into memory and parses it.
     * @param path JSON file path that should be loaded.
     * @param value Receives the parsed JSON root value when loading succeeds.
     * @return True when the file exists and contains valid JSON; otherwise false.
     */
    bool TryReadJsonFile(const std::filesystem::path& path, helen::JsonValue& value)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            return false;
        }

        const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        const std::optional<helen::JsonValue> parsed = helen::JsonParser::Parse(text);
        if (!parsed.has_value())
        {
            return false;
        }

        value = *parsed;
        return true;
    }

    /**
     * @brief Returns one named object member when the supplied JSON value is an object.
     * @param value JSON object that should contain the requested member.
     * @param key Member name to locate.
     * @return Pointer to the member value when present; otherwise nullptr.
     */
    const helen::JsonValue* FindObjectMember(const helen::JsonValue& value, std::string_view key)
    {
        if (!value.IsObject())
        {
            return nullptr;
        }

        return value.FindMember(key);
    }

    /**
     * @brief Returns one JSON string member as a C++ string.
     * @param value JSON value that should contain a string.
     * @return Stored string when the value is a string; otherwise no value.
     */
    std::optional<std::string> TryGetString(const helen::JsonValue* value)
    {
        if (value == nullptr)
        {
            return std::nullopt;
        }

        const std::string* string_value = value->AsString();
        if (string_value == nullptr)
        {
            return std::nullopt;
        }

        return *string_value;
    }

    /**
     * @brief Returns one JSON integer member when the stored number is integral and in range.
     * @param value JSON value that should contain an integral number.
     * @return Stored integer value when conversion succeeds; otherwise no value.
     */
    std::optional<int> TryGetInt(const helen::JsonValue* value)
    {
        if (value == nullptr)
        {
            return std::nullopt;
        }

        const std::optional<double> number = value->AsNumber();
        if (!number.has_value() || !std::isfinite(*number) || std::floor(*number) != *number)
        {
            return std::nullopt;
        }

        if (*number < static_cast<double>((std::numeric_limits<int>::min)()) ||
            *number > static_cast<double>((std::numeric_limits<int>::max)()))
        {
            return std::nullopt;
        }

        return static_cast<int>(*number);
    }

    /**
     * @brief Returns one JSON unsigned size value when the stored number is integral and non-negative.
     * @param value JSON value that should contain an unsigned integer.
     * @return Stored size value when conversion succeeds; otherwise no value.
     */
    std::optional<std::size_t> TryGetSizeValue(const helen::JsonValue* value)
    {
        if (value == nullptr)
        {
            return std::nullopt;
        }

        const std::optional<double> number = value->AsNumber();
        if (!number.has_value() || !std::isfinite(*number) || std::floor(*number) != *number || *number < 0.0)
        {
            return std::nullopt;
        }

        if (*number > static_cast<double>((std::numeric_limits<std::size_t>::max)()))
        {
            return std::nullopt;
        }

        return static_cast<std::size_t>(*number);
    }

    /**
     * @brief Returns one JSON unsigned file-size value when the stored number is integral and non-negative.
     * @param value JSON value that should contain an unsigned integer.
     * @return Stored file-size value when conversion succeeds; otherwise no value.
     */
    std::optional<std::uintmax_t> TryGetFileSizeValue(const helen::JsonValue* value)
    {
        const std::optional<std::size_t> size_value = TryGetSizeValue(value);
        if (!size_value.has_value())
        {
            return std::nullopt;
        }

        return static_cast<std::uintmax_t>(*size_value);
    }

    std::optional<std::uint32_t> TryGetUnsigned32Value(const helen::JsonValue* value);

    /**
     * @brief Returns whether the supplied text is an exact 64-character SHA-256 hexadecimal digest.
     * @param value Text that should contain an ASCII hexadecimal digest.
     * @return True when the value contains exactly 64 hexadecimal characters; otherwise false.
     */
    bool IsValidSha256Text(std::string_view value)
    {
        if (value.size() != 64)
        {
            return false;
        }

        for (const char character : value)
        {
            const bool is_decimal_digit = character >= '0' && character <= '9';
            const bool is_lower_hex = character >= 'a' && character <= 'f';
            const bool is_upper_hex = character >= 'A' && character <= 'F';
            if (!is_decimal_digit && !is_lower_hex && !is_upper_hex)
            {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Parses one exact file hash declaration from a nested JSON object.
     * @param value JSON object that should contain `size` and `sha256`.
     * @param definition Receives the parsed hash metadata on success.
     * @return True when both fields are present and valid; otherwise false.
     */
    bool ParseVirtualFileHash(const helen::JsonValue& value, helen::VirtualFileHashDefinition& definition)
    {
        const std::optional<std::uintmax_t> file_size = TryGetFileSizeValue(FindObjectMember(value, "size"));
        const std::optional<std::string> sha256 = TryGetString(FindObjectMember(value, "sha256"));
        if (!file_size.has_value() || !sha256.has_value() || !IsValidSha256Text(*sha256))
        {
            return false;
        }

        definition.FileSize = *file_size;
        definition.Sha256 = ToLowerAscii(*sha256);
        return true;
    }

    /**
     * @brief Returns whether one parsed virtual-file source kind is valid for the declared virtualization mode.
     * @param mode Virtualization mode declared by the pack.
     * @param kind Source kind parsed from the virtual-file source declaration.
     * @return True when the mode/source pairing is one of the supported combinations; otherwise false.
     */
    bool HasCompatibleVirtualFileModeAndSourceKind(std::string_view mode, helen::VirtualFileSourceKind kind)
    {
        if (mode == "replace-on-read")
        {
            return kind == helen::VirtualFileSourceKind::FullFile;
        }

        if (mode == "delta-on-read")
        {
            return kind == helen::VirtualFileSourceKind::DeltaFile;
        }

        return false;
    }

    /**
     * @brief Converts one optional virtual-file source kind text into the typed enum value.
     * @param value JSON value that should contain a source kind string when present.
     * @param kind Receives the parsed source kind on success.
     * @return True when the value is absent or one of the supported source kinds; otherwise false.
     */
    bool TryParseVirtualFileSourceKind(const helen::JsonValue* value, helen::VirtualFileSourceKind& kind)
    {
        const std::optional<std::string> kind_text = TryGetString(value);
        if (!kind_text.has_value())
        {
            kind = helen::VirtualFileSourceKind::FullFile;
            return true;
        }

        if (*kind_text == "full-file")
        {
            kind = helen::VirtualFileSourceKind::FullFile;
            return true;
        }

        if (*kind_text == "delta-file")
        {
            kind = helen::VirtualFileSourceKind::DeltaFile;
            return true;
        }

        return false;
    }

    /**
     * @brief Parses one explicit virtual-file source declaration from a JSON object.
     * @param value JSON object that should describe the source metadata.
     * @param definition Receives the parsed source metadata on success.
     * @return True when the source declaration is valid; otherwise false.
     */
    bool ParseVirtualFileSource(const helen::JsonValue& value, helen::VirtualFileSourceDefinition& definition)
    {
        definition.Kind = helen::VirtualFileSourceKind::FullFile;
        if (!TryParseVirtualFileSourceKind(FindObjectMember(value, "kind"), definition.Kind))
        {
            return false;
        }

        const std::optional<std::string> source_path = TryGetString(FindObjectMember(value, "path"));
        if (!source_path.has_value() || source_path->empty())
        {
            return false;
        }

        definition.Path = std::filesystem::path(*source_path);

        if (definition.Kind == helen::VirtualFileSourceKind::DeltaFile)
        {
            const helen::JsonValue* base_value = FindObjectMember(value, "base");
            const helen::JsonValue* target_value = FindObjectMember(value, "target");
            const std::optional<std::uint32_t> chunk_size = TryGetUnsigned32Value(FindObjectMember(value, "chunkSize"));
            if (base_value == nullptr || target_value == nullptr || !chunk_size.has_value())
            {
                return false;
            }

            if (!base_value->IsObject() || !target_value->IsObject())
            {
                return false;
            }

            if (!ParseVirtualFileHash(*base_value, definition.Base) || !ParseVirtualFileHash(*target_value, definition.Target))
            {
                return false;
            }

            if (*chunk_size == 0)
            {
                return false;
            }

            definition.ChunkSize = *chunk_size;
        }

        return true;
    }

    /**
     * @brief Parses one unsigned integer from either a JSON number or a numeric string such as `0x006B00DA`.
     * @param value JSON value that should contain an unsigned integer.
     * @return Parsed pointer-sized integer when conversion succeeds; otherwise no value.
     */
    std::optional<std::uintptr_t> TryGetUnsignedAddressValue(const helen::JsonValue* value)
    {
        const std::optional<std::size_t> numeric_value = TryGetSizeValue(value);
        if (numeric_value.has_value())
        {
            return static_cast<std::uintptr_t>(*numeric_value);
        }

        const std::optional<std::string> text = TryGetString(value);
        if (!text.has_value() || text->empty())
        {
            return std::nullopt;
        }

        try
        {
            std::size_t consumed = 0;
            const unsigned long long parsed_value = std::stoull(*text, &consumed, 0);
            if (consumed != text->size() || parsed_value > (std::numeric_limits<std::uintptr_t>::max)())
            {
                return std::nullopt;
            }

            return static_cast<std::uintptr_t>(parsed_value);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    /**
     * @brief Parses one unsigned 32-bit integer from either a JSON number or a numeric string.
     * @param value JSON value that should contain an unsigned 32-bit integer.
     * @return Parsed 32-bit value when conversion succeeds; otherwise no value.
     */
    std::optional<std::uint32_t> TryGetUnsigned32Value(const helen::JsonValue* value)
    {
        const std::optional<std::uintptr_t> parsed_value = TryGetUnsignedAddressValue(value);
        if (!parsed_value.has_value() || *parsed_value > (std::numeric_limits<std::uint32_t>::max)())
        {
            return std::nullopt;
        }

        return static_cast<std::uint32_t>(*parsed_value);
    }

    /**
     * @brief Returns whether one double value can be represented safely inside float32 storage.
     * @param value Double value that should be validated.
     * @return True when the value is finite and within float32 range; otherwise false.
     */
    bool IsValidFloat32(double value)
    {
        return std::isfinite(value) &&
            value >= static_cast<double>(std::numeric_limits<float>::lowest()) &&
            value <= static_cast<double>(std::numeric_limits<float>::max());
    }

    /**
     * @brief Returns child directories beneath one root in deterministic lexicographic order.
     * @param root Directory whose child directories should be enumerated.
     * @return Sorted child-directory paths.
     */
    std::vector<std::filesystem::path> ListChildDirectoriesSorted(const std::filesystem::path& root)
    {
        std::vector<std::filesystem::path> directories;

        std::error_code error_code;
        if (!std::filesystem::exists(root, error_code) || error_code)
        {
            return directories;
        }

        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root, error_code))
        {
            if (error_code)
            {
                directories.clear();
                return directories;
            }

            if (entry.is_directory(error_code) && !error_code)
            {
                directories.push_back(entry.path());
            }
        }

        std::sort(directories.begin(), directories.end());
        return directories;
    }

    /**
     * @brief Parses one config entry declaration from pack metadata.
     * @param value JSON object that should describe a config entry.
     * @param definition Receives the parsed config entry on success.
     * @return True when the config entry is valid; otherwise false.
     */
    bool ParseConfigEntry(const helen::JsonValue& value, helen::ConfigEntryDefinition& definition)
    {
        definition.Key = TryGetString(FindObjectMember(value, "key")).value_or("");
        definition.Type = TryGetString(FindObjectMember(value, "type")).value_or("");
        const std::optional<int> default_value = TryGetInt(FindObjectMember(value, "defaultValue"));
        if (definition.Key.empty() || definition.Type.empty() || !default_value.has_value())
        {
            return false;
        }

        definition.DefaultValue = *default_value;
        return true;
    }

    /**
     * @brief Parses one feature declaration from pack metadata.
     * @param value JSON object that should describe a feature.
     * @param definition Receives the parsed feature definition on success.
     * @return True when the feature declaration is valid; otherwise false.
     */
    bool ParseFeature(const helen::JsonValue& value, helen::FeatureDefinition& definition)
    {
        definition.Id = TryGetString(FindObjectMember(value, "id")).value_or("");
        definition.Name = TryGetString(FindObjectMember(value, "name")).value_or("");
        definition.Kind = TryGetString(FindObjectMember(value, "kind")).value_or("");
        definition.ConfigKey = TryGetString(FindObjectMember(value, "configKey")).value_or("");
        const std::optional<int> default_value = TryGetInt(FindObjectMember(value, "defaultValue"));
        if (definition.Id.empty() || definition.Name.empty() || definition.Kind.empty() || definition.ConfigKey.empty() || !default_value.has_value())
        {
            return false;
        }

        definition.DefaultValue = *default_value;
        return true;
    }

    /**
     * @brief Parses one top-level pack manifest.
     * @param root JSON root object loaded from `pack.json`.
     * @param definition Receives the parsed pack definition on success.
     * @return True when the manifest is valid; otherwise false.
     */
    bool ParsePackDefinition(const helen::JsonValue& root, helen::PackDefinition& definition)
    {
        definition.Id = TryGetString(FindObjectMember(root, "id")).value_or("");
        definition.Name = TryGetString(FindObjectMember(root, "name")).value_or("");
        definition.Description = TryGetString(FindObjectMember(root, "description")).value_or("");
        if (definition.Id.empty() || definition.Name.empty())
        {
            return false;
        }

        const helen::JsonValue* targets_value = FindObjectMember(root, "targets");
        const helen::JsonValue::Array* targets = targets_value != nullptr ? targets_value->AsArray() : nullptr;
        if (targets == nullptr || targets->empty())
        {
            return false;
        }

        for (const helen::JsonValue& target : *targets)
        {
            const helen::JsonValue* executables_value = FindObjectMember(target, "executables");
            const helen::JsonValue::Array* executables = executables_value != nullptr ? executables_value->AsArray() : nullptr;
            if (executables == nullptr || executables->empty())
            {
                return false;
            }

            for (const helen::JsonValue& executable_value : *executables)
            {
                const std::optional<std::string> executable = TryGetString(&executable_value);
                if (!executable.has_value() || executable->empty())
                {
                    return false;
                }

                definition.Executables.push_back(*executable);
            }
        }

        if (const helen::JsonValue* config_value = FindObjectMember(root, "config"))
        {
            const helen::JsonValue::Array* config_entries = config_value->AsArray();
            if (config_entries == nullptr)
            {
                return false;
            }

            for (const helen::JsonValue& config_entry_value : *config_entries)
            {
                helen::ConfigEntryDefinition config_entry;
                if (!ParseConfigEntry(config_entry_value, config_entry))
                {
                    return false;
                }

                definition.ConfigEntries.push_back(std::move(config_entry));
            }
        }

        if (const helen::JsonValue* features_value = FindObjectMember(root, "features"))
        {
            const helen::JsonValue::Array* features = features_value->AsArray();
            if (features == nullptr)
            {
                return false;
            }

            for (const helen::JsonValue& feature_value : *features)
            {
                helen::FeatureDefinition feature;
                if (!ParseFeature(feature_value, feature))
                {
                    return false;
                }

                definition.Features.push_back(std::move(feature));
            }
        }

        const helen::JsonValue* builds_value = FindObjectMember(root, "builds");
        const helen::JsonValue::Array* builds = builds_value != nullptr ? builds_value->AsArray() : nullptr;
        if (builds == nullptr || builds->empty())
        {
            return false;
        }

        for (const helen::JsonValue& build_value : *builds)
        {
            const std::optional<std::string> build_id = TryGetString(&build_value);
            if (!build_id.has_value() || build_id->empty())
            {
                return false;
            }

            definition.BuildIds.push_back(*build_id);
        }

        return true;
    }

    /**
     * @brief Parses one build manifest loaded from `build.json`.
     * @param root JSON root object loaded from the build manifest.
     * @param expected_build_id Build identifier implied by the selected build directory.
     * @param definition Receives the parsed build definition on success.
     * @return True when the build manifest is valid; otherwise false.
     */
    bool ParseBuildManifest(const helen::JsonValue& root, const std::string& expected_build_id, helen::BuildDefinition& definition)
    {
        definition.Id = TryGetString(FindObjectMember(root, "id")).value_or("");
        if (definition.Id.empty() || definition.Id != expected_build_id)
        {
            return false;
        }

        definition.Match.ExecutableName = TryGetString(FindObjectMember(root, "executable")).value_or("");
        const helen::JsonValue* match_value = FindObjectMember(root, "match");
        if (definition.Match.ExecutableName.empty() || match_value == nullptr || !match_value->IsObject())
        {
            return false;
        }

        const std::optional<std::uintmax_t> file_size = TryGetFileSizeValue(FindObjectMember(*match_value, "fileSize"));
        const std::optional<std::string> sha256 = TryGetString(FindObjectMember(*match_value, "sha256"));
        if (!file_size.has_value() || !sha256.has_value() || sha256->empty())
        {
            return false;
        }

        definition.Match.FileSize = *file_size;
        definition.Match.Sha256 = ToLowerAscii(*sha256);

        if (const helen::JsonValue* startup_commands_value = FindObjectMember(root, "startupCommands"))
        {
            const helen::JsonValue::Array* startup_commands = startup_commands_value->AsArray();
            if (startup_commands == nullptr)
            {
                return false;
            }

            for (const helen::JsonValue& command_value : *startup_commands)
            {
                const std::optional<std::string> command_id = TryGetString(&command_value);
                if (!command_id.has_value() || command_id->empty())
                {
                    return false;
                }

                definition.StartupCommandIds.push_back(*command_id);
            }
        }

        return true;
    }

    /**
     * @brief Parses one virtual file declaration from `files.json`.
     * @param value JSON object that should describe a virtual file.
     * @param definition Receives the parsed virtual file definition on success.
     * @return True when the declaration is valid; otherwise false.
     */
    bool ParseVirtualFile(const helen::JsonValue& value, helen::VirtualFileDefinition& definition)
    {
        definition.Id = TryGetString(FindObjectMember(value, "id")).value_or("");
        definition.Mode = TryGetString(FindObjectMember(value, "mode")).value_or("");
        std::optional<std::string> game_path = TryGetString(FindObjectMember(value, "gamePath"));
        if (!game_path.has_value())
        {
            game_path = TryGetString(FindObjectMember(value, "path"));
        }

        if (definition.Id.empty() || definition.Mode.empty() || !game_path.has_value() || game_path->empty())
        {
            return false;
        }

        definition.GamePath = std::filesystem::path(*game_path);

        const helen::JsonValue* source_value = FindObjectMember(value, "source");
        if (source_value == nullptr)
        {
            return false;
        }

        if (const std::optional<std::string> source_text = TryGetString(source_value))
        {
            definition.Source.Kind = helen::VirtualFileSourceKind::FullFile;
            definition.Source.Path = std::filesystem::path(*source_text);
            return !definition.Source.Path.empty() &&
                HasCompatibleVirtualFileModeAndSourceKind(definition.Mode, definition.Source.Kind);
        }

        if (!source_value->IsObject())
        {
            return false;
        }

        if (!ParseVirtualFileSource(*source_value, definition.Source))
        {
            return false;
        }

        return HasCompatibleVirtualFileModeAndSourceKind(definition.Mode, definition.Source.Kind);
    }

    /**
     * @brief Parses one command map entry from `commands.json`.
     * @param value JSON object that should describe a mapping entry.
     * @param definition Receives the parsed map entry on success.
     * @return True when the mapping entry is valid; otherwise false.
     */
    bool ParseCommandMapEntry(const helen::JsonValue& value, helen::CommandMapEntryDefinition& definition)
    {
        const std::optional<int> match = TryGetInt(FindObjectMember(value, "match"));
        const helen::JsonValue* mapped_value = FindObjectMember(value, "value");
        const std::optional<double> number = mapped_value != nullptr ? mapped_value->AsNumber() : std::nullopt;
        if (!match.has_value() || !number.has_value() || !std::isfinite(*number))
        {
            return false;
        }

        definition.Match = *match;
        definition.Value = *number;
        return true;
    }

    /**
     * @brief Parses one command step from `commands.json`.
     * @param value JSON object that should describe a command step.
     * @param definition Receives the parsed command step on success.
     * @return True when the step declaration is valid; otherwise false.
     */
    bool ParseCommandStep(const helen::JsonValue& value, helen::CommandStepDefinition& definition)
    {
        definition.Kind = TryGetString(FindObjectMember(value, "kind")).value_or("");
        definition.ConfigKey = TryGetString(FindObjectMember(value, "configKey")).value_or("");
        definition.ValueName = TryGetString(FindObjectMember(value, "valueName")).value_or("");
        definition.InputValueName = TryGetString(FindObjectMember(value, "inputValueName")).value_or("");
        definition.OutputValueName = TryGetString(FindObjectMember(value, "outputValueName")).value_or("");
        definition.Target = TryGetString(FindObjectMember(value, "target")).value_or("");
        definition.CommandId = TryGetString(FindObjectMember(value, "command")).value_or("");
        if (definition.CommandId.empty())
        {
            definition.CommandId = TryGetString(FindObjectMember(value, "commandId")).value_or("");
        }

        definition.Message = TryGetString(FindObjectMember(value, "message")).value_or("");

        if (definition.Kind == "read-config-int")
        {
            return !definition.ConfigKey.empty() && !definition.ValueName.empty();
        }

        if (definition.Kind == "map-int-to-double")
        {
            if (definition.InputValueName.empty() || definition.OutputValueName.empty())
            {
                return false;
            }

            const helen::JsonValue* mappings_value = FindObjectMember(value, "mappings");
            const helen::JsonValue::Array* mappings = mappings_value != nullptr ? mappings_value->AsArray() : nullptr;
            if (mappings == nullptr || mappings->empty())
            {
                return false;
            }

            for (const helen::JsonValue& mapping_value : *mappings)
            {
                helen::CommandMapEntryDefinition mapping;
                if (!ParseCommandMapEntry(mapping_value, mapping))
                {
                    return false;
                }

                definition.Mappings.push_back(std::move(mapping));
            }

            return true;
        }

        if (definition.Kind == "set-live-double")
        {
            return !definition.Target.empty() && !definition.ValueName.empty();
        }

        if (definition.Kind == "run-command")
        {
            return !definition.CommandId.empty();
        }

        if (definition.Kind == "log-message")
        {
            return !definition.Message.empty();
        }

        if (definition.Kind == "load-batman-graphics-draft-into-config")
        {
            return true;
        }

        if (definition.Kind == "load-batman-subtitle-size-into-config")
        {
            return true;
        }

        if (definition.Kind == "apply-batman-graphics-config")
        {
            return true;
        }

        if (definition.Kind == "apply-batman-subtitle-size-config")
        {
            return true;
        }

        if (definition.Kind == "sync-batman-graphics-detail-level")
        {
            return true;
        }

        if (definition.Kind == "sync-batman-graphics-detail-preset")
        {
            return true;
        }

        return false;
    }

    /**
     * @brief Parses one command definition from `commands.json`.
     * @param value JSON object that should describe a command.
     * @param definition Receives the parsed command definition on success.
     * @return True when the command declaration is valid; otherwise false.
     */
    bool ParseCommandDefinition(const helen::JsonValue& value, helen::CommandDefinition& definition)
    {
        definition.Id = TryGetString(FindObjectMember(value, "id")).value_or("");
        definition.Name = TryGetString(FindObjectMember(value, "name")).value_or("");
        const helen::JsonValue* steps_value = FindObjectMember(value, "steps");
        const helen::JsonValue::Array* steps = steps_value != nullptr ? steps_value->AsArray() : nullptr;
        if (definition.Id.empty() || definition.Name.empty() || steps == nullptr)
        {
            return false;
        }

        for (const helen::JsonValue& step_value : *steps)
        {
            helen::CommandStepDefinition step;
            if (!ParseCommandStep(step_value, step))
            {
                return false;
            }

            definition.Steps.push_back(std::move(step));
        }

        return true;
    }

    /**
     * @brief Parses one external binding definition from binding metadata.
     * @param value JSON object that should describe one external binding.
     * @param definition Receives the parsed external binding on success.
     * @return True when the binding declaration is valid; otherwise false.
     */
    bool ParseExternalBinding(const helen::JsonValue& value, helen::ExternalBindingDefinition& definition)
    {
        definition.Id = TryGetString(FindObjectMember(value, "id")).value_or("");
        definition.ExternalName = TryGetString(FindObjectMember(value, "externalName")).value_or("");
        definition.Mode = TryGetString(FindObjectMember(value, "mode")).value_or("");
        definition.ConfigKey = TryGetString(FindObjectMember(value, "configKey")).value_or("");
        definition.CommandId = TryGetString(FindObjectMember(value, "command")).value_or("");
        if (definition.CommandId.empty())
        {
            definition.CommandId = TryGetString(FindObjectMember(value, "commandId")).value_or("");
        }

        if (definition.Id.empty() || definition.ExternalName.empty() || definition.Mode.empty())
        {
            return false;
        }

        if (definition.Mode == "get-int" || definition.Mode == "set-int")
        {
            return !definition.ConfigKey.empty();
        }

        if (definition.Mode == "run-command")
        {
            return !definition.CommandId.empty();
        }

        return false;
    }

    /**
     * @brief Parses one runtime slot declaration from `hooks.json`.
     * @param value JSON object that should describe one runtime slot.
     * @param declared_slot_ids Set of slot identifiers already accepted for this build.
     * @param definition Receives the parsed runtime slot on success.
     * @return True when the runtime slot is valid; otherwise false.
     */
    bool ParseRuntimeSlot(const helen::JsonValue& value, std::set<std::string>& declared_slot_ids, helen::RuntimeSlotDefinition& definition)
    {
        definition.Id = TryGetString(FindObjectMember(value, "id")).value_or("");
        definition.Type = TryGetString(FindObjectMember(value, "type")).value_or("");
        const helen::JsonValue* initial_value = FindObjectMember(value, "initialValue");
        const std::optional<double> parsed_initial_value = initial_value != nullptr ? initial_value->AsNumber() : std::nullopt;
        if (definition.Id.empty() || definition.Type != "float32" || !parsed_initial_value.has_value() || !IsValidFloat32(*parsed_initial_value))
        {
            return false;
        }

        if (!declared_slot_ids.emplace(definition.Id).second)
        {
            return false;
        }

        definition.InitialValue = *parsed_initial_value;
        return true;
    }

    /**
     * @brief Parses one observer validation check from `hooks.json`.
     * @param value JSON object that should describe one state-observer check.
     * @param definition Receives the parsed check definition on success.
     * @return True when the check declaration is valid; otherwise false.
     */
    bool ParseStateObserverCheck(const helen::JsonValue& value, helen::MemoryStateObserverCheckDefinition& definition)
    {
        definition.Comparison = TryGetString(FindObjectMember(value, "comparison")).value_or("");
        const std::optional<int> offset = TryGetInt(FindObjectMember(value, "offset"));
        if (definition.Comparison.empty() || !offset.has_value())
        {
            return false;
        }

        definition.Offset = *offset;
        definition.ExpectedValue = TryGetInt(FindObjectMember(value, "expectedValue"));
        definition.CompareOffset = TryGetInt(FindObjectMember(value, "compareOffset"));

        if (definition.Comparison == "equals-constant")
        {
            return definition.ExpectedValue.has_value();
        }

        if (definition.Comparison == "equals-value-at-offset")
        {
            return definition.CompareOffset.has_value();
        }

        return false;
    }

    /**
     * @brief Parses one observer mapping entry from `hooks.json`.
     * @param value JSON object that should describe one raw-to-config mapping.
     * @param definition Receives the parsed mapping on success.
     * @return True when the mapping declaration is valid; otherwise false.
     */
    bool ParseStateObserverMapping(const helen::JsonValue& value, helen::MemoryStateObserverMapEntryDefinition& definition)
    {
        const std::optional<int> match = TryGetInt(FindObjectMember(value, "match"));
        const std::optional<int> mapped_value = TryGetInt(FindObjectMember(value, "value"));
        if (!match.has_value() || !mapped_value.has_value())
        {
            return false;
        }

        definition.Match = *match;
        definition.Value = *mapped_value;
        return true;
    }

    /**
     * @brief Parses one memory-state observer declaration from `hooks.json`.
     * @param value JSON object that should describe one state observer.
     * @param definition Receives the parsed observer definition on success.
     * @return True when the observer declaration is valid; otherwise false.
     */
    bool ParseStateObserver(const helen::JsonValue& value, helen::MemoryStateObserverDefinition& definition)
    {
        definition.Id = TryGetString(FindObjectMember(value, "id")).value_or("");
        definition.TargetConfigKey = TryGetString(FindObjectMember(value, "targetConfigKey")).value_or("");
        definition.CommandId = TryGetString(FindObjectMember(value, "command"));
        definition.ScanStartAddress = TryGetUnsignedAddressValue(FindObjectMember(value, "scanStartAddress")).value_or(0);
        definition.ScanEndAddress = TryGetUnsignedAddressValue(FindObjectMember(value, "scanEndAddress")).value_or(0);

        const std::optional<std::size_t> scan_stride = TryGetSizeValue(FindObjectMember(value, "scanStride"));
        const std::optional<int> value_offset = TryGetInt(FindObjectMember(value, "valueOffset"));
        const std::optional<int> poll_interval = TryGetInt(FindObjectMember(value, "pollIntervalMs"));
        if (definition.Id.empty() ||
            definition.TargetConfigKey.empty() ||
            definition.ScanStartAddress == 0 ||
            definition.ScanEndAddress <= definition.ScanStartAddress ||
            !scan_stride.has_value() ||
            *scan_stride == 0 ||
            !value_offset.has_value() ||
            !poll_interval.has_value() ||
            *poll_interval <= 0)
        {
            return false;
        }

        definition.ScanStride = *scan_stride;
        definition.ValueOffset = *value_offset;
        definition.PollIntervalMs = *poll_interval;

        const helen::JsonValue* checks_value = FindObjectMember(value, "checks");
        const helen::JsonValue::Array* checks = checks_value != nullptr ? checks_value->AsArray() : nullptr;
        if (checks == nullptr || checks->empty())
        {
            return false;
        }

        for (const helen::JsonValue& check_value : *checks)
        {
            helen::MemoryStateObserverCheckDefinition check;
            if (!ParseStateObserverCheck(check_value, check))
            {
                return false;
            }

            definition.Checks.push_back(std::move(check));
        }

        const helen::JsonValue* mappings_value = FindObjectMember(value, "mappings");
        const helen::JsonValue::Array* mappings = mappings_value != nullptr ? mappings_value->AsArray() : nullptr;
        if (mappings == nullptr || mappings->empty())
        {
            return false;
        }

        for (const helen::JsonValue& mapping_value : *mappings)
        {
            helen::MemoryStateObserverMapEntryDefinition mapping;
            if (!ParseStateObserverMapping(mapping_value, mapping))
            {
                return false;
            }

            definition.Mappings.push_back(std::move(mapping));
        }

        return true;
    }

    /**
     * @brief Parses one hook relocation source operand from `hooks.json`.
     * @param value JSON object that should describe a relocation source.
     * @param declared_slot_ids Runtime slots already declared for the build.
     * @param definition Receives the parsed relocation source on success.
     * @return True when the relocation source declaration is valid; otherwise false.
     */
    bool ParseRelocationSource(const helen::JsonValue& value, const std::set<std::string>& declared_slot_ids, helen::HookBlobRelocationSourceDefinition& definition)
    {
        definition.Kind = TryGetString(FindObjectMember(value, "kind")).value_or("");
        definition.Slot = TryGetString(FindObjectMember(value, "slot")).value_or("");
        definition.ModuleName = TryGetString(FindObjectMember(value, "module")).value_or("");
        definition.ExportName = TryGetString(FindObjectMember(value, "export")).value_or("");
        definition.BlobOffset = TryGetSizeValue(FindObjectMember(value, "blobOffset"));
        if (definition.Kind.empty())
        {
            return false;
        }

        if (definition.Kind == "runtime-slot")
        {
            return !definition.Slot.empty() && declared_slot_ids.contains(definition.Slot);
        }

        if (definition.Kind == "hook-target" || definition.Kind == "hook-resume")
        {
            return true;
        }

        if (definition.Kind == "module-export")
        {
            return !definition.ModuleName.empty() && !definition.ExportName.empty();
        }

        if (definition.Kind == "blob-offsetof")
        {
            return definition.BlobOffset.has_value();
        }

        return false;
    }

    /**
     * @brief Parses one relocation definition from `hooks.json`.
     * @param value JSON object that should describe one blob relocation.
     * @param declared_slot_ids Runtime slots already declared for the build.
     * @param definition Receives the parsed relocation on success.
     * @return True when the relocation declaration is valid; otherwise false.
     */
    bool ParseRelocationDefinition(const helen::JsonValue& value, const std::set<std::string>& declared_slot_ids, helen::HookBlobRelocationDefinition& definition)
    {
        const std::optional<std::size_t> offset = TryGetSizeValue(FindObjectMember(value, "offset"));
        definition.Encoding = TryGetString(FindObjectMember(value, "encoding")).value_or("");
        const helen::JsonValue* source_value = FindObjectMember(value, "source");
        if (!offset.has_value() || definition.Encoding.empty() || source_value == nullptr || !source_value->IsObject())
        {
            return false;
        }

        if (definition.Encoding != "abs32" && definition.Encoding != "rel32")
        {
            return false;
        }

        definition.Offset = *offset;
        return ParseRelocationSource(*source_value, declared_slot_ids, definition.Source);
    }

    /**
     * @brief Parses one hook blob declaration from `hooks.json`.
     * @param value JSON object that should describe a blob-backed hook payload.
     * @param declared_slot_ids Runtime slots already declared for the build.
     * @param definition Receives the parsed blob definition on success.
     * @return True when the blob declaration is valid; otherwise false.
     */
    bool ParseHookBlob(const helen::JsonValue& value, const std::set<std::string>& declared_slot_ids, helen::HookBlobDefinition& definition)
    {
        const std::optional<std::string> asset_path = TryGetString(FindObjectMember(value, "assetPath"));
        const std::optional<std::size_t> entry_offset = TryGetSizeValue(FindObjectMember(value, "entryOffset"));
        if (!asset_path.has_value() || asset_path->empty() || !entry_offset.has_value())
        {
            return false;
        }

        definition.AssetPath = std::filesystem::path(*asset_path);
        definition.EntryOffset = *entry_offset;

        const helen::JsonValue* relocations_value = FindObjectMember(value, "relocations");
        const helen::JsonValue::Array* relocations = relocations_value != nullptr ? relocations_value->AsArray() : nullptr;
        if (relocations == nullptr)
        {
            return false;
        }

        for (const helen::JsonValue& relocation_value : *relocations)
        {
            helen::HookBlobRelocationDefinition relocation;
            if (!ParseRelocationDefinition(relocation_value, declared_slot_ids, relocation))
            {
                return false;
            }

            definition.Relocations.push_back(std::move(relocation));
        }

        return true;
    }

    /**
     * @brief Parses one hook definition from `hooks.json`.
     * @param value JSON object that should describe one native hook.
     * @param declared_slot_ids Runtime slots already declared for the build.
     * @param definition Receives the parsed hook definition on success.
     * @return True when the hook declaration is valid; otherwise false.
     */
    bool ParseHookDefinition(const helen::JsonValue& value, const std::set<std::string>& declared_slot_ids, helen::HookDefinition& definition)
    {
        definition.Id = TryGetString(FindObjectMember(value, "id")).value_or("");
        definition.ModuleName = TryGetString(FindObjectMember(value, "module")).value_or("");
        definition.SectionName = TryGetString(FindObjectMember(value, "section")).value_or("");
        definition.Pattern = TryGetString(FindObjectMember(value, "pattern")).value_or("");
        definition.ExpectedBytes = TryGetString(FindObjectMember(value, "expectedBytes")).value_or("");
        definition.Action = TryGetString(FindObjectMember(value, "action")).value_or("");
        definition.RelativeVirtualAddress = TryGetUnsigned32Value(FindObjectMember(value, "rva"));

        const std::optional<std::size_t> overwrite_length = TryGetSizeValue(FindObjectMember(value, "overwriteLength"));
        const std::optional<std::size_t> resume_offset = TryGetSizeValue(FindObjectMember(value, "resumeOffsetFromTarget"));
        const helen::JsonValue* blob_value = FindObjectMember(value, "blob");
        if (definition.Id.empty() ||
            definition.ModuleName.empty() ||
            definition.Action != "inline-jump-to-pack-blob" ||
            !overwrite_length.has_value() ||
            *overwrite_length == 0 ||
            !resume_offset.has_value() ||
            blob_value == nullptr ||
            !blob_value->IsObject())
        {
            return false;
        }

        const bool has_pattern = !definition.Pattern.empty();
        const bool has_rva = definition.RelativeVirtualAddress.has_value();
        if (has_pattern == has_rva)
        {
            return false;
        }

        definition.OverwriteLength = *overwrite_length;
        definition.ResumeOffsetFromTarget = *resume_offset;
        return ParseHookBlob(*blob_value, declared_slot_ids, definition.Blob);
    }

    /**
     * @brief Parses the optional `files.json` manifest into the build definition.
     * @param path Files-manifest path that may or may not exist.
     * @param definition Build definition that should receive parsed virtual files.
     * @return True when the manifest is absent or valid; otherwise false.
     */
    bool LoadFilesManifest(const std::filesystem::path& path, helen::BuildDefinition& definition)
    {
        if (!std::filesystem::exists(path))
        {
            return true;
        }

        helen::JsonValue root;
        if (!TryReadJsonFile(path, root))
        {
            return false;
        }

        const helen::JsonValue* files_value = FindObjectMember(root, "virtualFiles");
        const helen::JsonValue::Array* files = files_value != nullptr ? files_value->AsArray() : nullptr;
        if (files == nullptr)
        {
            return false;
        }

        for (const helen::JsonValue& file_value : *files)
        {
            helen::VirtualFileDefinition file_definition;
            if (!ParseVirtualFile(file_value, file_definition))
            {
                return false;
            }

            definition.VirtualFiles.push_back(std::move(file_definition));
        }

        return true;
    }

    /**
     * @brief Parses the optional `commands.json` manifest into the build definition.
     * @param path Commands-manifest path that may or may not exist.
     * @param definition Build definition that should receive parsed commands.
     * @return True when the manifest is absent or valid; otherwise false.
     */
    bool LoadCommandsManifest(const std::filesystem::path& path, helen::BuildDefinition& definition)
    {
        if (!std::filesystem::exists(path))
        {
            return true;
        }

        helen::JsonValue root;
        if (!TryReadJsonFile(path, root))
        {
            return false;
        }

        const helen::JsonValue* commands_value = FindObjectMember(root, "commands");
        const helen::JsonValue::Array* commands = commands_value != nullptr ? commands_value->AsArray() : nullptr;
        if (commands == nullptr)
        {
            return false;
        }

        for (const helen::JsonValue& command_value : *commands)
        {
            helen::CommandDefinition command;
            if (!ParseCommandDefinition(command_value, command))
            {
                return false;
            }

            definition.Commands.push_back(std::move(command));
        }

        return true;
    }

    /**
     * @brief Parses the optional hook and observer manifest into the build definition.
     * @param path Hook-manifest path that may or may not exist.
     * @param definition Build definition that should receive parsed slots, observers, hooks, and inline bindings.
     * @return True when the manifest is absent or valid; otherwise false.
     */
    bool LoadHooksManifest(const std::filesystem::path& path, helen::BuildDefinition& definition)
    {
        if (!std::filesystem::exists(path))
        {
            return true;
        }

        helen::JsonValue root;
        if (!TryReadJsonFile(path, root))
        {
            return false;
        }

        std::set<std::string> declared_slot_ids;

        if (const helen::JsonValue* slots_value = FindObjectMember(root, "runtimeSlots"))
        {
            const helen::JsonValue::Array* slots = slots_value->AsArray();
            if (slots == nullptr)
            {
                return false;
            }

            for (const helen::JsonValue& slot_value : *slots)
            {
                helen::RuntimeSlotDefinition slot_definition;
                if (!ParseRuntimeSlot(slot_value, declared_slot_ids, slot_definition))
                {
                    return false;
                }

                definition.RuntimeSlots.push_back(std::move(slot_definition));
            }
        }

        if (const helen::JsonValue* observers_value = FindObjectMember(root, "stateObservers"))
        {
            const helen::JsonValue::Array* observers = observers_value->AsArray();
            if (observers == nullptr)
            {
                return false;
            }

            for (const helen::JsonValue& observer_value : *observers)
            {
                helen::MemoryStateObserverDefinition observer_definition;
                if (!ParseStateObserver(observer_value, observer_definition))
                {
                    return false;
                }

                definition.StateObservers.push_back(std::move(observer_definition));
            }
        }

        if (const helen::JsonValue* hooks_value = FindObjectMember(root, "hooks"))
        {
            const helen::JsonValue::Array* hooks = hooks_value->AsArray();
            if (hooks == nullptr)
            {
                return false;
            }

            for (const helen::JsonValue& hook_value : *hooks)
            {
                helen::HookDefinition hook_definition;
                if (!ParseHookDefinition(hook_value, declared_slot_ids, hook_definition))
                {
                    return false;
                }

                definition.Hooks.push_back(std::move(hook_definition));
            }
        }

        if (const helen::JsonValue* bindings_value = FindObjectMember(root, "externalBindings"))
        {
            const helen::JsonValue::Array* bindings = bindings_value->AsArray();
            if (bindings == nullptr)
            {
                return false;
            }

            for (const helen::JsonValue& binding_value : *bindings)
            {
                helen::ExternalBindingDefinition binding_definition;
                if (!ParseExternalBinding(binding_value, binding_definition))
                {
                    return false;
                }

                definition.ExternalBindings.push_back(std::move(binding_definition));
            }
        }

        return true;
    }

    /**
     * @brief Parses the optional standalone binding manifest into the build definition.
     * @param path Binding-manifest path that may or may not exist.
     * @param definition Build definition that should receive parsed bindings.
     * @return True when the manifest is absent or valid; otherwise false.
     */
    bool LoadBindingsManifest(const std::filesystem::path& path, helen::BuildDefinition& definition)
    {
        if (!std::filesystem::exists(path))
        {
            return true;
        }

        helen::JsonValue root;
        if (!TryReadJsonFile(path, root))
        {
            return false;
        }

        const helen::JsonValue* bindings_value = FindObjectMember(root, "externalBindings");
        if (bindings_value == nullptr)
        {
            bindings_value = FindObjectMember(root, "bindings");
        }

        const helen::JsonValue::Array* bindings = bindings_value != nullptr ? bindings_value->AsArray() : nullptr;
        if (bindings == nullptr)
        {
            return false;
        }

        for (const helen::JsonValue& binding_value : *bindings)
        {
            helen::ExternalBindingDefinition binding_definition;
            if (!ParseExternalBinding(binding_value, binding_definition))
            {
                return false;
            }

            definition.ExternalBindings.push_back(std::move(binding_definition));
        }

        return true;
    }

    /**
     * @brief Loads one full build definition from the selected build directory.
     * @param build_directory Build directory that should contain the split manifest files.
     * @param expected_build_id Build identifier implied by the selected build directory name.
     * @param definition Receives the parsed build definition on success.
     * @return True when the build directory contains a valid build definition; otherwise false.
     */
    bool LoadBuildDefinition(const std::filesystem::path& build_directory, const std::string& expected_build_id, helen::BuildDefinition& definition)
    {
        helen::Logf(L"[pack] LoadBuildDefinition: reading build.json");
        helen::JsonValue build_root;
        if (!TryReadJsonFile(build_directory / "build.json", build_root))
        {
            helen::Logf(L"[pack] LoadBuildDefinition: build.json missing or invalid");
            return false;
        }

        helen::Logf(L"[pack] LoadBuildDefinition: parsing build manifest");
        if (!ParseBuildManifest(build_root, expected_build_id, definition))
        {
            helen::Logf(L"[pack] LoadBuildDefinition: parse build manifest failed");
            return false;
        }

        helen::Logf(L"[pack] LoadBuildDefinition: loading files.json");
        if (!LoadFilesManifest(build_directory / "files.json", definition))
        {
            helen::Logf(L"[pack] LoadBuildDefinition: files.json failed");
            return false;
        }

        helen::Logf(L"[pack] LoadBuildDefinition: loading hooks.json");
        if (!LoadHooksManifest(build_directory / "hooks.json", definition))
        {
            helen::Logf(L"[pack] LoadBuildDefinition: hooks.json failed");
            return false;
        }

        helen::Logf(L"[pack] LoadBuildDefinition: loading bindings.json");
        if (!LoadBindingsManifest(build_directory / "bindings.json", definition))
        {
            helen::Logf(L"[pack] LoadBuildDefinition: bindings.json failed");
            return false;
        }

        helen::Logf(L"[pack] LoadBuildDefinition: loading commands.json");
        if (!LoadCommandsManifest(build_directory / "commands.json", definition))
        {
            helen::Logf(L"[pack] LoadBuildDefinition: commands.json failed");
            return false;
        }

        helen::Logf(L"[pack] LoadBuildDefinition: success");
        return true;
    }

    /**
     * @brief Returns whether the supplied pack targets the requested executable name.
     * @param definition Parsed pack definition whose executable list should be examined.
     * @param executable_name Executable file name requested by the caller.
     * @return True when the pack declares support for the executable name; otherwise false.
     */
    bool SupportsExecutable(const helen::PackDefinition& definition, std::string_view executable_name)
    {
        for (const std::string& supported_name : definition.Executables)
        {
            if (EqualsAsciiIgnoreCase(supported_name, executable_name))
            {
                return true;
            }
        }

        return false;
    }

    /**
     * @brief Returns whether the supplied build matches the requested executable fingerprint.
     * @param definition Parsed build definition whose fingerprint should be compared.
     * @param executable_name Executable file name requested by the caller.
     * @param executable_size Exact executable file size requested by the caller.
     * @param executable_sha256 Lowercase SHA-256 digest requested by the caller.
     * @return True when every build-match component matches exactly; otherwise false.
     */
    bool MatchesFingerprint(
        const helen::BuildDefinition& definition,
        std::string_view executable_name,
        std::uintmax_t executable_size,
        std::string_view executable_sha256)
    {
        return EqualsAsciiIgnoreCase(definition.Match.ExecutableName, executable_name) &&
            definition.Match.FileSize == executable_size &&
            definition.Match.Sha256 == ToLowerAscii(std::string(executable_sha256));
    }
}

namespace helen
{
    /**
     * @brief Loads the first pack build whose executable name and fingerprint match the requested executable.
     * @param packs_directory Root directory that contains one subdirectory per pack.
     * @param executable_name Executable file name reported by the host process.
     * @param executable_size Exact executable file size used for strict build matching.
     * @param executable_sha256 Lowercase SHA-256 hex digest used for strict build matching.
     * @return A resolved loaded pack when one build matches; otherwise no value.
     */
    std::optional<LoadedBuildPack> PackRepository::LoadForExecutable(
        const std::filesystem::path& packs_directory,
        const std::string& executable_name,
        std::uintmax_t executable_size,
        const std::string& executable_sha256) const
    {
        if (packs_directory.empty() || executable_name.empty() || executable_sha256.empty())
        {
            return std::nullopt;
        }

        helen::Logf(L"[pack] scanning packs_directory");
        for (const std::filesystem::path& pack_directory : ListChildDirectoriesSorted(packs_directory))
        {
            helen::Logf(L"[pack] inspecting pack directory");
            JsonValue pack_root;
            if (!TryReadJsonFile(pack_directory / "pack.json", pack_root))
            {
                helen::Logf(L"[pack] skipped: pack.json missing or invalid");
                continue;
            }

            PackDefinition pack_definition;
            if (!ParsePackDefinition(pack_root, pack_definition))
            {
                helen::Logf(L"[pack] skipped: failed to parse pack definition");
                continue;
            }

            helen::Logf(L"[pack] loaded pack executables count=%zu", pack_definition.Executables.size());

            if (!SupportsExecutable(pack_definition, executable_name))
            {
                helen::Logf(L"[pack] does not support this executable");
                continue;
            }

            helen::Logf(L"[pack] supports executable, checking %zu builds", pack_definition.BuildIds.size());

            for (const std::string& build_id : pack_definition.BuildIds)
            {
                const std::filesystem::path build_directory = pack_directory / "builds" / build_id;
                helen::Logf(L"[pack] checking build");
                BuildDefinition build_definition;
                if (!LoadBuildDefinition(build_directory, build_id, build_definition))
                {
                    helen::Logf(L"[pack] build failed to load");
                    continue;
                }

                helen::Logf(L"[pack] build loaded: size=%llu",
                    static_cast<unsigned long long>(build_definition.Match.FileSize));

                helen::Logf(L"[pack] fingerprint compare: expected_size=%llu actual_size=%llu",
                    static_cast<unsigned long long>(build_definition.Match.FileSize),
                    static_cast<unsigned long long>(executable_size));

                if (!MatchesFingerprint(build_definition, executable_name, executable_size, executable_sha256))
                {
                    helen::Logf(L"[pack] fingerprint mismatch");
                    continue;
                }

                LoadedBuildPack loaded_pack;
                loaded_pack.PackDirectory = pack_directory;
                loaded_pack.BuildDirectory = build_directory;
                loaded_pack.Pack = std::move(pack_definition);
                loaded_pack.Build = std::move(build_definition);
                return loaded_pack;
            }
        }

        return std::nullopt;
    }
}
