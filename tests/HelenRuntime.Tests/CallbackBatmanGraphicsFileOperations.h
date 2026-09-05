#pragma once

#include <HelenHook/BatmanGraphicsFileOperations.h>
#include <functional>
#include <stdexcept>
#include <utility>

/** @brief Runs a required test callback at the real publication boundary without mocking the writer or file effects. */
class CallbackBatmanGraphicsFileOperations : public helen::BatmanGraphicsFileOperations {
private:
    /** @brief Test-controlled boundary action used for deterministic reentrancy and concurrency checks. */
    std::function<void()> BeforeReplace;
public:
    /** @brief Requires a real boundary action rather than silently accepting an empty callback. */
    explicit CallbackBatmanGraphicsFileOperations(std::function<void()> before_replace) : BeforeReplace(std::move(before_replace)) {
        if (!BeforeReplace) {
            throw std::invalid_argument("Publication test callback is required.");
        }
    }
    /** @brief Invokes the boundary action, then performs actual Win32 publication on temporary test files. */
    bool Replace(const std::filesystem::path& target, const std::filesystem::path& staged, unsigned long& error) const override {
        BeforeReplace();
        return BatmanGraphicsFileOperations::Replace(target, staged, error);
    }
};
