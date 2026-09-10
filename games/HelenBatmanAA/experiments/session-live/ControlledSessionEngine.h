#pragma once
#include "SessionGraphicsEngine.h"
#include <functional>
#include <stdexcept>

/** @brief Models only unavailable retail engine calls; real backend code owns validation, publication and locks. */
class ControlledSessionEngine final : public helen::SessionGraphicsEngine {
public:
    /** @brief Complete current values, independently supplied by the test. */
    helen::BatmanGraphicsDraftState Live;
    /** @brief Stages observed at the external boundary in exact order. */
    std::string Calls;
    /** @brief Named stage that throws after its boundary is entered; empty means no injected failure. */
    std::string FailAt;
    /** @brief Optional reentrant action invoked while the real backend owns its Apply scope. */
    std::function<void()> DuringApply;
    /** @brief Requires a complete fixture rather than constructing invalid defaults. */
    explicit ControlledSessionEngine(const helen::BatmanGraphicsDraftState& live) : Live(live) {}
    /** @brief Converts controlled engine values into the real immutable snapshot type. */
    helen::BatmanGraphicsSnapshot Capture() const override {
        helen::BatmanGraphicsSnapshot::Values values;
        for (unsigned index=0; index<14; ++index) { values[index] = Live.Get(static_cast<helen::BatmanGraphicsField>(index)); }
        return helen::BatmanGraphicsSnapshot(values);
    }
    /** @brief Models verified ordinary fields and unsupported physics/stereo transitions. */
    helen::SessionGraphicsDelta::Capabilities Capabilities() const noexcept override {
        helen::SessionGraphicsDelta::Capabilities values;
        values.fill(true);
        values[10] = false;
        values[11] = false;
        return values;
    }
    /** @brief Records a nonmutating native validation boundary. */
    void Preflight(const helen::BatmanGraphicsDraftState&, const helen::BatmanGraphicsDraftState&,
        const helen::SessionGraphicsDelta&) override { Enter("P"); }
    /** @brief Records the point at which a complete overlay may be published. */
    void Stage(const helen::BatmanGraphicsDraftState&, const helen::SessionGraphicsDelta&) override { Enter("S"); }
    /** @brief Models engine mutation and synchronous reentry during a real backend transaction. */
    void Apply(const helen::BatmanGraphicsDraftState& draft, const helen::SessionGraphicsDelta&) override {
        Live = draft;
        if (DuringApply) { DuringApply(); }
        Enter("A");
    }
    /** @brief Models readback failure after engine mutation instead of falsely returning success. */
    void Verify(const helen::BatmanGraphicsDraftState&, const helen::SessionGraphicsDelta&) override { Enter("V"); }
private:
    /** @brief Makes boundary order and failure location observable without implementing backend behavior in the fake. */
    void Enter(const std::string& stage) { Calls += stage; if (FailAt == stage) { throw std::runtime_error("Injected native boundary failure"); } }
};
