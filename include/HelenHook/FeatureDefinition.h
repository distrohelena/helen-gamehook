#pragma once

#include <string>

namespace helen
{
    /**
     * @brief Describes one editor-visible feature exposed by a split pack.
     */
    class FeatureDefinition
    {
    public:
        /** @brief Stable feature identifier used by runtime and tooling declarations. */
        std::string Id;
        /** @brief Human-readable feature name shown in editor and launcher UIs. */
        std::string Name;
        /** @brief Declarative feature kind such as enum, slider, or toggle. */
        std::string Kind;
        /** @brief Config key that stores the feature's selected value. */
        std::string ConfigKey;
        /** @brief Default integer value applied when the config store has no persisted value yet. */
        int DefaultValue{};
    };
}