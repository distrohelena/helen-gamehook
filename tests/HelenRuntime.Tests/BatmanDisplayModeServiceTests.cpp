#include <HelenHook/BatmanDisplayModeService.h>
#include <HelenHook/BatmanDisplayModeResponseProvider.h>
#include <HelenHook/CommandDispatcher.h>

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    /**
     * @brief Stops the native test runner when a display-mode expectation is not satisfied.
     * @param condition Boolean assertion result.
     * @param message Failure message describing the expectation.
     */
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Verifies one catalog mode's dimensions.
     * @param mode Catalog mode under test.
     * @param width Expected horizontal dimension.
     * @param height Expected vertical dimension.
     * @param message Failure message describing the expected pair.
     */
    void ExpectMode(const helen::BatmanDisplayMode& mode, int width, int height, const char* message)
    {
        Expect(mode.GetWidth() == width && mode.GetHeight() == height, message);
    }
}

/**
 * @brief Verifies supported display-mode normalization, scalar exposure, rejection, and revalidation behavior.
 */
void RunBatmanDisplayModeServiceTests()
{
    {
        helen::BatmanDisplayModeService service(
            []() -> std::optional<helen::BatmanDisplayEnvironment>
            {
                return helen::BatmanDisplayEnvironment(
                    L"DISPLAY1",
                    { helen::BatmanDisplayMode(1920, 1080), helen::BatmanDisplayMode(1280, 720) },
                    helen::BatmanDisplayMode(1920, 1040),
                    helen::BatmanDisplayMode(1920, 1080));
            });

        Expect(
            service.Refresh(helen::BatmanDisplayModeCatalogKind::Windowed, 2560, 1600),
            "Expected the windowed catalog to accept a configured custom size.");
        Expect(
            service.FindModeIndex(helen::BatmanDisplayModeCatalogKind::Windowed, 2560, 1600).has_value(),
            "Expected the windowed catalog to retain the configured 2560x1600 pair.");
        Expect(
            service.FindModeIndex(helen::BatmanDisplayModeCatalogKind::Windowed, 1280, 720).has_value(),
            "Expected the windowed catalog to include a common fitting size.");
        Expect(
            service.QueryCatalogScalar(helen::BatmanDisplayModeCatalogKind::Windowed, 5200).has_value(),
            "Expected the windowed catalog count request to resolve.");
        Expect(
            !service.FindModeIndex(helen::BatmanDisplayModeCatalogKind::Windowed, 3440, 1440).has_value(),
            "Expected the windowed catalog to reject a size larger than the usable work area.");

        Expect(
            service.Refresh(helen::BatmanDisplayModeCatalogKind::Fullscreen, 2560, 1600),
            "Expected the fullscreen catalog to refresh from supported monitor modes.");
        Expect(
            !service.FindModeIndex(helen::BatmanDisplayModeCatalogKind::Fullscreen, 2560, 1600).has_value(),
            "Expected fullscreen to exclude an unsupported custom windowed size.");
        Expect(
            service.QueryCatalogScalar(helen::BatmanDisplayModeCatalogKind::Fullscreen, 5400).has_value(),
            "Expected the fullscreen catalog count request to resolve.");
        const std::optional<helen::BatmanDisplayMode> fullscreen_current = service.GetCurrentMode(
            helen::BatmanDisplayModeCatalogKind::Fullscreen);
        Expect(
            fullscreen_current.has_value() && *fullscreen_current == helen::BatmanDisplayMode(2560, 1600),
            "Expected fullscreen catalog terminal values to retain the exact persisted pair.");
        const std::optional<helen::BatmanDisplayMode> fullscreen_desktop = service.GetDesktopMode(
            helen::BatmanDisplayModeCatalogKind::Fullscreen);
        Expect(
            fullscreen_desktop.has_value() && *fullscreen_desktop == helen::BatmanDisplayMode(1920, 1080),
            "Expected fullscreen desktop fallback to remain separately available.");
    }

    {
        helen::BatmanDisplayModeService service(
            []() -> std::optional<helen::BatmanDisplayEnvironment>
            {
                return helen::BatmanDisplayEnvironment(
                    L"DISPLAY1",
                    { helen::BatmanDisplayMode(1920, 1080) },
                    helen::BatmanDisplayMode(1920, 1080),
                    helen::BatmanDisplayMode(1920, 1080));
            });
        Expect(
            service.Refresh(helen::BatmanDisplayModeCatalogKind::Fullscreen, 1920, 1080),
            "Expected the initial fullscreen catalog to refresh.");
        const std::size_t original_count = service.GetModeCount(helen::BatmanDisplayModeCatalogKind::Fullscreen);
        Expect(
            !service.Refresh(helen::BatmanDisplayModeCatalogKind::Windowed, 0, 0),
            "Expected an invalid windowed custom pair to fail catalog refresh.");
        Expect(
            service.GetModeCount(helen::BatmanDisplayModeCatalogKind::Fullscreen) == original_count,
            "Expected a failed catalog refresh to preserve the existing fullscreen catalog.");
    }

    {
        helen::BatmanDisplayMode work_area(1920, 1040);
        helen::BatmanDisplayModeService service(
            [&work_area]() -> std::optional<helen::BatmanDisplayEnvironment>
            {
                return helen::BatmanDisplayEnvironment(
                    L"DISPLAY1",
                    { helen::BatmanDisplayMode(1920, 1080) },
                    work_area,
                    helen::BatmanDisplayMode(1920, 1080));
            });
        Expect(
            service.Refresh(helen::BatmanDisplayModeCatalogKind::Windowed, 2560, 1600),
            "Expected the initial windowed catalog to refresh for revalidation.");
        const std::optional<std::size_t> common_index = service.FindModeIndex(
            helen::BatmanDisplayModeCatalogKind::Windowed,
            1280,
            720);
        const std::optional<std::size_t> custom_index = service.FindModeIndex(
            helen::BatmanDisplayModeCatalogKind::Windowed,
            2560,
            1600);
        Expect(common_index.has_value() && custom_index.has_value(), "Expected both common and custom windowed modes.");
        work_area = helen::BatmanDisplayMode(1000, 700);
        Expect(
            !service.RevalidateMode(helen::BatmanDisplayModeCatalogKind::Windowed, *common_index).has_value(),
            "Expected a common windowed mode outside the fresh work area to fail revalidation.");
        Expect(
            service.RevalidateMode(helen::BatmanDisplayModeCatalogKind::Windowed, *custom_index).has_value(),
            "Expected the original configured custom windowed mode to remain exempt from work-area filtering.");
    }

    {
        helen::BatmanDisplayModeService service(
            [](std::wstring& device_name, std::vector<helen::BatmanDisplayMode>& modes)
            {
                device_name = L"DISPLAY1";
                modes = {
                    helen::BatmanDisplayMode(1920, 1080),
                    helen::BatmanDisplayMode(1280, 720),
                    helen::BatmanDisplayMode(1920, 1080),
                    helen::BatmanDisplayMode(3440, 1440)
                };
                return true;
            });

        Expect(service.Refresh(), "Expected the supported display-mode enumeration to refresh successfully.");
        Expect(service.GetModeCount() == 3, "Expected duplicate display modes to be removed.");
        ExpectMode(service.GetMode(0), 1280, 720, "Expected display modes to sort by pixel area first.");
        ExpectMode(service.GetMode(1), 1920, 1080, "Expected the middle display mode to remain after normalization.");
        ExpectMode(service.GetMode(2), 3440, 1440, "Expected the largest display mode to sort last.");
        Expect(service.QueryCatalogScalar(4700) == std::optional<int>(3), "Expected scalar 4700 to expose the captured mode count.");
        Expect(service.QueryCatalogScalar(4701) == std::optional<int>(1280), "Expected the first scalar width to expose the sorted catalog.");
        Expect(service.QueryCatalogScalar(4702) == std::optional<int>(720), "Expected the first scalar height to expose the sorted catalog.");
        Expect(service.QueryCatalogScalar(4705) == std::optional<int>(3440), "Expected the third scalar width to expose the sorted catalog.");
        Expect(service.QueryCatalogScalar(4706) == std::optional<int>(1440), "Expected the third scalar height to expose the sorted catalog.");
        Expect(!service.QueryCatalogScalar(4699).has_value(), "Expected scalar requests below the catalog range to be rejected.");
        Expect(!service.QueryCatalogScalar(4707).has_value(), "Expected scalar requests beyond the captured catalog to be rejected.");
        Expect(
            service.FindModeIndex(1920, 1080) == std::optional<std::size_t>(1),
            "Expected exact dimensions to resolve to their catalog index.");
        Expect(!service.FindModeIndex(1920, 1200).has_value(), "Expected an exact pair absent from the catalog to remain unresolved.");
    }

    {
        helen::BatmanDisplayModeService service(
            [](std::wstring& device_name, std::vector<helen::BatmanDisplayMode>& modes)
            {
                device_name = L"DISPLAY1";
                modes = {
                    helen::BatmanDisplayMode(1600, 640),
                    helen::BatmanDisplayMode(1280, 800)
                };
                return true;
            });

        Expect(service.Refresh(), "Expected equal-area display modes to refresh successfully.");
        ExpectMode(service.GetMode(0), 1280, 800, "Expected equal-area modes to sort by width first.");
        ExpectMode(service.GetMode(1), 1600, 640, "Expected equal-area modes to sort by width before height.");
    }

    {
        helen::BatmanDisplayModeService service(
            [](std::wstring& device_name, std::vector<helen::BatmanDisplayMode>& modes)
            {
                device_name = L"DISPLAY1";
                modes.emplace_back(0, 1080);
                return true;
            });
        Expect(!service.Refresh(), "Expected zero-width display modes to be rejected.");
    }

    {
        helen::BatmanDisplayModeService service(
            [](std::wstring& device_name, std::vector<helen::BatmanDisplayMode>&)
            {
                device_name = L"DISPLAY1";
                return true;
            });
        Expect(!service.Refresh(), "Expected an empty display-mode enumeration to be rejected.");
    }

    {
        helen::BatmanDisplayModeService service(
            [](std::wstring& device_name, std::vector<helen::BatmanDisplayMode>& modes)
            {
                device_name = L"DISPLAY1";
                for (int index = 1; index <= 99; ++index)
                {
                    modes.emplace_back(index, 1000);
                }
                return true;
            });
        Expect(!service.Refresh(), "Expected an enumeration exceeding the maximum mode count to be rejected.");
    }

    {
        const std::vector<int> qualifying_process_window_candidates = { 1, 2 };
        helen::BatmanDisplayModeService service(
            [&qualifying_process_window_candidates](std::wstring&, std::vector<helen::BatmanDisplayMode>&)
            {
                const std::size_t candidate_count = qualifying_process_window_candidates.size();
                return candidate_count == 1;
            });
        Expect(qualifying_process_window_candidates.size() == 2, "Expected the ambiguity fixture to contain two qualifying process windows.");
        Expect(!service.Refresh(), "Expected two qualifying process windows to be rejected as ambiguous.");
    }

    {
        int enumeration_count = 0;
        helen::BatmanDisplayModeService service(
            [&enumeration_count](std::wstring& device_name, std::vector<helen::BatmanDisplayMode>& modes)
            {
                ++enumeration_count;
                device_name = L"DISPLAY1";
                if (enumeration_count == 1)
                {
                    modes = {
                        helen::BatmanDisplayMode(1920, 1080),
                        helen::BatmanDisplayMode(1280, 720)
                    };
                    return true;
                }

                modes = {
                    helen::BatmanDisplayMode(3440, 1440),
                    helen::BatmanDisplayMode(1280, 720)
                };
                return true;
            });

        Expect(service.Refresh(), "Expected the initial catalog enumeration to succeed before revalidation.");
        const std::optional<helen::BatmanDisplayMode> removed_mode = service.RevalidateMode(1);
        Expect(!removed_mode.has_value(), "Expected a selected pair removed by re-enumeration to fail revalidation.");
        Expect(service.GetModeCount() == 2, "Expected revalidation to preserve the original catalog semantics.");
        ExpectMode(service.GetMode(1), 1920, 1080, "Expected revalidation not to reinterpret the selected index against a reordered catalog.");
    }

    {
        int enumeration_count = 0;
        helen::BatmanDisplayModeService service(
            [&enumeration_count](std::wstring& device_name, std::vector<helen::BatmanDisplayMode>& modes)
            {
                ++enumeration_count;
                device_name = enumeration_count == 1 ? L"DISPLAY1" : L"DISPLAY2";
                modes.emplace_back(1920, 1080);
                return true;
            });

        Expect(service.Refresh(), "Expected the initial display identity enumeration to succeed.");
        const std::optional<helen::BatmanDisplayMode> revalidated_mode = service.RevalidateMode(0);
        Expect(!revalidated_mode.has_value(), "Expected a matching pair on a different display device to fail revalidation.");
    }
}

/**
 * @brief Verifies provider count ownership rejects cross-bank and stale terminal responses.
 */
void RunBatmanDisplayModeResponseProviderTests()
{
    bool environment_available = true;
    helen::BatmanDisplayModeService service(
        [&environment_available]() -> std::optional<helen::BatmanDisplayEnvironment>
        {
            if (!environment_available)
            {
                return std::nullopt;
            }
            return helen::BatmanDisplayEnvironment(
                L"DISPLAY1",
                { helen::BatmanDisplayMode(1280, 720), helen::BatmanDisplayMode(1920, 1080) },
                helen::BatmanDisplayMode(1920, 1080),
                helen::BatmanDisplayMode(1920, 1080));
        });
    helen::CommandDispatcher dispatcher;
    dispatcher.RegisterConfigInt("resolutionWidth", 2560);
    dispatcher.RegisterConfigInt("resolutionHeight", 1600);
    helen::BatmanDisplayModeResponseProvider provider(service, dispatcher);

    Expect(
        !provider.Resolve("wrongProvider", helen::BatmanDisplayModeResponseProvider::WindowedCatalogRequest).has_value(),
        "Unknown display provider unexpectedly resolved a request.");
    Expect(
        provider.Resolve(helen::BatmanDisplayModeResponseProvider::ProviderId, helen::BatmanDisplayModeResponseProvider::WindowedCatalogRequest).has_value(),
        "Windowed catalog count did not resolve.");
    Expect(
        !provider.Resolve("wrongProvider", helen::BatmanDisplayModeResponseProvider::WindowedCurrentWidthRequest).has_value(),
        "Unknown provider unexpectedly mutated an owned catalog sequence.");
    Expect(
        provider.Resolve(helen::BatmanDisplayModeResponseProvider::ProviderId, helen::BatmanDisplayModeResponseProvider::WindowedCurrentWidthRequest) == std::optional<int>(2560),
        "Unknown provider changed the active windowed catalog sequence.");
    environment_available = false;
    Expect(
        !provider.Resolve(helen::BatmanDisplayModeResponseProvider::ProviderId, helen::BatmanDisplayModeResponseProvider::FullscreenCatalogRequest).has_value(),
        "Failed fullscreen count unexpectedly succeeded after monitor discovery failed.");
    Expect(
        !provider.Resolve(helen::BatmanDisplayModeResponseProvider::ProviderId, helen::BatmanDisplayModeResponseProvider::WindowedCurrentHeightRequest).has_value(),
        "Failed fullscreen count left the prior windowed terminal sequence usable.");
    environment_available = true;
    Expect(
        provider.Resolve(helen::BatmanDisplayModeResponseProvider::ProviderId, helen::BatmanDisplayModeResponseProvider::WindowedCatalogRequest).has_value(),
        "Windowed catalog did not recover after monitor discovery resumed.");
    Expect(
        !provider.Resolve(helen::BatmanDisplayModeResponseProvider::ProviderId, helen::BatmanDisplayModeResponseProvider::FullscreenCurrentWidthRequest).has_value(),
        "Fullscreen terminal width crossed from a windowed catalog sequence.");
    Expect(
        provider.Resolve(helen::BatmanDisplayModeResponseProvider::ProviderId, helen::BatmanDisplayModeResponseProvider::WindowedCatalogRequest).has_value(),
        "Windowed catalog did not restart after a cross-bank request invalidated its sequence.");
    Expect(
        provider.Resolve(helen::BatmanDisplayModeResponseProvider::ProviderId, helen::BatmanDisplayModeResponseProvider::WindowedCurrentWidthRequest) == std::optional<int>(2560),
        "Windowed terminal width did not retain the exact persisted pair.");
    Expect(
        provider.Resolve(helen::BatmanDisplayModeResponseProvider::ProviderId, helen::BatmanDisplayModeResponseProvider::WindowedCurrentHeightRequest) == std::optional<int>(1600),
        "Windowed terminal height did not retain the exact persisted pair.");
    Expect(
        !provider.Resolve(helen::BatmanDisplayModeResponseProvider::ProviderId, helen::BatmanDisplayModeResponseProvider::WindowedCurrentHeightRequest).has_value(),
        "Consumed windowed terminal height was reused after its sequence completed.");

    Expect(
        provider.Resolve(helen::BatmanDisplayModeResponseProvider::ProviderId, helen::BatmanDisplayModeResponseProvider::FullscreenCatalogRequest).has_value(),
        "Fullscreen catalog count did not resolve.");
    Expect(
        provider.Resolve(helen::BatmanDisplayModeResponseProvider::ProviderId, helen::BatmanDisplayModeResponseProvider::FullscreenCurrentWidthRequest) == std::optional<int>(2560),
        "Fullscreen terminal width did not retain the exact persisted pair.");
    Expect(
        provider.Resolve(helen::BatmanDisplayModeResponseProvider::ProviderId, helen::BatmanDisplayModeResponseProvider::FullscreenCurrentHeightRequest) == std::optional<int>(1600),
        "Fullscreen terminal height did not retain the exact persisted pair.");
    Expect(
        provider.Resolve(helen::BatmanDisplayModeResponseProvider::ProviderId, helen::BatmanDisplayModeResponseProvider::DesktopWidthRequest) == std::optional<int>(1920),
        "Desktop fallback width did not resolve from the monitor snapshot.");
    Expect(
        provider.Resolve(helen::BatmanDisplayModeResponseProvider::ProviderId, helen::BatmanDisplayModeResponseProvider::DesktopHeightRequest) == std::optional<int>(1080),
        "Desktop fallback height did not resolve from the monitor snapshot.");
    environment_available = false;
    Expect(
        !provider.Resolve(helen::BatmanDisplayModeResponseProvider::ProviderId, helen::BatmanDisplayModeResponseProvider::FullscreenCatalogRequest).has_value(),
        "Failed fullscreen count unexpectedly succeeded in the desktop provenance test.");
    Expect(
        !provider.Resolve(helen::BatmanDisplayModeResponseProvider::ProviderId, helen::BatmanDisplayModeResponseProvider::DesktopWidthRequest).has_value(),
        "Failed fullscreen count left a stale desktop pair available.");
}
