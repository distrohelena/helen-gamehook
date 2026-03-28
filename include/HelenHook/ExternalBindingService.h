#pragma once

#include <map>
#include <string>

#include <HelenHook/ExternalBindingDefinition.h>

namespace helen
{
    class CommandDispatcher;
    class CommandExecutor;

    /**
     * @brief Resolves patched gameplay callback names into typed Helen config and command operations.
     *
     * The service stores bindings by external callback name and performs no process integration on its
     * own, which keeps it deterministic and easy to validate in unit tests.
     */
    class ExternalBindingService
    {
    public:
        /**
         * @brief Creates a binding service that uses the supplied dispatcher and executor for runtime work.
         * @param dispatcher Config dispatcher used for get-int and set-int bindings.
         * @param executor Command executor used for command dispatch after a binding resolves.
         */
        ExternalBindingService(CommandDispatcher& dispatcher, CommandExecutor& executor);

        /**
         * @brief Registers one binding definition for later lookup by external callback name.
         * @param binding Binding definition that should become available to patched gameplay assets.
         */
        void Register(const ExternalBindingDefinition& binding);

        /**
         * @brief Tries to read one integer config value through a registered get-int binding.
         * @param external_name External callback name invoked by the patched asset.
         * @param key Config key requested by the caller and matched against the registered binding.
         * @param value Receives the resolved config value when a binding and config entry are found.
         * @return True when a matching binding resolved and a config value was read successfully; otherwise false.
         */
        bool TryHandleGetInt(const std::string& external_name, const std::string& key, int& value) const;

        /**
         * @brief Tries to write one integer config value through a registered set-int binding.
         * @param external_name External callback name invoked by the patched asset.
         * @param key Config key requested by the caller and matched against the registered binding.
         * @param value Integer value that should be written when the binding resolves.
         * @return True when a matching binding updated config successfully and any follow-up command ran; otherwise false.
         */
        bool TryHandleSetInt(const std::string& external_name, const std::string& key, int value);

        /**
         * @brief Tries to run one configured command through a registered run-command binding.
         * @param external_name External callback name invoked by the patched asset.
         * @param command_id Command identifier requested by the caller and matched against the registered binding.
         * @return True when a matching binding ran its configured command successfully; otherwise false.
         */
        bool TryHandleRunCommand(const std::string& external_name, const std::string& command_id);

    private:
        /** @brief Config dispatcher used by get-int and set-int bindings. */
        CommandDispatcher& dispatcher_;
        /** @brief Command executor used by set-int follow-up commands and run-command bindings. */
        CommandExecutor& executor_;
        /** @brief Registered bindings keyed by their external callback names. */
        std::multimap<std::string, ExternalBindingDefinition> bindings_;
    };
}