#include <HelenHook/D3d9TextureBindingTransaction.h>
#include <HelenHook/Log.h>
#include <stdexcept>
#include <utility>

namespace helen {
    D3d9TextureBindingTransaction::D3d9TextureBindingTransaction(IDirect3DDevice9& device,
        std::shared_ptr<IDirect3DTexture9> replacement, GetTextureFunction get_texture, SetTextureFunction set_texture,
        RevisionFunction read_revision)
        : Device(device), Replacement(std::move(replacement)), GetTexture(get_texture), SetTexture(set_texture), ReadRevision(read_revision) {
        if (!Replacement || GetTexture == nullptr || SetTexture == nullptr || ReadRevision == nullptr) {
            throw std::invalid_argument("Binding transaction requires a replacement and original dispatch.");
        }
    }

    D3d9TextureBindingTransaction::~D3d9TextureBindingTransaction() {
        if (Committed) { return; }
        for (std::size_t index = StageCount; index > 0; --index) {
            const DWORD stage = static_cast<DWORD>(index - 1);
            if (!Changed[stage]) { continue; }
            Microsoft::WRL::ComPtr<IDirect3DBaseTexture9> current;
            const std::uint64_t revision = ReadRevision(Device);
            HRESULT result = GetTexture(&Device, stage, current.GetAddressOf());
            if (SUCCEEDED(result) && revision == ReadRevision(Device) && current.Get() == Replacement.get()) {
                result = SetTexture(&Device, stage, Previous[stage].Get());
            }
            if (FAILED(result)) {
                Logf(L"[d3d9] failed cache binding rollback stage=%lu hr=0x%08lX",
                    static_cast<unsigned long>(stage), static_cast<unsigned long>(result));
            }
        }
    }

    HRESULT D3d9TextureBindingTransaction::Apply(IDirect3DTexture9* source) {
        for (DWORD stage = 0; stage < StageCount; ++stage) {
            Microsoft::WRL::ComPtr<IDirect3DBaseTexture9> bound;
            const std::uint64_t revision = ReadRevision(Device);
            HRESULT result = GetTexture(&Device, stage, bound.GetAddressOf());
            if (FAILED(result)) { return result; }
            if (bound.Get() != source || revision != ReadRevision(Device)) { continue; }
            Previous[stage] = std::move(bound);
            Changed[stage] = true;
            result = SetTexture(&Device, stage, Replacement.get());
            if (FAILED(result)) { return result; }
        }
        return S_OK;
    }

    void D3d9TextureBindingTransaction::Commit() noexcept {
        Committed = true;
    }
}
