#include <HelenHook/BatmanGraphicsConfigService.h>
#include "FaultingBatmanGraphicsFileOperations.h"
#include "CallbackBatmanGraphicsFileOperations.h"
#include "../../HelenGameHook/BatmanGraphicsExternalInterface.h"
#include "../../HelenGameHook/BatmanGraphicsRuntime.h"
#include "BatmanStockDispatchCapture.h"
#include <cstring>
#include <HelenHook/BatmanGraphicsSessionService.h>
#include <HelenHook/BatmanDisplayModeService.h>
#include <HelenHook/CommandDefinition.h>
#include <HelenHook/CommandDispatcher.h>
#include <HelenHook/CommandExecutor.h>
#include <HelenHook/CommandMapEntryDefinition.h>
#include <HelenHook/CommandStepDefinition.h>
#include <HelenHook/Log.h>
#include <HelenHook/RuntimeSlotDefinition.h>
#include <HelenHook/RuntimeValueStore.h>

#include <array>
#include <atomic>
#include <exception>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
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
     * @brief Returns a process-lifetime display-mode service for graphics tests that do not exercise mode selection.
     * @return Required display-mode service dependency shared by legacy graphics command fixtures.
     */
    helen::BatmanDisplayModeService& GetGraphicsTestDisplayModeService()
    {
        static helen::BatmanDisplayModeService service;
        return service;
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
        map_step.Mappings.push_back(helen::CommandMapEntryDefinition{ .Match = 0, .Value = 1.0 });
        map_step.Mappings.push_back(helen::CommandMapEntryDefinition{ .Match = 1, .Value = 1.5 });
        map_step.Mappings.push_back(helen::CommandMapEntryDefinition{ .Match = 2, .Value = 2.0 });
        map_step.Mappings.push_back(helen::CommandMapEntryDefinition{ .Match = 3, .Value = 4.0 });
        map_step.Mappings.push_back(helen::CommandMapEntryDefinition{ .Match = 4, .Value = 6.0 });
        map_step.Mappings.push_back(helen::CommandMapEntryDefinition{ .Match = 5, .Value = 8.0 });
        command.Steps.push_back(map_step);

        helen::CommandStepDefinition set_live_step;
        set_live_step.Kind = "set-live-double";
        set_live_step.Target = "subtitle.scale";
        set_live_step.ValueName = "subtitleScale";
        command.Steps.push_back(set_live_step);

        return command;
    }

    /**
     * @brief Builds the native command step that applies one selected Batman display-mode index.
     * @return Command definition containing the selected-resolution operation.
     */
    helen::CommandDefinition CreateSetBatmanGraphicsResolutionModeCommand()
    {
        helen::CommandDefinition command;
        command.Id = "setBatmanGraphicsResolutionMode";
        command.Name = "Set Batman Graphics Resolution Mode";

        helen::CommandStepDefinition step;
        step.Kind = "set-batman-graphics-resolution-mode";
        command.Steps.push_back(step);
        return command;
    }

    /**
     * @brief Verifies selected Batman resolution application against a revalidated supported-mode catalog.
     *
     * The command must write the exact selected pair, reject a mode removed between catalog capture and
     * application, and reject invalid or absent selection keys without disturbing the existing pair.
     */
    void RunBatmanResolutionModeCommandTest()
    {
        std::vector<helen::BatmanDisplayMode> current_modes = {
            helen::BatmanDisplayMode(1280, 720),
            helen::BatmanDisplayMode(1920, 1080),
            helen::BatmanDisplayMode(3440, 1440)
        };
        const helen::BatmanDisplayModeService::EnvironmentEnumerationCallback enumeration_callback =
            [&current_modes]() -> std::optional<helen::BatmanDisplayEnvironment>
            {
                return helen::BatmanDisplayEnvironment(
                    L"DISPLAY1",
                    current_modes,
                    helen::BatmanDisplayMode(3440, 1440),
                    helen::BatmanDisplayMode(1920, 1080));
            };
        helen::BatmanDisplayModeService display_mode_service(enumeration_callback);
        Expect(
            display_mode_service.Refresh(helen::BatmanDisplayModeCatalogKind::Fullscreen, 1920, 1080),
            "Resolution test display catalog did not refresh.");

        helen::CommandDispatcher dispatcher;
        dispatcher.RegisterConfigInt("fullscreen", 1);
        dispatcher.RegisterConfigInt("resolutionModeIndex", 0);
        dispatcher.RegisterConfigInt("resolutionWidth", 1024);
        dispatcher.RegisterConfigInt("resolutionHeight", 768);
        Expect(dispatcher.TrySetInt("resolutionModeIndex", 1), "Failed to select the 1920x1080 resolution mode.");

        helen::BatmanGraphicsConfigService graphics_config_service(
            std::filesystem::path("resolution-mode-command-test.ini"),
            display_mode_service);
        helen::RuntimeValueStore runtime_values;
        helen::CommandExecutor executor(dispatcher, runtime_values, graphics_config_service);
        Expect(
            executor.RegisterCommand(CreateSetBatmanGraphicsResolutionModeCommand()),
            "Failed to register the selected-resolution command.");
        Expect(executor.RunCommand("setBatmanGraphicsResolutionMode"), "Selected-resolution command failed.");
        Expect(dispatcher.TryGetInt("resolutionWidth") == 1920, "Selected resolution width was not applied exactly.");
        Expect(dispatcher.TryGetInt("resolutionHeight") == 1080, "Selected resolution height was not applied exactly.");

        current_modes = {
            helen::BatmanDisplayMode(1280, 720),
            helen::BatmanDisplayMode(3440, 1440)
        };
        Expect(
            !executor.RunCommand("setBatmanGraphicsResolutionMode"),
            "Selected-resolution command accepted a mode removed from the display.");
        Expect(dispatcher.TryGetInt("resolutionWidth") == 1920, "Stale-catalog failure changed the resolution width.");
        Expect(dispatcher.TryGetInt("resolutionHeight") == 1080, "Stale-catalog failure changed the resolution height.");

        Expect(dispatcher.TrySetInt("resolutionModeIndex", 99), "Failed to seed an out-of-range resolution index.");
        Expect(!executor.RunCommand("setBatmanGraphicsResolutionMode"), "Out-of-range resolution index unexpectedly succeeded.");
        Expect(dispatcher.TryGetInt("resolutionWidth") == 1920, "Out-of-range failure changed the resolution width.");
        Expect(dispatcher.TryGetInt("resolutionHeight") == 1080, "Out-of-range failure changed the resolution height.");

        current_modes = {
            helen::BatmanDisplayMode(1280, 720),
            helen::BatmanDisplayMode(1920, 1080),
            helen::BatmanDisplayMode(3440, 1440)
        };
        helen::CommandDispatcher missing_width_dispatcher;
        missing_width_dispatcher.RegisterConfigInt("resolutionModeIndex", 1);
        missing_width_dispatcher.RegisterConfigInt("resolutionHeight", 900);
        helen::BatmanGraphicsConfigService missing_width_graphics_service(
            std::filesystem::path("missing-resolution-width-test.ini"),
            display_mode_service);
        helen::CommandExecutor missing_width_executor(
            missing_width_dispatcher,
            runtime_values,
            missing_width_graphics_service);
        Expect(
            missing_width_executor.RegisterCommand(CreateSetBatmanGraphicsResolutionModeCommand()),
            "Failed to register the missing-width resolution command.");
        Expect(
            !missing_width_executor.RunCommand("setBatmanGraphicsResolutionMode"),
            "Missing resolution width unexpectedly succeeded.");
        Expect(
            missing_width_dispatcher.TryGetInt("resolutionHeight") == 900,
            "Missing-width failure changed the existing resolution height.");

        helen::CommandDispatcher missing_height_dispatcher;
        missing_height_dispatcher.RegisterConfigInt("resolutionModeIndex", 1);
        missing_height_dispatcher.RegisterConfigInt("resolutionWidth", 1600);
        helen::BatmanGraphicsConfigService missing_height_graphics_service(
            std::filesystem::path("missing-resolution-height-test.ini"),
            display_mode_service);
        helen::CommandExecutor missing_height_executor(
            missing_height_dispatcher,
            runtime_values,
            missing_height_graphics_service);
        Expect(
            missing_height_executor.RegisterCommand(CreateSetBatmanGraphicsResolutionModeCommand()),
            "Failed to register the missing-height resolution command.");
        Expect(
            !missing_height_executor.RunCommand("setBatmanGraphicsResolutionMode"),
            "Missing resolution height unexpectedly succeeded.");
        Expect(
            missing_height_dispatcher.TryGetInt("resolutionWidth") == 1600,
            "Missing-height failure changed the existing resolution width.");

        helen::CommandDispatcher missing_index_dispatcher;
        missing_index_dispatcher.RegisterConfigInt("resolutionWidth", 1600);
        missing_index_dispatcher.RegisterConfigInt("resolutionHeight", 900);
        helen::BatmanGraphicsConfigService missing_index_graphics_service(
            std::filesystem::path("missing-resolution-index-test.ini"),
            display_mode_service);
        helen::CommandExecutor missing_index_executor(
            missing_index_dispatcher,
            runtime_values,
            missing_index_graphics_service);
        Expect(
            missing_index_executor.RegisterCommand(CreateSetBatmanGraphicsResolutionModeCommand()),
            "Failed to register the missing-index resolution command.");
        Expect(
            !missing_index_executor.RunCommand("setBatmanGraphicsResolutionMode"),
            "Missing resolution index unexpectedly succeeded.");
        Expect(
            missing_index_dispatcher.TryGetInt("resolutionWidth") == 1600,
            "Missing-index failure changed the resolution width.");
        Expect(
            missing_index_dispatcher.TryGetInt("resolutionHeight") == 900,
            "Missing-index failure changed the resolution height.");
    }

    /**
     * @brief Verifies that a selected resolution index is never interpreted against the wrong mode catalog.
     */
    void RunBatmanResolutionCatalogKindGuardTest()
    {
        helen::BatmanDisplayModeService service(
            []() -> std::optional<helen::BatmanDisplayEnvironment>
            {
                return helen::BatmanDisplayEnvironment(
                    L"DISPLAY1",
                    { helen::BatmanDisplayMode(1280, 720), helen::BatmanDisplayMode(1920, 1080) },
                    helen::BatmanDisplayMode(1920, 1080),
                    helen::BatmanDisplayMode(1920, 1080));
            });
        Expect(
            service.Refresh(helen::BatmanDisplayModeCatalogKind::Windowed, 1920, 1080),
            "Windowed catalog guard fixture did not refresh.");
        Expect(
            service.Refresh(helen::BatmanDisplayModeCatalogKind::Fullscreen, 1920, 1080),
            "Fullscreen catalog guard fixture did not refresh.");

        helen::CommandDispatcher dispatcher;
        dispatcher.RegisterConfigInt("fullscreen", 0);
        dispatcher.RegisterConfigInt("resolutionModeIndex", 0);
        dispatcher.RegisterConfigInt("resolutionWidth", 1920);
        dispatcher.RegisterConfigInt("resolutionHeight", 1080);
        helen::BatmanGraphicsConfigService config_service(
            std::filesystem::path("resolution-catalog-kind-guard.ini"),
            service);
        Expect(
            config_service.ApplySelectedResolutionModeToDispatcher(dispatcher),
            "Resolution apply rejected the matching windowed catalog after fullscreen refresh.");
        Expect(dispatcher.TryGetInt("resolutionWidth") == 640, "Windowed catalog index zero selected the wrong width.");
        Expect(dispatcher.TryGetInt("resolutionHeight") == 480, "Windowed catalog index zero selected the wrong height.");
        Expect(dispatcher.TrySetInt("fullscreen", 1), "Failed to switch catalog-kind guard fixture to fullscreen.");
        Expect(
            config_service.ApplySelectedResolutionModeToDispatcher(dispatcher),
            "Resolution apply rejected the matching fullscreen catalog after windowed selection.");
        Expect(dispatcher.TryGetInt("resolutionWidth") == 1280, "Fullscreen catalog index zero selected the wrong width.");
        Expect(dispatcher.TryGetInt("resolutionHeight") == 720, "Fullscreen catalog index zero selected the wrong height.");

        helen::CommandDispatcher missing_mode_dispatcher;
        missing_mode_dispatcher.RegisterConfigInt("resolutionModeIndex", 0);
        missing_mode_dispatcher.RegisterConfigInt("resolutionWidth", 1600);
        missing_mode_dispatcher.RegisterConfigInt("resolutionHeight", 900);
        helen::BatmanGraphicsConfigService missing_mode_config_service(
            std::filesystem::path("resolution-catalog-kind-missing-mode.ini"),
            service);
        Expect(
            !missing_mode_config_service.ApplySelectedResolutionModeToDispatcher(missing_mode_dispatcher),
            "Resolution apply unexpectedly accepted a dispatcher without fullscreen mode.");
        Expect(missing_mode_dispatcher.TryGetInt("resolutionWidth") == 1600, "Missing mode changed resolution width.");
        Expect(missing_mode_dispatcher.TryGetInt("resolutionHeight") == 900, "Missing mode changed resolution height.");
    }

    /**
     * @brief Builds one subtitle-size command that also persists the value back to `BmEngine.ini`.
     * @param id Stable command identifier assigned to the definition.
     * @return Declarative command definition for the persistence validation flow.
     */
    helen::CommandDefinition CreateApplySubtitleSizePersistCommand(const char* id)
    {
        helen::CommandDefinition command = CreateApplySubtitleSizeCommand(id);

        helen::CommandStepDefinition persist_step;
        persist_step.Kind = "apply-batman-subtitle-size-config";
        command.Steps.push_back(persist_step);

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
     * @brief Holds the seven normalized Batman quality leaves that determine the selected detail preset.
     * @remarks Each field uses the menu convention where zero is disabled and one is enabled; spherical lighting is normalized before INI inversion.
     */
    struct BatmanQualityLeaves
    {
        /** @brief Normalized Bloom state. */
        int Bloom;
        /** @brief Normalized Dynamic Shadows state. */
        int DynamicShadows;
        /** @brief Normalized Motion Blur state. */
        int MotionBlur;
        /** @brief Normalized Distortion state. */
        int Distortion;
        /** @brief Normalized Fog Volumes state. */
        int FogVolumes;
        /** @brief Normalized spherical harmonic lighting state before persistence inversion. */
        int SphericalHarmonicLighting;
        /** @brief Normalized Ambient Occlusion state. */
        int AmbientOcclusion;
    };

    /**
     * @brief Writes one literal quality-leaf table row into the dispatcher draft.
     * @param dispatcher Dispatcher that owns the Batman quality config keys.
     * @param leaves Seven normalized values that should become the current draft.
     */
    void SetBatmanQualityLeaves(helen::CommandDispatcher& dispatcher, const BatmanQualityLeaves& leaves)
    {
        Expect(dispatcher.TrySetInt("bloom", leaves.Bloom), "Failed to set the Bloom quality leaf.");
        Expect(dispatcher.TrySetInt("dynamicShadows", leaves.DynamicShadows), "Failed to set the Dynamic Shadows quality leaf.");
        Expect(dispatcher.TrySetInt("motionBlur", leaves.MotionBlur), "Failed to set the Motion Blur quality leaf.");
        Expect(dispatcher.TrySetInt("distortion", leaves.Distortion), "Failed to set the Distortion quality leaf.");
        Expect(dispatcher.TrySetInt("fogVolumes", leaves.FogVolumes), "Failed to set the Fog Volumes quality leaf.");
        Expect(dispatcher.TrySetInt("sphericalHarmonicLighting", leaves.SphericalHarmonicLighting), "Failed to set the spherical-lighting quality leaf.");
        Expect(dispatcher.TrySetInt("ambientOcclusion", leaves.AmbientOcclusion), "Failed to set the Ambient Occlusion quality leaf.");
    }

    /**
     * @brief Asserts that the dispatcher contains every literal quality leaf from one preset or custom derivation case.
     * @param dispatcher Dispatcher whose current quality draft should be checked.
     * @param leaves Expected normalized values for all seven quality keys.
     */
    void ExpectBatmanQualityLeaves(const helen::CommandDispatcher& dispatcher, const BatmanQualityLeaves& leaves)
    {
        Expect(dispatcher.TryGetInt("bloom") == leaves.Bloom, "Batman Bloom quality leaf did not match the literal case.");
        Expect(dispatcher.TryGetInt("dynamicShadows") == leaves.DynamicShadows, "Batman Dynamic Shadows quality leaf did not match the literal case.");
        Expect(dispatcher.TryGetInt("motionBlur") == leaves.MotionBlur, "Batman Motion Blur quality leaf did not match the literal case.");
        Expect(dispatcher.TryGetInt("distortion") == leaves.Distortion, "Batman Distortion quality leaf did not match the literal case.");
        Expect(dispatcher.TryGetInt("fogVolumes") == leaves.FogVolumes, "Batman Fog Volumes quality leaf did not match the literal case.");
        Expect(dispatcher.TryGetInt("sphericalHarmonicLighting") == leaves.SphericalHarmonicLighting, "Batman spherical-lighting quality leaf did not match the literal case.");
        Expect(dispatcher.TryGetInt("ambientOcclusion") == leaves.AmbientOcclusion, "Batman Ambient Occlusion quality leaf did not match the literal case.");
    }

    /**
     * @brief Trims ASCII whitespace from one parsed INI token without changing its backing document.
     * @param text Token view that should be normalized for exact section, key, or value comparison.
     * @return View covering the token after leading and trailing ASCII whitespace is removed.
     */
    std::string_view TrimBatmanIniToken(std::string_view text)
    {
        while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
        {
            text.remove_prefix(1);
        }

        while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r'))
        {
            text.remove_suffix(1);
        }

        return text;
    }

    /**
     * @brief Reads one exact section/key value from a decoded Batman INI document.
     * @param text Decoded INI document whose section and key should be parsed.
     * @param section Section header that must contain the requested key.
     * @param key Exact key name whose value should be returned.
     * @return Owning value when the exact section/key assignment exists; otherwise no value.
     * @remarks Section state follows production parsing: a line beginning with '[' replaces the active section state, malformed headers clear it, and characters inside the brackets are significant. Key/value whitespace is normalized only after section selection.
     */
    std::optional<std::string> TryReadBatmanIniValue(std::string_view text, const char* section, const char* key)
    {
        bool in_target_section = false;
        std::size_t line_start = 0;
        while (line_start <= text.size())
        {
            const std::size_t newline_position = text.find('\n', line_start);
            const std::size_t line_end = newline_position == std::string_view::npos ? text.size() : newline_position;
            std::string_view line = TrimBatmanIniToken(text.substr(line_start, line_end - line_start));

            if (!line.empty() && line.front() == '[')
            {
                in_target_section =
                    line.size() >= 2 && line.back() == ']' && line.substr(1, line.size() - 2) == section;
            }

            if (in_target_section)
            {
                const std::size_t separator_position = line.find('=');
                if (separator_position != std::string_view::npos &&
                    TrimBatmanIniToken(line.substr(0, separator_position)) == key)
                {
                    return std::string(TrimBatmanIniToken(line.substr(separator_position + 1)));
                }
            }

            if (newline_position == std::string_view::npos)
            {
                break;
            }

            line_start = newline_position + 1;
        }

        return std::nullopt;
    }

    /**
     * @brief Asserts one exact parsed assignment in a decoded Batman INI document.
     * @param text Decoded INI document whose section/key assignment should be checked.
     * @param section Exact INI section containing the assignment.
     * @param key Exact INI key whose value is required.
     * @param expected Expected value after INI whitespace normalization.
     * @param message Failure message reported when the exact assignment is absent or differs.
     */
    void ExpectBatmanIniValue(
        std::string_view text,
        const char* section,
        const char* key,
        std::string_view expected,
        const char* message)
    {
        const std::optional<std::string> actual = TryReadBatmanIniValue(text, section, key);
        Expect(actual.has_value() && *actual == expected, message);
    }

    /**
     * @brief Asserts one exact parsed boolean assignment in a decoded Batman INI document.
     * @param text Decoded INI document whose boolean assignment should be checked.
     * @param key Exact SystemSettings key whose boolean value is required.
     * @param enabled Expected normalized boolean value, encoded as True when nonzero and False otherwise.
     * @param message Failure message reported when the exact assignment is absent or differs.
     */
    void ExpectBatmanIniBoolean(std::string_view text, const char* key, int enabled, const char* message)
    {
        const std::string expected_value = enabled != 0 ? "True" : "False";
        ExpectBatmanIniValue(text, "SystemSettings", key, expected_value, message);
    }

    /**
     * @brief Asserts all seven normalized quality leaves in one persisted Batman INI, including spherical-lighting inversion.
     * @param text Decoded INI text whose quality assignments should be checked.
     * @param leaves Expected normalized quality values before INI encoding.
     */
    void ExpectBatmanPersistedQualityLeaves(std::string_view text, const BatmanQualityLeaves& leaves)
    {
        ExpectBatmanIniBoolean(text, "Bloom", leaves.Bloom, "Persisted Batman Bloom value did not match the normalized draft.");
        ExpectBatmanIniBoolean(text, "DynamicShadows", leaves.DynamicShadows, "Persisted Batman Dynamic Shadows value did not match the normalized draft.");
        ExpectBatmanIniBoolean(text, "MotionBlur", leaves.MotionBlur, "Persisted Batman Motion Blur value did not match the normalized draft.");
        ExpectBatmanIniBoolean(text, "Distortion", leaves.Distortion, "Persisted Batman Distortion value did not match the normalized draft.");
        ExpectBatmanIniBoolean(text, "FogVolumes", leaves.FogVolumes, "Persisted Batman Fog Volumes value did not match the normalized draft.");
        ExpectBatmanIniBoolean(text, "DisableSphericalHarmonicLights", leaves.SphericalHarmonicLighting == 0 ? 1 : 0, "Persisted Batman spherical lighting value did not apply the required inverse.");
        ExpectBatmanIniBoolean(text, "AmbientOcclusion", leaves.AmbientOcclusion, "Persisted Batman Ambient Occlusion value did not match the normalized draft.");
    }

    /**
     * @brief Associates one literal seven-leaf draft with the detail level that synchronization must derive.
     * @remarks The table includes the four canonical presets and two deliberately noncanonical combinations that must normalize to Custom (4).
     */
    struct BatmanQualityDetailCase
    {
        /** @brief Human-readable case label used when iterating the literal table. */
        const char* Name;
        /** @brief Literal normalized values seeded before detail-level synchronization. */
        BatmanQualityLeaves Leaves;
        /** @brief Detail level expected after running `syncBatmanGraphicsDetailLevel`. */
        int ExpectedDetailLevel;
    };

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
     * @brief Writes ASCII fixture text using Batman launcher's native UTF-16LE encoding and byte-order mark.
     * @param path Target file path that should receive the encoded text.
     * @param text ASCII-only fixture text whose characters should be widened without transformation.
     */
    void WriteAsciiAsUtf16LittleEndianText(const std::filesystem::path& path, std::string_view text)
    {
        std::filesystem::create_directories(path.parent_path());

        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::runtime_error("Failed to open the UTF-16LE Batman graphics INI fixture for writing.");
        }

        const unsigned char byte_order_mark[] = { 0xFF, 0xFE };
        stream.write(reinterpret_cast<const char*>(byte_order_mark), sizeof(byte_order_mark));
        for (const unsigned char character : text)
        {
            if (character > 0x7F)
            {
                throw std::runtime_error("UTF-16LE Batman graphics INI fixtures must contain ASCII text only.");
            }

            const unsigned char encoded_character[] = { character, 0x00 };
            stream.write(reinterpret_cast<const char*>(encoded_character), sizeof(encoded_character));
        }

        if (!stream)
        {
            throw std::runtime_error("Failed to write the UTF-16LE Batman graphics INI fixture.");
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
     * @brief Reads every byte from one test fixture without applying text decoding or newline normalization.
     * @param path File path whose exact on-disk bytes should be captured.
     * @return Exact byte sequence currently stored at `path`.
     */
    std::string ReadAllBytes(const std::filesystem::path& path)
    {
        return ReadAllText(path);
    }

    /**
     * @brief Opens one fixture for shared reads while denying subsequent write and delete sharing.
     * @param path Existing fixture path that should be publication-blocked.
     * @return Open Win32 handle that remains valid until the caller closes it.
     * @throws std::runtime_error Thrown when the fixture cannot be opened with the requested sharing policy.
     */
    HANDLE OpenBatmanIniDenyingWriteDeleteSharing(const std::filesystem::path& path)
    {
        const HANDLE handle = CreateFileW(
            path.wstring().c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            throw std::runtime_error("Failed to open the Batman graphics INI fixture with write/delete sharing denied.");
        }

        return handle;
    }

    /**
     * @brief Verifies that a graphics fixture directory contains no transaction stage or recovery artifacts.
     * @param fixture_directory Directory containing one isolated Batman graphics fixture.
     */
    void ExpectNoBatmanGraphicsTransactionArtifacts(const std::filesystem::path& fixture_directory)
    {
        std::error_code iteration_error;
        bool found_recovery_artifact = false;
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(fixture_directory, iteration_error))
        {
            const std::string file_name = entry.path().filename().string();
            found_recovery_artifact = found_recovery_artifact || file_name.find("-recovery") != std::string::npos;
            Expect(
                file_name.find(".helenhook-") == std::string::npos,
                "Batman graphics apply left a transaction stage or recovery artifact in the fixture directory.");
        }

        Expect(!iteration_error, "Failed to inspect the Batman graphics fixture directory for transaction artifacts.");
        Expect(!found_recovery_artifact, "Batman graphics apply left an owned recovery artifact in the fixture directory.");
    }

    /**
     * @brief Verifies that a failed publication preserved one target's existence and exact raw bytes.
     * @param path Target file path whose pre-apply snapshot should still be present.
     * @param expected_bytes Exact bytes captured before the publication attempt.
     * @param missing_message Failure message used when the target disappeared during publication.
     * @param mismatch_message Failure message used when the target bytes changed during publication.
     */
    void ExpectBatmanIniBytesEqual(
        const std::filesystem::path& path,
        const std::string& expected_bytes,
        const char* missing_message,
        const char* mismatch_message)
    {
        Expect(std::filesystem::exists(path), missing_message);
        Expect(ReadAllBytes(path) == expected_bytes, mismatch_message);
    }

    /**
     * @brief Reads one ASCII-only UTF-16LE fixture while validating that its native encoding was preserved.
     * @param path UTF-16LE test file that should be decoded.
     * @return Decoded ASCII text without its byte-order mark.
     */
    std::string ReadAsciiFromUtf16LittleEndianText(const std::filesystem::path& path)
    {
        const std::string bytes = ReadAllText(path);
        if (bytes.size() < 2 ||
            static_cast<unsigned char>(bytes[0]) != 0xFF ||
            static_cast<unsigned char>(bytes[1]) != 0xFE ||
            (bytes.size() % 2) != 0)
        {
            throw std::runtime_error("Batman graphics INI fixture did not preserve UTF-16LE encoding.");
        }

        std::string text;
        text.reserve((bytes.size() - 2) / 2);
        for (std::size_t index = 2; index < bytes.size(); index += 2)
        {
            if (bytes[index + 1] != '\0' || static_cast<unsigned char>(bytes[index]) > 0x7F)
            {
                throw std::runtime_error("Batman graphics INI fixture contains unsupported non-ASCII UTF-16LE text.");
            }

            text.push_back(bytes[index]);
        }

        return text;
    }

    /**
     * @brief Builds a unique temporary `BmEngine.ini` path for the current test process.
     * @param scenario_name Isolated child-directory name that keeps one fixture scenario separate from other scenarios.
     * @return Absolute temporary path that the current test may create and overwrite freely.
     */
    std::filesystem::path CreateTemporaryBatmanGraphicsIniPath(std::string_view scenario_name = "default")
    {
        const DWORD process_id = GetCurrentProcessId();
        const std::filesystem::path root =
            std::filesystem::temp_directory_path() /
            "HelenRuntimeTests" /
            ("batman-graphics-" + std::to_string(process_id)) /
            std::string(scenario_name);
        std::filesystem::create_directories(root);
        return root / "BmEngine.ini";
    }

    /**
     * @brief Returns the sibling `BmGame.ini` path that sits next to one `BmEngine.ini` path.
     * @param engine_ini_path Absolute or relative `BmEngine.ini` path used by the graphics-config service.
     * @return Sibling `BmGame.ini` path in the same directory as `engine_ini_path`.
     */
    std::filesystem::path GetSiblingBatmanGameIniPath(const std::filesystem::path& engine_ini_path)
    {
        return engine_ini_path.parent_path() / "BmGame.ini";
    }

    /**
     * @brief Returns the sibling launcher-owned `UserEngine.ini` path for one generated `BmEngine.ini` path.
     * @param engine_ini_path Absolute or relative `BmEngine.ini` path used by the graphics-config service.
     * @return Sibling `UserEngine.ini` path in the same directory as `engine_ini_path`.
     */
    std::filesystem::path GetSiblingBatmanUserEngineIniPath(const std::filesystem::path& engine_ini_path)
    {
        return engine_ini_path.parent_path() / "UserEngine.ini";
    }

    /**
     * @brief Builds a deliberately conflicting generated Batman graphics INI body for launcher-authority tests.
     * @return Test INI text with every required Batman graphics key present but quality/detail values distinct from UserEngine.ini.
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
            "DetailMode=0\r\n"
            "Bloom=False\r\n"
            "DynamicShadows=False\r\n"
            "MotionBlur=False\r\n"
            "Distortion=False\r\n"
            "FogVolumes=False\r\n"
            "DisableSphericalHarmonicLights=True\r\n"
            "AmbientOcclusion=False\r\n"
            "Stereo=False\r\n";
    }

    /**
     * @brief Builds launcher-owned graphics text with deliberate Group 1 values that differ from generated `BmEngine.ini`.
     * @param include_stereo True when the complete required draft should include the `Stereo` setting; false creates an incomplete fixture.
     * @return UTF-8 fixture text whose values exercise launcher authority and complete-draft validation.
     */
    std::string CreateBatmanLauncherOwnedGraphicsIniText(bool include_stereo)
    {
        std::string text =
            "[Engine.Engine]\r\n"
            "PhysXLevel=1\r\n"
            "\r\n"
            "[SystemSettings]\r\n"
            "Fullscreen=False\r\n"
            "UseVsync=True\r\n"
            "ResX=2560\r\n"
            "ResY=1440\r\n"
            "MaxMultisamples=8\r\n"
            "DetailMode=2\r\n"
            "Bloom=True\r\n"
            "DynamicShadows=True\r\n"
            "MotionBlur=True\r\n"
            "Distortion=True\r\n"
            "FogVolumes=True\r\n"
            "DisableSphericalHarmonicLights=False\r\n"
            "AmbientOcclusion=True\r\n";

        if (include_stereo)
        {
            text += "Stereo=True\r\n";
        }

        text += "LauncherOwnedSentinel=PreserveMe\r\n";
        return text;
    }

    /**
     * @brief Builds a minimal INI body with the required subtitle-size key for persistence tests.
     * @param console_font_size Encoded subtitle font size written as `Engine.HUD.ConsoleFontSize`.
     * @return Test INI text containing only the required subtitle section.
     */
    std::string CreateBatmanSubtitleIniText(int console_font_size)
    {
        return
            "[Engine.HUD]\r\n"
            "ConsoleFontSize=" + std::to_string(console_font_size) + "\r\n";
    }

    /**
     * @brief Builds a minimal INI body without an `Engine.HUD` section.
     * @return Test INI text that requires insertion of `Engine.HUD.ConsoleFontSize`.
     */
    std::string CreateBatmanSubtitleIniTextMissingHudSection()
    {
        return
            "[SystemSettings]\r\n"
            "Fullscreen=True\r\n"
            "ResX=1920\r\n"
            "ResY=1080\r\n";
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

    /**
     * @brief Seeds one dispatcher with a complete, deliberately distinctive Batman graphics state.
     * @param dispatcher Dispatcher that should receive the state values.
     * @param high_state True for the high preset; false for the medium preset.
     */
    void SeedConcurrentBatmanGraphicsState(helen::CommandDispatcher& dispatcher, bool high_state)
    {
        RegisterBatmanGraphicsConfigKeys(dispatcher);
        Expect(dispatcher.TrySetInt("fullscreen", high_state ? 1 : 0), "Failed to seed concurrent fullscreen state.");
        Expect(dispatcher.TrySetInt("resolutionWidth", high_state ? 3840 : 1920), "Failed to seed concurrent horizontal resolution state.");
        Expect(dispatcher.TrySetInt("resolutionHeight", high_state ? 2160 : 1080), "Failed to seed concurrent vertical resolution state.");
        Expect(dispatcher.TrySetInt("vsync", high_state ? 1 : 0), "Failed to seed concurrent VSync state.");
        Expect(dispatcher.TrySetInt("msaa", high_state ? 3 : 0), "Failed to seed concurrent MSAA state.");
        Expect(dispatcher.TrySetInt("detailLevel", high_state ? 3 : 1), "Failed to seed concurrent detail state.");
        Expect(dispatcher.TrySetInt("bloom", 1), "Failed to seed concurrent Bloom state.");
        Expect(dispatcher.TrySetInt("dynamicShadows", 1), "Failed to seed concurrent shadow state.");
        Expect(dispatcher.TrySetInt("motionBlur", high_state ? 1 : 0), "Failed to seed concurrent motion-blur state.");
        Expect(dispatcher.TrySetInt("distortion", high_state ? 1 : 0), "Failed to seed concurrent distortion state.");
        Expect(dispatcher.TrySetInt("fogVolumes", high_state ? 1 : 0), "Failed to seed concurrent fog state.");
        Expect(dispatcher.TrySetInt("sphericalHarmonicLighting", high_state ? 1 : 0), "Failed to seed concurrent spherical-lighting state.");
        Expect(dispatcher.TrySetInt("ambientOcclusion", high_state ? 1 : 0), "Failed to seed concurrent ambient-occlusion state.");
        Expect(dispatcher.TrySetInt("physx", high_state ? 2 : 0), "Failed to seed concurrent PhysX state.");
        Expect(dispatcher.TrySetInt("stereo", high_state ? 1 : 0), "Failed to seed concurrent stereo state.");
    }

    /**
     * @brief Checks whether a decoded published INI contains every assignment for one concurrent test state.
     * @param text Decoded INI text whose complete state should be checked.
     * @param high_state True to check the high state; false to check the medium state.
     * @return True only when all distinguishing assignments are present with the expected values.
     */
    bool ContainsConcurrentBatmanGraphicsState(std::string_view text, bool high_state)
    {
        const std::array<std::string_view, 15> expected_values = high_state
            ? std::array<std::string_view, 15>{
                "Fullscreen=True", "UseVsync=True", "ResX=3840", "ResY=2160", "MaxMultisamples=8",
                "DetailMode=2", "Bloom=True", "DynamicShadows=True", "MotionBlur=True", "Distortion=True",
                "FogVolumes=True", "DisableSphericalHarmonicLights=False", "AmbientOcclusion=True", "PhysXLevel=2", "Stereo=True" }
            : std::array<std::string_view, 15>{
                "Fullscreen=False", "UseVsync=False", "ResX=1920", "ResY=1080", "MaxMultisamples=1",
                "DetailMode=1", "Bloom=True", "DynamicShadows=True", "MotionBlur=False", "Distortion=False",
                "FogVolumes=False", "DisableSphericalHarmonicLights=True", "AmbientOcclusion=False", "PhysXLevel=0", "Stereo=False" };

        for (const std::string_view expected_value : expected_values)
        {
            if (text.find(expected_value) == std::string_view::npos)
            {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Runs coordinated concurrent applies against one fixture and verifies serialized, complete publication.
     * @param scenario_name Unique fixture directory name for this concurrency scenario.
     */
    void RunConcurrentBatmanGraphicsApplyTest(std::string_view scenario_name)
    {
        const std::filesystem::path engine_ini_path = CreateTemporaryBatmanGraphicsIniPath(scenario_name);
        const std::filesystem::path user_ini_path = GetSiblingBatmanUserEngineIniPath(engine_ini_path);

        constexpr int apply_round_count = 4;
        for (int round = 0; round < apply_round_count; ++round)
        {
            WriteAllText(engine_ini_path, CreateBatmanGraphicsIniText());
            WriteAsciiAsUtf16LittleEndianText(user_ini_path, CreateBatmanLauncherOwnedGraphicsIniText(true));

            helen::CommandDispatcher low_dispatcher;
            helen::CommandDispatcher high_dispatcher;
            SeedConcurrentBatmanGraphicsState(low_dispatcher, false);
            SeedConcurrentBatmanGraphicsState(high_dispatcher, true);

            helen::BatmanGraphicsConfigService low_service(engine_ini_path, GetGraphicsTestDisplayModeService());
            helen::BatmanGraphicsConfigService high_service(engine_ini_path, GetGraphicsTestDisplayModeService());
            std::atomic<int> ready_count{ 0 };
            std::atomic<bool> release_threads{ false };
            bool low_result = false;
            bool high_result = false;
            std::exception_ptr low_exception;
            std::exception_ptr high_exception;

            std::thread low_thread([&]() {
                ready_count.fetch_add(1, std::memory_order_release);
                while (!release_threads.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }

                try
                {
                    low_result = low_service.ApplyFromDispatcher(low_dispatcher);
                }
                catch (...)
                {
                    low_exception = std::current_exception();
                }
            });
            std::thread high_thread([&]() {
                ready_count.fetch_add(1, std::memory_order_release);
                while (!release_threads.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }

                try
                {
                    high_result = high_service.ApplyFromDispatcher(high_dispatcher);
                }
                catch (...)
                {
                    high_exception = std::current_exception();
                }
            });

            while (ready_count.load(std::memory_order_acquire) != 2)
            {
                std::this_thread::yield();
            }

            release_threads.store(true, std::memory_order_release);
            low_thread.join();
            high_thread.join();

            Expect(low_exception == nullptr && high_exception == nullptr, "Concurrent Batman graphics apply threw an unexpected exception.");
            Expect(low_result && high_result, "Concurrent Batman graphics apply did not complete both calls successfully.");

            const std::string engine_text = ReadAllText(engine_ini_path);
            const std::string user_text = ReadAsciiFromUtf16LittleEndianText(user_ini_path);
            const bool engine_low = ContainsConcurrentBatmanGraphicsState(engine_text, false);
            const bool engine_high = ContainsConcurrentBatmanGraphicsState(engine_text, true);
            const bool user_low = ContainsConcurrentBatmanGraphicsState(user_text, false);
            const bool user_high = ContainsConcurrentBatmanGraphicsState(user_text, true);
            Expect(engine_low != engine_high, "Concurrent Batman publication left an incomplete generated state.");
            Expect(user_low != user_high, "Concurrent Batman publication left an incomplete launcher state.");
            Expect(engine_low == user_low && engine_high == user_high, "Concurrent Batman publication left the two INIs internally inconsistent.");
            ExpectNoBatmanGraphicsTransactionArtifacts(engine_ini_path.parent_path());
        }
    }
}

/**
 * @brief Verifies that a selected supported resolution flows through the normal graphics Apply transaction.
 *
 * The resolution command stages the exact catalog pair in the dispatcher, after which the existing
 * graphics Apply command must publish that pair and Fullscreen to both INI documents.
 */
void RunBatmanResolutionModeApplyIntegrationTest();

/**
 * @brief Verifies that declarative commands update live runtime slots on success and roll back state when a later step fails.
 */
void RunCommandExecutorTests()
{
    {
        BatmanStockDispatchCapture handler;
        std::array<unsigned char, 16> first_movie{};
        std::array<unsigned char, 16> second_movie{};
        std::array<unsigned char, 64> arguments{};
        const char* first_name = "FE_GetControlType";
        HelenGraphicsDispatch(&handler, nullptr, first_movie.data(), first_name, arguments.data(), 4);
        Expect(handler.Calls == 1 && handler.Movie == first_movie.data() && handler.Name == first_name &&
            handler.Arguments == arguments.data() && handler.Count == 4, "Production dispatch changed stock thiscall arguments.");
        const char* second_name = "Helen_Graphics_GetV1Extra";
        HelenGraphicsDispatch(&handler, nullptr, second_movie.data(), second_name, nullptr, 0);
        Expect(handler.Calls == 2 && handler.Movie == second_movie.data() && handler.Name == second_name &&
            handler.Arguments == nullptr && handler.Count == 0, "Unknown name reused another movie's stock call.");
        handler.ThrowOnCall = true;
        bool stock_exception_escaped = false;
        try {
            HelenGraphicsDispatch(&handler, nullptr, first_movie.data(), nullptr, nullptr, 0);
        } catch (const std::logic_error&) {
            stock_exception_escaped = true;
        }
        Expect(stock_exception_escaped && handler.Name == nullptr, "Graphics dispatch swallowed or changed a stock exception.");
    }
    {
        const std::filesystem::path engine = CreateTemporaryBatmanGraphicsIniPath("direct-runtime");
        WriteAllText(engine, CreateBatmanGraphicsIniText());
        WriteAsciiAsUtf16LittleEndianText(GetSiblingBatmanUserEngineIniPath(engine), CreateBatmanLauncherOwnedGraphicsIniText(true));
        std::array<unsigned char, 0x9DC + sizeof(helen::BatmanGraphicsPrimitiveResult)> movie{};
        HelenGraphicsDispatch(nullptr, nullptr, movie.data(), "Helen_Graphics_OpenV1", nullptr, 0);
        Expect(movie[0x9DC] == 0, "Unbound owned dispatch fabricated a result.");
        helen::InitializeBatmanGraphicsRuntime(engine);
        HelenGraphicsDispatch(nullptr, nullptr, movie.data(), "Helen_Graphics_OpenV1", nullptr, 0);
        Expect(movie[0x9DC] == 3, "Bound production dispatch did not write the verified movie return slot.");
        bool duplicate_rejected = false;
        try {
            helen::InitializeBatmanGraphicsRuntime(engine);
        } catch (const std::logic_error&) {
            duplicate_rejected = true;
        }
        Expect(duplicate_rejected, "Runtime silently replaced active native ownership.");
        helen::ResetBatmanGraphicsRuntime();
        movie[0x9DC] = 0;
        HelenGraphicsDispatch(nullptr, nullptr, movie.data(), "Helen_Graphics_OpenV1", nullptr, 0);
        Expect(movie[0x9DC] == 0, "Retired runtime still accepted new graphics work.");
    }
    {
        const std::filesystem::path engine = CreateTemporaryBatmanGraphicsIniPath("direct-callback");
        const std::filesystem::path user = GetSiblingBatmanUserEngineIniPath(engine);
        WriteAllText(engine, CreateBatmanGraphicsIniText());
        WriteAsciiAsUtf16LittleEndianText(user, CreateBatmanLauncherOwnedGraphicsIniText(true));
        helen::BatmanDisplayModeService display;
        helen::BatmanGraphicsConfigService config(engine, display);
        helen::BatmanGraphicsSessionService sessions(config, display);
        helen::BatmanGraphicsExternalInterface adapter(sessions);
        helen::BatmanGraphicsPrimitiveResult result{};
        Expect(adapter.TryHandle("Helen_Graphics_OpenV1", nullptr, 0, result), "Direct Open was not owned.");
        double identity = 0;
        std::memcpy(&identity, result.Payload, sizeof(identity));
        Expect(result.Type == 3 && identity > 0, "Direct Open did not return a numeric session identity.");
        std::array<unsigned char, 32> arguments{};
        const std::uint32_t numeric_type = 3;
        const double field = 2;
        std::memcpy(arguments.data(), &numeric_type, sizeof(numeric_type));
        std::memcpy(arguments.data() + 8, &identity, sizeof(identity));
        std::memcpy(arguments.data() + 16, &numeric_type, sizeof(numeric_type));
        std::memcpy(arguments.data() + 24, &field, sizeof(field));
        result.Type = 0;
        Expect(adapter.TryHandle("Helen_Graphics_GetV1", arguments.data(), 2, result), "Direct getter fell through to stock.");
        double state = -1;
        std::memcpy(&state, result.Payload, sizeof(state));
        Expect(result.Type == 3 && state == 3, "Direct getter did not return the fixture's normalized 8x MSAA.");
        result.Type = 0;
        Expect(adapter.TryHandle("Helen_Graphics_GetV1", arguments.data(), 1, result) && result.Type == 0,
            "Wrong-arity owned getter was accepted or forwarded.");
        arguments[0] = 2;
        Expect(adapter.TryHandle("Helen_Graphics_GetV1", arguments.data(), 2, result) && result.Type == 0,
            "Boolean session identity was coerced or forwarded.");
        const auto before = result;
        Expect(!adapter.TryHandle("FE_GetControlType", arguments.data(), 2, result) &&
            std::memcmp(&before, &result, sizeof(result)) == 0, "Stock call result was changed by direct adapter.");
        Expect(!adapter.TryHandle("Helen_Graphics_GetV1Extra", nullptr, 0, result), "Prefix match intercepted an unknown call.");
    }

    {
        const std::filesystem::path engine = CreateTemporaryBatmanGraphicsIniPath("direct-session");
        const std::filesystem::path user = GetSiblingBatmanUserEngineIniPath(engine);
        WriteAllText(engine, CreateBatmanGraphicsIniText());
        WriteAsciiAsUtf16LittleEndianText(user, CreateBatmanLauncherOwnedGraphicsIniText(true));
        helen::BatmanDisplayModeService display([]() -> std::optional<helen::BatmanDisplayEnvironment> {
            return helen::BatmanDisplayEnvironment(L"DISPLAY1", {{1280, 720}, {1920, 1080}}, {1920, 1040}, {1920, 1080});
        });
        std::optional<helen::BatmanGraphicsSessionService> owned_sessions;
        std::uint32_t active_session = 0;
        std::uint32_t active_transaction = 0;
        unsigned publication_entries = 0;
        CallbackBatmanGraphicsFileOperations files([&]() {
            Expect(owned_sessions.has_value(), "Test session lifetime was not established before publication.");
            ++publication_entries;
            Expect(!owned_sessions->Open() && !owned_sessions->Close(active_session) &&
                !owned_sessions->CancelApply(active_session, active_transaction),
                "Reentrant entry replaced or canceled the committing session.");
            bool concurrent_rejected = false;
            std::thread contender([&]() {
                concurrent_rejected = !owned_sessions->Open().has_value();
            });
            contender.join();
            Expect(concurrent_rejected, "Concurrent Open replaced the committing session.");
        });
        helen::BatmanGraphicsConfigService config(engine, display, files);
        owned_sessions.emplace(config, display);
        helen::BatmanGraphicsSessionService& sessions = *owned_sessions;
        const auto opened = sessions.Open();
        Expect(opened.has_value(), "Direct session Open failed.");
        active_session = *opened;
        Expect(sessions.ModeCount(*opened, helen::BatmanDisplayModeCatalogKind::Fullscreen) == 2,
            "Session did not expose captured mode count.");
        Expect(sessions.ModeWidth(*opened, helen::BatmanDisplayModeCatalogKind::Fullscreen, 0) == 1280 &&
            sessions.ModeHeight(*opened, helen::BatmanDisplayModeCatalogKind::Fullscreen, 0) == 720,
            "Session mode getter changed the exact captured pair.");
        Expect(!sessions.ModeWidth(*opened, helen::BatmanDisplayModeCatalogKind::Fullscreen, 2),
            "Session accepted an out-of-range resolution index.");
        Expect(sessions.Get(*opened, helen::BatmanGraphicsField::CanApply) == 1, "Complete session incorrectly disabled Apply.");
        Expect(!sessions.BeginApply(*opened), "Apply was allowed during read transfer.");
        Expect(sessions.EndRead(*opened), "EndRead failed.");
        Expect(!sessions.Get(*opened, helen::BatmanGraphicsField::Vsync), "EndRead left transfer getters active.");
        const auto transaction = sessions.BeginApply(*opened);
        Expect(transaction.has_value(), "EndRead released baseline needed by BeginApply.");
        active_transaction = *transaction;
        Expect(!sessions.BeginApply(*opened), "Second staged transaction was accepted.");
        const std::string before = ReadAllBytes(user);
        Expect(sessions.SetField(*opened, *transaction, helen::BatmanGraphicsField::Vsync, 1), "Session draft edit failed.");
        Expect(ReadAllBytes(user) == before, "Staging edited the launcher file.");
        const auto result = sessions.Commit(*opened, *transaction);
        Expect(result.has_value() && result->Outcome == helen::BatmanGraphicsApplyOutcome::Committed, "Session commit failed.");
        Expect(publication_entries == 2, "Commit did not exercise both real publication boundaries.");
        Expect(!sessions.Commit(*opened, *transaction), "Duplicate Commit was accepted.");
        const auto staged = sessions.BeginApply(*opened);
        Expect(staged.has_value(), "Successful commit did not permit a new transaction.");
        Expect(sessions.SetField(*opened, *staged, helen::BatmanGraphicsField::Fullscreen, 1), "Fullscreen staging failed.");
        Expect(!sessions.SetResolution(*opened, *staged, helen::BatmanDisplayModeCatalogKind::Windowed, 0),
            "Resolution kind mismatch was accepted.");
        Expect(sessions.SetResolution(*opened, *staged, helen::BatmanDisplayModeCatalogKind::Fullscreen, 0),
            "Session could not stage a catalog selection after EndRead.");
        Expect(!sessions.SetField(*opened, *staged, helen::BatmanGraphicsField::Fullscreen, 0),
            "Fullscreen edit invalidated an already staged resolution selection.");
        Expect(sessions.CancelApply(*opened, *staged), "Uncommitted transaction could not be discarded.");
        const auto replacement = sessions.Open();
        Expect(replacement.has_value() && *replacement != *opened, "Open reused a session identity.");
        Expect(!sessions.Close(*opened) && !sessions.SetField(*opened, *staged, helen::BatmanGraphicsField::Vsync, 0),
            "Stale session affected a replacement editor.");
        Expect(sessions.Close(*replacement), "Current idle session could not close.");
    }

    {
        const std::filesystem::path engine = CreateTemporaryBatmanGraphicsIniPath("direct-cleanup-failure");
        const std::filesystem::path user = GetSiblingBatmanUserEngineIniPath(engine);
        WriteAllText(engine, CreateBatmanGraphicsIniText());
        WriteAsciiAsUtf16LittleEndianText(user, CreateBatmanLauncherOwnedGraphicsIniText(true));
        FaultingBatmanGraphicsFileOperations files(false, true);
        helen::BatmanGraphicsConfigService service(engine, GetGraphicsTestDisplayModeService(), files);
        std::optional<helen::BatmanGraphicsDraftState> draft = service.CaptureReadSnapshot().TryCreateDraft();
        Expect(draft.has_value() && draft->TrySet(helen::BatmanGraphicsField::Vsync, 1), "Cleanup fixture draft invalid.");
        const helen::BatmanGraphicsApplyResult result = service.ApplyDraft(*draft);
        Expect(result.Outcome == helen::BatmanGraphicsApplyOutcome::CommittedCleanupFailed,
            "Cleanup failure after publication was not distinguished from failed Apply.");
        Expect(ReadAllText(engine).find("UseVsync=True") != std::string::npos &&
            ReadAsciiFromUtf16LittleEndianText(user).find("UseVsync=True") != std::string::npos,
            "Cleanup-failed commit did not retain both published targets.");
        Expect(!result.RecoveryPaths.empty() && !service.IsApplyLocked(), "Cleanup failure lost evidence or falsely locked integrity.");
        for (const std::filesystem::path& path : result.RecoveryPaths) {
            Expect(path.parent_path() == engine.parent_path(), "Recovery evidence escaped test fixture.");
            Expect(helen::BatmanGraphicsFileOperations::Native().Remove(path), "Test artifact cleanup failed.");
        }
    }

    for (int blocked_target : {0, 1, 2}) {
        const std::filesystem::path engine_path = CreateTemporaryBatmanGraphicsIniPath(
            "direct-persistence-" + std::to_string(blocked_target));
        const std::filesystem::path user_path = GetSiblingBatmanUserEngineIniPath(engine_path);
        WriteAllText(engine_path, CreateBatmanGraphicsIniText());
        WriteAsciiAsUtf16LittleEndianText(user_path, CreateBatmanLauncherOwnedGraphicsIniText(true));
        const std::string original_engine = ReadAllBytes(engine_path);
        const std::string original_user = ReadAllBytes(user_path);
        helen::BatmanGraphicsConfigService service(engine_path, GetGraphicsTestDisplayModeService());
        std::optional<helen::BatmanGraphicsDraftState> draft = service.CaptureReadSnapshot().TryCreateDraft();
        Expect(draft.has_value(), "Direct persistence fixture could not create a complete draft.");
        Expect(draft->TrySet(helen::BatmanGraphicsField::Vsync, 1), "Direct persistence draft edit failed.");
        const HANDLE blocked_handle = blocked_target == 0 ? INVALID_HANDLE_VALUE :
            OpenBatmanIniDenyingWriteDeleteSharing(blocked_target == 1 ? engine_path : user_path);
        const helen::BatmanGraphicsApplyResult result = service.ApplyDraft(*draft);
        if (blocked_handle != INVALID_HANDLE_VALUE) {
            Expect(CloseHandle(blocked_handle) != FALSE, "Failed to close direct publication blocker.");
        }
        if (blocked_target == 0) {
            Expect(result.Outcome == helen::BatmanGraphicsApplyOutcome::Committed, "Direct draft did not report verified commit.");
            Expect(ReadAllText(engine_path).find("UseVsync=True") != std::string::npos,
                "Direct draft failed to persist generated VSync.");
            Expect(ReadAsciiFromUtf16LittleEndianText(user_path).find("UseVsync=True") != std::string::npos,
                "Direct draft failed to persist launcher VSync or preserve encoding.");
        } else {
            Expect(result.Outcome == helen::BatmanGraphicsApplyOutcome::NotApplied,
                "Reconciled publication failure reported the wrong outcome.");
            Expect(ReadAllBytes(engine_path) == original_engine && ReadAllBytes(user_path) == original_user,
                "NotApplied result did not preserve exact original files.");
        }
        Expect(!service.IsApplyLocked(), "Known consistent publication locked future Apply.");
        ExpectNoBatmanGraphicsTransactionArtifacts(engine_path.parent_path());
    }

    helen::CommandDispatcher dispatcher;
    dispatcher.RegisterConfigInt("ui.subtitleSize", 1);
    Expect(dispatcher.TrySetInt("ui.subtitleSize", 2), "Failed to seed the subtitle size config for the command executor tests.");

    helen::RuntimeValueStore runtime_values;
    Expect(runtime_values.RegisterSlot(CreateSubtitleScaleSlot()), "Failed to register the live subtitle scale slot.");

    const std::filesystem::path unused_batman_ini_path = CreateTemporaryBatmanGraphicsIniPath();
    helen::BatmanGraphicsConfigService graphics_config_service(unused_batman_ini_path, GetGraphicsTestDisplayModeService());
    helen::CommandExecutor executor(dispatcher, runtime_values, graphics_config_service);
    Expect(executor.RegisterCommand(CreateApplySubtitleSizeCommand("applySubtitleSize")), "Failed to register the happy-path subtitle command.");
    Expect(!executor.RegisterCommand(CreateApplySubtitleSizeCommand("applySubtitleSize")), "Duplicate command registration unexpectedly succeeded.");

    Expect(executor.RunCommand("applySubtitleSize"), "Happy-path command execution unexpectedly failed.");
    const std::optional<double> applied_value = runtime_values.TryGetDouble("subtitle.scale");
    Expect(applied_value.has_value(), "Happy-path command execution removed the live subtitle scale slot.");
    ExpectNear(*applied_value, 2.0, 0.001, "Happy-path command execution wrote the wrong live subtitle scale.");

    {
        const std::filesystem::path subtitle_ini_path = CreateTemporaryBatmanGraphicsIniPath();
        WriteAllText(subtitle_ini_path, CreateBatmanSubtitleIniText(5));

        helen::CommandDispatcher subtitle_dispatcher;
        subtitle_dispatcher.RegisterConfigInt("ui.subtitleSize", 0);

        helen::RuntimeValueStore subtitle_runtime_values;
        Expect(subtitle_runtime_values.RegisterSlot(CreateSubtitleScaleSlot()), "Failed to register the subtitle test runtime slot.");

        helen::BatmanGraphicsConfigService subtitle_graphics_config_service(subtitle_ini_path, GetGraphicsTestDisplayModeService());
        helen::CommandExecutor subtitle_executor(subtitle_dispatcher, subtitle_runtime_values, subtitle_graphics_config_service);
        Expect(subtitle_dispatcher.TrySetInt("ui.subtitleSize", 4), "Failed to seed the subtitle size config for the persistence command test.");
        Expect(subtitle_executor.RegisterCommand(CreateApplySubtitleSizePersistCommand("applySubtitleSizeWithPersistence")), "Failed to register the subtitle persistence command.");
        Expect(subtitle_executor.RunCommand("applySubtitleSizeWithPersistence"), "Subtitle persistence command execution unexpectedly failed.");
        Expect(ReadAllText(subtitle_ini_path).find("ConsoleFontSize=9") != std::string::npos, "Subtitle persistence command did not update Engine.HUD.ConsoleFontSize.");
        }

        {
            const std::filesystem::path subtitle_ini_path = CreateTemporaryBatmanGraphicsIniPath();
            WriteAllText(subtitle_ini_path, CreateBatmanSubtitleIniTextMissingHudSection());

            helen::CommandDispatcher subtitle_dispatcher;
            subtitle_dispatcher.RegisterConfigInt("ui.subtitleSize", 0);

            helen::RuntimeValueStore subtitle_runtime_values;
            Expect(subtitle_runtime_values.RegisterSlot(CreateSubtitleScaleSlot()), "Failed to register the subtitle upsert runtime slot.");

            helen::BatmanGraphicsConfigService subtitle_graphics_config_service(subtitle_ini_path, GetGraphicsTestDisplayModeService());
            helen::CommandExecutor subtitle_executor(subtitle_dispatcher, subtitle_runtime_values, subtitle_graphics_config_service);
            Expect(subtitle_dispatcher.TrySetInt("ui.subtitleSize", 2), "Failed to seed the subtitle size config for the insert test.");
            Expect(subtitle_executor.RegisterCommand(CreateApplySubtitleSizePersistCommand("applySubtitleSizeWithUpsert")), "Failed to register the subtitle upsert command.");
            Expect(subtitle_executor.RunCommand("applySubtitleSizeWithUpsert"), "Subtitle upsert command execution unexpectedly failed.");

            const std::string saved_text = ReadAllText(subtitle_ini_path);
            Expect(saved_text.find("[Engine.HUD]") != std::string::npos, "Subtitle upsert did not create Engine.HUD section.");
            Expect(saved_text.find("ConsoleFontSize=7") != std::string::npos, "Subtitle upsert did not persist ConsoleFontSize=7.");
        }

        {
            const std::filesystem::path engine_ini_path = CreateTemporaryBatmanGraphicsIniPath();
            const std::filesystem::path game_ini_path = GetSiblingBatmanGameIniPath(engine_ini_path);
            WriteAllText(engine_ini_path, CreateBatmanSubtitleIniTextMissingHudSection());
            WriteAllText(game_ini_path, CreateBatmanSubtitleIniText(9));

            helen::CommandDispatcher subtitle_dispatcher;
            subtitle_dispatcher.RegisterConfigInt("ui.subtitleSize", 0);

            helen::RuntimeValueStore subtitle_runtime_values;
            Expect(subtitle_runtime_values.RegisterSlot(CreateSubtitleScaleSlot()), "Failed to register the subtitle sibling-path runtime slot.");

            helen::BatmanGraphicsConfigService subtitle_graphics_config_service(engine_ini_path, GetGraphicsTestDisplayModeService());
            helen::CommandExecutor subtitle_executor(subtitle_dispatcher, subtitle_runtime_values, subtitle_graphics_config_service);
            Expect(subtitle_executor.RegisterCommand(CreateApplySubtitleSizePersistCommand("applySubtitleSizeWithSiblingBmGame")), "Failed to register the subtitle sibling-path persist command.");

            const std::filesystem::path subtitle_load_log_path = engine_ini_path.parent_path() / "subtitle-load.log";
            std::error_code subtitle_load_log_remove_error;
            std::filesystem::remove(subtitle_load_log_path, subtitle_load_log_remove_error);
            Expect(!subtitle_load_log_remove_error, "Subtitle load log fixture could not be isolated.");
            const std::filesystem::path previous_log_path = helen::GetLogPath();
            helen::SetLogPath(subtitle_load_log_path);
            const bool subtitle_load_result = subtitle_graphics_config_service.LoadSubtitleSizeIntoDispatcher(subtitle_dispatcher);
            helen::SetLogPath(previous_log_path);

            Expect(subtitle_load_result, "Subtitle load did not read the sibling BmGame.ini value.");
            Expect(subtitle_dispatcher.TryGetInt("ui.subtitleSize") == 4, "Subtitle load mapped the sibling BmGame.ini value incorrectly.");
            Expect(
                ReadAllText(subtitle_load_log_path).find("Engine.HUD.ConsoleFontSize=9") != std::string::npos,
                "Subtitle load log did not report the exact persisted ConsoleFontSize value.");

            Expect(subtitle_dispatcher.TrySetInt("ui.subtitleSize", 2), "Failed to seed the subtitle size config for sibling BmGame.ini persistence.");
            Expect(subtitle_executor.RunCommand("applySubtitleSizeWithSiblingBmGame"), "Subtitle sibling-path persistence command execution unexpectedly failed.");

            const std::string saved_engine_text = ReadAllText(engine_ini_path);
            const std::string saved_game_text = ReadAllText(game_ini_path);
            Expect(saved_engine_text.find("ConsoleFontSize=") == std::string::npos, "Subtitle sibling-path persistence unexpectedly wrote ConsoleFontSize into BmEngine.ini.");
            Expect(saved_game_text.find("ConsoleFontSize=7") != std::string::npos, "Subtitle sibling-path persistence did not update BmGame.ini.");
        }

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

    const std::string malformed_section_fixture =
        "[SystemSettings]\r\n"
        "[Malformed\r\n"
        "Bloom=True\r\n";
    Expect(
        !TryReadBatmanIniValue(malformed_section_fixture, "SystemSettings", "Bloom").has_value(),
        "The Batman INI assertion parser attributed a key after a malformed section header to the prior section.");
    const std::string padded_section_fixture =
        "[ SystemSettings ]\r\n"
        "Bloom=True\r\n";
    Expect(
        !TryReadBatmanIniValue(padded_section_fixture, "SystemSettings", "Bloom").has_value(),
        "The Batman INI assertion parser trimmed section-header contents that production treats as significant.");

    {
        const std::filesystem::path batman_ini_path = CreateTemporaryBatmanGraphicsIniPath();
        const std::filesystem::path batman_user_ini_path = GetSiblingBatmanUserEngineIniPath(batman_ini_path);
        WriteAllText(batman_ini_path, CreateBatmanGraphicsIniText());
        WriteAsciiAsUtf16LittleEndianText(batman_user_ini_path, CreateBatmanLauncherOwnedGraphicsIniText(true));
        const BatmanQualityLeaves generated_fixture_quality = {
            .Bloom = 0,
            .DynamicShadows = 0,
            .MotionBlur = 0,
            .Distortion = 0,
            .FogVolumes = 0,
            .SphericalHarmonicLighting = 0,
            .AmbientOcclusion = 0
        };
        const BatmanQualityLeaves launcher_fixture_quality = {
            .Bloom = 1,
            .DynamicShadows = 1,
            .MotionBlur = 1,
            .Distortion = 1,
            .FogVolumes = 1,
            .SphericalHarmonicLighting = 1,
            .AmbientOcclusion = 1
        };
        const std::string generated_fixture_text = ReadAllText(batman_ini_path);
        const std::string launcher_fixture_text = ReadAsciiFromUtf16LittleEndianText(batman_user_ini_path);
        ExpectBatmanPersistedQualityLeaves(generated_fixture_text, generated_fixture_quality);
        ExpectBatmanPersistedQualityLeaves(launcher_fixture_text, launcher_fixture_quality);
        ExpectBatmanIniValue(generated_fixture_text, "SystemSettings", "DetailMode", "0", "Generated Batman fixture DetailMode was not deliberately distinct.");
        ExpectBatmanIniValue(launcher_fixture_text, "SystemSettings", "DetailMode", "2", "Launcher Batman fixture DetailMode was not deliberately distinct.");

        helen::CommandDispatcher batman_dispatcher;
        RegisterBatmanGraphicsConfigKeys(batman_dispatcher);

        helen::RuntimeValueStore batman_runtime_values;
        helen::BatmanGraphicsConfigService batman_graphics_config_service(batman_ini_path, GetGraphicsTestDisplayModeService());
        helen::CommandExecutor batman_executor(batman_dispatcher, batman_runtime_values, batman_graphics_config_service);

        Expect(batman_executor.RegisterCommand(CreateLoadBatmanGraphicsDraftCommand()), "Failed to register the Batman graphics load command.");
        Expect(batman_executor.RegisterCommand(CreateSyncBatmanGraphicsPresetCommand()), "Failed to register the Batman preset-sync command.");
        Expect(batman_executor.RegisterCommand(CreateSyncBatmanGraphicsDetailLevelCommand()), "Failed to register the Batman detail-sync command.");
        Expect(batman_executor.RegisterCommand(CreateApplyBatmanGraphicsDraftCommand()), "Failed to register the Batman graphics apply command.");

        Expect(batman_executor.RunCommand("loadBatmanGraphicsDraftIntoConfig"), "Batman graphics load command unexpectedly failed.");
        Expect(batman_dispatcher.TryGetInt("fullscreen") == 0, "Batman graphics load read the wrong fullscreen state.");
        Expect(batman_dispatcher.TryGetInt("resolutionWidth") == 2560, "Batman graphics load read the wrong resolution width.");
        Expect(batman_dispatcher.TryGetInt("resolutionHeight") == 1440, "Batman graphics load read the wrong resolution height.");
        Expect(batman_dispatcher.TryGetInt("vsync") == 1, "Batman graphics load read the wrong VSync state.");
        Expect(batman_dispatcher.TryGetInt("msaa") == 3, "Batman graphics load read the wrong MSAA state.");
        Expect(batman_dispatcher.TryGetInt("detailLevel") == 3, "Batman graphics load failed to derive the Very High detail preset.");
        Expect(batman_dispatcher.TryGetInt("ambientOcclusion") == 1, "Batman graphics load read the wrong ambient-occlusion state.");
        Expect(batman_dispatcher.TryGetInt("physx") == 1, "Batman graphics load read the wrong PhysX state.");
        Expect(batman_dispatcher.TryGetInt("stereo") == 1, "Batman graphics load read the wrong stereo state.");
        Expect(batman_dispatcher.TryGetInt("bloom") == 1, "Launcher Bloom was not loaded.");
        Expect(batman_dispatcher.TryGetInt("dynamicShadows") == 1, "Launcher Dynamic Shadows were not loaded.");
        Expect(batman_dispatcher.TryGetInt("motionBlur") == 1, "Launcher Motion Blur was not loaded.");
        Expect(batman_dispatcher.TryGetInt("distortion") == 1, "Launcher Distortion was not loaded.");
        Expect(batman_dispatcher.TryGetInt("fogVolumes") == 1, "Launcher Fog Volumes were not loaded.");
        Expect(batman_dispatcher.TryGetInt("sphericalHarmonicLighting") == 1, "Launcher spherical lighting was not inverted correctly.");
        Expect(batman_dispatcher.TryGetInt("ambientOcclusion") == 1, "Launcher Ambient Occlusion was not loaded.");

        const std::array<BatmanQualityDetailCase, 6> quality_detail_cases = {
            BatmanQualityDetailCase{
                "Low",
                BatmanQualityLeaves{ .Bloom = 0, .DynamicShadows = 0, .MotionBlur = 0, .Distortion = 0, .FogVolumes = 0, .SphericalHarmonicLighting = 0, .AmbientOcclusion = 0 },
                0 },
            BatmanQualityDetailCase{
                "Medium",
                BatmanQualityLeaves{ .Bloom = 1, .DynamicShadows = 1, .MotionBlur = 0, .Distortion = 0, .FogVolumes = 0, .SphericalHarmonicLighting = 0, .AmbientOcclusion = 0 },
                1 },
            BatmanQualityDetailCase{
                "High",
                BatmanQualityLeaves{ .Bloom = 1, .DynamicShadows = 1, .MotionBlur = 1, .Distortion = 1, .FogVolumes = 1, .SphericalHarmonicLighting = 1, .AmbientOcclusion = 0 },
                2 },
            BatmanQualityDetailCase{
                "VeryHigh",
                BatmanQualityLeaves{ .Bloom = 1, .DynamicShadows = 1, .MotionBlur = 1, .Distortion = 1, .FogVolumes = 1, .SphericalHarmonicLighting = 1, .AmbientOcclusion = 1 },
                3 },
            BatmanQualityDetailCase{
                "CustomA",
                BatmanQualityLeaves{ .Bloom = 0, .DynamicShadows = 1, .MotionBlur = 1, .Distortion = 0, .FogVolumes = 1, .SphericalHarmonicLighting = 0, .AmbientOcclusion = 1 },
                4 },
            BatmanQualityDetailCase{
                "CustomB",
                BatmanQualityLeaves{ .Bloom = 1, .DynamicShadows = 0, .MotionBlur = 0, .Distortion = 1, .FogVolumes = 0, .SphericalHarmonicLighting = 1, .AmbientOcclusion = 0 },
                4 }
        };

        for (const BatmanQualityDetailCase& detail_case : quality_detail_cases)
        {
            SetBatmanQualityLeaves(batman_dispatcher, detail_case.Leaves);
            Expect(batman_executor.RunCommand("syncBatmanGraphicsDetailLevel"), "Batman detail-sync command failed for a literal quality case.");
            Expect(batman_dispatcher.TryGetInt("detailLevel") == detail_case.ExpectedDetailLevel, detail_case.Name);
        }

        for (int preset_detail_level = 0; preset_detail_level <= 3; ++preset_detail_level)
        {
            Expect(batman_dispatcher.TrySetInt("detailLevel", preset_detail_level), "Failed to select a literal Batman detail preset.");
            Expect(batman_executor.RunCommand("syncBatmanGraphicsPreset"), "Batman preset-sync command failed for a literal quality preset.");
            ExpectBatmanQualityLeaves(batman_dispatcher, quality_detail_cases[static_cast<std::size_t>(preset_detail_level)].Leaves);
        }

        const BatmanQualityLeaves custom_leaves = quality_detail_cases[4].Leaves;
        SetBatmanQualityLeaves(batman_dispatcher, custom_leaves);
        Expect(batman_executor.RunCommand("syncBatmanGraphicsDetailLevel"), "Batman detail-sync command unexpectedly failed.");
        Expect(batman_dispatcher.TryGetInt("detailLevel") == 4, "Batman detail-sync did not derive the Custom detail state.");
        const std::string pre_custom_engine_ini_text = ReadAllText(batman_ini_path);
        ExpectBatmanIniValue(pre_custom_engine_ini_text, "SystemSettings", "DetailMode", "0", "Custom Batman apply fixture did not start from the deliberate generated DetailMode value.");
        Expect(batman_executor.RunCommand("applyBatmanGraphicsDraft"), "Batman graphics apply command failed for the complete custom quality draft.");
        Expect(batman_dispatcher.TryGetInt("detailLevel") == 4, "Batman graphics apply did not preserve the Custom detail state.");
        ExpectBatmanQualityLeaves(batman_dispatcher, custom_leaves);

        const std::string custom_engine_ini_text = ReadAllText(batman_ini_path);
        const std::string custom_user_ini_text = ReadAsciiFromUtf16LittleEndianText(batman_user_ini_path);
        ExpectBatmanIniValue(custom_engine_ini_text, "SystemSettings", "DetailMode", "2", "Custom Batman apply did not normalize DetailMode in BmEngine.ini.");
        ExpectBatmanIniValue(custom_user_ini_text, "SystemSettings", "DetailMode", "2", "Custom Batman apply did not normalize DetailMode in UserEngine.ini.");
        ExpectBatmanPersistedQualityLeaves(custom_engine_ini_text, custom_leaves);
        ExpectBatmanPersistedQualityLeaves(custom_user_ini_text, custom_leaves);
        ExpectBatmanIniBoolean(custom_engine_ini_text, "DisableSphericalHarmonicLights", 1, "Custom apply did not invert spherical harmonic lighting in BmEngine.ini.");
        ExpectBatmanIniBoolean(custom_user_ini_text, "DisableSphericalHarmonicLights", 1, "Custom apply did not invert spherical harmonic lighting in UserEngine.ini.");

        Expect(batman_dispatcher.TrySetInt("detailLevel", 1), "Failed to restore the Batman medium detail-level preset.");
        Expect(batman_executor.RunCommand("syncBatmanGraphicsPreset"), "Batman preset-sync failed during apply setup.");
        Expect(batman_dispatcher.TrySetInt("vsync", 1), "Failed to seed the Batman VSync draft.");
        Expect(batman_dispatcher.TrySetInt("msaa", 0), "Failed to seed the Batman MSAA draft.");
        Expect(batman_dispatcher.TrySetInt("physx", 1), "Failed to seed the Batman PhysX draft.");
        Expect(batman_dispatcher.TrySetInt("stereo", 1), "Failed to seed the Batman stereo draft.");

        Expect(batman_executor.RunCommand("applyBatmanGraphicsDraft"), "Batman graphics apply command unexpectedly failed.");
        Expect(batman_dispatcher.TryGetInt("detailLevel") == 1, "Batman graphics apply did not preserve the Medium detail preset.");

        const std::string saved_ini_text = ReadAllText(batman_ini_path);
        const std::string saved_user_ini_text = ReadAsciiFromUtf16LittleEndianText(batman_user_ini_path);
        ExpectBatmanIniBoolean(saved_ini_text, "UseVsync", 1, "Batman graphics apply did not persist VSync in BmEngine.ini.");
        ExpectBatmanIniBoolean(saved_user_ini_text, "UseVsync", 1, "Batman graphics apply did not persist VSync in launcher-owned UserEngine.ini.");
        ExpectBatmanIniBoolean(saved_ini_text, "Fullscreen", 0, "Batman graphics apply did not persist fullscreen state in BmEngine.ini.");
        ExpectBatmanIniBoolean(saved_user_ini_text, "Fullscreen", 0, "Batman graphics apply did not persist fullscreen state in launcher-owned UserEngine.ini.");
        ExpectBatmanIniValue(saved_ini_text, "SystemSettings", "ResX", "2560", "Batman graphics apply did not persist horizontal resolution in BmEngine.ini.");
        ExpectBatmanIniValue(saved_user_ini_text, "SystemSettings", "ResX", "2560", "Batman graphics apply did not persist horizontal resolution in launcher-owned UserEngine.ini.");
        ExpectBatmanIniValue(saved_ini_text, "SystemSettings", "ResY", "1440", "Batman graphics apply did not persist vertical resolution in BmEngine.ini.");
        ExpectBatmanIniValue(saved_user_ini_text, "SystemSettings", "ResY", "1440", "Batman graphics apply did not persist vertical resolution in launcher-owned UserEngine.ini.");
        ExpectBatmanIniValue(saved_ini_text, "SystemSettings", "MaxMultisamples", "1", "Batman graphics apply did not persist disabled MSAA in BmEngine.ini.");
        ExpectBatmanIniValue(saved_user_ini_text, "SystemSettings", "MaxMultisamples", "1", "Batman graphics apply did not persist disabled MSAA in launcher-owned UserEngine.ini.");
        ExpectBatmanIniValue(saved_ini_text, "SystemSettings", "DetailMode", "1", "Batman graphics apply did not persist the Medium detail mode in BmEngine.ini.");
        ExpectBatmanIniValue(saved_user_ini_text, "SystemSettings", "DetailMode", "1", "Batman graphics apply did not persist the Medium detail mode in launcher-owned UserEngine.ini.");
        ExpectBatmanIniBoolean(saved_ini_text, "Bloom", 1, "Batman graphics apply did not persist Bloom for Medium in BmEngine.ini.");
        ExpectBatmanIniBoolean(saved_user_ini_text, "Bloom", 1, "Batman graphics apply did not persist Bloom for Medium in launcher-owned UserEngine.ini.");
        ExpectBatmanIniBoolean(saved_ini_text, "DynamicShadows", 1, "Batman graphics apply did not persist Dynamic Shadows for Medium in BmEngine.ini.");
        ExpectBatmanIniBoolean(saved_user_ini_text, "DynamicShadows", 1, "Batman graphics apply did not persist Dynamic Shadows for Medium in launcher-owned UserEngine.ini.");
        ExpectBatmanIniBoolean(saved_ini_text, "MotionBlur", 0, "Batman graphics apply did not persist Motion Blur for Medium in BmEngine.ini.");
        ExpectBatmanIniBoolean(saved_user_ini_text, "MotionBlur", 0, "Batman graphics apply did not persist Motion Blur for Medium in launcher-owned UserEngine.ini.");
        ExpectBatmanIniBoolean(saved_ini_text, "Distortion", 0, "Batman graphics apply did not persist Distortion for Medium in BmEngine.ini.");
        ExpectBatmanIniBoolean(saved_user_ini_text, "Distortion", 0, "Batman graphics apply did not persist Distortion for Medium in launcher-owned UserEngine.ini.");
        ExpectBatmanIniBoolean(saved_ini_text, "FogVolumes", 0, "Batman graphics apply did not persist Fog Volumes for Medium in BmEngine.ini.");
        ExpectBatmanIniBoolean(saved_user_ini_text, "FogVolumes", 0, "Batman graphics apply did not persist Fog Volumes for Medium in launcher-owned UserEngine.ini.");
        ExpectBatmanIniBoolean(saved_ini_text, "DisableSphericalHarmonicLights", 1, "Batman graphics apply did not invert spherical harmonic lighting for Medium in BmEngine.ini.");
        ExpectBatmanIniBoolean(saved_user_ini_text, "DisableSphericalHarmonicLights", 1, "Batman graphics apply did not invert spherical harmonic lighting for Medium in launcher-owned UserEngine.ini.");
        ExpectBatmanIniBoolean(saved_ini_text, "AmbientOcclusion", 0, "Batman graphics apply did not persist Ambient Occlusion for Medium in BmEngine.ini.");
        ExpectBatmanIniBoolean(saved_user_ini_text, "AmbientOcclusion", 0, "Batman graphics apply did not persist Ambient Occlusion for Medium in launcher-owned UserEngine.ini.");
        ExpectBatmanIniValue(saved_ini_text, "Engine.Engine", "PhysXLevel", "1", "Batman graphics apply did not persist PhysX in BmEngine.ini.");
        ExpectBatmanIniValue(saved_user_ini_text, "Engine.Engine", "PhysXLevel", "1", "Batman graphics apply did not persist PhysX in launcher-owned UserEngine.ini.");
        ExpectBatmanIniBoolean(saved_ini_text, "Stereo", 1, "Batman graphics apply did not persist stereo in BmEngine.ini.");
        ExpectBatmanIniBoolean(saved_user_ini_text, "Stereo", 1, "Batman graphics apply did not persist stereo in launcher-owned UserEngine.ini.");
        ExpectBatmanIniValue(saved_user_ini_text, "SystemSettings", "LauncherOwnedSentinel", "PreserveMe", "Batman graphics apply did not preserve unrelated UserEngine.ini content.");
    }

    {
        const std::filesystem::path engine_ini_path = CreateTemporaryBatmanGraphicsIniPath("missing-user-engine");
        const std::filesystem::path user_ini_path = GetSiblingBatmanUserEngineIniPath(engine_ini_path);
        WriteAllText(engine_ini_path, CreateBatmanGraphicsIniText());
        std::error_code remove_error;
        std::filesystem::remove(user_ini_path, remove_error);
        Expect(!remove_error && !std::filesystem::exists(user_ini_path), "Missing UserEngine.ini fixture could not be isolated.");

        helen::CommandDispatcher batman_dispatcher;
        RegisterBatmanGraphicsConfigKeys(batman_dispatcher);
        helen::RuntimeValueStore batman_runtime_values;
        helen::BatmanGraphicsConfigService batman_graphics_config_service(engine_ini_path, GetGraphicsTestDisplayModeService());
        helen::CommandExecutor batman_executor(batman_dispatcher, batman_runtime_values, batman_graphics_config_service);
        Expect(batman_executor.RegisterCommand(CreateLoadBatmanGraphicsDraftCommand()), "Failed to register the missing-launcher-INI load command.");
        Expect(!batman_executor.RunCommand("loadBatmanGraphicsDraftIntoConfig"), "Batman graphics load unexpectedly fell back to BmEngine.ini when UserEngine.ini was missing.");
    }

    {
        const std::filesystem::path engine_ini_path = CreateTemporaryBatmanGraphicsIniPath("incomplete-user-engine");
        const std::filesystem::path user_ini_path = GetSiblingBatmanUserEngineIniPath(engine_ini_path);
        WriteAllText(engine_ini_path, CreateBatmanGraphicsIniText());
        WriteAsciiAsUtf16LittleEndianText(user_ini_path, CreateBatmanLauncherOwnedGraphicsIniText(false));

        helen::CommandDispatcher batman_dispatcher;
        RegisterBatmanGraphicsConfigKeys(batman_dispatcher);
        helen::RuntimeValueStore batman_runtime_values;
        helen::BatmanGraphicsConfigService batman_graphics_config_service(engine_ini_path, GetGraphicsTestDisplayModeService());
        helen::CommandExecutor batman_executor(batman_dispatcher, batman_runtime_values, batman_graphics_config_service);
        Expect(batman_executor.RegisterCommand(CreateLoadBatmanGraphicsDraftCommand()), "Failed to register the incomplete-launcher-INI load command.");
        Expect(!batman_executor.RunCommand("loadBatmanGraphicsDraftIntoConfig"), "Batman graphics load unexpectedly fell back to BmEngine.ini when UserEngine.ini was incomplete.");
    }

    {
        const std::filesystem::path engine_ini_path = CreateTemporaryBatmanGraphicsIniPath("generated-publication-blocked");
        const std::filesystem::path user_ini_path = GetSiblingBatmanUserEngineIniPath(engine_ini_path);
        WriteAllText(engine_ini_path, CreateBatmanGraphicsIniText());
        WriteAsciiAsUtf16LittleEndianText(user_ini_path, CreateBatmanLauncherOwnedGraphicsIniText(true));

        const std::string original_engine_bytes = ReadAllBytes(engine_ini_path);
        const std::string original_user_bytes = ReadAllBytes(user_ini_path);

        helen::CommandDispatcher batman_dispatcher;
        RegisterBatmanGraphicsConfigKeys(batman_dispatcher);
        helen::BatmanGraphicsConfigService batman_graphics_config_service(engine_ini_path, GetGraphicsTestDisplayModeService());
        const HANDLE blocked_engine_handle = OpenBatmanIniDenyingWriteDeleteSharing(engine_ini_path);
        const bool apply_result = batman_graphics_config_service.ApplyFromDispatcher(batman_dispatcher);
        const BOOL close_result = CloseHandle(blocked_engine_handle);

        Expect(close_result != FALSE, "Failed to close the generated Batman graphics INI publication-blocking handle.");
        Expect(!apply_result, "Batman graphics apply unexpectedly succeeded with generated INI publication blocked.");
        ExpectBatmanIniBytesEqual(
            engine_ini_path,
            original_engine_bytes,
            "Generated INI disappeared after blocked publication.",
            "Generated INI bytes changed after blocked publication.");
        ExpectBatmanIniBytesEqual(
            user_ini_path,
            original_user_bytes,
            "UserEngine.ini disappeared after generated publication was blocked.",
            "UserEngine.ini bytes changed after generated publication was blocked.");
        ExpectNoBatmanGraphicsTransactionArtifacts(engine_ini_path.parent_path());
    }

    {
        const std::filesystem::path engine_ini_path = CreateTemporaryBatmanGraphicsIniPath("launcher-publication-blocked");
        const std::filesystem::path user_ini_path = GetSiblingBatmanUserEngineIniPath(engine_ini_path);
        WriteAllText(engine_ini_path, CreateBatmanGraphicsIniText());
        WriteAsciiAsUtf16LittleEndianText(user_ini_path, CreateBatmanLauncherOwnedGraphicsIniText(true));

        const std::string original_engine_bytes = ReadAllBytes(engine_ini_path);
        const std::string original_user_bytes = ReadAllBytes(user_ini_path);

        helen::CommandDispatcher batman_dispatcher;
        RegisterBatmanGraphicsConfigKeys(batman_dispatcher);
        helen::BatmanGraphicsConfigService batman_graphics_config_service(engine_ini_path, GetGraphicsTestDisplayModeService());
        const HANDLE blocked_user_handle = OpenBatmanIniDenyingWriteDeleteSharing(user_ini_path);
        const bool apply_result = batman_graphics_config_service.ApplyFromDispatcher(batman_dispatcher);
        const BOOL close_result = CloseHandle(blocked_user_handle);

        Expect(close_result != FALSE, "Failed to close the launcher Batman graphics INI publication-blocking handle.");
        Expect(!apply_result, "Batman graphics apply unexpectedly succeeded with UserEngine.ini publication blocked.");
        ExpectBatmanIniBytesEqual(
            engine_ini_path,
            original_engine_bytes,
            "Generated INI disappeared after launcher publication failed.",
            "Generated INI bytes were not restored after launcher publication failed.");
        ExpectBatmanIniBytesEqual(
            user_ini_path,
            original_user_bytes,
            "UserEngine.ini disappeared after blocked publication.",
            "UserEngine.ini bytes changed after blocked publication.");
        ExpectNoBatmanGraphicsTransactionArtifacts(engine_ini_path.parent_path());
    }

    RunConcurrentBatmanGraphicsApplyTest("concurrent-publication");
    RunBatmanResolutionModeCommandTest();
    RunBatmanResolutionCatalogKindGuardTest();
    RunBatmanResolutionModeApplyIntegrationTest();
}

/**
 * @brief Verifies that a selected supported resolution flows through the normal graphics Apply transaction.
 *
 * The resolution command stages the exact catalog pair in the dispatcher, after which the existing
 * graphics Apply command must publish that pair and Fullscreen to both INI documents.
 */
void RunBatmanResolutionModeApplyIntegrationTest()
{
    std::vector<helen::BatmanDisplayMode> current_modes = {
        helen::BatmanDisplayMode(1280, 720),
        helen::BatmanDisplayMode(1920, 1080),
        helen::BatmanDisplayMode(3440, 1440)
    };
    const helen::BatmanDisplayModeService::EnvironmentEnumerationCallback enumeration_callback =
        [&current_modes]() -> std::optional<helen::BatmanDisplayEnvironment>
        {
            return helen::BatmanDisplayEnvironment(
                L"DISPLAY1",
                current_modes,
                helen::BatmanDisplayMode(3440, 1440),
                helen::BatmanDisplayMode(1920, 1080));
        };
    helen::BatmanDisplayModeService display_mode_service(enumeration_callback);
    Expect(
        display_mode_service.Refresh(helen::BatmanDisplayModeCatalogKind::Fullscreen, 1920, 1080),
        "Resolution Apply integration catalog did not refresh.");

    const std::filesystem::path engine_ini_path = CreateTemporaryBatmanGraphicsIniPath("resolution-apply-integration");
    const std::filesystem::path user_ini_path = GetSiblingBatmanUserEngineIniPath(engine_ini_path);
    WriteAllText(engine_ini_path, CreateBatmanGraphicsIniText());
    WriteAsciiAsUtf16LittleEndianText(user_ini_path, CreateBatmanLauncherOwnedGraphicsIniText(true));

    helen::CommandDispatcher dispatcher;
    RegisterBatmanGraphicsConfigKeys(dispatcher);
    dispatcher.RegisterConfigInt("resolutionModeIndex", 0);
    helen::RuntimeValueStore runtime_values;
    helen::BatmanGraphicsConfigService graphics_config_service(engine_ini_path, display_mode_service);
    helen::CommandExecutor executor(dispatcher, runtime_values, graphics_config_service);
    Expect(executor.RegisterCommand(CreateSetBatmanGraphicsResolutionModeCommand()), "Failed to register the resolution Apply integration command.");
    Expect(executor.RegisterCommand(CreateApplyBatmanGraphicsDraftCommand()), "Failed to register the graphics Apply integration command.");
    Expect(dispatcher.TrySetInt("resolutionModeIndex", 1), "Failed to seed the resolution Apply integration index.");
    Expect(dispatcher.TrySetInt("fullscreen", 1), "Failed to seed fullscreen for the resolution Apply integration.");
    Expect(executor.RunCommand("setBatmanGraphicsResolutionMode"), "Resolution Apply integration selection failed.");
    Expect(dispatcher.TryGetInt("resolutionWidth") == 1920, "Resolution Apply integration selected the wrong width.");
    Expect(dispatcher.TryGetInt("resolutionHeight") == 1080, "Resolution Apply integration selected the wrong height.");
    Expect(executor.RunCommand("applyBatmanGraphicsDraft"), "Graphics Apply failed after selecting a supported resolution.");

    const std::string engine_text = ReadAllText(engine_ini_path);
    const std::string user_text = ReadAsciiFromUtf16LittleEndianText(user_ini_path);
    ExpectBatmanIniBoolean(engine_text, "Fullscreen", 1, "Graphics Apply did not publish selected fullscreen to BmEngine.ini.");
    ExpectBatmanIniBoolean(user_text, "Fullscreen", 1, "Graphics Apply did not publish selected fullscreen to UserEngine.ini.");
    ExpectBatmanIniValue(engine_text, "SystemSettings", "ResX", "1920", "Graphics Apply did not publish selected ResX to BmEngine.ini.");
    ExpectBatmanIniValue(user_text, "SystemSettings", "ResX", "1920", "Graphics Apply did not publish selected ResX to UserEngine.ini.");
    ExpectBatmanIniValue(engine_text, "SystemSettings", "ResY", "1080", "Graphics Apply did not publish selected ResY to BmEngine.ini.");
    ExpectBatmanIniValue(user_text, "SystemSettings", "ResY", "1080", "Graphics Apply did not publish selected ResY to UserEngine.ini.");
}

/** @brief Runs last because uncertain publication deliberately locks every graphics writer for the remainder of the process. */
void RunBatmanGraphicsIntegrityFailureTests() {
    const std::filesystem::path engine = CreateTemporaryBatmanGraphicsIniPath("direct-integrity-failure");
    const std::filesystem::path user = GetSiblingBatmanUserEngineIniPath(engine);
    WriteAllText(engine, CreateBatmanGraphicsIniText());
    WriteAsciiAsUtf16LittleEndianText(user, CreateBatmanLauncherOwnedGraphicsIniText(true));
    const std::string original_engine = ReadAllBytes(engine);
    const std::string original_user = ReadAllBytes(user);
    FaultingBatmanGraphicsFileOperations files(true, false);
    helen::BatmanGraphicsConfigService service(engine, GetGraphicsTestDisplayModeService(), files);
    std::optional<helen::BatmanGraphicsDraftState> draft = service.CaptureReadSnapshot().TryCreateDraft();
    Expect(draft.has_value(), "Integrity failure fixture invalid.");
    Expect(draft->TrySet(helen::BatmanGraphicsField::Vsync, 1), "Failed to set integrity test draft.");
    const helen::BatmanGraphicsApplyResult result = service.ApplyDraft(*draft);
    Expect(result.Outcome == helen::BatmanGraphicsApplyOutcome::IntegrityUncertain,
        "Failed reconciliation was presented as verified rollback.");
    Expect(ReadAllBytes(engine) != original_engine && ReadAllBytes(user) == original_user,
        "Integrity test did not actually produce a partial publication.");
    Expect(service.IsApplyLocked() && !result.RecoveryPaths.empty(), "Uncertain publication lost lockout or evidence.");
    bool original_engine_retained = false;
    bool original_user_retained = false;
    for (const std::filesystem::path& path : result.RecoveryPaths) {
        Expect(path.parent_path() == engine.parent_path(), "Integrity evidence escaped test fixture.");
        if (std::filesystem::exists(path)) {
            const std::string bytes = ReadAllBytes(path);
            original_engine_retained = original_engine_retained || bytes == original_engine;
            original_user_retained = original_user_retained || bytes == original_user;
        }
    }
    Expect(original_engine_retained && original_user_retained, "Original recovery bytes were deleted after failed reconciliation.");
    const std::string partial_engine = ReadAllBytes(engine);
    helen::BatmanGraphicsConfigService reopened(engine, GetGraphicsTestDisplayModeService());
    Expect(reopened.IsApplyLocked(), "Recreating the service cleared process integrity lockout.");
    Expect(reopened.ApplyDraft(*draft).Outcome == helen::BatmanGraphicsApplyOutcome::IntegrityUncertain,
        "A recreated writer accepted Apply after uncertain publication.");
    Expect(ReadAllBytes(engine) == partial_engine && ReadAllBytes(user) == original_user,
        "Locked writer attempted hidden repair or another publication.");
    for (const std::filesystem::path& path : result.RecoveryPaths) {
        Expect(helen::BatmanGraphicsFileOperations::Native().Remove(path), "Unable to clean test-owned integrity evidence.");
    }
}
