#include "SessionGraphicsOverlay.h"
#include "SessionStagingFile.h"
#include <HelenHook/ExecutableFingerprint.h>
#include <fstream>
#include <stdexcept>

namespace {
    /** @brief Converts ASCII configuration identifiers to the Win32 profile API's Unicode interface. */
    std::wstring Wide(const char* text) { const std::string value(text); return std::wstring(value.begin(),value.end()); }
    /** @brief Encodes only validated protocol values using the existing Boolean/integer INI conventions. */
    std::wstring Encoded(helen::BatmanGraphicsField field, int value) {
        const int encoded = helen::SessionGraphicsDelta::Encode(field,value);
        if (field == helen::BatmanGraphicsField::Msaa || field == helen::BatmanGraphicsField::Physx ||
            field == helen::BatmanGraphicsField::PersistedWidth || field == helen::BatmanGraphicsField::PersistedHeight) {
            return std::to_wstring(encoded);
        }
        return encoded == 1 ? L"True" : L"False";
    }
    /** @brief Requires an existing, untruncated selected key; staging never invents missing engine configuration. */
    std::wstring ProfileValue(const std::filesystem::path& path, helen::BatmanGraphicsField field) {
        wchar_t value[128];
        const DWORD count = GetPrivateProfileStringW(Wide(helen::SessionGraphicsDelta::Section(field)).c_str(),
            Wide(helen::SessionGraphicsDelta::Key(field)).c_str(),L"",value,128,path.c_str());
        if (count == 0 || count >= 127) { throw std::runtime_error("Required session INI key missing or truncated"); }
        return value;
    }
}
namespace helen {
    SessionGraphicsOverlay::SessionGraphicsOverlay(FileWriteRoutingService& routing, std::filesystem::path original)
        : Routing(routing), Original(std::move(original)) {}
    std::filesystem::path SessionGraphicsOverlay::RequirePath() const {
        std::filesystem::path selected;
        for (const FileWriteRoutingService::RouteDiagnostics& route : Routing.GetRouteDiagnostics()) {
            if (std::filesystem::equivalent(route.OriginalPath,Original)) {
                if (!selected.empty() || route.WritePolicy != FileWritePolicy::Redirect || route.ReadPolicy != FileReadPolicy::Redirected ||
                    route.OverlayPath.empty() || !std::filesystem::is_regular_file(route.OverlayPath) ||
                    std::filesystem::equivalent(Original,route.OverlayPath)) {
                    throw std::runtime_error("Session requires exactly one distinct redirected-read/write overlay");
                }
                selected = route.OverlayPath;
            }
        }
        if (selected.empty()) { throw std::runtime_error("Active session route unavailable"); }
        return selected;
    }
    void SessionGraphicsOverlay::Stage(const BatmanGraphicsDraftState& draft, const SessionGraphicsDelta& delta) const {
        if (delta.Fields().empty()) { return; }
        const std::filesystem::path overlay = RequirePath();
        const std::string originalBefore = ReadBytes(Original);
        const std::string overlayBefore = ReadBytes(overlay);
        const SessionStagingFile staged(overlay);
        if (ReadBytes(staged.GetPath()) != overlayBefore) { throw std::runtime_error("Session overlay changed during staging capture"); }
        for (const BatmanGraphicsField field : delta.Fields()) {
            (void)ProfileValue(staged.GetPath(),field);
            const std::wstring value = Encoded(field,draft.Get(field));
            if (!WritePrivateProfileStringW(Wide(SessionGraphicsDelta::Section(field)).c_str(),
                Wide(SessionGraphicsDelta::Key(field)).c_str(),value.c_str(),staged.GetPath().c_str())) {
                throw std::runtime_error("Private session INI staging failed");
            }
        }
        // This API deliberately returns zero for its cache-flush operation (not a write-success Boolean).
        // Verify the resulting file below instead of treating that documented zero as a failed write.
        (void)WritePrivateProfileStringW(nullptr,nullptr,nullptr,staged.GetPath().c_str());
        for (const BatmanGraphicsField field : delta.Fields()) {
            if (_wcsicmp(ProfileValue(staged.GetPath(),field).c_str(),Encoded(field,draft.Get(field)).c_str()) != 0) {
                throw std::runtime_error("Private session INI verification failed");
            }
        }
        const std::string target = ReadBytes(staged.GetPath());
        // Routing serializes the replacement against every routed open and refuses outstanding handles.
        // The original destination is deliberately passed to routing, never to native ReplaceFile.
        if (ReadBytes(Original) != originalBefore || ReadBytes(overlay) != overlayBefore) {
            throw std::runtime_error("Original or session INI changed before publication");
        }
        if (!Routing.PublishSessionReplacement(Original,overlayBefore,staged.GetPath())) {
            throw std::runtime_error("Session overlay publication refused: " + std::to_string(GetLastError()));
        }
        if (ReadBytes(overlay) != target || ReadBytes(Original) != originalBefore) {
            throw std::runtime_error("Published session or original bytes failed verification");
        }
    }
    std::string SessionGraphicsOverlay::ReadBytes(const std::filesystem::path& path) {
        std::ifstream stream(path,std::ios::binary);
        if (!stream) { throw std::runtime_error("Session integrity file open failed"); }
        const std::string bytes((std::istreambuf_iterator<char>(stream)),std::istreambuf_iterator<char>());
        if (stream.bad()) { throw std::runtime_error("Session integrity file read failed"); }
        return bytes;
    }
}
