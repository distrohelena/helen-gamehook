#include "BatmanGraphicsRuntimeContext.h"

#include <utility>

namespace helen {
    BatmanGraphicsRuntimeContext::BatmanGraphicsRuntimeContext(const std::filesystem::path& engineIniPath)
        : BatmanGraphicsRuntimeContext(engineIniPath, nullptr) {
    }

    BatmanGraphicsRuntimeContext::BatmanGraphicsRuntimeContext(const std::filesystem::path& engineIniPath,
        std::shared_ptr<FileWriteRoutingService> routing_service)
        : Display(), Config(engineIniPath, Display, std::move(routing_service)), Sessions(Config, Display), Adapter(Sessions) {
    }

    void BatmanGraphicsRuntimeContext::Handle(const char* name, const void* arguments, unsigned count,
        BatmanGraphicsPrimitiveResult& result) {
        Adapter.TryHandle(name, arguments, count, result);
    }
}
