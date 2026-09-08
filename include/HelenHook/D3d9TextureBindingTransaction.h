#pragma once
#include <d3d9.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>
#include <memory>

namespace helen {
    /** Rolls back only this cache attempt's surviving stage bindings if publication fails.
     * Caller owns the device and holds no registry/dispatch lock during the transaction.
     */
    class D3d9TextureBindingTransaction {
    public:
        /** Original driver query, resolved before entering external calls. */
        using GetTextureFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9**);
        /** Original driver bind, resolved before entering external calls. */
        using SetTextureFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*);
        /** Reads application binding/reset revision without holding a lock across driver calls. */
        using RevisionFunction = std::uint64_t(*)(IDirect3DDevice9&);
    private:
        /** Pixel texture stages inspected by the existing replacement cache. */
        static constexpr std::size_t StageCount = 16;
        /** Required device lifetime is held by the caller throughout rollback. */
        IDirect3DDevice9& Device;
        /** Shared ownership prevents publication failure from freeing the bound candidate early. */
        std::shared_ptr<IDirect3DTexture9> Replacement;
        /** Original query used for inspection and conditional rollback. */
        GetTextureFunction GetTexture;
        /** Original bind used for application and conditional rollback. */
        SetTextureFunction SetTexture;
        /** Detects reentrant application mutations while a driver query is in progress. */
        RevisionFunction ReadRevision;
        /** Owned previous references survive until a failed attempt restores its stages. */
        std::array<Microsoft::WRL::ComPtr<IDirect3DBaseTexture9>, StageCount> Previous;
        /** Marks attempted stage changes, including drivers that mutate before reporting failure. */
        std::array<bool, StageCount> Changed{};
        /** Successful publication transfers stage responsibility to the persistent cache. */
        bool Committed = false;
    public:
        /** Stores required device/resource ownership and already-resolved driver methods. */
        D3d9TextureBindingTransaction(IDirect3DDevice9& device, std::shared_ptr<IDirect3DTexture9> replacement,
            GetTextureFunction get_texture, SetTextureFunction set_texture, RevisionFunction read_revision);
        /** Conditionally rolls back unpublished stages; any rollback failure is explicitly logged. */
        ~D3d9TextureBindingTransaction();
        /** A stage transaction cannot be copied because rollback has one responsible owner. */
        D3d9TextureBindingTransaction(const D3d9TextureBindingTransaction&) = delete;
        /** Assignment cannot transfer active rollback responsibility implicitly. */
        D3d9TextureBindingTransaction& operator=(const D3d9TextureBindingTransaction&) = delete;
        /** Rebinds stages currently containing source; keeps prior references for failure rollback. */
        HRESULT Apply(IDirect3DTexture9* source);
        /** Marks successful publication; subsequent destruction only releases temporary references. */
        void Commit() noexcept;
    };
}
