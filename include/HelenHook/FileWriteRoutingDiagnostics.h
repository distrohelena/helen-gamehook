#pragma once

#include <filesystem>
#include <string>

#include <HelenHook/FileReadPolicy.h>
#include <HelenHook/FileWritePolicy.h>

namespace helen::routing_detail {
/**
 * @brief Immutable diagnostic details describing one active session route.
 *
 * The service exposes this type through a documented alias so existing callers retain
 * `FileWriteRoutingService::RouteDiagnostics` without keeping multiple declarations together.
 */
struct RouteDiagnostics {
    /** @brief Stable route declaration identifier. */
    std::string Id;
    /** @brief Canonical protected original path. */
    std::filesystem::path OriginalPath;
    /** @brief Session-owned overlay path, or empty when no overlay exists. */
    std::filesystem::path OverlayPath;
    /** @brief Active write policy. */
    FileWritePolicy WritePolicy = FileWritePolicy::Deny;
    /** @brief Active read source policy. */
    FileReadPolicy ReadPolicy = FileReadPolicy::Original;
};
} // namespace helen::routing_detail
