#pragma once

#include <HelenHook/BatmanGraphicsConfigService.h>
#include <HelenHook/BatmanGraphicsSession.h>
#include <atomic>

namespace helen {
    /** @brief Serializes direct graphics callbacks and owns their bounded session/transaction lifetime. */
    class BatmanGraphicsSessionService {
    private:
        /** @brief Required parser and writer, also owning process integrity lockout. */
        BatmanGraphicsConfigService& Config;
        /** @brief Required display-rule service used for capture and exact-pair revalidation. */
        BatmanDisplayModeService& Display;
        /** @brief Rejects concurrent/reentrant operations, including attempts during synchronous commit. */
        std::atomic_flag Busy = ATOMIC_FLAG_INIT;
        /** @brief One supported editor; new idle opens retire prior IDs and staged drafts. */
        std::optional<BatmanGraphicsSession> Session;
        /** @brief Issues monotonically increasing identities or fails before uint32 wraparound. */
        std::optional<std::uint32_t> NextIdentity();
        /** @brief Validates a handle while this operation owns Busy. */
        bool Matches(std::uint32_t session) const noexcept;
        /** @brief Validates both handles while this operation owns Busy. */
        bool MatchesTransaction(std::uint32_t session, std::uint32_t transaction) const noexcept;
        /** @brief Selects a genuinely available immutable catalog; invalid flavors return null. */
        const BatmanDisplayCatalog* FindCatalog(BatmanDisplayModeCatalogKind kind) const noexcept;
    public:
        /** @brief Binds initialized required services without performing capture or hook publication. */
        BatmanGraphicsSessionService(BatmanGraphicsConfigService& config, BatmanDisplayModeService& display);
        /** @brief Captures one new editor state; rejects competing operations and invalidates older idle handles. */
        std::optional<std::uint32_t> Open();
        /** @brief Returns a captured scalar during read transfer, including explicit transaction availability. */
        std::optional<int> Get(std::uint32_t session, BatmanGraphicsField field);
        /** @brief Returns count for a captured catalog during read transfer. */
        std::optional<int> ModeCount(std::uint32_t session, BatmanDisplayModeCatalogKind kind);
        /** @brief Returns one captured width after checking session, catalog and index. */
        std::optional<int> ModeWidth(std::uint32_t session, BatmanDisplayModeCatalogKind kind, std::size_t index);
        /** @brief Returns one captured height after checking session, catalog and index. */
        std::optional<int> ModeHeight(std::uint32_t session, BatmanDisplayModeCatalogKind kind, std::size_t index);
        /** @brief Closes primitive transfer without releasing the editor's baseline or catalogs. */
        bool EndRead(std::uint32_t session);
        /** @brief Creates a staged complete draft only after read transfer and when persistence is permitted. */
        std::optional<std::uint32_t> BeginApply(std::uint32_t session);
        /** @brief Edits only the transaction draft; invalid values never change baseline or files. */
        bool SetField(std::uint32_t session, std::uint32_t transaction, BatmanGraphicsField field, int value);
        /** @brief Stages the exact pair from this session's catalog after checking staged Fullscreen. */
        bool SetResolution(std::uint32_t session, std::uint32_t transaction, BatmanDisplayModeCatalogKind kind, std::size_t index);
        /** @brief Consumes the transaction once and returns only after publication/reconciliation has completed. */
        std::optional<BatmanGraphicsApplyResult> Commit(std::uint32_t session, std::uint32_t transaction);
        /** @brief Discards only uncommitted staged edits; completed transactions cannot be canceled. */
        bool CancelApply(std::uint32_t session, std::uint32_t transaction);
        /** @brief Retires an idle editor without persistence; rejected while another entry owns the service. */
        bool Close(std::uint32_t session);
    };
}
