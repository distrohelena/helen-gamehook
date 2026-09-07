#pragma once

#include <filesystem>
#include <string>

#include <HelenHook/FileWriteRoute.h>

namespace helen::routing_detail {
/**
 * @brief Holds one validated route until all declarations and session copies are ready.
 *
 * Keeping this staging record separate from the service container prevents partially
 * initialized routes from becoming visible when a later declaration fails validation.
 */
struct PendingRoute {
    /** @brief Route declaration with its original path replaced by the canonical native path. */
    helen::FileWriteRoute Route;
    /** @brief Canonical lower-case path key used by route maps. */
    std::wstring Key;
    /** @brief Lexical lower-case path key accepted as an ordinary alias. */
    std::wstring LexicalKey;
    /** @brief Stable native file identity captured during validation. */
    std::wstring Identity;
    /** @brief Session overlay path assigned after the unique directory is created. */
    std::filesystem::path OverlayPath;
};
} // namespace helen::routing_detail
