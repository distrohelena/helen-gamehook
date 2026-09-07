#include <HelenHook/FileWriteRoutingRuntimeOwner.h>

#include <stdexcept>

namespace helen {
FileWriteRoutingRuntimeOwner::FileWriteRoutingRuntimeOwner(std::shared_ptr<FileWriteRoutingService> service)
    : service_(std::move(service)) {
    if (service_ == nullptr) {
        throw std::invalid_argument("File-write routing runtime owner requires an initialized service.");
    }
}

FileWriteRoutingRuntimeOwner::~FileWriteRoutingRuntimeOwner() = default;

std::shared_ptr<FileWriteRoutingService> FileWriteRoutingRuntimeOwner::GetService() const {
    return service_;
}
} // namespace helen
