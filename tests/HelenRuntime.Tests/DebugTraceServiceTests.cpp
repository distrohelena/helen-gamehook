#include <HelenHook/DebugTraceService.h>
#include <HelenHook/JsonParser.h>

#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>

namespace
{
    /**
     * @brief Throws when one required test condition is false so the shared runner stops at the first failed assertion.
     * @param condition Boolean condition that must hold for the current test case.
     * @param message Failure text reported by the shared runtime test runner.
     */
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Creates one unique temporary directory for the debug trace service tests.
     * @param test_name Stable short name used to keep temporary files recognizable during debugging.
     * @return Newly created empty directory path rooted under the system temporary directory.
     */
    std::filesystem::path CreateTemporaryDirectory(const std::string& test_name)
    {
        const std::filesystem::path directory_path =
            std::filesystem::temp_directory_path() /
            (test_name + "-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
        std::filesystem::create_directories(directory_path);
        return directory_path;
    }

    /**
     * @brief Removes one temporary directory tree after the test finishes without masking earlier failures.
     * @param directory_path Directory tree that should be removed when it still exists.
     */
    void RemoveDirectoryTree(const std::filesystem::path& directory_path)
    {
        std::error_code error;
        std::filesystem::remove_all(directory_path, error);
    }

    /**
     * @brief Reads one complete text file into memory for JSON assertions.
     * @param path File path whose full contents should be returned.
     * @return Entire file contents as a narrow string.
     */
    std::string ReadAllText(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            throw std::runtime_error("Failed to open the debug trace output file.");
        }

        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }
}

/**
 * @brief Verifies that the debug trace service writes a structured runtime snapshot and newline-delimited event records.
 */
void RunDebugTraceServiceTests()
{
    const std::filesystem::path test_root = CreateTemporaryDirectory("HelenRuntime-DebugTraceServiceTests");
    try
    {
        const std::filesystem::path state_path = test_root / "batman-debug-state.json";
        const std::filesystem::path events_path = test_root / "batman-debug-events.jsonl";

        helen::DebugTraceService trace_service(state_path, events_path);

        helen::DebugTraceService::Snapshot snapshot;
        snapshot.Runtime.PackId = "batman-aa-subtitles";
        snapshot.Runtime.BuildId = "steam-goty-1.0";
        snapshot.Runtime.ModulePath = "D:/steam/Binaries/HelenGameHook.dll";
        snapshot.Runtime.HelenRoot = "D:/steam/Binaries/helengamehook";
        snapshot.Counters.ConfigWrites = 2;
        snapshot.Counters.HookHits = 7;
        snapshot.LastConfigWrite.HasValue = true;
        snapshot.LastConfigWrite.Key = "ui.subtitleSize";
        snapshot.LastConfigWrite.Value = 2;
        snapshot.LastConfigWrite.Succeeded = true;

        helen::DebugTraceService::SlotSnapshot slot;
        slot.Id = "subtitle.scale";
        slot.Address = 0x12345678;
        slot.Value = 8.0;
        snapshot.Slots.push_back(slot);

        helen::DebugTraceService::HookSnapshot hook;
        hook.Id = "subtitleSignalWrapper01";
        hook.TargetAddress = 0x00401000;
        hook.BlobAddress = 0x10002000;
        hook.EntryAddress = 0x10001000;
        hook.ResumeAddress = 0x0040100A;
        hook.HitCount = 3;

        helen::DebugTraceService::RelocationSnapshot relocation;
        relocation.Offset = 99;
        relocation.Encoding = "rel32";
        relocation.SourceKind = "module-export";
        relocation.SourceLabel = "HelenGameHook.dll!HelenSetConfigIntA";
        relocation.ResolvedAddress = 0x50004000;
        hook.Relocations.push_back(relocation);
        snapshot.Hooks.push_back(hook);

        trace_service.WriteSnapshot(snapshot);

        helen::DebugTraceService::EventRecord hook_hit_event;
        hook_hit_event.Type = "hook-hit";
        hook_hit_event.HookId = "subtitleSignalWrapper01";
        hook_hit_event.IntValue = 3;
        hook_hit_event.HasIntValue = true;
        trace_service.AppendEvent(hook_hit_event);

        helen::DebugTraceService::EventRecord config_write_event;
        config_write_event.Type = "config-write";
        config_write_event.Key = "ui.subtitleSize";
        config_write_event.IntValue = 2;
        config_write_event.HasIntValue = true;
        config_write_event.Result = "ok";
        trace_service.AppendEvent(config_write_event);

        const std::string state_text = ReadAllText(state_path);
        const std::optional<helen::JsonValue> parsed_state = helen::JsonParser::Parse(state_text);
        Expect(parsed_state.has_value(), "Expected the debug trace state file to contain valid JSON.");

        const helen::JsonValue* runtime = parsed_state->FindMember("runtime");
        Expect(runtime != nullptr, "Expected the debug trace state file to contain a runtime object.");
        Expect(runtime->FindMember("packId") != nullptr, "Expected the debug trace state runtime object to contain packId.");

        const helen::JsonValue* slots = parsed_state->FindMember("slots");
        Expect(slots != nullptr && slots->AsArray() != nullptr, "Expected the debug trace state file to contain a slots array.");
        Expect(slots->AsArray()->size() == 1, "Expected the debug trace state file to contain one slot snapshot.");

        const helen::JsonValue* hooks = parsed_state->FindMember("hooks");
        Expect(hooks != nullptr && hooks->AsArray() != nullptr, "Expected the debug trace state file to contain a hooks array.");
        Expect(hooks->AsArray()->size() == 1, "Expected the debug trace state file to contain one hook snapshot.");

        const helen::JsonValue* counters = parsed_state->FindMember("counters");
        Expect(counters != nullptr, "Expected the debug trace state file to contain counters.");
        Expect(counters->FindMember("hookHits") != nullptr, "Expected the debug trace state counters to contain hookHits.");

        const std::string events_text = ReadAllText(events_path);
        const std::size_t newline_count = static_cast<std::size_t>(std::count(events_text.begin(), events_text.end(), '\n'));
        Expect(newline_count == 2, "Expected the debug trace events file to contain two JSONL records.");

        const std::size_t first_newline = events_text.find('\n');
        Expect(first_newline != std::string::npos, "Expected the debug trace events file to contain a first JSONL newline.");
        const std::string first_line = events_text.substr(0, first_newline);
        const std::string second_line = events_text.substr(first_newline + 1, events_text.find('\n', first_newline + 1) - (first_newline + 1));

        const std::optional<helen::JsonValue> parsed_first_event = helen::JsonParser::Parse(first_line);
        const std::optional<helen::JsonValue> parsed_second_event = helen::JsonParser::Parse(second_line);
        Expect(parsed_first_event.has_value(), "Expected the first debug trace event line to contain valid JSON.");
        Expect(parsed_second_event.has_value(), "Expected the second debug trace event line to contain valid JSON.");
        Expect(parsed_first_event->FindMember("type") != nullptr, "Expected the first debug trace event to contain a type field.");
        Expect(parsed_second_event->FindMember("result") != nullptr, "Expected the second debug trace event to contain a result field.");
    }
    catch (...)
    {
        RemoveDirectoryTree(test_root);
        throw;
    }

    RemoveDirectoryTree(test_root);
}
