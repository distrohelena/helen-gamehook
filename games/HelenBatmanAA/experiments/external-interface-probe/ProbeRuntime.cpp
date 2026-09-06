#include "ProbeRuntime.h"
#include "DispatchAdapter.h"
#include <HelenHook/Log.h>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace {
    /** Process-lifetime owner, initialized synchronously before the declarative hook is published. */
    std::unique_ptr<FullscreenProbe> Reader;
}

void InitializeBatmanDirectProbe(const std::filesystem::path& launcherIniPath) {
    if (Reader != nullptr) { throw std::logic_error("Direct probe already initialized."); }
    Reader = std::make_unique<FullscreenProbe>(launcherIniPath.wstring());
    DispatchAdapter::Bind(*Reader);
    helen::Logf(L"[direct-probe] reader bound before hook installation ini=%ls", launcherIniPath.c_str());
}

/** Experimental fastcall entry: stock calls retain their original handler; owned-call exceptions stay native. */
extern "C" __declspec(dllexport) void __fastcall HelenProbeDispatch(
    void* handler, void* unusedEdx, void* movie, const char* methodName, const void* arguments, unsigned argumentCount) {
    if (methodName == nullptr || std::strcmp(methodName, "Helen_ProbeGetFullscreen") != 0) {
        DispatchAdapter::Dispatch(handler, unusedEdx, movie, methodName, arguments, argumentCount);
        return;
    }
    try {
        DispatchAdapter::Dispatch(handler, unusedEdx, movie, methodName, arguments, argumentCount);
        const PrimitiveResult& result = *reinterpret_cast<const PrimitiveResult*>(static_cast<unsigned char*>(movie) + 0x9DC);
        helen::Logf(L"[direct-probe] synchronous Fullscreen movie=%p argc=%u type=%u value=%u",
            movie, argumentCount, static_cast<unsigned>(result.Type), static_cast<unsigned>(result.Payload[0]));
    } catch (const std::exception& error) {
        helen::Logf(L"[direct-probe] Fullscreen failed: %hs", error.what());
    } catch (...) {
        helen::Log(L"[direct-probe] Fullscreen failed with an unknown native exception.");
    }
}
