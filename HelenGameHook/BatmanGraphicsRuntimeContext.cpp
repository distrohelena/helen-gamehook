#include "BatmanGraphicsRuntimeContext.h"

#include <utility>

namespace helen {
    BatmanGraphicsRuntimeContext::BatmanGraphicsRuntimeContext(const std::filesystem::path& engineIniPath)
        : BatmanGraphicsRuntimeContext(engineIniPath, nullptr) {
    }

    BatmanGraphicsRuntimeContext::BatmanGraphicsRuntimeContext(const std::filesystem::path& engineIniPath,
        std::shared_ptr<FileWriteRoutingService> routing_service)
        : Display(), Config(engineIniPath, Display, routing_service),
#if defined(HELEN_ENABLE_SESSION_LIVE)
          Native(Config,std::move(routing_service),engineIniPath), Live(Native), Sessions(Live,Display),
#else
          Sessions(Config, Display),
#endif
          Adapter(Sessions) {
    }

    void BatmanGraphicsRuntimeContext::Handle(const char* name, const void* arguments, unsigned count,
        BatmanGraphicsPrimitiveResult& result) {
        Adapter.TryHandle(name, arguments, count, result);
    }
}
