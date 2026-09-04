#include <HelenHook/BatmanDisplayModeService.h>

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
        helen::BatmanDisplayModeService service(
            [](std::wstring&, std::vector<helen::BatmanDisplayMode>&)
            {
                return false;
            });
        Expect(!service.Refresh(), "Expected an ambiguous process-window enumeration result to be rejected.");
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
}
