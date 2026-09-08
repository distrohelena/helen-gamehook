#include <HelenHook/D3d9ResetDiagnostics.h>
#include <stdexcept>

namespace {
    /** Stops the console suite when a diagnostic contract is violated. */
    void Expect(bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    }
}

/** Covers missing diagnostics, retry spam, lost request fields, and stale per-device suppression. */
void RunD3d9ResetDiagnosticsTests() {
    helen::D3d9ResetDiagnostics diagnostics;
    D3DPRESENT_PARAMETERS request{};
    request.BackBufferWidth = 2560;
    request.BackBufferHeight = 1440;
    request.BackBufferFormat = D3DFMT_X8R8G8B8;
    request.BackBufferCount = 1;
    request.MultiSampleType = D3DMULTISAMPLE_4_SAMPLES;
    request.MultiSampleQuality = 2;
    request.SwapEffect = D3DSWAPEFFECT_DISCARD;
    request.Windowed = TRUE;
    request.EnableAutoDepthStencil = TRUE;
    request.AutoDepthStencilFormat = D3DFMT_D24S8;
    request.Flags = D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;
    request.FullScreen_RefreshRateInHz = 0;
    request.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    Expect(!diagnostics.Record(D3D_OK, request, 123), "Successful Reset must not emit a failure.");
    const std::optional<std::wstring> first = diagnostics.Record(D3DERR_INVALIDCALL, request, 123);
    Expect(first.has_value(), "Failed Reset must emit its HRESULT and request.");
    for (const wchar_t* field : {L"hr=0x8876086c", L"thread=123", L"width=2560", L"height=1440",
        L"format=22", L"buffers=1", L"multisample=4", L"quality=2", L"swapEffect=1",
        L"windowed=1", L"autoDepth=1", L"depthFormat=75", L"flags=2", L"refresh=0", L"interval=1"}) {
        Expect(first->find(field) != std::wstring::npos, "Failure diagnostic lost a request field.");
    }
    for (int retry = 0; retry < 100; ++retry) {
        Expect(!diagnostics.Record(D3DERR_INVALIDCALL, request, 123), "Identical retries must not spam the log.");
    }
    Expect(diagnostics.Record(D3DERR_DEVICELOST, request, 123).has_value(), "Changed HRESULT must be reported.");
    request.BackBufferWidth = 1920;
    Expect(diagnostics.Record(D3DERR_DEVICELOST, request, 123).has_value(), "Changed request must be reported.");
    Expect(diagnostics.Record(D3DERR_DEVICELOST, request, 456).has_value(), "Changed thread must be reported.");
    const auto missing = diagnostics.Record(D3DERR_INVALIDCALL, std::nullopt, 456);
    Expect(missing && missing->find(L"parameters=null") != std::wstring::npos, "Null input must remain explicit.");
    Expect(diagnostics.Record(D3D_OK, request, 456).has_value(), "Recovery must be reported once.");
    Expect(!diagnostics.Record(D3D_OK, request, 456), "Repeated success must remain silent.");
    Expect(diagnostics.Record(D3DERR_INVALIDCALL, request, 456).has_value(), "Recovery must rearm failure logging.");
    helen::D3d9ResetDiagnostics otherDevice;
    Expect(otherDevice.Record(D3DERR_INVALIDCALL, request, 456).has_value(), "Devices must not share suppression state.");
}
