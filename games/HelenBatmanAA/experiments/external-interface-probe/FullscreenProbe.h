#pragma once
#include "PrimitiveResult.h"
#include <string>

/** Isolated read-only prototype; never installed by the shipping pack or linked into HelenGameHook. */
class FullscreenProbe {
private:
    /** Explicit launcher INI path; never inferred from the process working directory. */
    std::wstring IniPath;
public:
    /** Requires a nonempty absolute path to the launcher-owned UserEngine.ini. */
    explicit FullscreenProbe(std::wstring iniPath);
    /** Handles only the zero-argument probe name; failed reads remain undefined, never fabricated Off. */
    bool TryHandle(const char* methodName, unsigned argumentCount, PrimitiveResult& result) const;
};
