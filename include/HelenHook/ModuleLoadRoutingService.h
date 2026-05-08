#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace helen
{
    /**
     * @brief Describes one Helen-owned alternate DLL path keyed by a requested module name.
     *
     * The runtime validates these definitions when the service is constructed so duplicate
     * module names and empty replacement paths fail before any routing decisions are made.
     */
    struct ModuleRedirectDefinition
    {
        /**
         * @brief Requested module name that should resolve to the replacement path.
         *
         * The service normalizes this name into a lower-case leaf module filename before validation.
         */
        std::wstring RequestedName;

        /**
         * @brief Replacement DLL path that Helen will use in a later redirect phase.
         *
         * The current log-only slice validates this path but does not yet load it.
         */
        std::filesystem::path ReplacementPath;
    };

    /**
     * @brief Captures one module-load routing result for logging and future redirect support.
     */
    struct ModuleLoadRoutingDecision
    {
        /**
         * @brief Loader API that produced the request.
         */
        std::wstring ApiName;

        /**
         * @brief Original module text passed to the loader hook before normalization.
         */
        std::wstring OriginalRequestText;

        /**
         * @brief Normalized leaf module name used for alias matching.
         */
        std::wstring RequestedModuleName;

        /**
         * @brief True when the normalized module name matches one of the built-in Bink aliases.
         */
        bool IsBinkAlias{};

        /**
         * @brief Replacement DLL path that a later redirect phase could use for this request.
         */
        std::optional<std::filesystem::path> RedirectTargetPath;
    };

    /**
     * @brief Normalizes and classifies module-load requests for the Batman Bink routing hook.
     *
     * The service is intentionally pure: it validates redirect metadata, identifies Bink aliases,
     * and formats a log message. It does not install hooks or call Win32 loader APIs.
     */
    class ModuleLoadRoutingService
    {
    public:
        /**
         * @brief Constructs one routing service with optional future redirect metadata.
         * @param redirects Helen-owned redirect definitions that should be validated up front.
         *
         * Duplicate requested names and empty replacement paths are rejected immediately so
         * the runtime never reaches an ambiguous routing state.
         */
        explicit ModuleLoadRoutingService(std::vector<ModuleRedirectDefinition> redirects = {});

        /**
         * @brief Classifies one loader call into a normalized routing decision.
         * @param api_name Human-readable loader API name used for diagnostics.
         * @param requested_module_name Raw module text passed to the loader function.
         * @return A routing decision that preserves the original text and the normalized module name.
         */
        ModuleLoadRoutingDecision DescribeRequest(std::wstring_view api_name, std::wstring_view requested_module_name) const;

        /**
         * @brief Formats one routing decision into the log text used by the hook set.
         * @param decision Routing decision produced by DescribeRequest.
         * @return One diagnostic line ready for Helen logging.
         */
        std::wstring BuildLogMessage(const ModuleLoadRoutingDecision& decision) const;

    private:
        /**
         * @brief Normalizes a requested module name into a lower-case leaf filename.
         * @param module_name Requested module text that may include path separators.
         * @return Lower-case leaf module name when the text is meaningful; otherwise no value.
         */
        static std::optional<std::wstring> NormalizeModuleName(std::wstring_view module_name);

        /**
         * @brief Returns true when one normalized module name is one of Batman's built-in Bink DLL aliases.
         * @param normalized_module_name Lower-case leaf module filename.
         * @return True when the module name is one of the built-in Bink aliases.
         */
        static bool IsBinkAlias(std::wstring_view normalized_module_name) noexcept;

        /**
         * @brief Validates the constructor-provided redirect metadata and stores a normalized copy.
         * @param redirects Redirect definitions provided to the constructor.
         */
        void ValidateRedirectDefinitions(std::vector<ModuleRedirectDefinition> redirects);

        /**
         * @brief Converts one wide string to lower-case using the current C locale.
         * @param text String that should be normalized for case-insensitive comparisons.
         * @return Lower-case copy of the supplied string.
         */
        static std::wstring ToLowerWide(std::wstring text);

        /**
         * @brief Stores the validated redirect definitions for future routing phases.
         */
        std::vector<ModuleRedirectDefinition> Redirects_;
    };
}
