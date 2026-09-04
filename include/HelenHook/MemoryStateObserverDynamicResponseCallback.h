#pragma once

#include <functional>
#include <optional>
#include <string>

namespace helen
{
    /**
     * @brief Computes a bounded scalar response for one dynamic observer request.
     *
     * The provider identifier selects the runtime-owned response source, while the raw request
     * value identifies the observer request that is being answered. An absent result indicates
     * that the provider cannot produce a response for that request; callers are responsible for
     * enforcing the response bounds declared by the observer contract.
     */
    using MemoryStateObserverDynamicResponseCallback = std::function<std::optional<int>(const std::string& provider_id, int raw_request_value)>;
}
