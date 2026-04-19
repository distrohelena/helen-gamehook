#pragma once

#include <map>
#include <string>
#include <vector>

#include <HelenHook/CommandDefinition.h>

namespace helen
{
    class BatmanGraphicsConfigService;
    class CommandDispatcher;
    class RuntimeValueStore;

    /**
     * @brief Executes constrained declarative commands against registered config keys and declared live runtime slots.
     *
     * Execution is intentionally limited to a small set of step kinds so pack data remains predictable,
     * testable, and safe to validate ahead of runtime. Live runtime writes are committed only when the full command run succeeds so callers can treat a failed command as a transactional no-op.
     */
    class CommandExecutor
    {
    public:
        /**
         * @brief Binds the executor to the config dispatcher and declared runtime slot store it should mutate.
         * @param dispatcher Registered config dispatcher used by read-config-int steps.
         * @param runtime_values Declared runtime slot store updated by set-live-double steps.
         * @param graphics_config_service Batman-specific graphics config bridge used by the Batman graphics command steps.
         */
        CommandExecutor(
            CommandDispatcher& dispatcher,
            RuntimeValueStore& runtime_values,
            BatmanGraphicsConfigService& graphics_config_service);

        /**
         * @brief Registers one command definition by its stable identifier.
         * @param command Declarative command definition that should become runnable.
         * @return True when the command identifier was new and the command was stored; otherwise false.
         */
        bool RegisterCommand(const CommandDefinition& command);

        /**
         * @brief Runs one registered command by identifier.
         * @param command_id Stable identifier of the command that should execute.
         * @return True when every step succeeds; otherwise false.
         */
        bool RunCommand(const std::string& command_id);

    private:
        /**
         * @brief Runs one resolved command definition using the current transient execution frame.
         * @param command Command definition being executed.
         * @param int_values Named transient integer values available to this run chain.
         * @param double_values Named transient double values available to this run chain.
         * @param command_stack Active command call stack used to reject recursive loops.
         * @return True when every step succeeds; otherwise false.
         */
        bool RunCommand(
            const CommandDefinition& command,
            std::map<std::string, int>& int_values,
            std::map<std::string, double>& double_values,
            std::vector<std::string>& command_stack);

        /**
         * @brief Executes one step against the current transient execution frame.
         * @param step Step definition being executed.
         * @param int_values Named transient integer values available to this run chain.
         * @param double_values Named transient double values available to this run chain.
         * @param command_stack Active command call stack used to reject recursive loops.
         * @return True when the step succeeds; otherwise false.
         */
        bool ExecuteStep(
            const CommandStepDefinition& step,
            std::map<std::string, int>& int_values,
            std::map<std::string, double>& double_values,
            std::vector<std::string>& command_stack);

        /** @brief Registered config dispatcher used by read-config-int steps. */
        CommandDispatcher& dispatcher_;
        /** @brief Declared runtime slot store updated by set-live-double steps and restored when a command run fails. */
        RuntimeValueStore& runtime_values_;
        /** @brief Batman-specific graphics config bridge used by the Batman graphics command steps. */
        BatmanGraphicsConfigService& graphics_config_service_;
        /** @brief Registered command definitions keyed by their stable identifiers. */
        std::map<std::string, CommandDefinition> commands_;
    };
}
