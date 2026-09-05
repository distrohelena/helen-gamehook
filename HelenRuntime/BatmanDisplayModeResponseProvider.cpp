#include <HelenHook/BatmanDisplayModeResponseProvider.h>

#include <HelenHook/CommandDispatcher.h>

namespace helen
{
    BatmanDisplayModeResponseProvider::BatmanDisplayModeResponseProvider(
        BatmanDisplayModeService& display_mode_service,
        CommandDispatcher& command_dispatcher)
        : display_mode_service_(display_mode_service),
          command_dispatcher_(command_dispatcher)
    {
    }

    std::optional<int> BatmanDisplayModeResponseProvider::Resolve(
        const std::string& provider_id,
        int raw_request)
    {
        if (provider_id != ProviderId)
        {
            return std::nullopt;
        }

        const bool is_windowed_count = raw_request == WindowedCatalogRequest;
        const bool is_fullscreen_count = raw_request == FullscreenCatalogRequest;
        const bool is_legacy_count = raw_request == LegacyCatalogRequest;
        if (is_legacy_count || is_windowed_count || is_fullscreen_count)
        {
            current_pair_.reset();
            origin_request_.reset();
            desktop_mode_.reset();
            desktop_origin_request_.reset();
            const std::optional<CommandIntPair> current_pair = command_dispatcher_.TryGetIntPair(
                "resolutionWidth",
                "resolutionHeight");
            if (!current_pair.has_value())
            {
                return std::nullopt;
            }

            if (is_legacy_count)
            {
                if (!display_mode_service_.Refresh())
                {
                    return std::nullopt;
                }
                catalog_kind_ = BatmanDisplayModeCatalogKind::Fullscreen;
            }
            else
            {
                catalog_kind_ = is_windowed_count
                    ? BatmanDisplayModeCatalogKind::Windowed
                    : BatmanDisplayModeCatalogKind::Fullscreen;
                if (!display_mode_service_.Refresh(
                        catalog_kind_,
                        current_pair->FirstValue,
                        current_pair->SecondValue))
                {
                    return std::nullopt;
                }
            }

            current_pair_ = current_pair;
            origin_request_ = raw_request;
            if (is_fullscreen_count || is_legacy_count)
            {
                desktop_mode_ = display_mode_service_.GetDesktopMode(catalog_kind_);
                desktop_origin_request_ = raw_request;
                if (!desktop_mode_.has_value())
                {
                    current_pair_.reset();
                    origin_request_.reset();
                    desktop_origin_request_.reset();
                    return std::nullopt;
                }
            }
            const std::optional<int> value = is_legacy_count
                ? display_mode_service_.QueryCatalogScalar(raw_request)
                : display_mode_service_.QueryCatalogScalar(catalog_kind_, raw_request);
            if (!value.has_value())
            {
                current_pair_.reset();
                origin_request_.reset();
                desktop_mode_.reset();
                desktop_origin_request_.reset();
            }
            return value;
        }

        const bool is_windowed_request = raw_request > WindowedCatalogRequest && raw_request <= WindowedCurrentHeightRequest;
        const bool is_fullscreen_request = raw_request > FullscreenCatalogRequest && raw_request <= FullscreenCurrentHeightRequest;
        const bool is_legacy_current_width = raw_request == LegacyCurrentWidthRequest;
        const bool is_legacy_current_height = raw_request == LegacyCurrentHeightRequest;
        const bool is_windowed_current_width = raw_request == WindowedCurrentWidthRequest;
        const bool is_windowed_current_height = raw_request == WindowedCurrentHeightRequest;
        const bool is_fullscreen_current_width = raw_request == FullscreenCurrentWidthRequest;
        const bool is_fullscreen_current_height = raw_request == FullscreenCurrentHeightRequest;
        if (is_legacy_current_width || is_legacy_current_height || is_windowed_current_width || is_windowed_current_height ||
            is_fullscreen_current_width || is_fullscreen_current_height)
        {
            const int expected_origin = is_legacy_current_width || is_legacy_current_height
                ? LegacyCatalogRequest
                : (is_windowed_current_width || is_windowed_current_height
                    ? WindowedCatalogRequest
                    : FullscreenCatalogRequest);
            if (!current_pair_.has_value() || !origin_request_.has_value() || *origin_request_ != expected_origin)
            {
                current_pair_.reset();
                origin_request_.reset();
                desktop_mode_.reset();
                desktop_origin_request_.reset();
                return std::nullopt;
            }

            if (is_legacy_current_width || is_windowed_current_width || is_fullscreen_current_width)
            {
                return current_pair_->FirstValue;
            }

            const int current_height = current_pair_->SecondValue;
            current_pair_.reset();
            origin_request_.reset();
            return current_height;
        }

        if (raw_request == DesktopWidthRequest || raw_request == DesktopHeightRequest)
        {
            const bool has_fullscreen_desktop_origin = desktop_origin_request_.has_value() &&
                (*desktop_origin_request_ == FullscreenCatalogRequest || *desktop_origin_request_ == LegacyCatalogRequest);
            if (!desktop_mode_.has_value() || !has_fullscreen_desktop_origin)
            {
                return std::nullopt;
            }
            if (raw_request == DesktopWidthRequest)
            {
                return desktop_mode_->GetWidth();
            }
            const int desktop_height = desktop_mode_->GetHeight();
            desktop_mode_.reset();
            desktop_origin_request_.reset();
            return desktop_height;
        }

        if (is_windowed_request)
        {
            if (!origin_request_.has_value() || *origin_request_ != WindowedCatalogRequest)
            {
                current_pair_.reset();
                origin_request_.reset();
                desktop_mode_.reset();
                desktop_origin_request_.reset();
                return std::nullopt;
            }
            const std::optional<int> value = display_mode_service_.QueryCatalogScalar(
                BatmanDisplayModeCatalogKind::Windowed,
                raw_request);
            if (!value.has_value())
            {
                current_pair_.reset();
                origin_request_.reset();
                desktop_mode_.reset();
                desktop_origin_request_.reset();
            }
            return value;
        }

        if (is_fullscreen_request)
        {
            if (!origin_request_.has_value() || *origin_request_ != FullscreenCatalogRequest)
            {
                current_pair_.reset();
                origin_request_.reset();
                desktop_mode_.reset();
                desktop_origin_request_.reset();
                return std::nullopt;
            }
            const std::optional<int> value = display_mode_service_.QueryCatalogScalar(
                BatmanDisplayModeCatalogKind::Fullscreen,
                raw_request);
            if (!value.has_value())
            {
                current_pair_.reset();
                origin_request_.reset();
                desktop_mode_.reset();
                desktop_origin_request_.reset();
            }
            return value;
        }

        if (raw_request >= LegacyCatalogRequest + 1 && raw_request < LegacyCurrentWidthRequest)
        {
            if (!origin_request_.has_value() || *origin_request_ != LegacyCatalogRequest)
            {
                current_pair_.reset();
                origin_request_.reset();
                desktop_mode_.reset();
                desktop_origin_request_.reset();
                return std::nullopt;
            }
            const std::optional<int> value = display_mode_service_.QueryCatalogScalar(raw_request);
            if (!value.has_value())
            {
                current_pair_.reset();
                origin_request_.reset();
                desktop_mode_.reset();
                desktop_origin_request_.reset();
            }
            return value;
        }

        return std::nullopt;
    }
}
