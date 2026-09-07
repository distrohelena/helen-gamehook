#include "BatmanGraphicsRuntime.h"
#include "BatmanGraphicsExternalInterface.h"
#include "BatmanGraphicsRuntimeContext.h"
#include <HelenHook/Log.h>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {
    /** @brief Atomic publication protects acquisition only; an acquired context remains alive through dispatch and reconciliation. */
    std::atomic<std::shared_ptr<helen::BatmanGraphicsRuntimeContext>> Context;
}

namespace helen {
    void InitializeBatmanGraphicsRuntime(const std::filesystem::path& engineIniPath) {
        InitializeBatmanGraphicsRuntime(engineIniPath, nullptr);
    }

    void InitializeBatmanGraphicsRuntime(const std::filesystem::path& engineIniPath,
        std::shared_ptr<FileWriteRoutingService> routing_service) {
        const std::shared_ptr<BatmanGraphicsRuntimeContext> initialized =
            std::make_shared<BatmanGraphicsRuntimeContext>(engineIniPath, std::move(routing_service));
        std::shared_ptr<BatmanGraphicsRuntimeContext> expected;
        if (!Context.compare_exchange_strong(expected, initialized)) {
            throw std::logic_error("Direct graphics runtime is already bound.");
        }
        Logf(L"[graphics-direct] runtime bound before hook publication ini=%ls", engineIniPath.c_str());
    }
    void ResetBatmanGraphicsRuntime() {
        Context.store({});
    }
}

extern "C" __declspec(dllexport) void __fastcall HelenGraphicsDispatch(
    void* handler, void*, void* movie, const char* name, const void* arguments, unsigned count) noexcept(false) {
    if (!helen::BatmanGraphicsExternalInterface::Owns(name)) {
        helen::BatmanGraphicsExternalInterface::ForwardStock(handler, movie, name, arguments, count);
        return;
    }
    const std::shared_ptr<helen::BatmanGraphicsRuntimeContext> context = Context.load();
    if (!context || movie == nullptr) {
        helen::Log(L"[graphics-direct] rejected owned dispatch without a bound runtime or movie.");
        return;
    }
    /** @brief Engine-owned pre-cleared GAS primitive slot verified against the pinned retail executable. */
    helen::BatmanGraphicsPrimitiveResult& result = *reinterpret_cast<helen::BatmanGraphicsPrimitiveResult*>(
        static_cast<unsigned char*>(movie) + 0x9DC);
    context->Handle(name, arguments, count, result);
}
