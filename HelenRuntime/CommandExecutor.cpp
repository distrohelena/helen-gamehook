#include <HelenHook/BatmanGraphicsConfigService.h>
#include <HelenHook/CommandExecutor.h>

#include <HelenHook/CommandDispatcher.h>
#include <HelenHook/Log.h>
#include <HelenHook/RuntimeValueStore.h>

#include <algorithm>
#include <optional>
#include <string_view>
#include <windows.h>

namespace
{
    /**
     * @brief Converts one UTF-8 message into UTF-16 for runtime log output.
     * @param text UTF-8 message text that should be converted.
     * @return UTF-16 message text, or an empty string when conversion fails.
     */
    std::wstring ToWideString(std::string_view text)
    {
        if (text.empty())
        {
            return {};
        }

        const int required_length = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0);
        if (required_length <= 0)
        {
            return {};
        }

        std::wstring wide_text(static_cast<std::size_t>(required_length), L'\0');
        const int actual_length = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            wide_text.data(),
            required_length);
        if (actual_length != required_length)
        {
            return {};
        }

        return wide_text;
    }

    /**
     * @brief Returns the mapped double value for one integer input when an exact mapping exists.
     * @param step Mapping step definition whose entries should be searched.
     * @param input_value Integer input value that should be translated.
     * @return Matching mapped double value when found; otherwise no value.
     */
    std::optional<double> TryResolveMappedValue(const helen::CommandStepDefinition& step, int input_value)
    {
        for (const helen::CommandMapEntryDefinition& mapping : step.Mappings)
        {
            if (mapping.Match == input_value)
            {
                return mapping.Value;
            }
        }

        return std::nullopt;
    }
}

namespace helen
{
    /**
     * @brief Binds the executor to the config dispatcher and declared runtime slot store it should mutate.
     * @param dispatcher Registered config dispatcher used by read-config-int steps.
     * @param runtime_values Declared runtime slot store updated by set-live-double steps.
     */
    CommandExecutor::CommandExecutor(
        CommandDispatcher& dispatcher,
        RuntimeValueStore& runtime_values,
        BatmanGraphicsConfigService& graphics_config_service)
        : dispatcher_(dispatcher),
          runtime_values_(runtime_values),
          graphics_config_service_(graphics_config_service)
    {
    }

    /**
     * @brief Registers one command definition by its stable identifier.
     * @param command Declarative command definition that should become runnable.
     * @return True when the command identifier was new and the command was stored; otherwise false.
     */
    bool CommandExecutor::RegisterCommand(const CommandDefinition& command)
    {
        if (command.Id.empty() || commands_.contains(command.Id))
        {
            return false;
        }

        commands_.emplace(command.Id, command);
        return true;
    }

    /**
     * @brief Runs one registered command by identifier.
     * @param command_id Stable identifier of the command that should execute.
     * @return True when every step succeeds; otherwise false.
     */
    bool CommandExecutor::RunCommand(const std::string& command_id)
    {
        const auto found = commands_.find(command_id);
        if (found == commands_.end())
        {
            return false;
        }

        std::map<std::string, int> int_values;
        std::map<std::string, double> double_values;
        std::vector<std::string> command_stack;
        const RuntimeValueSnapshot snapshot = runtime_values_.TakeSnapshot();

        if (RunCommand(found->second, int_values, double_values, command_stack))
        {
            return true;
        }

        runtime_values_.RestoreSnapshot(snapshot);
        return false;
    }

    /**
     * @brief Runs one resolved command definition using the current transient execution frame.
     * @param command Command definition being executed.
     * @param int_values Named transient integer values available to this run chain.
     * @param double_values Named transient double values available to this run chain.
     * @param command_stack Active command call stack used to reject recursive loops.
     * @return True when every step succeeds; otherwise false.
     */
    bool CommandExecutor::RunCommand(
        const CommandDefinition& command,
        std::map<std::string, int>& int_values,
        std::map<std::string, double>& double_values,
        std::vector<std::string>& command_stack)
    {
        if (command.Id.empty())
        {
            return false;
        }

        if (std::find(command_stack.begin(), command_stack.end(), command.Id) != command_stack.end())
        {
            return false;
        }

        command_stack.push_back(command.Id);
        bool succeeded = true;
        for (const CommandStepDefinition& step : command.Steps)
        {
            if (!ExecuteStep(step, int_values, double_values, command_stack))
            {
                succeeded = false;
                break;
            }
        }

        command_stack.pop_back();
        return succeeded;
    }

    /**
     * @brief Executes one step against the current transient execution frame.
     * @param step Step definition being executed.
     * @param int_values Named transient integer values available to this run chain.
     * @param double_values Named transient double values available to this run chain.
     * @param command_stack Active command call stack used to reject recursive loops.
     * @return True when the step succeeds; otherwise false.
     */
    bool CommandExecutor::ExecuteStep(
        const CommandStepDefinition& step,
        std::map<std::string, int>& int_values,
        std::map<std::string, double>& double_values,
        std::vector<std::string>& command_stack)
    {
        if (step.Kind == "read-config-int")
        {
            if (step.ConfigKey.empty() || step.ValueName.empty())
            {
                return false;
            }

            const std::optional<int> value = dispatcher_.TryGetInt(step.ConfigKey);
            if (!value.has_value())
            {
                return false;
            }

            int_values[step.ValueName] = *value;
            return true;
        }

        if (step.Kind == "map-int-to-double")
        {
            if (step.InputValueName.empty() || step.OutputValueName.empty() || step.Mappings.empty())
            {
                return false;
            }

            const auto input = int_values.find(step.InputValueName);
            if (input == int_values.end())
            {
                return false;
            }

            const std::optional<double> mapped_value = TryResolveMappedValue(step, input->second);
            if (!mapped_value.has_value())
            {
                return false;
            }

            double_values[step.OutputValueName] = *mapped_value;
            return true;
        }

        if (step.Kind == "set-live-double")
        {
            if (step.Target.empty() || step.ValueName.empty())
            {
                return false;
            }

            const auto value = double_values.find(step.ValueName);
            if (value == double_values.end())
            {
                return false;
            }

            return runtime_values_.SetDouble(step.Target, value->second);
        }

        if (step.Kind == "run-command")
        {
            if (step.CommandId.empty())
            {
                return false;
            }

            const auto found = commands_.find(step.CommandId);
            if (found == commands_.end())
            {
                return false;
            }

            return RunCommand(found->second, int_values, double_values, command_stack);
        }

        if (step.Kind == "log-message")
        {
            if (step.Message.empty())
            {
                return false;
            }

            Log(ToWideString(step.Message));
            return true;
        }

        if (step.Kind == "load-batman-graphics-draft-into-config")
        {
            return graphics_config_service_.LoadIntoDispatcher(dispatcher_);
        }

        if (step.Kind == "load-batman-subtitle-size-into-config")
        {
            return graphics_config_service_.LoadSubtitleSizeIntoDispatcher(dispatcher_);
        }

        if (step.Kind == "apply-batman-graphics-config")
        {
            return graphics_config_service_.ApplyFromDispatcher(dispatcher_);
        }

        if (step.Kind == "apply-batman-subtitle-size-config")
        {
            return graphics_config_service_.ApplySubtitleSizeFromDispatcher(dispatcher_);
        }

        if (step.Kind == "sync-batman-graphics-detail-level")
        {
            return graphics_config_service_.SyncDetailLevelFromDispatcher(dispatcher_);
        }

        if (step.Kind == "sync-batman-graphics-detail-preset")
        {
            return graphics_config_service_.ApplySelectedDetailLevelToDispatcher(dispatcher_);
        }

        return false;
    }
}
