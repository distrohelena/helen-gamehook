#include <HelenHook/DebugTraceService.h>

#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>

namespace
{
    /**
     * @brief Ensures the parent directory for one debug output file exists.
     * @param path Output file path whose parent directory should be created when needed.
     */
    void EnsureParentDirectory(const std::filesystem::path& path)
    {
        const std::filesystem::path parent_path = path.parent_path();
        if (parent_path.empty())
        {
            return;
        }

        std::error_code error_code;
        std::filesystem::create_directories(parent_path, error_code);
    }

    /**
     * @brief Writes one JSON string literal with minimal escaping.
     * @param stream Output stream that should receive the escaped JSON string literal.
     * @param text Text that should be emitted as one JSON string.
     */
    void WriteJsonString(std::ostream& stream, std::string_view text)
    {
        stream << '"';
        for (const char character : text)
        {
            switch (character)
            {
            case '\\':
                stream << "\\\\";
                break;
            case '"':
                stream << "\\\"";
                break;
            case '\r':
                stream << "\\r";
                break;
            case '\n':
                stream << "\\n";
                break;
            case '\t':
                stream << "\\t";
                break;
            default:
                if (static_cast<unsigned char>(character) < 0x20)
                {
                    stream << "\\u00";
                    constexpr char HexDigits[] = "0123456789abcdef";
                    stream << HexDigits[(character >> 4) & 0x0F];
                    stream << HexDigits[character & 0x0F];
                }
                else
                {
                    stream << character;
                }

                break;
            }
        }

        stream << '"';
    }

    /**
     * @brief Writes one complete runtime snapshot JSON document.
     * @param stream Output stream that should receive the JSON document.
     * @param snapshot Structured debug snapshot that should be serialized.
     */
    void WriteSnapshotJson(std::ostream& stream, const helen::DebugTraceService::Snapshot& snapshot)
    {
        stream << "{\n";
        stream << "  \"runtime\": {\n";
        stream << "    \"packId\": ";
        WriteJsonString(stream, snapshot.Runtime.PackId);
        stream << ",\n    \"buildId\": ";
        WriteJsonString(stream, snapshot.Runtime.BuildId);
        stream << ",\n    \"modulePath\": ";
        WriteJsonString(stream, snapshot.Runtime.ModulePath);
        stream << ",\n    \"helenRoot\": ";
        WriteJsonString(stream, snapshot.Runtime.HelenRoot);
        stream << "\n  },\n";

        stream << "  \"counters\": {\n";
        stream << "    \"configWrites\": " << snapshot.Counters.ConfigWrites << ",\n";
        stream << "    \"hookHits\": " << snapshot.Counters.HookHits << "\n";
        stream << "  },\n";

        stream << "  \"lastConfigWrite\": {\n";
        stream << "    \"hasValue\": " << (snapshot.LastConfigWrite.HasValue ? "true" : "false") << ",\n";
        stream << "    \"key\": ";
        WriteJsonString(stream, snapshot.LastConfigWrite.Key);
        stream << ",\n    \"value\": " << snapshot.LastConfigWrite.Value << ",\n";
        stream << "    \"succeeded\": " << (snapshot.LastConfigWrite.Succeeded ? "true" : "false") << "\n";
        stream << "  },\n";

        stream << "  \"slots\": [\n";
        for (std::size_t index = 0; index < snapshot.Slots.size(); ++index)
        {
            const helen::DebugTraceService::SlotSnapshot& slot = snapshot.Slots[index];
            stream << "    {\n";
            stream << "      \"id\": ";
            WriteJsonString(stream, slot.Id);
            stream << ",\n      \"address\": " << slot.Address << ",\n";
            stream << "      \"value\": " << slot.Value << "\n";
            stream << "    }";
            if (index + 1 != snapshot.Slots.size())
            {
                stream << ',';
            }

            stream << '\n';
        }

        stream << "  ],\n";
        stream << "  \"hooks\": [\n";
        for (std::size_t hook_index = 0; hook_index < snapshot.Hooks.size(); ++hook_index)
        {
            const helen::DebugTraceService::HookSnapshot& hook = snapshot.Hooks[hook_index];
            stream << "    {\n";
            stream << "      \"id\": ";
            WriteJsonString(stream, hook.Id);
            stream << ",\n      \"targetAddress\": " << hook.TargetAddress << ",\n";
            stream << "      \"blobAddress\": " << hook.BlobAddress << ",\n";
            stream << "      \"entryAddress\": " << hook.EntryAddress << ",\n";
            stream << "      \"resumeAddress\": " << hook.ResumeAddress << ",\n";
            stream << "      \"hitCount\": " << hook.HitCount << ",\n";
            stream << "      \"relocations\": [\n";

            for (std::size_t relocation_index = 0; relocation_index < hook.Relocations.size(); ++relocation_index)
            {
                const helen::DebugTraceService::RelocationSnapshot& relocation = hook.Relocations[relocation_index];
                stream << "        {\n";
                stream << "          \"offset\": " << relocation.Offset << ",\n";
                stream << "          \"encoding\": ";
                WriteJsonString(stream, relocation.Encoding);
                stream << ",\n          \"sourceKind\": ";
                WriteJsonString(stream, relocation.SourceKind);
                stream << ",\n          \"sourceLabel\": ";
                WriteJsonString(stream, relocation.SourceLabel);
                stream << ",\n          \"resolvedAddress\": " << relocation.ResolvedAddress << "\n";
                stream << "        }";
                if (relocation_index + 1 != hook.Relocations.size())
                {
                    stream << ',';
                }

                stream << '\n';
            }

            stream << "      ]\n";
            stream << "    }";
            if (hook_index + 1 != snapshot.Hooks.size())
            {
                stream << ',';
            }

            stream << '\n';
        }

        stream << "  ]\n";
        stream << "}\n";
    }

    /**
     * @brief Writes one newline-delimited debug event JSON object.
     * @param stream Output stream that should receive the event record.
     * @param event Event record that should be serialized.
     */
    void WriteEventJson(std::ostream& stream, const helen::DebugTraceService::EventRecord& event)
    {
        stream << "{";
        stream << "\"type\": ";
        WriteJsonString(stream, event.Type);

        if (!event.HookId.empty())
        {
            stream << ", \"hookId\": ";
            WriteJsonString(stream, event.HookId);
        }

        if (!event.Key.empty())
        {
            stream << ", \"key\": ";
            WriteJsonString(stream, event.Key);
        }

        if (!event.Result.empty())
        {
            stream << ", \"result\": ";
            WriteJsonString(stream, event.Result);
        }

        if (event.HasIntValue)
        {
            stream << ", \"intValue\": " << event.IntValue;
        }

        stream << "}\n";
    }
}

namespace helen
{
    /**
     * @brief Creates one debug trace service bound to the two runtime debug output files.
     * @param state_path Absolute state snapshot output path.
     * @param events_path Absolute JSONL event output path.
     */
    DebugTraceService::DebugTraceService(std::filesystem::path state_path, std::filesystem::path events_path)
        : state_path_(std::move(state_path)),
          events_path_(std::move(events_path))
    {
    }

    /**
     * @brief Overwrites the state snapshot file with the supplied structured runtime debug data.
     * @param snapshot Full runtime debug snapshot that should replace the previous state file contents.
     */
    void DebugTraceService::WriteSnapshot(const Snapshot& snapshot) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        EnsureParentDirectory(state_path_);

        std::ofstream stream(state_path_, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            return;
        }

        WriteSnapshotJson(stream, snapshot);
    }

    /**
     * @brief Appends one newline-delimited JSON event record to the events file.
     * @param event Event data that should be appended as one JSON object line.
     */
    void DebugTraceService::AppendEvent(const EventRecord& event) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        EnsureParentDirectory(events_path_);

        std::ofstream stream(events_path_, std::ios::binary | std::ios::app);
        if (!stream)
        {
            return;
        }

        WriteEventJson(stream, event);
    }
}
