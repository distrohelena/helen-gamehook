#include <HelenHook/FileWriteRoutingTransaction.h>

#include <HelenHook/FileWriteRoutingService.h>

namespace helen {
FileWriteRoutingTransaction::FileWriteRoutingTransaction(FileWriteRoutingService &service, std::vector<std::wstring> route_keys,
                                                         std::unique_lock<std::mutex> &&lock)
    : service_(&service), route_keys_(std::move(route_keys)), lock_(std::move(lock)) {
}

FileWriteRoutingTransaction::~FileWriteRoutingTransaction() {
    if (!settled_) {
        service_->LatchRoutesFailed(route_keys_);
        settled_ = true;
    }

    if (lock_.owns_lock()) {
        lock_.unlock();
    }
}

bool FileWriteRoutingTransaction::Synchronize(DWORD &error) {
    if (settled_) {
        error = ERROR_INVALID_STATE;
        return false;
    }

    const bool result = service_->SynchronizeRoutes(route_keys_, error);
    settled_ = true;
    if (lock_.owns_lock()) {
        lock_.unlock();
    }

    return result;
}

void FileWriteRoutingTransaction::CancelWithoutWrite() {
    if (settled_) {
        return;
    }

    settled_ = true;
    if (lock_.owns_lock()) {
        lock_.unlock();
    }
}
} // namespace helen
