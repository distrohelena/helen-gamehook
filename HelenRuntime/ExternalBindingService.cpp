#include <HelenHook/ExternalBindingService.h>

#include <HelenHook/CommandDispatcher.h>
#include <HelenHook/CommandExecutor.h>

#include <optional>
#include <stdexcept>

namespace helen
{
    ExternalBindingService::ExternalBindingService(CommandDispatcher& dispatcher, CommandExecutor& executor)
        : dispatcher_(dispatcher),
          executor_(executor)
    {
    }

    void ExternalBindingService::Register(const ExternalBindingDefinition& binding)
    {
        if (binding.ExternalName.empty())
        {
            throw std::invalid_argument("External binding requires a non-empty external name.");
        }

        if (binding.Mode.empty())
        {
            throw std::invalid_argument("External binding requires a non-empty mode.");
        }

        if (binding.Mode == "get-int" || binding.Mode == "set-int")
        {
            if (binding.ConfigKey.empty())
            {
                throw std::invalid_argument("Integer config bindings require a config key.");
            }
        }
        else if (binding.Mode == "run-command")
        {
            if (binding.CommandId.empty())
            {
                throw std::invalid_argument("Run-command bindings require a command identifier.");
            }
        }
        else
        {
            throw std::invalid_argument("External binding mode is not supported.");
        }

        bindings_.emplace(binding.ExternalName, binding);
    }

    bool ExternalBindingService::TryHandleGetInt(const std::string& external_name, const std::string& key, int& value) const
    {
        const auto range = bindings_.equal_range(external_name);
        for (auto it = range.first; it != range.second; ++it)
        {
            const ExternalBindingDefinition& binding = it->second;
            if (binding.Mode != "get-int" || binding.ConfigKey != key)
            {
                continue;
            }

            const auto current_value = dispatcher_.TryGetInt(binding.ConfigKey);
            if (!current_value.has_value())
            {
                return false;
            }

            value = *current_value;
            return true;
        }

        return false;
    }

    bool ExternalBindingService::TryHandleSetInt(const std::string& external_name, const std::string& key, int value)
    {
        const auto range = bindings_.equal_range(external_name);
        for (auto it = range.first; it != range.second; ++it)
        {
            const ExternalBindingDefinition& binding = it->second;
            if (binding.Mode != "set-int" || binding.ConfigKey != key)
            {
                continue;
            }

            const std::optional<int> previous_value = dispatcher_.TryGetInt(binding.ConfigKey);
            if (!previous_value.has_value())
            {
                return false;
            }

            if (!dispatcher_.TrySetInt(binding.ConfigKey, value))
            {
                return false;
            }

            if (binding.CommandId.empty())
            {
                return true;
            }

            if (executor_.RunCommand(binding.CommandId))
            {
                return true;
            }

            if (!dispatcher_.TrySetInt(binding.ConfigKey, *previous_value))
            {
                return false;
            }

            return false;
        }

        return false;
    }

    bool ExternalBindingService::TryHandleRunCommand(const std::string& external_name, const std::string& command_id)
    {
        const auto range = bindings_.equal_range(external_name);
        for (auto it = range.first; it != range.second; ++it)
        {
            const ExternalBindingDefinition& binding = it->second;
            if (binding.Mode != "run-command" || binding.CommandId != command_id)
            {
                continue;
            }

            return executor_.RunCommand(binding.CommandId);
        }

        return false;
    }
}