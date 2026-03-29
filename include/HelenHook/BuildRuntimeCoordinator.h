#pragma once

#include <memory>
#include <string>
#include <vector>

#include <HelenHook/MemoryStateObserverDefinition.h>
#include <HelenHook/MemoryStateObserverUpdate.h>

namespace helen
{
    class CommandDispatcher;
    class CommandExecutor;
    class MemoryStateObserverService;

    /**
     * @brief Coordinates build-scoped startup commands and live memory-state observers for one active pack build.
     *
     * The coordinator keeps Helen's startup behavior generic: startup command identifiers come from
     * pack data, and observer updates are applied through the existing config dispatcher and command
     * executor instead of custom game logic. This lets packs declare "mirror UI state into config and
     * run command X" without baking those workflows into the runtime DLL.
     */
    class BuildRuntimeCoordinator
    {
    public:
        /**
         * @brief Captures the startup command list, observer declarations, and generic command surfaces used by one active build.
         * @param startup_command_ids Command identifiers that must run once after runtime services are ready.
         * @param state_observers Declarative live-state observers that should mirror game state into Helen config.
         * @param command_dispatcher Registered config dispatcher updated by observer emissions.
         * @param command_executor Declarative command executor used for startup commands and observer follow-up commands.
         */
        BuildRuntimeCoordinator(
            std::vector<std::string> startup_command_ids,
            std::vector<MemoryStateObserverDefinition> state_observers,
            CommandDispatcher& command_dispatcher,
            CommandExecutor& command_executor);

        /**
         * @brief Stops any live observer thread before the coordinator is destroyed.
         */
        ~BuildRuntimeCoordinator();

        BuildRuntimeCoordinator(const BuildRuntimeCoordinator&) = delete;
        BuildRuntimeCoordinator& operator=(const BuildRuntimeCoordinator&) = delete;
        BuildRuntimeCoordinator(BuildRuntimeCoordinator&&) = delete;
        BuildRuntimeCoordinator& operator=(BuildRuntimeCoordinator&&) = delete;

        /**
         * @brief Runs the declared startup commands and starts live observer polling when observers were declared.
         * @return True when startup commands succeed and observers start successfully; otherwise false.
         */
        bool Start();

        /**
         * @brief Stops live observer polling and returns the coordinator to the idle state.
         */
        void Stop();

        /**
         * @brief Polls every declared state observer immediately, bypassing background interval throttling.
         * @return True when every observer poll succeeds or no observers are declared; otherwise false.
         */
        bool PollStateObserversOnce();

    private:
        /**
         * @brief Applies one mapped observer update through the generic config and command surfaces.
         * @param update Observer update emitted by the live-state scanner.
         */
        void HandleObserverUpdate(const MemoryStateObserverUpdate& update);

        /** @brief Startup command identifiers that must run once after runtime services are ready. */
        std::vector<std::string> startup_command_ids_;
        /** @brief Registered config dispatcher updated by observer emissions. */
        CommandDispatcher& command_dispatcher_;
        /** @brief Declarative command executor used by startup commands and observer follow-up commands. */
        CommandExecutor& command_executor_;
        /** @brief Optional live observer service created when the active build declares state observers. */
        std::unique_ptr<MemoryStateObserverService> observer_service_;
        /** @brief Tracks whether startup commands already ran and any declared observer thread is active. */
        bool started_ = false;
    };
}
