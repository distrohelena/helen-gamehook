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

    /**
     * @brief Updates two distinct registered integer config keys as one dispatcher operation.
     * @param first_key Registered key receiving the first value.
     * @param first_value Integer value assigned to the first key.
     * @param second_key Registered key receiving the second value.
     * @param second_value Integer value assigned to the second key.
     * @return True when both keys are registered and the pair is persisted successfully; otherwise false with both prior values retained.
     */
    bool CommandDispatcher::TrySetIntPair(
        const std::string& first_key,
        int first_value,
        const std::string& second_key,
        int second_value)
    {
        if (first_key == second_key)
        {
            return false;
        }

        const auto first = int_values_.find(first_key);
        const auto second = int_values_.find(second_key);
        if (first == int_values_.end() || second == int_values_.end())
        {
            return false;
        }

        const int previous_first_value = first->second;
        const int previous_second_value = second->second;
        first->second = first_value;
        second->second = second_value;
        if (config_store_ != nullptr)
        {
            try
            {
                config_store_->SetInt(first_key, first_value);
                config_store_->SetInt(second_key, second_value);
                config_store_->Save();
            }
            catch (...)
            {
                first->second = previous_first_value;
                second->second = previous_second_value;
                config_store_->SetInt(first_key, previous_first_value);
                config_store_->SetInt(second_key, previous_second_value);
                return false;
            }
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
