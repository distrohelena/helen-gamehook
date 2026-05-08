#pragma once

#include <string>

#include <HelenHook/ActivePackSet.h>
#include <HelenHook/LoadedBuildPackSet.h>

namespace helen
{
    /**
     * @brief Validates one loaded build-pack set and merges it into a runtime-ready active pack set.
     *
     * The builder is responsible for fail-fast conflict detection, additive declaration merging, and
     * preserving pack-local source context for asset-backed declarations before runtime services start.
     */
    class ActivePackSetBuilder
    {
    public:
        /**
         * @brief Validates and merges one loaded build-pack set into a runtime-ready active pack set.
         * @param loaded_pack_set Ordered loaded packs selected for one executable.
         * @param active_pack_set Receives the merged runtime view when validation succeeds.
         * @param failure_reason Receives a readable validation failure description when merging fails.
         * @return True when the pack set is valid and the merged runtime view was produced; otherwise false.
         */
        bool TryBuild(const LoadedBuildPackSet& loaded_pack_set, ActivePackSet& active_pack_set, std::string& failure_reason) const;
    };
}
