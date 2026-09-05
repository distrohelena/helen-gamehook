#include "BatmanGraphicsRuntimeContext.h"

namespace helen {
    BatmanGraphicsRuntimeContext::BatmanGraphicsRuntimeContext(const std::filesystem::path& engineIniPath)
        : Display(), Config(engineIniPath, Display), Sessions(Config, Display), Adapter(Sessions) {
    }

    void BatmanGraphicsRuntimeContext::Handle(const char* name, const void* arguments, unsigned count,
        BatmanGraphicsPrimitiveResult& result) {
        Adapter.TryHandle(name, arguments, count, result);
    }
}
