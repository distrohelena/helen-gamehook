#pragma once

#include <string>
#include <vector>

#include <HelenHook/CommandMapEntryDefinition.h>

namespace helen
{
    /**
     * @brief Declares one constrained step inside a split-pack command workflow.
     *
     * Each field is populated only for the step kinds that need it. Runtime execution rejects steps
     * that omit required values instead of guessing defaults.
     */
    class CommandStepDefinition
    {
    public:
        /** @brief Step kind that selects the runtime primitive executed by the command. */
        std::string Kind;
        /** @brief Config key read or written by the step when applicable. */
        std::string ConfigKey;
        /** @brief Named transient value produced or consumed by single-value steps. */
        std::string ValueName;
        /** @brief Named transient integer input consumed by mapping-oriented steps. */
        std::string InputValueName;
        /** @brief Named transient double output produced by mapping-oriented steps. */
        std::string OutputValueName;
        /** @brief Resolved runtime target identifier written by live-value steps. */
        std::string Target;
        /** @brief Another command identifier invoked by chaining steps. */
        std::string CommandId;
        /** @brief Diagnostic message emitted by log-oriented steps. */
        std::string Message;
        /** @brief Literal integer value used by constant-oriented steps that require one. */
        int IntegerValue{};
        /** @brief Ordered integer-to-double mappings declared for validated map-int-to-double steps. */
        std::vector<CommandMapEntryDefinition> Mappings;
    };
}
