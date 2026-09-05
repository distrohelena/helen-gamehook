#include <HelenHook/BatmanGraphicsSessionService.h>
#include <HelenHook/BatmanGraphicsOperationGuard.h>
#include <HelenHook/Log.h>

#include <limits>
#include <mutex>

namespace {
    /** @brief Serializes the short identity allocation only; never held across callbacks, I/O, or session operations. */
    std::mutex IdentityMutex;
    /** @brief Process-wide identity source shared even by replacement service instances; zero is never issued. */
    std::uint32_t LastIdentity = 0;
}

namespace helen {
    BatmanGraphicsSessionService::BatmanGraphicsSessionService(BatmanGraphicsConfigService& config, BatmanDisplayModeService& display)
        : Config(config), Display(display) {
    }

    std::optional<std::uint32_t> BatmanGraphicsSessionService::NextIdentity() {
        const std::lock_guard<std::mutex> lock(IdentityMutex);
        if (LastIdentity == std::numeric_limits<std::uint32_t>::max()) {
            return std::nullopt;
        }
        return ++LastIdentity;
    }

    bool BatmanGraphicsSessionService::Matches(std::uint32_t session) const noexcept {
        return Session.has_value() && Session->Id == session;
    }

    bool BatmanGraphicsSessionService::MatchesTransaction(std::uint32_t session, std::uint32_t transaction) const noexcept {
        return Matches(session) && Session->Transaction.has_value() && Session->Transaction->Id == transaction;
    }

    const BatmanDisplayCatalog* BatmanGraphicsSessionService::FindCatalog(BatmanDisplayModeCatalogKind kind) const noexcept {
        if (!Session.has_value()) {
            return nullptr;
        } else if (kind == BatmanDisplayModeCatalogKind::Windowed) {
            return Session->Windowed.has_value() ? &*Session->Windowed : nullptr;
        } else if (kind == BatmanDisplayModeCatalogKind::Fullscreen) {
            return Session->Fullscreen.has_value() ? &*Session->Fullscreen : nullptr;
        }
        return nullptr;
    }

    std::optional<std::uint32_t> BatmanGraphicsSessionService::Open() {
        const BatmanGraphicsOperationGuard guard(Busy);
        if (!guard.IsAcquired()) {
            return std::nullopt;
        }
        const std::optional<std::uint32_t> identity = NextIdentity();
        if (!identity.has_value()) {
            return std::nullopt;
        }
        Session.reset();
        BatmanGraphicsSnapshot snapshot = Config.CaptureReadSnapshot();
        std::optional<BatmanDisplayCatalog> windowed;
        std::optional<BatmanDisplayCatalog> fullscreen;
        const std::optional<int> width = snapshot.Get(BatmanGraphicsField::PersistedWidth);
        const std::optional<int> height = snapshot.Get(BatmanGraphicsField::PersistedHeight);
        if (width.has_value() && height.has_value()) {
            try {
                windowed = Display.CaptureCatalog(BatmanDisplayModeCatalogKind::Windowed, *width, *height);
            } catch (...) {
                Logf(L"[graphics] Windowed catalog capture failed with exception.");
            }
            try {
                fullscreen = Display.CaptureCatalog(BatmanDisplayModeCatalogKind::Fullscreen, *width, *height);
            } catch (...) {
                Logf(L"[graphics] Fullscreen catalog capture failed with exception.");
            }
        }
        Session.emplace(*identity, std::move(snapshot), std::move(windowed), std::move(fullscreen));
        return identity;
    }

    std::optional<int> BatmanGraphicsSessionService::Get(std::uint32_t session, BatmanGraphicsField field) {
        const BatmanGraphicsOperationGuard guard(Busy);
        if (!guard.IsAcquired() || !Matches(session) || !Session->ReadOpen) {
            return std::nullopt;
        }
        if (field == BatmanGraphicsField::CanApply) {
            return Session->Baseline.has_value() && !Config.IsApplyLocked() ? 1 : 0;
        } else if (field == BatmanGraphicsField::DesktopWidth || field == BatmanGraphicsField::DesktopHeight) {
            const BatmanDisplayCatalog* catalog = FindCatalog(BatmanDisplayModeCatalogKind::Fullscreen);
            if (catalog == nullptr) {
                return std::nullopt;
            }
            return field == BatmanGraphicsField::DesktopWidth ? catalog->GetDesktopMode().GetWidth() : catalog->GetDesktopMode().GetHeight();
        }
        return Session->Snapshot.Get(field);
    }

    std::optional<int> BatmanGraphicsSessionService::ModeCount(std::uint32_t session, BatmanDisplayModeCatalogKind kind) {
        const BatmanGraphicsOperationGuard guard(Busy);
        if (!guard.IsAcquired() || !Matches(session) || !Session->ReadOpen) {
            return std::nullopt;
        }
        const BatmanDisplayCatalog* catalog = FindCatalog(kind);
        return catalog != nullptr ? std::optional<int>(static_cast<int>(catalog->GetModes().size())) : std::nullopt;
    }

    std::optional<int> BatmanGraphicsSessionService::ModeWidth(std::uint32_t session, BatmanDisplayModeCatalogKind kind, std::size_t index) {
        const BatmanGraphicsOperationGuard guard(Busy);
        if (!guard.IsAcquired() || !Matches(session) || !Session->ReadOpen) {
            return std::nullopt;
        }
        const BatmanDisplayCatalog* catalog = FindCatalog(kind);
        return catalog != nullptr && index < catalog->GetModes().size() ?
            std::optional<int>(catalog->GetModes()[index].GetWidth()) : std::nullopt;
    }

    std::optional<int> BatmanGraphicsSessionService::ModeHeight(std::uint32_t session, BatmanDisplayModeCatalogKind kind, std::size_t index) {
        const BatmanGraphicsOperationGuard guard(Busy);
        if (!guard.IsAcquired() || !Matches(session) || !Session->ReadOpen) {
            return std::nullopt;
        }
        const BatmanDisplayCatalog* catalog = FindCatalog(kind);
        return catalog != nullptr && index < catalog->GetModes().size() ?
            std::optional<int>(catalog->GetModes()[index].GetHeight()) : std::nullopt;
    }

    bool BatmanGraphicsSessionService::EndRead(std::uint32_t session) {
        const BatmanGraphicsOperationGuard guard(Busy);
        if (!guard.IsAcquired() || !Matches(session) || !Session->ReadOpen) {
            return false;
        }
        Session->ReadOpen = false;
        return true;
    }

    std::optional<std::uint32_t> BatmanGraphicsSessionService::BeginApply(std::uint32_t session) {
        const BatmanGraphicsOperationGuard guard(Busy);
        if (!guard.IsAcquired() || !Matches(session) || Session->ReadOpen || Session->Transaction.has_value() ||
            !Session->Baseline.has_value() || Config.IsApplyLocked()) {
            return std::nullopt;
        }
        const std::optional<std::uint32_t> identity = NextIdentity();
        if (!identity.has_value()) {
            return std::nullopt;
        }
        Session->Transaction.emplace(*identity, *Session->Baseline);
        return identity;
    }

    bool BatmanGraphicsSessionService::SetField(std::uint32_t session, std::uint32_t transaction, BatmanGraphicsField field, int value) {
        const BatmanGraphicsOperationGuard guard(Busy);
        if (!guard.IsAcquired() || !MatchesTransaction(session, transaction)) {
            return false;
        }
        if (field == BatmanGraphicsField::Fullscreen && Session->Transaction->Resolution.has_value() &&
            value != static_cast<int>(Session->Transaction->Resolution->Kind)) {
            return false;
        }
        return Session->Transaction->Draft.TrySet(field, value);
    }

    bool BatmanGraphicsSessionService::SetResolution(std::uint32_t session, std::uint32_t transaction,
        BatmanDisplayModeCatalogKind kind, std::size_t index) {
        const BatmanGraphicsOperationGuard guard(Busy);
        if (!guard.IsAcquired() || !MatchesTransaction(session, transaction)) {
            return false;
        }
        const BatmanDisplayCatalog* catalog = FindCatalog(kind);
        if (catalog == nullptr || index >= catalog->GetModes().size() ||
            Session->Transaction->Draft.Get(BatmanGraphicsField::Fullscreen) != static_cast<int>(kind)) {
            return false;
        }
        const BatmanDisplayMode& mode = catalog->GetModes()[index];
        if (!Session->Transaction->Draft.TrySetResolution(mode.GetWidth(), mode.GetHeight())) {
            return false;
        }
        Session->Transaction->Resolution.emplace(kind, index);
        return true;
    }

    std::optional<BatmanGraphicsApplyResult> BatmanGraphicsSessionService::Commit(std::uint32_t session, std::uint32_t transaction) {
        const BatmanGraphicsOperationGuard guard(Busy);
        if (!guard.IsAcquired() || !MatchesTransaction(session, transaction)) {
            return std::nullopt;
        }
        BatmanGraphicsTransaction attempted = std::move(*Session->Transaction);
        Session->Transaction.reset();
        if (Config.IsApplyLocked()) {
            return BatmanGraphicsApplyResult(BatmanGraphicsApplyOutcome::IntegrityUncertain, {});
        }
        if (attempted.Draft.Get(BatmanGraphicsField::Fullscreen) != Session->Baseline->Get(BatmanGraphicsField::Fullscreen) &&
            !attempted.Resolution.has_value()) {
            return BatmanGraphicsApplyResult(BatmanGraphicsApplyOutcome::NotApplied, {});
        }
        try {
            if (attempted.Resolution.has_value()) {
                const BatmanDisplayCatalog* catalog = FindCatalog(attempted.Resolution->Kind);
                if (catalog == nullptr || !Display.RevalidateMode(*catalog, attempted.Resolution->Index).has_value()) {
                    return BatmanGraphicsApplyResult(BatmanGraphicsApplyOutcome::NotApplied, {});
                }
            }
            BatmanGraphicsApplyResult result = Config.ApplyDraft(attempted.Draft);
            if (result.Outcome == BatmanGraphicsApplyOutcome::Committed || result.Outcome == BatmanGraphicsApplyOutcome::CommittedCleanupFailed) {
                Session->Baseline = attempted.Draft;
                BatmanGraphicsSnapshot::Values values;
                for (std::size_t index = 0; index < values.size(); ++index) {
                    values[index] = attempted.Draft.Get(static_cast<BatmanGraphicsField>(index));
                }
                Session->Snapshot = BatmanGraphicsSnapshot(std::move(values));
            }
            return result;
        } catch (...) {
            Logf(L"[graphics] Direct Commit failed with exception; integrity lock=%d.", Config.IsApplyLocked() ? 1 : 0);
            return BatmanGraphicsApplyResult(Config.IsApplyLocked() ? BatmanGraphicsApplyOutcome::IntegrityUncertain :
                BatmanGraphicsApplyOutcome::NotApplied, {});
        }
    }

    bool BatmanGraphicsSessionService::CancelApply(std::uint32_t session, std::uint32_t transaction) {
        const BatmanGraphicsOperationGuard guard(Busy);
        if (!guard.IsAcquired() || !MatchesTransaction(session, transaction)) {
            return false;
        }
        Session->Transaction.reset();
        return true;
    }

    bool BatmanGraphicsSessionService::Close(std::uint32_t session) {
        const BatmanGraphicsOperationGuard guard(Busy);
        if (!guard.IsAcquired() || !Matches(session)) {
            return false;
        }
        Session.reset();
        return true;
    }
}
