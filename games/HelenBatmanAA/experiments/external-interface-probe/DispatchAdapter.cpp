#include "DispatchAdapter.h"
#include <stdexcept>

const FullscreenProbe* DispatchAdapter::Probe = nullptr;

void DispatchAdapter::Bind(const FullscreenProbe& probe) {
    if (Probe != nullptr) { throw std::logic_error("Probe already bound."); }
    Probe = &probe;
}

void __fastcall DispatchAdapter::Dispatch(void* handler, void*, void* movie,
    const char* methodName, const void* arguments, unsigned argumentCount) {
    if (Probe == nullptr || movie == nullptr || handler == nullptr) {
        throw std::logic_error("Dispatch requires binding and borrowed game objects.");
    }
    PrimitiveResult& result = *reinterpret_cast<PrimitiveResult*>(static_cast<unsigned char*>(movie) + 0x9DC);
    if (Probe->TryHandle(methodName, argumentCount, result)) { return; }
    /** Original virtual method uses thiscall, with the handler object in ECX. */
    using OriginalMethod = void (__thiscall*)(void*, void*, const char*, const void*, unsigned);
    const OriginalMethod* table = *static_cast<const OriginalMethod* const*>(handler);
    table[1](handler, movie, methodName, arguments, argumentCount);
}
