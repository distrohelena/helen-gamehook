#include <HelenHook/BatmanGraphicsConfigService.h>
#include <HelenHook/CommandDefinition.h>
#include <HelenHook/CommandDispatcher.h>
#include <HelenHook/CommandExecutor.h>
#include <HelenHook/CommandMapEntryDefinition.h>
#include <HelenHook/CommandStepDefinition.h>
#include <HelenHook/RuntimeSlotDefinition.h>
#include <HelenHook/RuntimeValueStore.h>

#include <filesystem>
#include <fstream>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <windows.h>

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

    /**
     * @brief Registers every Batman graphics draft config key used by the graphics-options menu tests.
     * @param dispatcher Dispatcher that should receive the Batman graphics config-key declarations.
     */
    void RegisterBatmanGraphicsConfigKeys(helen::CommandDispatcher& dispatcher)
    {
        dispatcher.RegisterConfigInt("fullscreen", 0);
        dispatcher.RegisterConfigInt("resolutionWidth", 0);
        dispatcher.RegisterConfigInt("resolutionHeight", 0);
        dispatcher.RegisterConfigInt("vsync", 0);
        dispatcher.RegisterConfigInt("msaa", 0);
        dispatcher.RegisterConfigInt("detailLevel", 0);
        dispatcher.RegisterConfigInt("bloom", 0);
        dispatcher.RegisterConfigInt("dynamicShadows", 0);
        dispatcher.RegisterConfigInt("motionBlur", 0);
        dispatcher.RegisterConfigInt("distortion", 0);
        dispatcher.RegisterConfigInt("fogVolumes", 0);
        dispatcher.RegisterConfigInt("sphericalHarmonicLighting", 0);
        dispatcher.RegisterConfigInt("ambientOcclusion", 0);
        dispatcher.RegisterConfigInt("physx", 0);
        dispatcher.RegisterConfigInt("stereo", 0);
    }

    /**
     * @brief Writes UTF-8 text to a test file, replacing any prior content.
     * @param path Target file path that should receive the supplied text.
     * @param text UTF-8 text content that should be written to disk.
     */
    void WriteAllText(const std::filesystem::path& path, std::string_view text)
    {
        std::filesystem::create_directories(path.parent_path());

        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::runtime_error("Failed to open the Batman graphics INI test file for writing.");
        }

        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream)
        {
            throw std::runtime_error("Failed to write the Batman graphics INI test file.");
        }
    }

    /**
     * @brief Reads the full UTF-8 text content of a test file.
     * @param path File path that should be loaded from disk.
     * @return Entire file content as one string.
     */
    std::string ReadAllText(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            throw std::runtime_error("Failed to open the Batman graphics INI test file for reading.");
        }

        return std::string(
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());
    }

    /**
     * @brief Builds a unique temporary `BmEngine.ini` path for the current test process.
     * @return Absolute temporary path that the current test may create and overwrite freely.
     */
    std::filesystem::path CreateTemporaryBatmanGraphicsIniPath()
    {
        const DWORD process_id = GetCurrentProcessId();
        const std::filesystem::path root =
            std::filesystem::temp_directory_path() /
            "HelenRuntimeTests" /
            ("batman-graphics-" + std::to_string(process_id));
        std::filesystem::create_directories(root);
        return root / "BmEngine.ini";
    }

    /**
     * @brief Builds a representative Batman graphics INI body that matches a very-high preset setup.
     * @return Test INI text with every required Batman graphics key present.
     */
    std::string CreateBatmanGraphicsIniText()
    {
        return
            "[Engine.Engine]\r\n"
            "PhysXLevel=2\r\n"
            "\r\n"
            "[SystemSettings]\r\n"
            "Fullscreen=False\r\n"
            "UseVsync=False\r\n"
            "ResX=2560\r\n"
            "ResY=1440\r\n"
            "MaxMultisamples=4\r\n"
            "DetailMode=2\r\n"
            "Bloom=True\r\n"
            "DynamicShadows=True\r\n"
            "MotionBlur=True\r\n"
            "Distortion=True\r\n"
            "FogVolumes=True\r\n"
            "DisableSphericalHarmonicLights=False\r\n"
            "AmbientOcclusion=True\r\n"
            "Stereo=False\r\n";
    }

    /**
     * @brief Builds a command that loads Batman graphics values from the INI-backed service into config.
     * @return Declarative command definition for the Batman graphics load flow.
     */
    helen::CommandDefinition CreateLoadBatmanGraphicsDraftCommand()
    {
        helen::CommandDefinition command;
        command.Id = "loadBatmanGraphicsDraftIntoConfig";
        command.Name = "Load Batman Graphics Draft Into Config";
        command.Steps.push_back(helen::CommandStepDefinition{ .Kind = "load-batman-graphics-draft-into-config" });
        return command;
    }

    /**
     * @brief Builds a command that applies the selected Batman detail preset to the other draft toggles.
     * @return Declarative command definition for preset synchronization.
     */
    helen::CommandDefinition CreateSyncBatmanGraphicsPresetCommand()
    {
        helen::CommandDefinition command;
        command.Id = "syncBatmanGraphicsPreset";
        command.Name = "Sync Batman Graphics Preset";
        command.Steps.push_back(helen::CommandStepDefinition{ .Kind = "sync-batman-graphics-detail-preset" });
        return command;
    }

    /**
     * @brief Builds a command that recomputes Batman `detailLevel` from the current individual draft toggles.
     * @return Declarative command definition for detail-level normalization.
     */
    helen::CommandDefinition CreateSyncBatmanGraphicsDetailLevelCommand()
    {
        helen::CommandDefinition command;
        command.Id = "syncBatmanGraphicsDetailLevel";
        command.Name = "Sync Batman Graphics Detail Level";
        command.Steps.push_back(helen::CommandStepDefinition{ .Kind = "sync-batman-graphics-detail-level" });
        return command;
    }

    /**
     * @brief Builds a command that writes the Batman graphics draft back to disk and then reloads the saved values.
     * @return Declarative command definition for the Batman graphics apply flow.
     */
    helen::CommandDefinition CreateApplyBatmanGraphicsDraftCommand()
    {
        helen::CommandDefinition command;
        command.Id = "applyBatmanGraphicsDraft";
        command.Name = "Apply Batman Graphics Draft";
        command.Steps.push_back(helen::CommandStepDefinition{ .Kind = "apply-batman-graphics-config" });
        command.Steps.push_back(helen::CommandStepDefinition{ .Kind = "load-batman-graphics-draft-into-config" });
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

    const std::filesystem::path unused_batman_ini_path = CreateTemporaryBatmanGraphicsIniPath();
    helen::BatmanGraphicsConfigService graphics_config_service(unused_batman_ini_path);
    helen::CommandExecutor executor(dispatcher, runtime_values, graphics_config_service);
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

    {
        const std::filesystem::path batman_ini_path = CreateTemporaryBatmanGraphicsIniPath();
        WriteAllText(batman_ini_path, CreateBatmanGraphicsIniText());

        helen::CommandDispatcher batman_dispatcher;
        RegisterBatmanGraphicsConfigKeys(batman_dispatcher);

        helen::RuntimeValueStore batman_runtime_values;
        helen::BatmanGraphicsConfigService batman_graphics_config_service(batman_ini_path);
        helen::CommandExecutor batman_executor(batman_dispatcher, batman_runtime_values, batman_graphics_config_service);

        Expect(batman_executor.RegisterCommand(CreateLoadBatmanGraphicsDraftCommand()), "Failed to register the Batman graphics load command.");
        Expect(batman_executor.RegisterCommand(CreateSyncBatmanGraphicsPresetCommand()), "Failed to register the Batman preset-sync command.");
        Expect(batman_executor.RegisterCommand(CreateSyncBatmanGraphicsDetailLevelCommand()), "Failed to register the Batman detail-sync command.");
        Expect(batman_executor.RegisterCommand(CreateApplyBatmanGraphicsDraftCommand()), "Failed to register the Batman graphics apply command.");

        Expect(batman_executor.RunCommand("loadBatmanGraphicsDraftIntoConfig"), "Batman graphics load command unexpectedly failed.");
        Expect(batman_dispatcher.TryGetInt("fullscreen") == 0, "Batman graphics load read the wrong fullscreen state.");
        Expect(batman_dispatcher.TryGetInt("resolutionWidth") == 2560, "Batman graphics load read the wrong resolution width.");
        Expect(batman_dispatcher.TryGetInt("resolutionHeight") == 1440, "Batman graphics load read the wrong resolution height.");
        Expect(batman_dispatcher.TryGetInt("vsync") == 0, "Batman graphics load read the wrong VSync state.");
        Expect(batman_dispatcher.TryGetInt("msaa") == 2, "Batman graphics load read the wrong MSAA state.");
        Expect(batman_dispatcher.TryGetInt("detailLevel") == 3, "Batman graphics load failed to derive the Very High detail preset.");
        Expect(batman_dispatcher.TryGetInt("ambientOcclusion") == 1, "Batman graphics load read the wrong ambient-occlusion state.");
        Expect(batman_dispatcher.TryGetInt("physx") == 2, "Batman graphics load read the wrong PhysX state.");

        Expect(batman_dispatcher.TrySetInt("detailLevel", 1), "Failed to seed the Batman medium detail-level preset.");
        Expect(batman_executor.RunCommand("syncBatmanGraphicsPreset"), "Batman preset-sync command unexpectedly failed.");
        Expect(batman_dispatcher.TryGetInt("bloom") == 1, "Batman preset-sync did not keep Bloom enabled for Medium.");
        Expect(batman_dispatcher.TryGetInt("dynamicShadows") == 1, "Batman preset-sync did not keep Dynamic Shadows enabled for Medium.");
        Expect(batman_dispatcher.TryGetInt("motionBlur") == 0, "Batman preset-sync did not disable Motion Blur for Medium.");
        Expect(batman_dispatcher.TryGetInt("distortion") == 0, "Batman preset-sync did not disable Distortion for Medium.");
        Expect(batman_dispatcher.TryGetInt("fogVolumes") == 0, "Batman preset-sync did not disable Fog Volumes for Medium.");
        Expect(batman_dispatcher.TryGetInt("sphericalHarmonicLighting") == 0, "Batman preset-sync did not disable spherical harmonic lighting for Medium.");
        Expect(batman_dispatcher.TryGetInt("ambientOcclusion") == 0, "Batman preset-sync did not disable ambient occlusion for Medium.");

        Expect(batman_dispatcher.TrySetInt("bloom", 0), "Failed to seed the Batman custom detail override.");
        Expect(batman_executor.RunCommand("syncBatmanGraphicsDetailLevel"), "Batman detail-sync command unexpectedly failed.");
        Expect(batman_dispatcher.TryGetInt("detailLevel") == 4, "Batman detail-sync did not derive the Custom detail state.");

        Expect(batman_dispatcher.TrySetInt("detailLevel", 1), "Failed to restore the Batman medium detail-level preset.");
        Expect(batman_executor.RunCommand("syncBatmanGraphicsPreset"), "Batman preset-sync failed during apply setup.");
        Expect(batman_dispatcher.TrySetInt("vsync", 1), "Failed to seed the Batman VSync draft.");
        Expect(batman_dispatcher.TrySetInt("msaa", 0), "Failed to seed the Batman MSAA draft.");
        Expect(batman_dispatcher.TrySetInt("physx", 1), "Failed to seed the Batman PhysX draft.");
        Expect(batman_dispatcher.TrySetInt("stereo", 1), "Failed to seed the Batman stereo draft.");

        Expect(batman_executor.RunCommand("applyBatmanGraphicsDraft"), "Batman graphics apply command unexpectedly failed.");
        Expect(batman_dispatcher.TryGetInt("detailLevel") == 1, "Batman graphics apply did not preserve the Medium detail preset.");

        const std::string saved_ini_text = ReadAllText(batman_ini_path);
        Expect(saved_ini_text.find("UseVsync=True") != std::string::npos, "Batman graphics apply did not persist VSync.");
        Expect(saved_ini_text.find("MaxMultisamples=1") != std::string::npos, "Batman graphics apply did not persist disabled MSAA.");
        Expect(saved_ini_text.find("DetailMode=1") != std::string::npos, "Batman graphics apply did not persist the Medium detail mode.");
        Expect(saved_ini_text.find("Bloom=True") != std::string::npos, "Batman graphics apply did not persist Bloom for Medium.");
        Expect(saved_ini_text.find("MotionBlur=False") != std::string::npos, "Batman graphics apply did not persist Motion Blur for Medium.");
        Expect(saved_ini_text.find("Distortion=False") != std::string::npos, "Batman graphics apply did not persist Distortion for Medium.");
        Expect(saved_ini_text.find("FogVolumes=False") != std::string::npos, "Batman graphics apply did not persist Fog Volumes for Medium.");
        Expect(saved_ini_text.find("DisableSphericalHarmonicLights=True") != std::string::npos, "Batman graphics apply did not persist spherical harmonic lighting for Medium.");
        Expect(saved_ini_text.find("AmbientOcclusion=False") != std::string::npos, "Batman graphics apply did not persist ambient occlusion for Medium.");
        Expect(saved_ini_text.find("PhysXLevel=1") != std::string::npos, "Batman graphics apply did not persist PhysX.");
        Expect(saved_ini_text.find("Stereo=True") != std::string::npos, "Batman graphics apply did not persist stereo.");
    }
}
