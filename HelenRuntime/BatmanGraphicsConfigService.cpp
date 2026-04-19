#include <HelenHook/BatmanGraphicsConfigService.h>

#include <HelenHook/CommandDispatcher.h>

#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{
    /**
     * @brief Stores the normalized Batman graphics draft values used by the ActionScript graphics menu.
     */
    struct BatmanGraphicsDraftState
    {
        /** @brief Normalized fullscreen state where `0` means windowed and `1` means fullscreen. */
        int Fullscreen{};
        /** @brief Horizontal resolution currently exposed to the graphics menu. */
        int ResolutionWidth{};
        /** @brief Vertical resolution currently exposed to the graphics menu. */
        int ResolutionHeight{};
        /** @brief Normalized VSync state where `0` means disabled and `1` means enabled. */
        int Vsync{};
        /** @brief Normalized MSAA menu state where `0` is disabled and higher values map to larger sample counts. */
        int Msaa{};
        /** @brief Normalized Batman detail preset state where `0-3` are presets and `4` is custom. */
        int DetailLevel{};
        /** @brief Normalized Bloom state where `0` means disabled and `1` means enabled. */
        int Bloom{};
        /** @brief Normalized Dynamic Shadows state where `0` means disabled and `1` means enabled. */
        int DynamicShadows{};
        /** @brief Normalized Motion Blur state where `0` means disabled and `1` means enabled. */
        int MotionBlur{};
        /** @brief Normalized Distortion state where `0` means disabled and `1` means enabled. */
        int Distortion{};
        /** @brief Normalized Fog Volumes state where `0` means disabled and `1` means enabled. */
        int FogVolumes{};
        /** @brief Normalized spherical-harmonic-lighting state where `0` means disabled and `1` means enabled. */
        int SphericalHarmonicLighting{};
        /** @brief Normalized ambient-occlusion state where `0` means disabled and `1` means enabled. */
        int AmbientOcclusion{};
        /** @brief Normalized PhysX quality state where `0` is off, `1` is normal, and `2` is high. */
        int Physx{};
        /** @brief Normalized stereo-rendering state where `0` means disabled and `1` means enabled. */
        int Stereo{};
    };

    /**
     * @brief Describes one Batman graphics detail preset and the encoded UE3 detail-mode it should persist.
     */
    struct BatmanGraphicsPresetDefinition
    {
        /** @brief Menu-visible Batman detail-level state for this preset. */
        int DetailLevel{};
        /** @brief Raw `DetailMode` value that UE3 expects for this preset. */
        int DetailMode{};
        /** @brief Bloom toggle encoded by this preset. */
        int Bloom{};
        /** @brief Dynamic-shadows toggle encoded by this preset. */
        int DynamicShadows{};
        /** @brief Motion-blur toggle encoded by this preset. */
        int MotionBlur{};
        /** @brief Distortion toggle encoded by this preset. */
        int Distortion{};
        /** @brief Fog-volumes toggle encoded by this preset. */
        int FogVolumes{};
        /** @brief Spherical-harmonic-lighting toggle encoded by this preset. */
        int SphericalHarmonicLighting{};
        /** @brief Ambient-occlusion toggle encoded by this preset. */
        int AmbientOcclusion{};
    };

    /** @brief Canonical Batman graphics presets derived from the retail launcher detail settings. */
    constexpr std::array<BatmanGraphicsPresetDefinition, 4> BatmanGraphicsPresets = {
        BatmanGraphicsPresetDefinition{
            .DetailLevel = 0,
            .DetailMode = 0,
            .Bloom = 0,
            .DynamicShadows = 0,
            .MotionBlur = 0,
            .Distortion = 0,
            .FogVolumes = 0,
            .SphericalHarmonicLighting = 0,
            .AmbientOcclusion = 0
        },
        BatmanGraphicsPresetDefinition{
            .DetailLevel = 1,
            .DetailMode = 1,
            .Bloom = 1,
            .DynamicShadows = 1,
            .MotionBlur = 0,
            .Distortion = 0,
            .FogVolumes = 0,
            .SphericalHarmonicLighting = 0,
            .AmbientOcclusion = 0
        },
        BatmanGraphicsPresetDefinition{
            .DetailLevel = 2,
            .DetailMode = 1,
            .Bloom = 1,
            .DynamicShadows = 1,
            .MotionBlur = 1,
            .Distortion = 1,
            .FogVolumes = 1,
            .SphericalHarmonicLighting = 1,
            .AmbientOcclusion = 0
        },
        BatmanGraphicsPresetDefinition{
            .DetailLevel = 3,
            .DetailMode = 2,
            .Bloom = 1,
            .DynamicShadows = 1,
            .MotionBlur = 1,
            .Distortion = 1,
            .FogVolumes = 1,
            .SphericalHarmonicLighting = 1,
            .AmbientOcclusion = 1
        }
    };

    /**
     * @brief Removes ASCII leading and trailing whitespace from one string view.
     * @param text Text view that should be trimmed.
     * @return Trimmed text as an owning string.
     */
    std::string TrimAscii(std::string_view text)
    {
        std::size_t start = 0;
        while (start < text.size())
        {
            const char character = text[start];
            if (character != ' ' && character != '\t' && character != '\r' && character != '\n')
            {
                break;
            }

            ++start;
        }

        std::size_t end = text.size();
        while (end > start)
        {
            const char character = text[end - 1];
            if (character != ' ' && character != '\t' && character != '\r' && character != '\n')
            {
                break;
            }

            --end;
        }

        return std::string(text.substr(start, end - start));
    }

    /**
     * @brief Returns true when two ASCII strings match ignoring character case.
     * @param left First ASCII string that should be compared.
     * @param right Second ASCII string that should be compared.
     * @return True when the strings match ignoring ASCII case; otherwise false.
     */
    bool EqualsIgnoreCaseAscii(std::string_view left, std::string_view right)
    {
        if (left.size() != right.size())
        {
            return false;
        }

        for (std::size_t index = 0; index < left.size(); ++index)
        {
            char left_character = left[index];
            if (left_character >= 'A' && left_character <= 'Z')
            {
                left_character = static_cast<char>(left_character - 'A' + 'a');
            }

            char right_character = right[index];
            if (right_character >= 'A' && right_character <= 'Z')
            {
                right_character = static_cast<char>(right_character - 'A' + 'a');
            }

            if (left_character != right_character)
            {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Splits one INI assignment line into key and value components.
     * @param line Raw INI line that may contain one `key=value` assignment.
     * @param key Receives the trimmed assignment key on success.
     * @param value Receives the trimmed assignment value on success.
     * @return True when the line contains one assignment; otherwise false.
     */
    bool TrySplitIniAssignment(std::string_view line, std::string& key, std::string& value)
    {
        const std::size_t separator_index = line.find('=');
        if (separator_index == std::string_view::npos)
        {
            return false;
        }

        key = TrimAscii(line.substr(0, separator_index));
        value = TrimAscii(line.substr(separator_index + 1));
        return !key.empty();
    }

    /**
     * @brief Returns true when one INI line declares the supplied section.
     * @param line Raw INI line that may contain one section declaration.
     * @param section_name Exact section name that should be matched.
     * @return True when the line declares the supplied section; otherwise false.
     */
    bool IsIniSectionDeclaration(std::string_view line, std::string_view section_name)
    {
        const std::string trimmed = TrimAscii(line);
        if (trimmed.size() < 3 || trimmed.front() != '[' || trimmed.back() != ']')
        {
            return false;
        }

        return trimmed.substr(1, trimmed.size() - 2) == section_name;
    }

    /**
     * @brief Reads every line from one text file while normalizing trailing carriage returns away.
     * @param path Text file path that should be loaded.
     * @return File lines without trailing carriage returns when the file opens successfully; otherwise no value.
     */
    std::optional<std::vector<std::string>> TryReadAllLines(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            return std::nullopt;
        }

        std::vector<std::string> lines;
        std::string line;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            lines.push_back(line);
        }

        if (stream.bad())
        {
            return std::nullopt;
        }

        return lines;
    }

    /**
     * @brief Writes text lines back to disk using CRLF line endings.
     * @param path File path that should receive the supplied lines.
     * @param lines Normalized text lines that should be written to the file.
     * @return True when the file is written successfully; otherwise false.
     */
    bool WriteAllLines(const std::filesystem::path& path, const std::vector<std::string>& lines)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            return false;
        }

        for (std::size_t index = 0; index < lines.size(); ++index)
        {
            stream << lines[index];
            if (index + 1 < lines.size())
            {
                stream << "\r\n";
            }
        }

        return static_cast<bool>(stream);
    }

    /**
     * @brief Reads one required INI value from a specific section.
     * @param lines Loaded INI file lines that should be searched.
     * @param section_name Exact section name that owns the required value.
     * @param key_name Exact key name that should be resolved within the section.
     * @return Trimmed INI value when found; otherwise no value.
     */
    std::optional<std::string> TryReadIniValue(
        const std::vector<std::string>& lines,
        std::string_view section_name,
        std::string_view key_name)
    {
        bool in_target_section = false;
        for (const std::string& line : lines)
        {
            const std::string trimmed = TrimAscii(line);
            if (trimmed.empty() || trimmed[0] == ';')
            {
                continue;
            }

            if (trimmed.front() == '[')
            {
                in_target_section = IsIniSectionDeclaration(trimmed, section_name);
                continue;
            }

            if (!in_target_section)
            {
                continue;
            }

            std::string key;
            std::string value;
            if (!TrySplitIniAssignment(trimmed, key, value))
            {
                continue;
            }

            if (key == key_name)
            {
                return value;
            }
        }

        return std::nullopt;
    }

    /**
     * @brief Updates one required INI value inside a specific section.
     * @param lines Loaded INI file lines that should be updated in place.
     * @param section_name Exact section name that owns the target value.
     * @param key_name Exact key name that should be updated.
     * @param encoded_value Replacement value text that should be written.
     * @return True when the target key is found and replaced; otherwise false.
     */
    bool UpdateIniValue(
        std::vector<std::string>& lines,
        std::string_view section_name,
        std::string_view key_name,
        std::string_view encoded_value)
    {
        bool in_target_section = false;
        for (std::string& line : lines)
        {
            const std::string trimmed = TrimAscii(line);
            if (trimmed.empty() || trimmed[0] == ';')
            {
                continue;
            }

            if (trimmed.front() == '[')
            {
                in_target_section = IsIniSectionDeclaration(trimmed, section_name);
                continue;
            }

            if (!in_target_section)
            {
                continue;
            }

            std::string key;
            std::string value;
            if (!TrySplitIniAssignment(trimmed, key, value))
            {
                continue;
            }

            if (key == key_name)
            {
                line = std::string(key_name) + "=" + std::string(encoded_value);
                return true;
            }
        }

        return false;
    }

    /**
     * @brief Parses one strict integer setting from trimmed INI text.
     * @param text Trimmed INI value that should encode an integer.
     * @param value Receives the parsed integer on success.
     * @return True when the full value parses as an integer; otherwise false.
     */
    bool TryParseIntValue(std::string_view text, int& value)
    {
        const std::string trimmed = TrimAscii(text);
        if (trimmed.empty())
        {
            return false;
        }

        const char* const begin = trimmed.data();
        const char* const end = trimmed.data() + trimmed.size();
        const std::from_chars_result result = std::from_chars(begin, end, value);
        return result.ec == std::errc() && result.ptr == end;
    }

    /**
     * @brief Parses one UE3 boolean setting into the normalized menu representation.
     * @param text Trimmed INI value that should encode `True` or `False`.
     * @param value Receives `1` for true and `0` for false on success.
     * @return True when the value is a supported UE3 boolean token; otherwise false.
     */
    bool TryParseBoolValue(std::string_view text, int& value)
    {
        const std::string trimmed = TrimAscii(text);
        if (EqualsIgnoreCaseAscii(trimmed, "True"))
        {
            value = 1;
            return true;
        }

        if (EqualsIgnoreCaseAscii(trimmed, "False"))
        {
            value = 0;
            return true;
        }

        return false;
    }

    /**
     * @brief Encodes one normalized menu boolean into the UE3 `True` or `False` text format.
     * @param value Normalized menu boolean where `0` means false and `1` means true.
     * @param encoded_value Receives the encoded UE3 boolean token on success.
     * @return True when the value is a supported normalized boolean; otherwise false.
     */
    bool TryEncodeBoolValue(int value, std::string& encoded_value)
    {
        if (value == 0)
        {
            encoded_value = "False";
            return true;
        }

        if (value == 1)
        {
            encoded_value = "True";
            return true;
        }

        return false;
    }

    /**
     * @brief Maps one UE3 `MaxMultisamples` integer into the normalized Batman menu MSAA state.
     * @param encoded_value Raw UE3 `MaxMultisamples` integer value.
     * @param normalized_value Receives the normalized Batman MSAA state on success.
     * @return True when the encoded value maps to a supported Batman menu state; otherwise false.
     */
    bool TryMapMsaaFromIniValue(int encoded_value, int& normalized_value)
    {
        if (encoded_value <= 1)
        {
            normalized_value = 0;
            return true;
        }

        if (encoded_value == 2)
        {
            normalized_value = 1;
            return true;
        }

        if (encoded_value == 4)
        {
            normalized_value = 2;
            return true;
        }

        if (encoded_value == 8)
        {
            normalized_value = 3;
            return true;
        }

        if (encoded_value == 16)
        {
            normalized_value = 5;
            return true;
        }

        return false;
    }

    /**
     * @brief Encodes one normalized Batman menu MSAA state into the UE3 `MaxMultisamples` integer.
     * @param normalized_value Batman menu MSAA state that should be persisted.
     * @param encoded_value Receives the UE3 `MaxMultisamples` integer on success.
     * @return True when the normalized state maps to a supported UE3 sample count; otherwise false.
     */
    bool TryEncodeMsaaValue(int normalized_value, int& encoded_value)
    {
        if (normalized_value == 0)
        {
            encoded_value = 1;
            return true;
        }

        if (normalized_value == 1)
        {
            encoded_value = 2;
            return true;
        }

        if (normalized_value == 2)
        {
            encoded_value = 4;
            return true;
        }

        if (normalized_value == 3 || normalized_value == 4)
        {
            encoded_value = 8;
            return true;
        }

        if (normalized_value == 5 || normalized_value == 6)
        {
            encoded_value = 16;
            return true;
        }

        return false;
    }

    /**
     * @brief Returns the canonical Batman detail preset for one normalized detail-level state.
     * @param detail_level Normalized Batman menu detail-level state.
     * @return Matching preset definition when the detail level is one of the declared presets; otherwise no value.
     */
    std::optional<BatmanGraphicsPresetDefinition> TryGetPresetByDetailLevel(int detail_level)
    {
        for (const BatmanGraphicsPresetDefinition& preset : BatmanGraphicsPresets)
        {
            if (preset.DetailLevel == detail_level)
            {
                return preset;
            }
        }

        return std::nullopt;
    }

    /**
     * @brief Resolves the declared Batman detail preset that matches the current individual toggle values.
     * @param state Current normalized Batman graphics draft state.
     * @return Matching preset definition when the current toggles exactly match one preset; otherwise no value.
     */
    std::optional<BatmanGraphicsPresetDefinition> TryResolvePresetFromDraft(const BatmanGraphicsDraftState& state)
    {
        for (const BatmanGraphicsPresetDefinition& preset : BatmanGraphicsPresets)
        {
            if (preset.Bloom == state.Bloom &&
                preset.DynamicShadows == state.DynamicShadows &&
                preset.MotionBlur == state.MotionBlur &&
                preset.Distortion == state.Distortion &&
                preset.FogVolumes == state.FogVolumes &&
                preset.SphericalHarmonicLighting == state.SphericalHarmonicLighting &&
                preset.AmbientOcclusion == state.AmbientOcclusion)
            {
                return preset;
            }
        }

        return std::nullopt;
    }

    /**
     * @brief Derives the most appropriate UE3 `DetailMode` value for one normalized Batman graphics draft.
     * @param state Current normalized Batman graphics draft state.
     * @return Derived UE3 `DetailMode` integer that best matches the current draft state.
     */
    int DeriveDetailModeFromDraft(const BatmanGraphicsDraftState& state)
    {
        const std::optional<BatmanGraphicsPresetDefinition> preset = TryResolvePresetFromDraft(state);
        if (preset.has_value())
        {
            return preset->DetailMode;
        }

        if (state.AmbientOcclusion != 0)
        {
            return 2;
        }

        if (state.MotionBlur != 0 ||
            state.Distortion != 0 ||
            state.FogVolumes != 0 ||
            state.SphericalHarmonicLighting != 0 ||
            state.Bloom != 0 ||
            state.DynamicShadows != 0)
        {
            return 1;
        }

        return 0;
    }

    /**
     * @brief Reads one required dispatcher integer value and copies it into the supplied storage slot.
     * @param dispatcher Dispatcher that owns the required config key.
     * @param key Registered config key that should be resolved.
     * @param value Receives the resolved integer on success.
     * @return True when the key exists in the dispatcher; otherwise false.
     */
    bool TryReadDispatcherValue(const helen::CommandDispatcher& dispatcher, const char* key, int& value)
    {
        const std::optional<int> resolved_value = dispatcher.TryGetInt(key);
        if (!resolved_value.has_value())
        {
            return false;
        }

        value = *resolved_value;
        return true;
    }

    /**
     * @brief Writes one required dispatcher integer value back into the registered config store.
     * @param dispatcher Dispatcher that owns the required config key.
     * @param key Registered config key that should be updated.
     * @param value New integer value that should be stored.
     * @return True when the key exists in the dispatcher; otherwise false.
     */
    bool TryWriteDispatcherValue(helen::CommandDispatcher& dispatcher, const char* key, int value)
    {
        return dispatcher.TrySetInt(key, value);
    }

    /**
     * @brief Reads the full Batman graphics draft state from the current dispatcher values.
     * @param dispatcher Dispatcher that should supply the registered Batman graphics config keys.
     * @param state Receives the full normalized Batman graphics draft on success.
     * @return True when every required config key exists; otherwise false.
     */
    bool TryReadDraftStateFromDispatcher(const helen::CommandDispatcher& dispatcher, BatmanGraphicsDraftState& state)
    {
        return
            TryReadDispatcherValue(dispatcher, "fullscreen", state.Fullscreen) &&
            TryReadDispatcherValue(dispatcher, "resolutionWidth", state.ResolutionWidth) &&
            TryReadDispatcherValue(dispatcher, "resolutionHeight", state.ResolutionHeight) &&
            TryReadDispatcherValue(dispatcher, "vsync", state.Vsync) &&
            TryReadDispatcherValue(dispatcher, "msaa", state.Msaa) &&
            TryReadDispatcherValue(dispatcher, "detailLevel", state.DetailLevel) &&
            TryReadDispatcherValue(dispatcher, "bloom", state.Bloom) &&
            TryReadDispatcherValue(dispatcher, "dynamicShadows", state.DynamicShadows) &&
            TryReadDispatcherValue(dispatcher, "motionBlur", state.MotionBlur) &&
            TryReadDispatcherValue(dispatcher, "distortion", state.Distortion) &&
            TryReadDispatcherValue(dispatcher, "fogVolumes", state.FogVolumes) &&
            TryReadDispatcherValue(dispatcher, "sphericalHarmonicLighting", state.SphericalHarmonicLighting) &&
            TryReadDispatcherValue(dispatcher, "ambientOcclusion", state.AmbientOcclusion) &&
            TryReadDispatcherValue(dispatcher, "physx", state.Physx) &&
            TryReadDispatcherValue(dispatcher, "stereo", state.Stereo);
    }

    /**
     * @brief Writes the full Batman graphics draft state back into the registered dispatcher keys.
     * @param dispatcher Dispatcher that should receive the normalized Batman graphics draft values.
     * @param state Fully populated Batman graphics draft state that should be stored.
     * @return True when every required config key exists; otherwise false.
     */
    bool TryWriteDraftStateToDispatcher(helen::CommandDispatcher& dispatcher, const BatmanGraphicsDraftState& state)
    {
        return
            TryWriteDispatcherValue(dispatcher, "fullscreen", state.Fullscreen) &&
            TryWriteDispatcherValue(dispatcher, "resolutionWidth", state.ResolutionWidth) &&
            TryWriteDispatcherValue(dispatcher, "resolutionHeight", state.ResolutionHeight) &&
            TryWriteDispatcherValue(dispatcher, "vsync", state.Vsync) &&
            TryWriteDispatcherValue(dispatcher, "msaa", state.Msaa) &&
            TryWriteDispatcherValue(dispatcher, "detailLevel", state.DetailLevel) &&
            TryWriteDispatcherValue(dispatcher, "bloom", state.Bloom) &&
            TryWriteDispatcherValue(dispatcher, "dynamicShadows", state.DynamicShadows) &&
            TryWriteDispatcherValue(dispatcher, "motionBlur", state.MotionBlur) &&
            TryWriteDispatcherValue(dispatcher, "distortion", state.Distortion) &&
            TryWriteDispatcherValue(dispatcher, "fogVolumes", state.FogVolumes) &&
            TryWriteDispatcherValue(dispatcher, "sphericalHarmonicLighting", state.SphericalHarmonicLighting) &&
            TryWriteDispatcherValue(dispatcher, "ambientOcclusion", state.AmbientOcclusion) &&
            TryWriteDispatcherValue(dispatcher, "physx", state.Physx) &&
            TryWriteDispatcherValue(dispatcher, "stereo", state.Stereo);
    }

    /**
     * @brief Loads the normalized Batman graphics draft state from the INI file lines.
     * @param lines Parsed INI file lines that should be translated into the normalized menu state.
     * @param state Receives the normalized Batman graphics draft on success.
     * @return True when every required INI key is present and maps successfully; otherwise false.
     */
    bool TryReadDraftStateFromIniLines(const std::vector<std::string>& lines, BatmanGraphicsDraftState& state)
    {
        std::optional<std::string> raw_value = TryReadIniValue(lines, "SystemSettings", "Fullscreen");
        if (!raw_value.has_value() || !TryParseBoolValue(*raw_value, state.Fullscreen))
        {
            return false;
        }

        raw_value = TryReadIniValue(lines, "SystemSettings", "ResX");
        if (!raw_value.has_value() || !TryParseIntValue(*raw_value, state.ResolutionWidth))
        {
            return false;
        }

        raw_value = TryReadIniValue(lines, "SystemSettings", "ResY");
        if (!raw_value.has_value() || !TryParseIntValue(*raw_value, state.ResolutionHeight))
        {
            return false;
        }

        raw_value = TryReadIniValue(lines, "SystemSettings", "UseVsync");
        if (!raw_value.has_value() || !TryParseBoolValue(*raw_value, state.Vsync))
        {
            return false;
        }

        int raw_msaa = 0;
        raw_value = TryReadIniValue(lines, "SystemSettings", "MaxMultisamples");
        if (!raw_value.has_value() || !TryParseIntValue(*raw_value, raw_msaa) || !TryMapMsaaFromIniValue(raw_msaa, state.Msaa))
        {
            return false;
        }

        raw_value = TryReadIniValue(lines, "SystemSettings", "Bloom");
        if (!raw_value.has_value() || !TryParseBoolValue(*raw_value, state.Bloom))
        {
            return false;
        }

        raw_value = TryReadIniValue(lines, "SystemSettings", "DynamicShadows");
        if (!raw_value.has_value() || !TryParseBoolValue(*raw_value, state.DynamicShadows))
        {
            return false;
        }

        raw_value = TryReadIniValue(lines, "SystemSettings", "MotionBlur");
        if (!raw_value.has_value() || !TryParseBoolValue(*raw_value, state.MotionBlur))
        {
            return false;
        }

        raw_value = TryReadIniValue(lines, "SystemSettings", "Distortion");
        if (!raw_value.has_value() || !TryParseBoolValue(*raw_value, state.Distortion))
        {
            return false;
        }

        raw_value = TryReadIniValue(lines, "SystemSettings", "FogVolumes");
        if (!raw_value.has_value() || !TryParseBoolValue(*raw_value, state.FogVolumes))
        {
            return false;
        }

        int raw_disable_spherical_harmonic_lights = 0;
        raw_value = TryReadIniValue(lines, "SystemSettings", "DisableSphericalHarmonicLights");
        if (!raw_value.has_value() || !TryParseBoolValue(*raw_value, raw_disable_spherical_harmonic_lights))
        {
            return false;
        }

        state.SphericalHarmonicLighting = raw_disable_spherical_harmonic_lights == 0 ? 1 : 0;

        raw_value = TryReadIniValue(lines, "SystemSettings", "AmbientOcclusion");
        if (!raw_value.has_value() || !TryParseBoolValue(*raw_value, state.AmbientOcclusion))
        {
            return false;
        }

        raw_value = TryReadIniValue(lines, "Engine.Engine", "PhysXLevel");
        if (!raw_value.has_value() || !TryParseIntValue(*raw_value, state.Physx))
        {
            return false;
        }

        raw_value = TryReadIniValue(lines, "SystemSettings", "Stereo");
        if (!raw_value.has_value() || !TryParseBoolValue(*raw_value, state.Stereo))
        {
            return false;
        }

        const std::optional<BatmanGraphicsPresetDefinition> preset = TryResolvePresetFromDraft(state);
        state.DetailLevel = preset.has_value() ? preset->DetailLevel : 4;
        return true;
    }

    /**
     * @brief Applies one declared Batman detail preset to the supplied draft state.
     * @param state Draft state that should receive the preset-controlled toggle values.
     * @return True when `state.DetailLevel` resolves to a declared Batman preset; otherwise false.
     */
    bool ApplyDetailPresetToDraftState(BatmanGraphicsDraftState& state)
    {
        const std::optional<BatmanGraphicsPresetDefinition> preset = TryGetPresetByDetailLevel(state.DetailLevel);
        if (!preset.has_value())
        {
            return false;
        }

        state.Bloom = preset->Bloom;
        state.DynamicShadows = preset->DynamicShadows;
        state.MotionBlur = preset->MotionBlur;
        state.Distortion = preset->Distortion;
        state.FogVolumes = preset->FogVolumes;
        state.SphericalHarmonicLighting = preset->SphericalHarmonicLighting;
        state.AmbientOcclusion = preset->AmbientOcclusion;
        return true;
    }
}

namespace helen
{
    /**
     * @brief Binds the service to one concrete `BmEngine.ini` path.
     * @param ini_path Absolute or relative path to the Batman user engine INI file.
     * @throws std::invalid_argument Thrown when `ini_path` is empty.
     */
    BatmanGraphicsConfigService::BatmanGraphicsConfigService(std::filesystem::path ini_path)
        : ini_path_(std::move(ini_path))
    {
        if (ini_path_.empty())
        {
            throw std::invalid_argument("Batman graphics config service requires a non-empty INI path.");
        }
    }

    /**
     * @brief Reads the current Batman graphics settings from `BmEngine.ini` into registered config keys.
     * @param dispatcher Config dispatcher that receives the normalized graphics draft values.
     * @return True when every required INI value is present and every config key updates successfully; otherwise false.
     */
    bool BatmanGraphicsConfigService::LoadIntoDispatcher(CommandDispatcher& dispatcher) const
    {
        const std::optional<std::vector<std::string>> lines = TryReadAllLines(ini_path_);
        if (!lines.has_value())
        {
            return false;
        }

        BatmanGraphicsDraftState state;
        if (!TryReadDraftStateFromIniLines(*lines, state))
        {
            return false;
        }

        return TryWriteDraftStateToDispatcher(dispatcher, state);
    }

    /**
     * @brief Writes the current normalized graphics draft values back into `BmEngine.ini`.
     * @param dispatcher Config dispatcher that supplies the normalized graphics draft values.
     * @return True when every required config key is present and every target INI value is updated successfully; otherwise false.
     */
    bool BatmanGraphicsConfigService::ApplyFromDispatcher(const CommandDispatcher& dispatcher) const
    {
        BatmanGraphicsDraftState state;
        if (!TryReadDraftStateFromDispatcher(dispatcher, state))
        {
            return false;
        }

        if (state.DetailLevel != 4 && !ApplyDetailPresetToDraftState(state))
        {
            return false;
        }

        const std::optional<std::vector<std::string>> existing_lines = TryReadAllLines(ini_path_);
        if (!existing_lines.has_value())
        {
            return false;
        }

        std::vector<std::string> lines = *existing_lines;
        std::string encoded_value;

        if (!TryEncodeBoolValue(state.Fullscreen, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "Fullscreen", encoded_value))
        {
            return false;
        }

        encoded_value = std::to_string(state.ResolutionWidth);
        if (!UpdateIniValue(lines, "SystemSettings", "ResX", encoded_value))
        {
            return false;
        }

        encoded_value = std::to_string(state.ResolutionHeight);
        if (!UpdateIniValue(lines, "SystemSettings", "ResY", encoded_value))
        {
            return false;
        }

        if (!TryEncodeBoolValue(state.Vsync, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "UseVsync", encoded_value))
        {
            return false;
        }

        int encoded_msaa = 0;
        if (!TryEncodeMsaaValue(state.Msaa, encoded_msaa))
        {
            return false;
        }

        encoded_value = std::to_string(encoded_msaa);
        if (!UpdateIniValue(lines, "SystemSettings", "MaxMultisamples", encoded_value))
        {
            return false;
        }

        encoded_value = std::to_string(DeriveDetailModeFromDraft(state));
        if (!UpdateIniValue(lines, "SystemSettings", "DetailMode", encoded_value))
        {
            return false;
        }

        if (!TryEncodeBoolValue(state.Bloom, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "Bloom", encoded_value))
        {
            return false;
        }

        if (!TryEncodeBoolValue(state.DynamicShadows, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "DynamicShadows", encoded_value))
        {
            return false;
        }

        if (!TryEncodeBoolValue(state.MotionBlur, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "MotionBlur", encoded_value))
        {
            return false;
        }

        if (!TryEncodeBoolValue(state.Distortion, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "Distortion", encoded_value))
        {
            return false;
        }

        if (!TryEncodeBoolValue(state.FogVolumes, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "FogVolumes", encoded_value))
        {
            return false;
        }

        const int encoded_disable_spherical_harmonic_lights = state.SphericalHarmonicLighting == 0 ? 1 : 0;
        if (!TryEncodeBoolValue(encoded_disable_spherical_harmonic_lights, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "DisableSphericalHarmonicLights", encoded_value))
        {
            return false;
        }

        if (!TryEncodeBoolValue(state.AmbientOcclusion, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "AmbientOcclusion", encoded_value))
        {
            return false;
        }

        encoded_value = std::to_string(state.Physx);
        if (!UpdateIniValue(lines, "Engine.Engine", "PhysXLevel", encoded_value))
        {
            return false;
        }

        if (!TryEncodeBoolValue(state.Stereo, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "Stereo", encoded_value))
        {
            return false;
        }

        return WriteAllLines(ini_path_, lines);
    }

    /**
     * @brief Recomputes the derived `detailLevel` draft state from the current individual detail toggles.
     * @param dispatcher Config dispatcher that stores the current Batman graphics draft.
     * @return True when the required draft keys exist and the derived `detailLevel` is updated successfully; otherwise false.
     */
    bool BatmanGraphicsConfigService::SyncDetailLevelFromDispatcher(CommandDispatcher& dispatcher) const
    {
        BatmanGraphicsDraftState state;
        if (!TryReadDraftStateFromDispatcher(dispatcher, state))
        {
            return false;
        }

        const std::optional<BatmanGraphicsPresetDefinition> preset = TryResolvePresetFromDraft(state);
        state.DetailLevel = preset.has_value() ? preset->DetailLevel : 4;
        return TryWriteDispatcherValue(dispatcher, "detailLevel", state.DetailLevel);
    }

    /**
     * @brief Applies the currently selected `detailLevel` preset to the individual detail-toggle draft values.
     * @param dispatcher Config dispatcher that stores the current Batman graphics draft.
     * @return True when `detailLevel` resolves to a supported preset and the dependent draft values update successfully; otherwise false.
     */
    bool BatmanGraphicsConfigService::ApplySelectedDetailLevelToDispatcher(CommandDispatcher& dispatcher) const
    {
        BatmanGraphicsDraftState state;
        if (!TryReadDraftStateFromDispatcher(dispatcher, state))
        {
            return false;
        }

        if (!ApplyDetailPresetToDraftState(state))
        {
            return false;
        }

        return
            TryWriteDispatcherValue(dispatcher, "bloom", state.Bloom) &&
            TryWriteDispatcherValue(dispatcher, "dynamicShadows", state.DynamicShadows) &&
            TryWriteDispatcherValue(dispatcher, "motionBlur", state.MotionBlur) &&
            TryWriteDispatcherValue(dispatcher, "distortion", state.Distortion) &&
            TryWriteDispatcherValue(dispatcher, "fogVolumes", state.FogVolumes) &&
            TryWriteDispatcherValue(dispatcher, "sphericalHarmonicLighting", state.SphericalHarmonicLighting) &&
            TryWriteDispatcherValue(dispatcher, "ambientOcclusion", state.AmbientOcclusion);
    }

    /**
     * @brief Returns the INI path used by this service.
     * @return Bound `BmEngine.ini` path.
     */
    const std::filesystem::path& BatmanGraphicsConfigService::GetIniPath() const noexcept
    {
        return ini_path_;
    }
}
