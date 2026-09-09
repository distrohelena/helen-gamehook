#include <HelenHook/D3d9PresentationPolicy.h>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
    /** @brief Reports a policy-contract failure through the console rather than a GUI assertion. */
    void Expect(bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    }

    /** @brief Detects clobbered fields, missing overrides, partial array writes and one-shot consumption. */
    void CheckParameters() {
        helen::D3d9PresentationPolicy policy;
        std::array<D3DPRESENT_PARAMETERS, 3> parameters;
        std::memset(parameters.data(), 0x5A, sizeof(parameters));
        parameters[0].PresentationInterval = D3DPRESENT_INTERVAL_TWO;
        parameters[1].PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
        parameters[2].PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        const std::array<D3DPRESENT_PARAMETERS, 3> original = parameters;
        policy.Apply(parameters);
        Expect(std::memcmp(parameters.data(), original.data(), sizeof(parameters)) == 0,
            "Default policy changed application parameters");
        policy.SetVsyncOverride(helen::D3d9VsyncOverride::ForceOn);
        for (int visit = 0; visit < 2; ++visit) {
            parameters = original;
            policy.Apply(parameters);
            for (D3DPRESENT_PARAMETERS& entry : parameters) {
                Expect(entry.PresentationInterval == 1u, "ForceOn did not reach every presentation block");
            }
            for (std::size_t index = 0; index < parameters.size(); ++index) {
                parameters[index].PresentationInterval = original[index].PresentationInterval;
            }
            Expect(std::memcmp(parameters.data(), original.data(), sizeof(parameters)) == 0,
                "Override modified unrelated parameter bytes");
        }
        policy.SetVsyncOverride(helen::D3d9VsyncOverride::ForceOff);
        policy.Apply(parameters);
        for (const D3DPRESENT_PARAMETERS& entry : parameters) {
            Expect(entry.PresentationInterval == 0x80000000u, "ForceOff interval is not immediate");
        }
        policy.SetVsyncOverride(helen::D3d9VsyncOverride::GameControlled);
        parameters = original;
        policy.Apply(parameters);
        Expect(std::memcmp(parameters.data(), original.data(), sizeof(parameters)) == 0,
            "Disabling the override did not restore transparent forwarding");
        policy.Apply({});
        bool rejected = false;
        try { policy.SetVsyncOverride(static_cast<helen::D3d9VsyncOverride>(999)); }
        catch (const std::invalid_argument&) { rejected = true; }
        Expect(rejected, "Unknown policy was accepted");
        Expect(policy.GetVsyncOverride() == helen::D3d9VsyncOverride::GameControlled,
            "Rejected policy replaced the valid selection");
    }

    /** @brief Detects resampling the policy within one multi-adapter call during concurrent publication. */
    void CheckConcurrentPublication() {
        helen::D3d9PresentationPolicy policy;
        std::jthread writer([&policy] {
            for (int iteration = 0; iteration < 100000; ++iteration) {
                policy.SetVsyncOverride(helen::D3d9VsyncOverride::ForceOn);
                policy.SetVsyncOverride(helen::D3d9VsyncOverride::ForceOff);
            }
        });
        for (int iteration = 0; iteration < 10000; ++iteration) {
            std::array<D3DPRESENT_PARAMETERS, 8> parameters{};
            policy.Apply(parameters);
            const UINT interval = parameters[0].PresentationInterval;
            Expect(interval == 0u || interval == 1u || interval == 0x80000000u, "Invalid concurrent interval");
            for (const D3DPRESENT_PARAMETERS& entry : parameters) {
                Expect(entry.PresentationInterval == interval, "Mixed policies within one API call");
            }
        }
    }
}

/** @brief Runs address-free policy tests with crash dialogs disabled and explicit console failures. */
int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    try {
        CheckParameters();
        CheckConcurrentPublication();
        std::cout << "D3D9_PRESENTATION_POLICY_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
