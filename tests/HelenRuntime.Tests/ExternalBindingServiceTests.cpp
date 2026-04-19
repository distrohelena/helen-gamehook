#include <HelenHook/BatmanGraphicsConfigService.h>
#include <HelenHook/CommandDefinition.h>
#include <HelenHook/CommandDispatcher.h>
#include <HelenHook/CommandExecutor.h>
#include <HelenHook/CommandStepDefinition.h>
#include <HelenHook/ExternalBindingDefinition.h>
#include <HelenHook/ExternalBindingService.h>
#include <HelenHook/RuntimeValueStore.h>

#include <filesystem>
#include <stdexcept>
#include <windows.h>

namespace
{
    /**
     * @brief Throws when a required condition is false so the shared test harness stops at the first failed assertion.
     * @param condition Boolean condition under test.
     * @param message Failure text reported to stderr by the shared test main.
     */
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Builds the declarative command that maps the primary subtitle size config into the live scale slot.
     * @return Command definition used by the binding tests to prove command dispatch side effects.
     */
    helen::CommandDefinition CreateApplySubtitleSizeCommand()
    {
        helen::CommandDefinition command;
        command.Id = "applySubtitleSize";
        command.Name = "Apply Subtitle Size";
        command.Steps = {
            helen::CommandStepDefinition{
                .Kind = "read-config-int",
                .ConfigKey = "ui.subtitleSize",
                .ValueName = "subtitleSizeState"
            },
            helen::CommandStepDefinition{
                .Kind = "map-int-to-double",
                .InputValueName = "subtitleSizeState",
                .OutputValueName = "subtitleScale",
                .Mappings = {
                    helen::CommandMapEntryDefinition{ .Match = 0, .Value = 1.25 },
                    helen::CommandMapEntryDefinition{ .Match = 1, .Value = 1.5 },
                    helen::CommandMapEntryDefinition{ .Match = 2, .Value = 1.8 }
                }
            },
            helen::CommandStepDefinition{
                .Kind = "set-live-double",
                .ValueName = "subtitleScale",
                .Target = "subtitle.scale"
            }
        };

        return command;
    }

    /**
     * @brief Builds the declarative command that maps the alternate subtitle size config into a second live target.
     * @return Command definition used to prove that same-name bindings can target distinct commands.
     */
    helen::CommandDefinition CreateApplyAlternateSubtitleSizeCommand()
    {
        helen::CommandDefinition command;
        command.Id = "applyAlternateSubtitleSize";
        command.Name = "Apply Alternate Subtitle Size";
        command.Steps = {
            helen::CommandStepDefinition{
                .Kind = "read-config-int",
                .ConfigKey = "ui.altSubtitleSize",
                .ValueName = "alternateSubtitleSizeState"
            },
            helen::CommandStepDefinition{
                .Kind = "map-int-to-double",
                .InputValueName = "alternateSubtitleSizeState",
                .OutputValueName = "alternateSubtitleScale",
                .Mappings = {
                    helen::CommandMapEntryDefinition{ .Match = 0, .Value = 0.75 },
                    helen::CommandMapEntryDefinition{ .Match = 1, .Value = 1.25 },
                    helen::CommandMapEntryDefinition{ .Match = 2, .Value = 1.75 }
                }
            },
            helen::CommandStepDefinition{
                .Kind = "set-live-double",
                .ValueName = "alternateSubtitleScale",
                .Target = "other.subtitle.scale"
            }
        };

        return command;
    }

    /**
     * @brief Builds the declared primary subtitle scale slot used by the binding tests.
     * @return Runtime slot definition for the primary subtitle.scale float32 target.
     */
    helen::RuntimeSlotDefinition CreatePrimarySubtitleScaleSlot()
    {
        helen::RuntimeSlotDefinition definition;
        definition.Id = "subtitle.scale";
        definition.Type = "float32";
        definition.InitialValue = 1.25;
        return definition;
    }

    /**
     * @brief Builds the declared alternate subtitle scale slot used by the binding tests.
     * @return Runtime slot definition for the other.subtitle.scale float32 target.
     */
    helen::RuntimeSlotDefinition CreateAlternateSubtitleScaleSlot()
    {
        helen::RuntimeSlotDefinition definition;
        definition.Id = "other.subtitle.scale";
        definition.Type = "float32";
        definition.InitialValue = 0.75;
        return definition;
    }

    /**
     * @brief Builds a command that writes a live value and then fails later so transactional rollback can be verified.
     * @return Command definition that mutates runtime state before a guaranteed late failure.
     */
    helen::CommandDefinition CreateFailingCommand()
    {
        helen::CommandDefinition command = CreateApplySubtitleSizeCommand();
        command.Id = "applyBrokenSubtitleSize";
        command.Name = "Apply Broken Subtitle Size";
        command.Steps.push_back(helen::CommandStepDefinition{
            .Kind = "run-command",
            .CommandId = "missingNestedCommand"
        });
        return command;
    }

    /**
     * @brief Builds a unique temporary `BmEngine.ini` path for the current test process.
     * @return Absolute temporary path reserved for the current test run.
     */
    std::filesystem::path CreateTemporaryBatmanGraphicsIniPath()
    {
        const DWORD process_id = GetCurrentProcessId();
        return
            std::filesystem::temp_directory_path() /
            "HelenRuntimeTests" /
            ("external-bindings-" + std::to_string(process_id)) /
            "BmEngine.ini";
    }
}

/**
 * @brief Verifies that same-name external bindings resolve by config key or command identifier and that transactional set-int restores config on command failure.
 */
void RunExternalBindingServiceTests()
{
    helen::CommandDispatcher dispatcher;
    dispatcher.RegisterConfigInt("ui.subtitleSize", 1);
    dispatcher.RegisterConfigInt("ui.altSubtitleSize", 2);

    helen::RuntimeValueStore runtime_values;
    Expect(runtime_values.RegisterSlot(CreatePrimarySubtitleScaleSlot()), "Failed to register the primary runtime slot.");
    Expect(runtime_values.RegisterSlot(CreateAlternateSubtitleScaleSlot()), "Failed to register the alternate runtime slot.");
    const std::filesystem::path unused_batman_ini_path = CreateTemporaryBatmanGraphicsIniPath();
    helen::BatmanGraphicsConfigService graphics_config_service(unused_batman_ini_path);
    helen::CommandExecutor executor(dispatcher, runtime_values, graphics_config_service);
    Expect(executor.RegisterCommand(CreateApplySubtitleSizeCommand()), "Failed to register the primary subtitle-size command.");
    Expect(executor.RegisterCommand(CreateApplyAlternateSubtitleSizeCommand()), "Failed to register the alternate subtitle-size command.");
    Expect(executor.RegisterCommand(CreateFailingCommand()), "Failed to register the failing transaction command.");

    helen::ExternalBindingService bindings(dispatcher, executor);

    helen::ExternalBindingDefinition get_primary_binding;
    get_primary_binding.Id = "subtitleSizeGet";
    get_primary_binding.ExternalName = "Helen_GetInt";
    get_primary_binding.Mode = "get-int";
    get_primary_binding.ConfigKey = "ui.subtitleSize";
    bindings.Register(get_primary_binding);

    helen::ExternalBindingDefinition get_alternate_binding;
    get_alternate_binding.Id = "alternateSubtitleSizeGet";
    get_alternate_binding.ExternalName = "Helen_GetInt";
    get_alternate_binding.Mode = "get-int";
    get_alternate_binding.ConfigKey = "ui.altSubtitleSize";
    bindings.Register(get_alternate_binding);

    helen::ExternalBindingDefinition set_primary_binding;
    set_primary_binding.Id = "subtitleSizeSet";
    set_primary_binding.ExternalName = "Helen_SetInt";
    set_primary_binding.Mode = "set-int";
    set_primary_binding.ConfigKey = "ui.subtitleSize";
    set_primary_binding.CommandId = "applyBrokenSubtitleSize";
    bindings.Register(set_primary_binding);

    helen::ExternalBindingDefinition set_alternate_binding;
    set_alternate_binding.Id = "alternateSubtitleSizeSet";
    set_alternate_binding.ExternalName = "Helen_SetInt";
    set_alternate_binding.Mode = "set-int";
    set_alternate_binding.ConfigKey = "ui.altSubtitleSize";
    set_alternate_binding.CommandId = "applyAlternateSubtitleSize";
    bindings.Register(set_alternate_binding);

    helen::ExternalBindingDefinition run_primary_binding;
    run_primary_binding.Id = "subtitleSizeRun";
    run_primary_binding.ExternalName = "Helen_RunCommand";
    run_primary_binding.Mode = "run-command";
    run_primary_binding.CommandId = "applySubtitleSize";
    bindings.Register(run_primary_binding);

    helen::ExternalBindingDefinition run_alternate_binding;
    run_alternate_binding.Id = "alternateSubtitleSizeRun";
    run_alternate_binding.ExternalName = "Helen_RunCommand";
    run_alternate_binding.Mode = "run-command";
    run_alternate_binding.CommandId = "applyAlternateSubtitleSize";
    bindings.Register(run_alternate_binding);

    {
        int value = 0;
        Expect(bindings.TryHandleGetInt("Helen_GetInt", "ui.subtitleSize", value), "Primary get-int binding did not resolve.");
        Expect(value == 1, "Primary get-int binding returned the wrong value.");
        value = 0;
        Expect(bindings.TryHandleGetInt("Helen_GetInt", "ui.altSubtitleSize", value), "Alternate get-int binding did not resolve.");
        Expect(value == 2, "Alternate get-int binding returned the wrong value.");
    }

    {
        Expect(bindings.TryHandleRunCommand("Helen_RunCommand", "applySubtitleSize"), "Primary run-command binding did not resolve.");
        Expect(runtime_values.TryGetDouble("subtitle.scale") == 1.5, "Primary run-command binding wrote the wrong runtime value.");
        Expect(bindings.TryHandleRunCommand("Helen_RunCommand", "applyAlternateSubtitleSize"), "Alternate run-command binding did not resolve.");
        Expect(runtime_values.TryGetDouble("other.subtitle.scale") == 1.75, "Alternate run-command binding wrote the wrong runtime value.");
    }

    {
        Expect(bindings.TryHandleSetInt("Helen_SetInt", "ui.altSubtitleSize", 1), "Alternate set-int binding did not resolve.");
        Expect(dispatcher.TryGetInt("ui.altSubtitleSize") == 1, "Alternate set-int binding did not update config.");
        Expect(runtime_values.TryGetDouble("other.subtitle.scale") == 1.25, "Alternate set-int binding did not run its follow-up command.");
    }

    {
        const std::optional<double> before_runtime_value = runtime_values.TryGetDouble("subtitle.scale");
        Expect(before_runtime_value.has_value(), "Expected the primary runtime value to be seeded before transactional failure.");
        Expect(*before_runtime_value == 1.5, "Primary runtime value seed mismatch.");
        Expect(dispatcher.TryGetInt("ui.subtitleSize") == 1, "Primary config value was not seeded before transactional failure.");
        Expect(!bindings.TryHandleSetInt("Helen_SetInt", "ui.subtitleSize", 2), "Transactional set-int failure unexpectedly succeeded.");
        Expect(dispatcher.TryGetInt("ui.subtitleSize") == 1, "Transactional set-int failure did not roll config back.");
        Expect(runtime_values.TryGetDouble("subtitle.scale") == 1.5, "Transactional set-int failure changed the seeded runtime value.");
    }

    {
        int value = 0;
        Expect(bindings.TryHandleGetInt("MissingCallback", "ui.subtitleSize", value) == false, "Unexpected success for a missing external name.");
        Expect(value == 0, "Missing external name changed the output value.");
        Expect(bindings.TryHandleGetInt("Helen_GetInt", "ui.missing", value) == false, "Unexpected success for a missing config key.");
        Expect(bindings.TryHandleRunCommand("Helen_RunCommand", "missingCommand") == false, "Unexpected success for a missing command identifier.");
    }
}
