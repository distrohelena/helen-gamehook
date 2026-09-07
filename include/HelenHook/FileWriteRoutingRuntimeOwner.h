#pragma once

#include <memory>

#include <HelenHook/FileWriteRoutingService.h>

namespace helen {
/**
 * @brief Keeps the shared file-routing service alive independently of hook retirement.
 *
 * The runtime owns this holder through a heap pointer. Normal reset destroys it after hooks
 * are removed, while process detach releases the holder without invoking service cleanup under
 * the Windows loader lock. Copies of the service shared pointer can safely outlive a retired
 * callback context for Task 4 persistence integration.
 */
class FileWriteRoutingRuntimeOwner {
  public:
    /**
     * @brief Takes ownership of one initialized routing service.
     * @param service Shared service whose session must remain alive for direct callbacks.
     */
    explicit FileWriteRoutingRuntimeOwner(std::shared_ptr<FileWriteRoutingService> service);

    /** @brief Releases the shared service during normal runtime reset. */
    ~FileWriteRoutingRuntimeOwner();

    FileWriteRoutingRuntimeOwner(const FileWriteRoutingRuntimeOwner &) = delete;
    FileWriteRoutingRuntimeOwner &operator=(const FileWriteRoutingRuntimeOwner &) = delete;
    FileWriteRoutingRuntimeOwner(FileWriteRoutingRuntimeOwner &&) = delete;
    FileWriteRoutingRuntimeOwner &operator=(FileWriteRoutingRuntimeOwner &&) = delete;

    /**
     * @brief Returns a shared owner that Task 4 can pass into persistence callback contexts.
     * @return Shared routing service, never empty for a valid owner.
     */
    std::shared_ptr<FileWriteRoutingService> GetService() const;

  private:
    /** @brief Shared service retained across hook and direct-callback context lifetimes. */
    std::shared_ptr<FileWriteRoutingService> service_;
};
} // namespace helen
