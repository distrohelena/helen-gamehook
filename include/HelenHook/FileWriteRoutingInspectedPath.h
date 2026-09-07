#pragma once

#include <string>

#include <windows.h>

namespace helen::routing_detail {
/**
 * @brief Holds the native identity and canonical spelling captured during route validation.
 *
 * Callers use the identity, link count, and reparse state together so a path cannot silently
 * change from the validated regular file before routing state is committed.
 */
struct InspectedPath {
    /** @brief Full lexical path produced by native path resolution. */
    std::wstring FullPath;
    /** @brief Canonical final path produced by the inspected handle. */
    std::wstring CanonicalPath;
    /** @brief Stable volume and file-index identity used for alias detection. */
    std::wstring Identity;
    /** @brief True when any existing component is a reparse point. */
    bool HasReparseComponent = false;
    /** @brief True when the final object is a regular file. */
    bool IsRegularFile = false;
    /** @brief Native hard-link count captured for the final object. */
    DWORD NumberOfLinks = 0;
};
} // namespace helen::routing_detail
