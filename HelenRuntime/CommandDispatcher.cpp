#include <HelenHook/CommandDispatcher.h>

#include <HelenHook/JsonConfigStore.h>

namespace helen
{
    CommandDispatcher::CommandDispatcher() = default;

    CommandDispatcher::CommandDispatcher(JsonConfigStore& config_store)
        : config_store_(&config_store)
    {
    }

    void CommandDispatcher::RegisterConfigInt(const std::string& key, int default_value)
    {
        if (int_values_.contains(key))
        {
            return;
        }

        const int initial_value = config_store_ != nullptr
            ? config_store_->GetInt(key, default_value)
            : default_value;

        int_values_.emplace(key, initial_value);
        if (config_store_ != nullptr)
        {
            config_store_->SetInt(key, initial_value);
            config_store_->Save();
        }
    }

    bool CommandDispatcher::TrySetInt(const std::string& key, int value)
    {
        const auto found = int_values_.find(key);
        if (found == int_values_.end())
        {
            return false;
        }

        found->second = value;
        if (config_store_ != nullptr)
        {
            config_store_->SetInt(key, value);
            config_store_->Save();
        }

        return true;
    }

    std::optional<int> CommandDispatcher::TryGetInt(const std::string& key) const
    {
        const auto found = int_values_.find(key);
        if (found == int_values_.end())
        {
            return std::nullopt;
        }

        return found->second;
    }
}