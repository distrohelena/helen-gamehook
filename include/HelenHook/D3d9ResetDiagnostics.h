#pragma once

#include <d3d9.h>
#include <optional>
#include <string>

namespace helen {
    /** Reports changes in failed Reset requests without logging identical engine retries.
     * One instance belongs to each device; callers must serialize Record calls.
     */
    class D3d9ResetDiagnostics {
    private:
        /** Last emitted failure description; absence means no outstanding failure. */
        std::optional<std::wstring> LastFailure;

    public:
        /** Returns a failure or recovery message only when diagnostic state changes.
         * Parameters must be the pre-Reset snapshot because D3D may modify the input.
         * Null parameters represent an actual null API argument, not an invented request.
         * No device methods are called and the HRESULT is never altered.
         */
        std::optional<std::wstring> Record(HRESULT result,
            const std::optional<D3DPRESENT_PARAMETERS>& parameters, DWORD callingThread);
    };
}
