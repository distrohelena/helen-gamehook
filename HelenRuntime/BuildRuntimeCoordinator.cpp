#include <HelenHook/BuildRuntimeCoordinator.h>

#include <HelenHook/CommandDispatcher.h>
#include <HelenHook/CommandExecutor.h>
#include <HelenHook/Log.h>
#include <HelenHook/MemoryStateObserverService.h>
#include <HelenHook/MemoryStateObserverUpdate.h>

namespace helen
{
    BuildRuntimeCoordinator::BuildRuntimeCoordinator(
        std::vector<std::string> startup_command_ids,
        std::vector<MemoryStateObserverDefinition> state_observers,
        CommandDispatcher& command_dispatcher,
        CommandExecutor& command_executor)
        : startup_command_ids_(std::move(startup_command_ids))
        , command_dispatcher_(command_dispatcher)
        , command_executor_(command_executor)
    {
        if (!state_observers.empty())
        {
            observer_service_ = std::make_unique<MemoryStateObserverService>(
                std::move(state_observers),
                [this](const MemoryStateObserverUpdate& update)
                {
                    HandleObserverUpdate(update);
                },
                [this](const std::string& config_key)
                {
                    return command_dispatcher_.TryGetInt(config_key);
                });
        }
    }

    BuildRuntimeCoordinator::~BuildRuntimeCoordinator()
    {
        Stop();
    }

    bool BuildRuntimeCoordinator::Start()
    {
        if (started_)
        {
            return true;
        }

        for (const std::string& command_id : startup_command_ids_)
        {
            if (!command_executor_.RunCommand(command_id))
            {
                return false;
            }
        }

        if (observer_service_ != nullptr && !observer_service_->Start())
        {
            return false;
        }

        started_ = true;
        return true;
    }

    void BuildRuntimeCoordinator::Stop()
    {
        if (observer_service_ != nullptr)
        {
            observer_service_->Stop();
        }

        started_ = false;
    }

    bool BuildRuntimeCoordinator::PollStateObserversOnce()
    {
        if (observer_service_ == nullptr)
        {
            return true;
        }

        return observer_service_->PollOnce();
    }

    void BuildRuntimeCoordinator::HandleObserverUpdate(const MemoryStateObserverUpdate& update)
    {
        const bool set_succeeded = command_dispatcher_.TrySetInt(update.ConfigKey, update.MappedValue);
        if (!set_succeeded)
        {
            Logf(
                L"[observer] update id=%hs raw=%d mapped=%d key=%hs setResult=0",
                update.ObserverId.c_str(),
                update.RawValue,
                update.MappedValue,
                update.ConfigKey.c_str());
            return;
        }

        bool command_succeeded = true;
        if (update.CommandId.has_value())
        {
            command_succeeded = command_executor_.RunCommand(*update.CommandId);
        }

        Logf(
            L"[observer] update id=%hs raw=%d mapped=%d key=%hs setResult=1 command=%hs commandResult=%d",
            update.ObserverId.c_str(),
            update.RawValue,
            update.MappedValue,
            update.ConfigKey.c_str(),
            update.CommandId.has_value() ? update.CommandId->c_str() : "<none>",
            static_cast<int>(command_succeeded));
    }
}
