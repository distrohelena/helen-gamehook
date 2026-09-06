#pragma once
#include "FullscreenProbe.h"

/** Prototype x86 adapter for the inspected virtual-call boundary; contains no installer or polling worker. */
class DispatchAdapter {
private:
    /** Required process-lifetime reader, bound before any calls can reach the adapter. */
    static const FullscreenProbe* Probe;
public:
    /** Binds exactly once; caller retains ownership for the entire period the adapter can execute. */
    static void Bind(const FullscreenProbe& probe);
    /** Matches ECX=this plus four stack arguments; EDX is ignored and the callee removes sixteen bytes. */
    static void __fastcall Dispatch(void* handler, void* unusedEdx, void* movie,
        const char* methodName, const void* arguments, unsigned argumentCount);
};
