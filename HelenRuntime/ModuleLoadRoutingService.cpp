#include <HelenHook/ModuleLoadRoutingService.h>

#include <algorithm>
#include <cwctype>
#include <set>
#include <stdexcept>
#include <utility>

namespace
{
    /**
     * @brief Returns the leaf module text from a wide path-like request string.
     * @param module_name Requested module text that may contain directory separators.
     * @return Leaf filename portion of the supplied text.
     */
    std::wstring_view GetLeafModuleText(std::wstring_view module_name) noexcept
    {
        const std::size_t separator_position = module_name.find_last_of(L"\\/");
        if (separator_position == std::wstring_view::npos)
        {
            return module_name;
        }

        return module_name.substr(separator_position + 1);
    }
}

namespace helen
{
    /**
     * @brief Constructs one routing service and validates the provided redirect metadata.
     * @param redirects Helen-owned redirect definitions that should be checked before use.
     */
    ModuleLoadRoutingService::ModuleLoadRoutingService(std::vector<ModuleRedirectDefinition> redirects)
    {
        ValidateRedirectDefinitions(std::move(redirects));
    }

    /**
     * @brief Validates the constructor-provided redirect metadata and stores a normalized copy.
     * @param redirects Redirect definitions provided to the constructor.
     */
    void ModuleLoadRoutingService::ValidateRedirectDefinitions(std::vector<ModuleRedirectDefinition> redirects)
    {
        std::set<std::wstring> normalized_names;
        Redirects_.reserve(redirects.size());

        for (ModuleRedirectDefinition& redirect : redirects)
        {
            const std::optional<std::wstring> normalized_requested_name = NormalizeModuleName(redirect.RequestedName);
            if (!normalized_requested_name.has_value())
            {
                throw std::invalid_argument("redirect requested name must be a meaningful module name");
            }

            if (redirect.ReplacementPath.empty())
            {
                throw std::invalid_argument("redirect replacement path must not be empty");
            }

            if (!normalized_names.insert(*normalized_requested_name).second)
            {
                throw std::invalid_argument("redirect requested name must be unique after normalization");
            }

            redirect.RequestedName = *normalized_requested_name;
            Redirects_.push_back(std::move(redirect));
        }
    }

    /**
     * @brief Classifies one loader call into a normalized routing decision.
     * @param api_name Human-readable loader API name used for diagnostics.
     * @param requested_module_name Raw module text passed to the loader function.
     * @return A routing decision that preserves the original text and the normalized module name.
     */
    ModuleLoadRoutingDecision ModuleLoadRoutingService::DescribeRequest(std::wstring_view api_name, std::wstring_view requested_module_name) const
    {
        ModuleLoadRoutingDecision decision;
        decision.ApiName = api_name.empty() ? L"<unknown>" : std::wstring(api_name);
        decision.OriginalRequestText = requested_module_name.empty() ? L"<empty>" : std::wstring(requested_module_name);

        const std::optional<std::wstring> normalized_module_name = NormalizeModuleName(requested_module_name);
        if (normalized_module_name.has_value())
        {
            decision.RequestedModuleName = *normalized_module_name;
        }
        else
        {
            decision.RequestedModuleName = L"<invalid>";
        }

        decision.IsBinkAlias = IsBinkAlias(decision.RequestedModuleName);
        decision.RedirectTargetPath = std::nullopt;
        return decision;
    }

    /**
     * @brief Formats one routing decision into the log text used by the hook set.
     * @param decision Routing decision produced by DescribeRequest.
     * @return One diagnostic line ready for Helen logging.
     */
    std::wstring ModuleLoadRoutingService::BuildLogMessage(const ModuleLoadRoutingDecision& decision) const
    {
        std::wstring message = L"[runtime] module-load request api=";
        message += decision.ApiName;
        message += L" original=";
        message += decision.OriginalRequestText;
        message += L" requested=";
        message += decision.RequestedModuleName;
        message += L" matched=";
        message += decision.IsBinkAlias ? L"bink alias" : L"<no>";
        message += L" redirect=";
        if (decision.RedirectTargetPath.has_value())
        {
            message += decision.RedirectTargetPath->wstring();
        }
        else
        {
            message += L"<none>";
        }

        return message;
    }

    /**
     * @brief Normalizes a requested module name into a lower-case leaf filename.
     * @param module_name Requested module text that may include path separators.
     * @return Lower-case leaf module name when the text is meaningful; otherwise no value.
     */
    std::optional<std::wstring> ModuleLoadRoutingService::NormalizeModuleName(std::wstring_view module_name)
    {
        const std::wstring_view leaf_module_name = GetLeafModuleText(module_name);
        if (leaf_module_name.empty())
        {
            return std::nullopt;
        }

        return ToLowerWide(std::wstring(leaf_module_name));
    }

    /**
     * @brief Returns true when one normalized module name is one of Batman's built-in Bink DLL aliases.
     * @param normalized_module_name Lower-case leaf module filename.
     * @return True when the module name is one of the built-in Bink aliases.
     */
    bool ModuleLoadRoutingService::IsBinkAlias(std::wstring_view normalized_module_name) noexcept
    {
        return normalized_module_name == L"binkw32.dll" || normalized_module_name == L"bink2w32.dll";
    }

    /**
     * @brief Converts one wide string to lower-case using the current C locale.
     * @param text String that should be normalized for case-insensitive comparisons.
     * @return Lower-case copy of the supplied string.
     */
    std::wstring ModuleLoadRoutingService::ToLowerWide(std::wstring text)
    {
        for (wchar_t& character : text)
        {
            character = static_cast<wchar_t>(std::towlower(character));
        }

        return text;
    }
}
