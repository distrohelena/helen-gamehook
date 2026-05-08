#include <HelenHook/PackSelectionConfig.h>

#include <HelenHook/JsonParser.h>
#include <HelenHook/JsonValue.h>

#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>

namespace
{
    /**
     * @brief Reads one full text file into memory for JSON parsing.
     * @param path Filesystem path that should be loaded.
     * @return Complete file contents as a byte string.
     */
    std::string ReadAllText(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            throw std::runtime_error("Failed to open pack selection config file.");
        }

        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }
}

namespace helen
{
    PackSelectionConfig::PackSelectionConfig(std::filesystem::path path)
        : path_(std::move(path))
    {
        Load();
    }

    std::optional<std::vector<std::string>> PackSelectionConfig::TryGetEnabledPacks(const std::string& executable_name) const
    {
        const auto found = enabled_packs_by_executable_.find(executable_name);
        if (found == enabled_packs_by_executable_.end())
        {
            return std::nullopt;
        }

        return found->second;
    }

    void PackSelectionConfig::Load()
    {
        enabled_packs_by_executable_.clear();
        if (!std::filesystem::exists(path_))
        {
            return;
        }

        const std::optional<JsonValue> parsed = JsonParser::Parse(ReadAllText(path_));
        if (!parsed.has_value())
        {
            throw std::runtime_error("Pack selection config does not contain valid JSON.");
        }

        const JsonValue::Object* root = parsed->AsObject();
        if (root == nullptr)
        {
            throw std::runtime_error("Pack selection config root must be a JSON object.");
        }

        const auto enabled_packs_entry = root->find("enabledPacksByExecutable");
        if (enabled_packs_entry == root->end())
        {
            return;
        }

        const JsonValue::Object* enabled_packs_object = enabled_packs_entry->second.AsObject();
        if (enabled_packs_object == nullptr)
        {
            throw std::runtime_error("enabledPacksByExecutable must be a JSON object.");
        }

        for (const auto& [executable_name, pack_list_value] : *enabled_packs_object)
        {
            const JsonValue::Array* pack_array = pack_list_value.AsArray();
            if (pack_array == nullptr)
            {
                throw std::runtime_error("Enabled pack list must be a JSON array.");
            }

            std::set<std::string> seen_pack_ids;
            std::vector<std::string> pack_ids;
            for (const JsonValue& pack_value : *pack_array)
            {
                const std::string* pack_id = pack_value.AsString();
                if (pack_id == nullptr || pack_id->empty())
                {
                    throw std::runtime_error("Enabled pack id must be a non-empty string.");
                }

                if (!seen_pack_ids.insert(*pack_id).second)
                {
                    throw std::runtime_error("Enabled pack ids must be unique per executable.");
                }

                pack_ids.push_back(*pack_id);
            }

            enabled_packs_by_executable_.emplace(executable_name, std::move(pack_ids));
        }
    }
}
