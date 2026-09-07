#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

#include <HelenHook/Hook.h>

/**
 * @brief Patches one real test-executable GetFullPathNameW import to change CWD after capture.
 *
 * This guard exercises the production normalization boundary without adding a runtime test seam.
 * It is intentionally one-shot so a single request can prove that later native dispatch uses the
 * captured absolute operand rather than resolving the original relative spelling again.
 */
class CapturedPathSwitchGuard
{
public:
    /**
     * @brief Installs a one-shot switch for one exact path operand.
     * @param module Main test executable whose normalization import should be patched.
     * @param request_name Exact path operand that triggers the switch after normalization returns.
     * @param switch_directory Directory that becomes the process CWD after capture.
     */
    CapturedPathSwitchGuard(const helen::ModuleView& module, std::wstring request_name,
        std::filesystem::path switch_directory)
        : request_name_(std::move(request_name)), switch_directory_(std::move(switch_directory))
    {
        active_instance_ = this;
        if (!hook_.Install(module, "kernel32.dll", "GetFullPathNameW",
            reinterpret_cast<void*>(&GetFullPathNameWDetour)))
        {
            active_instance_ = nullptr;
            throw std::runtime_error("Captured-path GetFullPathNameW import hook installation failed.");
        }

        original_ = hook_.Original<GetFullPathNameWFunction>();
        if (original_ == nullptr)
        {
            hook_.Remove();
            active_instance_ = nullptr;
            throw std::runtime_error("Captured-path GetFullPathNameW original was not captured.");
        }
    }

    /** @brief Removes the import patch and clears the one-shot callback owner. */
    ~CapturedPathSwitchGuard()
    {
        hook_.Remove();
        if (active_instance_ == this)
        {
            active_instance_ = nullptr;
        }
    }

    /** @brief Prevents copying an active import patch that has a single callback owner. */
    CapturedPathSwitchGuard(const CapturedPathSwitchGuard&) = delete;
    /** @brief Prevents assigning an active import patch across independent CWD fixtures. */
    CapturedPathSwitchGuard& operator=(const CapturedPathSwitchGuard&) = delete;

    /** @brief Reports whether the native normalization boundary performed the requested CWD switch. */
    bool Triggered() const noexcept
    {
        return triggered_;
    }

private:
    /** @brief Exact native function-pointer type captured from the original import slot. */
    using GetFullPathNameWFunction = decltype(&GetFullPathNameW);

    /** @brief Current guard receiving the one-shot import callback. */
    static inline CapturedPathSwitchGuard* active_instance_ = nullptr;

    /** @brief Import patch that owns the original slot and restores it on destruction. */
    helen::IatHook hook_;
    /** @brief Original native normalization export used by the detour before changing CWD. */
    GetFullPathNameWFunction original_ = nullptr;
    /** @brief Relative request spelling that activates the one-shot switch. */
    std::wstring request_name_;
    /** @brief Destination process CWD selected after native normalization returns. */
    std::filesystem::path switch_directory_;
    /** @brief Whether this guard has already changed CWD for its request. */
    bool triggered_ = false;

    /**
     * @brief Calls native normalization, then changes CWD for the matching operand exactly once.
     * @param file_name Path being normalized by the production service.
     * @param buffer_size Number of UTF-16 elements available in the result buffer.
     * @param buffer Receives the native absolute path result.
     * @param file_part Receives the optional native file-part pointer.
     * @return Native GetFullPathNameW result returned before the CWD change.
     */
    static DWORD WINAPI GetFullPathNameWDetour(LPCWSTR file_name, DWORD buffer_size, LPWSTR buffer, LPWSTR* file_part)
    {
        CapturedPathSwitchGuard* const active = active_instance_;
        if (active == nullptr || active->original_ == nullptr)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return 0;
        }

        const DWORD result = active->original_(file_name, buffer_size, buffer, file_part);
        if (!active->triggered_ && file_name != nullptr && active->request_name_ == file_name)
        {
            if (SetCurrentDirectoryW(active->switch_directory_.c_str()) == FALSE)
            {
                throw std::runtime_error("Captured-path CWD switch failed after native path normalization.");
            }
            active->triggered_ = true;
        }
        return result;
    }
};
