#include "NoSaveResolutionProbe.h"
#include <HelenHook/ExecutableFingerprint.h>
#include <HelenHook/Log.h>
#include <windows.h>
#include <d3d9.h>
#include <atomic>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace {
    /** @brief A consumed experimental invocation cannot be retried in an uncertain device state. */
    std::atomic_flag Attempted = ATOMIC_FLAG_INIT;

    /** @brief Copy a required engine field without dereferencing an unchecked process pointer. */
    template<typename T> T Read(std::uintptr_t address) {
        T value;
        SIZE_T copied = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), &value, sizeof(value), &copied) || copied != sizeof(value)) {
            throw std::runtime_error("Probe cannot read required engine state");
        }
        return value;
    }

    /** @brief Fail before invocation when loaded instructions differ from the pinned build. */
    template<std::size_t N> void RequireBytes(std::uintptr_t address, const unsigned char (&expected)[N]) {
        unsigned char actual[N];
        SIZE_T copied = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), actual, N, &copied) ||
            copied != N || std::memcmp(actual, expected, N) != 0) {
            throw std::runtime_error("Probe executable instruction contract mismatch");
        }
    }

    /** @brief Verify on-disk identity once; only the pinned executable may use module-relative bindings. */
    std::uintptr_t VerifiedModule() {
        wchar_t path[32768];
        const DWORD length = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
        if (length == 0 || length >= std::size(path)) {
            throw std::runtime_error("Probe executable path unavailable");
        }
        const helen::ExecutableFingerprint fingerprint = helen::ExecutableFingerprint::FromPath(path);
        if (fingerprint.FileSize != 38758728 || fingerprint.Sha256 != "4dac1f5e2ac6710b7378fdce74601f616f4753e3756cb5fda63c7519cc2eb028") {
            throw std::runtime_error("Probe rejects foreign executable");
        }
        const std::uintptr_t module = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        const unsigned char resize[] = {0x83,0xEC,0x28,0x55,0x56,0x8B,0xF1,0x33,0xED,0x39,0xAE,0x80,0,0,0};
        RequireBytes(module + 0xAB91D0, resize);
        const unsigned char thread[] = {0x83,0x3D,0xE8,0x60,0x67,0x02,0x00,0x74,0x11,0xFF,0x15,0xB4,0x92,0xE2,0x01};
        // Absolute instruction operands require preferred-base loading in this throwaway proof.
        // Production support must validate relocated operands, not silently accept another layout.
        if (module != 0x400000) {
            throw std::runtime_error("Probe currently requires preferred-base loading");
        }
        RequireBytes(module + 0x315880, thread);
        return module;
    }

    /** @brief Release the temporary COM reference acquired only to observe backbuffer dimensions. */
    void ReleaseSurface(IDirect3DSurface9* surface) {
        surface->Release();
    }

    /** @brief Log client, engine viewport, and actual D3D backbuffer sizes without resizing or saving. */
    void Observe(std::uintptr_t module, std::uintptr_t owner, HWND window, const wchar_t* phase) {
        RECT client;
        if (!GetClientRect(window, &client)) {
            throw std::runtime_error("Probe client dimensions unavailable");
        }
        const std::uintptr_t renderer = Read<std::uintptr_t>(module + 0x22B0D94);
        IDirect3DDevice9* device = Read<IDirect3DDevice9*>(renderer + 0x10);
        if (device == nullptr) {
            throw std::runtime_error("Probe D3D device unavailable");
        }
        IDirect3DSurface9* raw = nullptr;
        const HRESULT acquired = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &raw);
        if (FAILED(acquired) || raw == nullptr) {
            throw std::runtime_error("Probe backbuffer unavailable");
        }
        const std::unique_ptr<IDirect3DSurface9, decltype(&ReleaseSurface)> surface(raw, ReleaseSurface);
        D3DSURFACE_DESC description;
        if (FAILED(surface->GetDesc(&description))) {
            throw std::runtime_error("Probe backbuffer description unavailable");
        }
        helen::Logf(L"[resolution-no-save] %ls client=%ldx%ld viewport=%ux%u backbuffer=%ux%u fullscreen=%u",
            phase, client.right-client.left, client.bottom-client.top,
            Read<unsigned>(owner+0x4C), Read<unsigned>(owner+0x50), description.Width, description.Height,
            Read<unsigned>(owner+0x58)&1);
    }
}

namespace helen {
    BatmanGraphicsApplyResult NoSaveResolutionProbe::Apply(const BatmanGraphicsDraftState& draft) {
        Log(L"[resolution-no-save] EXPERIMENT: Helen INI writer bypassed; engine persistence untouched.");
        try {
            static const std::uintptr_t module = VerifiedModule();
            if (draft.Get(BatmanGraphicsField::Fullscreen) != 0) {
                throw std::runtime_error("Probe supports only requested windowed mode");
            }
            if (Read<unsigned>(module+0x22760E8) == 0 || Read<DWORD>(module+0x22760E4) != GetCurrentThreadId()) {
                throw std::runtime_error("Probe requires initialized game thread");
            }
            // One live viewport avoids ambiguous ownership and device-wide maximum-size behavior.
            if (Read<int>(module+0x22CCAB4) != 1) {
                throw std::runtime_error("Probe requires exactly one registered viewport");
            }
            const std::uintptr_t entries = Read<std::uintptr_t>(module+0x22CCAB0);
            const std::uintptr_t owner = Read<std::uintptr_t>(entries);
            if (Read<std::uintptr_t>(owner) != module+0x1D2C408 || Read<std::uintptr_t>(owner+4) != module+0x1D2C380 ||
                Read<std::uintptr_t>(module+0x1D2C40C) != module+0xAB91D0) {
                throw std::runtime_error("Probe viewport type or resize entry mismatch");
            }
            const HWND window = Read<HWND>(owner+0x60);
            DWORD processId = 0;
            const DWORD windowThread = GetWindowThreadProcessId(window, &processId);
            if (!IsWindow(window) || processId != GetCurrentProcessId() || windowThread != GetCurrentThreadId() ||
                Read<std::uintptr_t>(owner+0x64) != 0 || Read<unsigned>(owner+0x80) != 0 || (Read<unsigned>(owner+0x58)&1) != 0) {
                throw std::runtime_error("Probe requires idle top-level windowed viewport on its owning thread");
            }
            const std::uintptr_t renderer = Read<std::uintptr_t>(module+0x22B0D94);
            if (Read<std::uintptr_t>(renderer) != module+0x1D28210 || Read<std::uintptr_t>(renderer+0x24) != 0) {
                throw std::runtime_error("Probe renderer unavailable or actively drawing a viewport");
            }
            const int width = draft.Get(BatmanGraphicsField::PersistedWidth);
            const int height = draft.Get(BatmanGraphicsField::PersistedHeight);
            if (width <= 0 || height <= 0 || (Read<unsigned>(owner+0x4C) == static_cast<unsigned>(width) &&
                Read<unsigned>(owner+0x50) == static_cast<unsigned>(height))) {
                throw std::runtime_error("Probe requires a different positive resolution");
            }
            if (Attempted.test_and_set()) {
                throw std::runtime_error("Probe already attempted; restart before another experiment");
            }
            Observe(module, owner, window, L"BEFORE");
            Logf(L"[resolution-no-save] CALL engine width=%d height=%d fullscreen=0; no Helen save follows.", width, height);
            SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
            /** @brief Verified x86 thiscall with width, height, fullscreen, x, y; existing-window path retains position. */
            using Resize = void(__thiscall*)(void*, unsigned, unsigned, int, int, int);
            const Resize resize = reinterpret_cast<Resize>(module+0xAB91D0);
            resize(reinterpret_cast<void*>(owner), static_cast<unsigned>(width), static_cast<unsigned>(height), 0, -1, -1);
            if (Read<int>(module+0x22CCAB4) != 1 || Read<std::uintptr_t>(Read<std::uintptr_t>(module+0x22CCAB0)) != owner ||
                Read<HWND>(owner+0x60) != window || !IsWindow(window)) {
                throw std::runtime_error("Probe viewport lifetime changed during resize");
            }
            Observe(module, owner, window, L"AFTER");
            Log(L"[resolution-no-save] RETURNED; compare INI snapshots now and after exit. UI Apply Failed is intentional; no saved success is asserted.");
        } catch (const std::exception& error) {
            Logf(L"[resolution-no-save] STOP: %hs; Helen writer remains bypassed.", error.what());
        }
        return BatmanGraphicsApplyResult(BatmanGraphicsApplyOutcome::NotApplied, {});
    }
}
