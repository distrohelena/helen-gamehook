#include <HelenHook/CommandDefinition.h>
#include <HelenHook/CommandDispatcher.h>
#include <HelenHook/CommandExecutor.h>
#include <HelenHook/CommandMapEntryDefinition.h>
#include <HelenHook/CommandStepDefinition.h>
#include <HelenHook/RuntimeSlotDefinition.h>
#include <HelenHook/RuntimeValueStore.h>

#include <cmath>
#include <optional>
#include <stdexcept>

namespace
{
    /**
     * @brief Throws when one required boolean condition is false so the shared test harness stops at the first failure.
     * @param condition Boolean condition that must evaluate to true.
     * @param message Failure message reported by the shared test runner.
     */
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Throws when two floating-point values differ by more than the supplied tolerance.
     * @param actual Value produced by the command executor.
     * @param expected Value required by the current test scenario.
     * @param tolerance Maximum permitted absolute difference.
     * @param message Failure message reported by the shared test runner.
     */
    void ExpectNear(double actual, double expected, double tolerance, const char* message)
    {
        if (std::fabs(actual - expected) > tolerance)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Builds one float32 runtime slot used by the command executor coverage.
     * @return Runtime slot definition for the live subtitle scale target.
     */
    helen::RuntimeSlotDefinition CreateSubtitleScaleSlot()
    {
        helen::RuntimeSlotDefinition definition;
        definition.Id = "subtitle.scale";
        definition.Type = "float32";
        definition.InitialValue = 1.5;
        return definition;
    }

    /**
     * @brief Builds one standard subtitle-size command that reads config, maps it, and updates the live slot.
     * @param id Stable command identifier assigned to the definition.
     * @return Fully populated command definition for the happy-path execution scenarios.
     */
    helen::CommandDefinition CreateApplySubtitleSizeCommand(const char* id)
    {
        helen::CommandDefinition command;
        command.Id = id;
        command.Name = "Apply Subtitle Size";

        helen::CommandStepDefinition read_step;
        read_step.Kind = "read-config-int";
        read_step.ConfigKey = "ui.subtitleSize";
        read_step.ValueName = "subtitleSizeState";
        command.Steps.push_back(read_step);

        helen::CommandStepDefinition map_step;
        map_step.Kind = "map-int-to-double";
        map_step.InputValueName = "subtitleSizeState";
        map_step.OutputValueName = "subtitleScale";
        map_step.Mappings.push_back(helen::CommandMapEntryDefinition{ .Match = 0, .Value = 1.25 });
        map_step.Mappings.push_back(helen::CommandMapEntryDefinition{ .Match = 1, .Value = 1.5 });
        map_step.Mappings.push_back(helen::CommandMapEntryDefinition{ .Match = 2, .Value = 1.8 });
        command.Steps.push_back(map_step);

        helen::CommandStepDefinition set_live_step;
        set_live_step.Kind = "set-live-double";
        set_live_step.Target = "subtitle.scale";
        set_live_step.ValueName = "subtitleScale";
        command.Steps.push_back(set_live_step);

        return command;
    }

    /**
     * @brief Builds one command that mutates runtime state and then fails so rollback can be validated.
     * @return Command definition that always fails after writing the live slot.
     */
    helen::CommandDefinition CreateFailingCommand()
    {
        helen::CommandDefinition command = CreateApplySubtitleSizeCommand("applyBrokenSubtitleSize");

        helen::CommandStepDefinition failing_step;
        failing_step.Kind = "run-command";
        failing_step.CommandId = "missingNestedCommand";
        command.Steps.push_back(failing_step);

        return command;
    }

    /**
     * @brief Builds one simple command that calls another command, used to prove recursive loop rejection.
     * @param id Stable command identifier assigned to the definition.
     * @param nested_command_id Stable nested command identifier invoked by the definition.
     * @return Minimal run-command-only definition for recursion tests.
     */
    helen::CommandDefinition CreateNestedCommand(const char* id, const char* nested_command_id)
    {
        helen::CommandDefinition command;
        command.Id = id;
        command.Name = "Nested Command";

        helen::CommandStepDefinition run_step;
        run_step.Kind = "run-command";
        run_step.CommandId = nested_command_id;
        command.Steps.push_back(run_step);

        return command;
    }
}

/**
 * @brief Verifies that declarative commands update live runtime slots on success and roll back state when a later step fails.
 */
void RunCommandExecutorTests()
{
    helen::CommandDispatcher dispatcher;
    dispatcher.RegisterConfigInt("ui.subtitleSize", 1);
    Expect(dispatcher.TrySetInt("ui.subtitleSize", 2), "Failed to seed the subtitle size config for the command executor tests.");

    helen::RuntimeValueStore runtime_values;
    Expect(runtime_values.RegisterSlot(CreateSubtitleScaleSlot()), "Failed to register the live subtitle scale slot.");

    helen::CommandExecutor executor(dispatcher, runtime_values);
    Expect(executor.RegisterCommand(CreateApplySubtitleSizeCommand("applySubtitleSize")), "Failed to register the happy-path subtitle command.");
    Expect(!executor.RegisterCommand(CreateApplySubtitleSizeCommand("applySubtitleSize")), "Duplicate command registration unexpectedly succeeded.");

    Expect(executor.RunCommand("applySubtitleSize"), "Happy-path command execution unexpectedly failed.");
    const std::optional<double> applied_value = runtime_values.TryGetDouble("subtitle.scale");
    Expect(applied_value.has_value(), "Happy-path command execution removed the live subtitle scale slot.");
    ExpectNear(*applied_value, 1.8, 0.001, "Happy-path command execution wrote the wrong live subtitle scale.");

    Expect(executor.RegisterCommand(CreateFailingCommand()), "Failed to register the rollback subtitle command.");
    Expect(runtime_values.SetDouble("subtitle.scale", 1.5), "Failed to reseed the subtitle scale before rollback validation.");
    Expect(!executor.RunCommand("applyBrokenSubtitleSize"), "Rollback command unexpectedly succeeded.");

    const std::optional<double> rolled_back_value = runtime_values.TryGetDouble("subtitle.scale");
    Expect(rolled_back_value.has_value(), "Rollback command removed the live subtitle scale slot.");
    ExpectNear(*rolled_back_value, 1.5, 0.001, "Rollback command failed to restore the previous live subtitle scale.");

    Expect(executor.RegisterCommand(CreateNestedCommand("loopA", "loopB")), "Failed to register the first recursive command.");
    Expect(executor.RegisterCommand(CreateNestedCommand("loopB", "loopA")), "Failed to register the second recursive command.");
    Expect(!executor.RunCommand("loopA"), "Recursive command loop unexpectedly succeeded.");

    Expect(!executor.RunCommand("missingCommand"), "Unknown command unexpectedly succeeded.");
}
