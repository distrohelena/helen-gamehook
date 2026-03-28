#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace helen
{
    /**
     * @brief Writes structured runtime debug state and append-only JSONL events for live troubleshooting.
     */
    class DebugTraceService
    {
    public:
        /**
         * @brief Describes the active runtime identity written into every state snapshot.
         */
        class RuntimeSnapshot
        {
        public:
            /** @brief Active pack identifier matched for the host executable. */
            std::string PackId;
            /** @brief Active build identifier matched for the host executable. */
            std::string BuildId;
            /** @brief Absolute path to the loaded Helen runtime module. */
            std::string ModulePath;
            /** @brief Absolute path to the resolved `helengamehook` root directory. */
            std::string HelenRoot;
        };

        /**
         * @brief Captures aggregate debug counters that summarize what the runtime observed so far.
         */
        class CounterSnapshot
        {
        public:
            /** @brief Number of direct config write calls recorded by the debug runtime surface. */
            std::uint64_t ConfigWrites = 0;
            /** @brief Number of hook-hit callbacks recorded across all installed hooks. */
            std::uint64_t HookHits = 0;
        };

        /**
         * @brief Captures the last direct config write observed by the debug runtime surface.
         */
        class ConfigWriteSnapshot
        {
        public:
            /** @brief Returns whether the runtime has observed at least one config write. */
            bool HasValue = false;
            /** @brief Config key used by the last observed direct config write. */
            std::string Key;
            /** @brief Integer value supplied by the last observed direct config write. */
            int Value = 0;
            /** @brief Returns whether the last observed direct config write succeeded. */
            bool Succeeded = false;
        };

        /**
         * @brief Captures one declared runtime slot address and current numeric contents.
         */
        class SlotSnapshot
        {
        public:
            /** @brief Stable runtime slot identifier. */
            std::string Id;
            /** @brief Absolute writable address of the runtime slot backing storage. */
            std::uintptr_t Address = 0;
            /** @brief Current numeric value stored in the runtime slot. */
            double Value = 0.0;
        };

        /**
         * @brief Captures one applied relocation inside an installed hook blob.
         */
        class RelocationSnapshot
        {
        public:
            /** @brief Byte offset inside the installed mutable blob where the relocation was written. */
            std::size_t Offset = 0;
            /** @brief Encoding used for the written relocation such as `abs32` or `rel32`. */
            std::string Encoding;
            /** @brief Declarative relocation source kind such as `runtime-slot` or `module-export`. */
            std::string SourceKind;
            /** @brief Human-readable relocation source detail such as a slot id or export name. */
            std::string SourceLabel;
            /** @brief Absolute address resolved from the relocation source before encoding. */
            std::uintptr_t ResolvedAddress = 0;
        };

        /**
         * @brief Captures one installed hook entry, blob placement, and relocation results.
         */
        class HookSnapshot
        {
        public:
            /** @brief Stable installed hook identifier. */
            std::string Id;
            /** @brief Absolute address of the overwritten hook target. */
            std::uintptr_t TargetAddress = 0;
            /** @brief Absolute base address of the mutable executable blob bytes. */
            std::uintptr_t BlobAddress = 0;
            /** @brief Absolute address that the inline hook jumps to before entering the blob payload. */
            std::uintptr_t EntryAddress = 0;
            /** @brief Absolute address where execution resumes after the blob finishes. */
            std::uintptr_t ResumeAddress = 0;
            /** @brief Number of execution hits recorded for this installed hook. */
            std::uint64_t HitCount = 0;
            /** @brief Applied relocation results captured during hook installation. */
            std::vector<RelocationSnapshot> Relocations;
        };

        /**
         * @brief Captures one full debug state snapshot written to `batman-debug-state.json`.
         */
        class Snapshot
        {
        public:
            /** @brief Active runtime identity written into the debug snapshot. */
            RuntimeSnapshot Runtime;
            /** @brief Aggregate debug counters observed so far. */
            CounterSnapshot Counters;
            /** @brief Last direct config write observed by the runtime. */
            ConfigWriteSnapshot LastConfigWrite;
            /** @brief Current declared runtime slot values and addresses. */
            std::vector<SlotSnapshot> Slots;
            /** @brief Current installed-hook placements and relocation results. */
            std::vector<HookSnapshot> Hooks;
        };

        /**
         * @brief Captures one append-only JSONL event emitted during runtime debugging.
         */
        class EventRecord
        {
        public:
            /** @brief Stable event type name written into the JSONL record. */
            std::string Type;
            /** @brief Optional hook identifier associated with the event. */
            std::string HookId;
            /** @brief Optional config key associated with the event. */
            std::string Key;
            /** @brief Optional result text associated with the event. */
            std::string Result;
            /** @brief Optional integer value associated with the event. */
            int IntValue = 0;
            /** @brief Returns whether IntValue should be serialized into the event record. */
            bool HasIntValue = false;
        };

        /**
         * @brief Creates one debug trace service bound to the two runtime debug output files.
         * @param state_path Absolute state snapshot output path.
         * @param events_path Absolute JSONL event output path.
         */
        DebugTraceService(std::filesystem::path state_path, std::filesystem::path events_path);

        /**
         * @brief Overwrites the state snapshot file with the supplied structured runtime debug data.
         * @param snapshot Full runtime debug snapshot that should replace the previous state file contents.
         */
        void WriteSnapshot(const Snapshot& snapshot) const;

        /**
         * @brief Appends one newline-delimited JSON event record to the events file.
         * @param event Event data that should be appended as one JSON object line.
         */
        void AppendEvent(const EventRecord& event) const;

    private:
        /** @brief Absolute snapshot file path updated with the latest known runtime state. */
        std::filesystem::path state_path_;

        /** @brief Absolute JSONL file path that receives append-only event records. */
        std::filesystem::path events_path_;

        /** @brief Serializes concurrent state and event writes so the debug files stay well-formed. */
        mutable std::mutex mutex_;
    };
}