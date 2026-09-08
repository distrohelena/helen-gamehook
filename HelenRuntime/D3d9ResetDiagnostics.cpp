#include <HelenHook/D3d9ResetDiagnostics.h>
#include <iomanip>
#include <locale>
#include <sstream>

namespace helen {
    std::optional<std::wstring> D3d9ResetDiagnostics::Record(HRESULT result,
        const std::optional<D3DPRESENT_PARAMETERS>& parameters, DWORD callingThread) {
        if (SUCCEEDED(result)) {
            if (!LastFailure) { return std::nullopt; }
            LastFailure.reset();
            return L"recovered after failed Reset; duplicate suppression cleared";
        }

        std::wostringstream message;
        message.imbue(std::locale::classic());
        message << L"failed hr=0x" << std::hex << std::setw(8) << std::setfill(L'0')
            << static_cast<unsigned long>(result) << std::dec << L" thread=" << callingThread;
        if (!parameters) {
            message << L" parameters=null";
        } else {
            const D3DPRESENT_PARAMETERS& requested = *parameters;
            message << L" width=" << requested.BackBufferWidth << L" height=" << requested.BackBufferHeight
                << L" format=" << requested.BackBufferFormat << L" buffers=" << requested.BackBufferCount
                << L" multisample=" << requested.MultiSampleType << L" quality=" << requested.MultiSampleQuality
                << L" swapEffect=" << requested.SwapEffect << L" hwnd=" << requested.hDeviceWindow
                << L" windowed=" << requested.Windowed << L" autoDepth=" << requested.EnableAutoDepthStencil
                << L" depthFormat=" << requested.AutoDepthStencilFormat << L" flags=" << requested.Flags
                << L" refresh=" << requested.FullScreen_RefreshRateInHz << L" interval=" << requested.PresentationInterval;
        }
        std::wstring failure = message.str();
        if (LastFailure && *LastFailure == failure) { return std::nullopt; }
        LastFailure = failure;
        return failure;
    }
}
